#include "telegram_notifier.h"
#include "telegram_logic.h"
#include "telegram_account_filter.h"

#include <winhttp.h>
#include <wincrypt.h>
#include <chrono>
#include <cstring>
#include <new>
#include <vector>

namespace telegram_notify {
namespace {

constexpr std::size_t kMaxQueue = 200;
constexpr wchar_t kHost[] = L"api.telegram.org";

struct InternetHandle {
    HINTERNET h = nullptr;
    ~InternetHandle() { if (h) WinHttpCloseHandle(h); }
};

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int need = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (need <= 0) return {};
    std::string out(static_cast<std::size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), need, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int need = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (need <= 0) return {};
    std::wstring out(static_cast<std::size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), need);
    return out;
}

std::string FormEncode(const std::wstring& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    const std::string utf8 = WideToUtf8(value);
    std::string out;
    out.reserve(utf8.size() * 3 / 2 + 8);
    for (unsigned char c : utf8) {
        const bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (safe) out.push_back(static_cast<char>(c));
        else if (c == ' ') out.push_back('+');
        else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::wstring WinError(DWORD code) {
    wchar_t* raw = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    std::wstring text;
    if (n && raw) text.assign(raw, n);
    else text = L"WinHTTP error " + std::to_wstring(code);
    if (raw) LocalFree(raw);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) text.pop_back();
    return text;
}

bool HttpRequest(const std::wstring& token, const std::wstring& method, const char* verb,
                 const std::string& body, DWORD& status, std::string& response, std::wstring& error) {
    if (token.empty()) { error = L"Bot Token đang trống"; return false; }
    InternetHandle session{WinHttpOpen(L"ThanLongItemConsolidator/1.4", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) { error = WinError(GetLastError()); return false; }
    WinHttpSetTimeouts(session.h, 5000, 5000, 5000, 7000);

    InternetHandle connection{WinHttpConnect(session.h, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.h) { error = WinError(GetLastError()); return false; }
    const std::wstring path = L"/bot" + token + L"/" + method;
    const wchar_t* verbWide = std::strcmp(verb, "POST") == 0 ? L"POST" : L"GET";
    InternetHandle request{WinHttpOpenRequest(connection.h, verbWide, path.c_str(),
                                              nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              WINHTTP_FLAG_SECURE)};
    if (!request.h) { error = WinError(GetLastError()); return false; }

    const wchar_t* headers = nullptr;
    DWORD headersLen = 0;
    if (!body.empty()) {
        headers = L"Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n";
        headersLen = static_cast<DWORD>(-1L);
    }
    const BOOL sent = WinHttpSendRequest(request.h, headers, headersLen,
                                         body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                                         static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (!sent) { error = WinError(GetLastError()); return false; }
    if (!WinHttpReceiveResponse(request.h, nullptr)) { error = WinError(GetLastError()); return false; }

    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        error = WinError(GetLastError()); return false;
    }

    response.clear();
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.h, &available)) { error = WinError(GetLastError()); return false; }
        if (available == 0) break;
        const std::size_t old = response.size();
        response.resize(old + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.h, response.data() + old, available, &read)) { error = WinError(GetLastError()); return false; }
        response.resize(old + read);
        if (response.size() > 1024 * 1024) { error = L"Telegram response vượt 1MB"; return false; }
    }
    return true;
}

Result ExecuteOnce(const Request& req) {
    Result out{};
    out.id = req.id;
    out.kind = req.kind;
    out.eventType = req.eventType;
    out.account = req.account;
    out.message = req.message;

    DWORD status = 0;
    std::string response;
    std::wstring error;
    bool transportOk = false;
    if (req.kind == TaskKind::TestBot) {
        transportOk = HttpRequest(req.botToken, L"getMe", "GET", {}, status, response, error);
    } else if (req.kind == TaskKind::DiscoverChatId) {
        transportOk = HttpRequest(req.botToken, L"getUpdates?limit=100&timeout=0", "GET", {}, status, response, error);
    } else {
        const std::string body = "chat_id=" + FormEncode(req.chatId) + "&text=" + FormEncode(req.message);
        transportOk = HttpRequest(req.botToken, L"sendMessage", "POST", body, status, response, error);
    }
    out.httpStatus = status;
    if (!transportOk) {
        out.detail = error.empty() ? L"Lỗi mạng không xác định" : error;
        return out;
    }

    const bool apiOk = telegram_logic::BotApiOk(response);
    if (status < 200 || status >= 300 || !apiOk) {
        if (status == 401) out.detail = L"HTTP 401 • Bot Token không hợp lệ/đã bị thu hồi";
        else if (status == 409 && req.kind == TaskKind::DiscoverChatId)
            out.detail = L"HTTP 409 • bot đang dùng webhook; getUpdates không dùng đồng thời. Hãy nhập Chat ID thủ công hoặc bỏ webhook nếu phù hợp.";
        else if (status == 400 && req.kind == TaskKind::SendMessage)
            out.detail = L"HTTP 400 • kiểm tra Chat ID/quyền bot trong chat";
        else out.detail = L"HTTP " + std::to_wstring(status) + L" • Telegram API trả ok=false";
        return out;
    }

    if (req.kind == TaskKind::DiscoverChatId) {
        const std::string chat = telegram_logic::FindLatestChatIdInJson(response);
        if (chat.empty()) {
            out.detail = L"Không thấy Chat ID trong getUpdates. Hãy mở bot/group và gửi một tin nhắn hoặc lệnh trước.";
            return out;
        }
        out.discoveredChatId = Utf8ToWide(chat);
        out.detail = L"Đã lấy Chat ID từ update mới nhất";
    } else if (req.kind == TaskKind::TestBot) {
        out.detail = L"Bot Token hợp lệ • getMe OK";
    } else {
        out.detail = L"Đã gửi • HTTP " + std::to_wstring(status);
        out.sent = true;
    }
    out.ok = true;
    return out;
}

} // namespace

bool ProtectTokenForCurrentUser(const std::wstring& plain, std::wstring& encoded, std::wstring& error) {
    encoded.clear(); error.clear();
    if (plain.empty()) return true;
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(plain.data()));
    input.cbData = static_cast<DWORD>(plain.size() * sizeof(wchar_t));
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"ThanLong Telegram Bot Token", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = L"DPAPI Protect thất bại: " + WinError(GetLastError());
        return false;
    }
    DWORD chars = 0;
    const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    if (!CryptBinaryToStringW(output.pbData, output.cbData, flags, nullptr, &chars)) {
        error = L"Base64 token thất bại: " + WinError(GetLastError());
        LocalFree(output.pbData); return false;
    }
    std::wstring temp(chars, L'\0');
    if (!CryptBinaryToStringW(output.pbData, output.cbData, flags, temp.data(), &chars)) {
        error = L"Base64 token thất bại: " + WinError(GetLastError());
        LocalFree(output.pbData); return false;
    }
    LocalFree(output.pbData);
    if (!temp.empty() && temp.back() == L'\0') temp.pop_back();
    encoded = std::move(temp);
    return true;
}

bool UnprotectTokenForCurrentUser(const std::wstring& encoded, std::wstring& plain, std::wstring& error) {
    plain.clear(); error.clear();
    if (encoded.empty()) return true;
    DWORD bytes = 0;
    if (!CryptStringToBinaryW(encoded.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &bytes, nullptr, nullptr)) {
        error = L"Token đã lưu không phải DPAPI/Base64 hợp lệ"; return false;
    }
    std::vector<BYTE> cipher(bytes);
    if (!CryptStringToBinaryW(encoded.c_str(), 0, CRYPT_STRING_BASE64, cipher.data(), &bytes, nullptr, nullptr)) {
        error = L"Không giải mã được Base64 token"; return false;
    }
    DATA_BLOB input{bytes, cipher.data()};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = L"DPAPI Unprotect thất bại: " + WinError(GetLastError()); return false;
    }
    if (output.cbData % sizeof(wchar_t) != 0) {
        LocalFree(output.pbData); error = L"Dữ liệu DPAPI token sai kích thước"; return false;
    }
    plain.assign(reinterpret_cast<wchar_t*>(output.pbData), output.cbData / sizeof(wchar_t));
    LocalFree(output.pbData);
    return true;
}

Worker::~Worker() { Stop(); }

bool Worker::Start(HWND notifyWindow, UINT resultMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (thread_.joinable()) return true;
    notifyWindow_ = notifyWindow;
    resultMessage_ = resultMessage;
    stop_ = false;
    try { thread_ = std::thread([this]{ ThreadMain(); }); }
    catch (...) { return false; }
    return true;
}

void Worker::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!thread_.joinable()) return;
        stop_ = true;
        queue_.clear();
    }
    cv_.notify_all();
    thread_.join();
}

bool Worker::Enqueue(Request request) {
    if (request.kind == TaskKind::SendMessage) {
        std::wstring skipReason;
        if (!telegram_account_filter::PrepareRequest(request.account, request.eventType, request.message, skipReason)) {
            Result skipped{};
            skipped.id = request.id;
            skipped.kind = request.kind;
            skipped.ok = true;
            skipped.eventType = request.eventType;
            skipped.account = request.account;
            skipped.detail = L"SKIP • " + (skipReason.empty() ? std::wstring(L"acc chưa được phép báo cáo") : skipReason);
            auto* heap = new (std::nothrow) Result(std::move(skipped));
            if (heap && (!notifyWindow_ || !PostMessageW(notifyWindow_, resultMessage_, 0, reinterpret_cast<LPARAM>(heap)))) delete heap;
            return true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_ || !thread_.joinable() || queue_.size() >= kMaxQueue) return false;
        queue_.push_back(std::move(request));
    }
    cv_.notify_one();
    return true;
}

std::size_t Worker::Pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

Result Worker::Execute(const Request& request) {
    Result last{};
    constexpr int delaysMs[3] = {0, 2000, 5000};
    for (int attempt = 0; attempt < 3; ++attempt) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (attempt > 0 && cv_.wait_for(lock, std::chrono::milliseconds(delaysMs[attempt]), [this]{ return stop_; })) {
                last.id = request.id; last.kind = request.kind; last.eventType = request.eventType; last.account = request.account;
                last.detail = L"Worker đang dừng";
                return last;
            }
            if (stop_) {
                last.id = request.id; last.kind = request.kind; last.eventType = request.eventType; last.account = request.account;
                last.detail = L"Worker đang dừng";
                return last;
            }
        }
        last = ExecuteOnce(request);
        if (last.ok) return last;
        if (last.httpStatus >= 400 && last.httpStatus < 500 && last.httpStatus != 408 && last.httpStatus != 429) break;
    }
    if (!last.detail.empty()) last.detail += L" • đã thử tối đa 3 lần";
    return last;
}

void Worker::ThreadMain() {
    while (true) {
        Request request{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&]{ return stop_ || !queue_.empty(); });
            if (stop_) break;
            request = std::move(queue_.front());
            queue_.pop_front();
        }
        Result result = Execute(request);
        auto* heap = new (std::nothrow) Result(std::move(result));
        if (!heap) continue;
        if (!notifyWindow_ || !PostMessageW(notifyWindow_, resultMessage_, 0, reinterpret_cast<LPARAM>(heap))) delete heap;
    }
}

} // namespace telegram_notify
