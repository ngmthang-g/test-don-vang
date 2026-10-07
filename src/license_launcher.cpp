#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <memory>
#include <atomic>

#include "license_build_config.h"
#include "native_obfuscation.h"

int WINAPI ThanLongCoreMain(HINSTANCE, HINSTANCE, PWSTR, int);

namespace {

constexpr wchar_t kProductTitle[] = L"Công cụ hỗ trợ game rảnh tay • 10.6";
constexpr wchar_t kLicenseWindowClass[] = L"AutoDonDoThanLongProMaxLicenseV99";
constexpr DWORD kHeartbeatMs = 12u * 60u * 60u * 1000u;
constexpr int kMaxHeartbeatTransportFailures = 1;
constexpr ULONGLONG kLicenseActionFreshnessMs = 13ull * 60ull * 60ull * 1000ull;
constexpr int IDC_LICENSE_KEY = 9101;
constexpr int IDC_LICENSE_ACTIVATE = 9102;
constexpr int IDC_LICENSE_STATUS = 9103;

std::wstring gActiveKey;
std::wstring gMachineHash;
std::wstring gLegacyMachineHash;
std::wstring gMachineName;
std::wstring gSessionId;
std::string gMachineProfileJson = "{}";
std::atomic<long long> gRemainingSecondsAtSync{-1};
std::atomic<ULONGLONG> gRemainingSyncTick{0};
std::atomic<bool> gLicenseActionGate{false};
std::atomic<unsigned long long> gLicenseSessionToken{0};
std::atomic<unsigned long long> gLicenseGuardCanary{0};

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), needed);
    return out;
}

std::wstring Trim(std::wstring value) {
    const auto notSpace = [](wchar_t ch) { return !iswspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::wstring NormalizeKey(std::wstring value) {
    value = Trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towupper(ch)); });
    return value;
}

std::string JsonEscape(const std::wstring& value) {
    const std::string utf8 = WideToUtf8(value);
    std::string out;
    out.reserve(utf8.size() + 8);
    for (unsigned char ch : utf8) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buf[7]{};
                    sprintf_s(buf, "\\u%04x", static_cast<unsigned>(ch));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(ch));
                }
        }
    }
    return out;
}

bool JsonBool(const std::string& json, const char* key, bool& value) {
    const std::string needle = std::string("\"") + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (json.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (json.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool JsonInt64(const std::string& json, const char* key, long long& value) {
    const std::string needle = std::string("\"") + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    const std::size_t start = pos;
    if (pos < json.size() && json[pos] == '-') ++pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos == start || (pos == start + 1 && json[start] == '-')) return false;
    try {
        value = std::stoll(json.substr(start, pos - start));
        return true;
    } catch (...) {
        return false;
    }
}

bool JsonString(const std::string& json, const char* key, std::wstring& value) {
    const std::string needle = std::string("\"") + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '"') return false;
    ++pos;
    std::string raw;
    while (pos < json.size()) {
        char ch = json[pos++];
        if (ch == '"') {
            value = Utf8ToWide(raw);
            return true;
        }
        if (ch == '\\' && pos < json.size()) {
            const char esc = json[pos++];
            switch (esc) {
                case '"': raw.push_back('"'); break;
                case '\\': raw.push_back('\\'); break;
                case '/': raw.push_back('/'); break;
                case 'b': raw.push_back('\b'); break;
                case 'f': raw.push_back('\f'); break;
                case 'n': raw.push_back('\n'); break;
                case 'r': raw.push_back('\r'); break;
                case 't': raw.push_back('\t'); break;
                default: return false;
            }
        } else {
            raw.push_back(ch);
        }
    }
    return false;
}

std::wstring LastErrorText(const wchar_t* prefix) {
    return std::wstring(prefix) + L" (Win32=" + std::to_wstring(GetLastError()) + L")";
}

std::wstring ApiBase() {
    std::wstring value = TlLicenseApiUrl();
    while (!value.empty() && value.back() == L'/') value.pop_back();
    return value;
}

template <std::size_t N>
__declspec(noinline) std::wstring DecodeWideXor(const std::array<std::uint16_t, N>& encoded) {
    volatile std::uint16_t runtimeKey = 0x5A37u;
    const std::uint16_t key = runtimeKey;
    std::wstring out;
    out.reserve(N);
    for (std::uint16_t v : encoded) out.push_back(static_cast<wchar_t>(v ^ key));
    return out;
}

std::wstring ValidateRoute() {
    static constexpr std::array<std::uint16_t, 9> e{23064,23105,23126,23131,23134,23123,23126,23107,23122};
    return DecodeWideXor(e);
}
std::wstring HeartbeatRoute() {
    static constexpr std::array<std::uint16_t, 10> e{23064,23135,23122,23126,23109,23107,23125,23122,23126,23107};
    return DecodeWideXor(e);
}
std::wstring NetworkUserAgent() {
    static constexpr std::array<std::uint16_t, 19> e{23158,23138,23139,23160,23139,23135,23126,23129,23163,23128,23129,23120,23143,23109,23128,23064,23044,23065,23043};
    return DecodeWideXor(e);
}
std::wstring HttpPostMethod() {
    static constexpr std::array<std::uint16_t, 4> e{23143,23160,23140,23139};
    return DecodeWideXor(e);
}
std::wstring HttpJsonHeaders() {
    static constexpr std::array<std::uint16_t, 73> e{23156,23128,23129,23107,23122,23129,23107,23066,23139,23118,23111,23122,23053,23063,23126,23111,23111,23131,23134,23124,23126,23107,23134,23128,23129,23064,23133,23108,23128,23129,23052,23063,23124,23135,23126,23109,23108,23122,23107,23050,23106,23107,23121,23066,23055,23098,23101,23158,23124,23124,23122,23111,23107,23053,23063,23126,23111,23111,23131,23134,23124,23126,23107,23134,23128,23129,23064,23133,23108,23128,23129,23098,23101};
    return DecodeWideXor(e);
}
std::wstring MachineGuidRegistryPath() {
    static constexpr std::array<std::uint16_t, 31> e{23140,23160,23153,23139,23136,23158,23141,23154,23147,23162,23134,23124,23109,23128,23108,23128,23121,23107,23147,23156,23109,23118,23111,23107,23128,23120,23109,23126,23111,23135,23118};
    return DecodeWideXor(e);
}
std::wstring MachineGuidRegistryName() {
    static constexpr std::array<std::uint16_t, 11> e{23162,23126,23124,23135,23134,23129,23122,23152,23106,23134,23123};
    return DecodeWideXor(e);
}
std::wstring BiosRegistryPath() {
    static constexpr std::array<std::uint16_t, 32> e{23167,23158,23141,23155,23136,23158,23141,23154,23147,23155,23154,23140,23156,23141,23166,23143,23139,23166,23160,23161,23147,23140,23118,23108,23107,23122,23130,23147,23157,23166,23160,23140};
    return DecodeWideXor(e);
}

bool PostJson(const wchar_t* route, const std::string& body, std::string& response, DWORD& statusCode, std::wstring& error) {
    response.clear();
    statusCode = 0;
    const std::wstring base = ApiBase();
    if (base.empty()) {
        error = L"Server license chưa được cấu hình trong bản build.";
        return false;
    }

    const std::wstring url = base + route;
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        error = LastErrorText(L"URL server license không hợp lệ");
        return false;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Server license bắt buộc phải dùng HTTPS.";
        return false;
    }

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength > 0) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    const std::wstring userAgent = NetworkUserAgent();
    HINTERNET session = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        error = LastErrorText(L"Không mở được WinHTTP");
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 7000, 7000);

    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    if (!connection) {
        error = LastErrorText(L"Không kết nối được server license");
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring httpMethod = HttpPostMethod();
    HINTERNET request = WinHttpOpenRequest(connection, httpMethod.c_str(), path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!request) {
        error = LastErrorText(L"Không tạo được request license");
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring headers = HttpJsonHeaders();
    const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                                         const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        error = LastErrorText(L"Server license không phản hồi");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusSize = sizeof(statusCode);
    (void)WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    constexpr DWORD kMaxResponseBytes = 64u * 1024u;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = LastErrorText(L"Không đọc được phản hồi license");
            response.clear();
            break;
        }
        if (available == 0) break;
        if (response.size() + available > kMaxResponseBytes) {
            error = L"Phản hồi license vượt giới hạn an toàn.";
            response.clear();
            break;
        }
        const std::size_t oldSize = response.size();
        response.resize(oldSize + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, response.data() + oldSize, available, &read)) {
            error = LastErrorText(L"Không đọc được dữ liệu license");
            response.clear();
            break;
        }
        response.resize(oldSize + read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return !response.empty();
}

std::wstring ReadRegistryString(HKEY root, const wchar_t* subkey, const wchar_t* name) {
    DWORD bytes = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY;
    if (RegGetValueW(root, subkey, name, flags, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes < sizeof(wchar_t)) return L"";
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(root, subkey, name, flags, nullptr, buffer.data(), &bytes) != ERROR_SUCCESS) return L"";
    return buffer.data();
}

std::wstring CurrentMachineName() {
    wchar_t buffer[256]{};
    DWORD count = static_cast<DWORD>(std::size(buffer));
    if (!GetComputerNameW(buffer, &count) || count == 0) return L"UNKNOWN-PC";
    return std::wstring(buffer, count);
}

std::wstring Sha256Bytes(const BYTE* data, DWORD size) {
    std::array<BYTE, 32> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    if (!data || size == 0 ||
        !CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr,
                               data, size, digest.data(), &digestSize) ||
        digestSize != digest.size()) {
        return L"";
    }
    std::wostringstream out;
    out << std::hex << std::setfill(L'0');
    for (BYTE b : digest) out << std::setw(2) << static_cast<unsigned>(b);
    return out.str();
}

std::wstring Sha256Hex(const std::wstring& text) {
    const std::string input = WideToUtf8(text);
    if (input.empty()) return L"";
    return Sha256Bytes(reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()));
}

std::wstring FirmwareTableHash() {
    const DWORD provider = 'RSMB';
    const UINT size = GetSystemFirmwareTable(provider, 0, nullptr, 0);
    if (size == 0 || size > 8u * 1024u * 1024u) return L"";
    std::vector<BYTE> data(size);
    const UINT read = GetSystemFirmwareTable(provider, 0, data.data(), size);
    if (read == 0 || read > size) return L"";
    return Sha256Bytes(data.data(), static_cast<DWORD>(read));
}

std::wstring HashComponent(const std::wstring& value) {
    return value.empty() ? L"" : Sha256Hex(value);
}

struct MachineFingerprint {
    std::wstring deviceHash;
    std::wstring legacyDeviceHash;
    std::string profileJson = "{}";
};

MachineFingerprint BuildMachineFingerprint() {
    const std::wstring machineGuidPath = MachineGuidRegistryPath();
    const std::wstring machineGuidName = MachineGuidRegistryName();
    const std::wstring machineGuid = ReadRegistryString(HKEY_LOCAL_MACHINE,
        machineGuidPath.c_str(), machineGuidName.c_str());

    wchar_t windowsDir[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDir, MAX_PATH);
    wchar_t root[] = L"C:\\";
    if (windowsDir[0] && windowsDir[1] == L':') root[0] = windowsDir[0];
    DWORD serial = 0;
    (void)GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);

    wchar_t processor[512]{};
    const DWORD processorLen = GetEnvironmentVariableW(L"PROCESSOR_IDENTIFIER", processor, static_cast<DWORD>(std::size(processor)));
    const std::wstring processorId = processorLen > 0 && processorLen < std::size(processor)
        ? std::wstring(processor, processorLen) : L"";

    const std::wstring biosKey = BiosRegistryPath();
    const std::wstring systemManufacturer = TL_OBF_W(L"SystemManufacturer");
    const std::wstring systemProductName = TL_OBF_W(L"SystemProductName");
    const std::wstring systemSku = TL_OBF_W(L"SystemSKU");
    const std::wstring systemFamily = TL_OBF_W(L"SystemFamily");
    const std::wstring boardManufacturer = TL_OBF_W(L"BaseBoardManufacturer");
    const std::wstring boardProduct = TL_OBF_W(L"BaseBoardProduct");
    const std::wstring boardVersion = TL_OBF_W(L"BaseBoardVersion");
    const std::wstring biosVendor = TL_OBF_W(L"BIOSVendor");
    const std::wstring biosVersion = TL_OBF_W(L"BIOSVersion");
    const std::wstring biosReleaseDate = TL_OBF_W(L"BIOSReleaseDate");
    const std::wstring systemIdentity =
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), systemManufacturer.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), systemProductName.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), systemSku.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), systemFamily.c_str());
    const std::wstring boardIdentity =
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), boardManufacturer.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), boardProduct.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), boardVersion.c_str());
    const std::wstring biosIdentity =
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), biosVendor.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), biosVersion.c_str()) + L"|" +
        ReadRegistryString(HKEY_LOCAL_MACHINE, biosKey.c_str(), biosReleaseDate.c_str());

    const std::wstring machineGuidHash = HashComponent(machineGuid);
    const std::wstring volumeHash = HashComponent(std::to_wstring(serial));
    const std::wstring cpuHash = HashComponent(processorId);
    const std::wstring systemHash = HashComponent(systemIdentity);
    const std::wstring boardHash = HashComponent(boardIdentity);
    const std::wstring biosHash = HashComponent(biosIdentity);
    const std::wstring firmwareHash = FirmwareTableHash();

    int available = 0;
    for (const auto* value : {&machineGuidHash, &volumeHash, &cpuHash, &systemHash, &boardHash, &biosHash, &firmwareHash})
        if (!value->empty()) ++available;
    if (available < 4) return {};

    const std::wstring combined = L"TLPRO-FP-V2|" + machineGuidHash + L"|" + volumeHash + L"|" +
                                  cpuHash + L"|" + systemHash + L"|" + boardHash + L"|" +
                                  biosHash + L"|" + firmwareHash;
    MachineFingerprint result{};
    result.deviceHash = Sha256Hex(combined);
    // Giữ fingerprint v1 cũ để server nâng key đang hoạt động sang v2 mà không
    // tự chặn nhầm chính máy đã kích hoạt trước khi nâng cấp tool.
    result.legacyDeviceHash = Sha256Hex(machineGuid + L"|" + std::to_wstring(serial) + L"|" + processorId);
    if (result.deviceHash.empty() || result.legacyDeviceHash.empty()) return {};

    auto hex = [](const std::wstring& value) { return WideToUtf8(value); };
    result.profileJson = std::string("{") +
        "\"fingerprintVersion\":2," +
        "\"machineGuidHash\":\"" + hex(machineGuidHash) + "\"," +
        "\"systemVolumeHash\":\"" + hex(volumeHash) + "\"," +
        "\"cpuHash\":\"" + hex(cpuHash) + "\"," +
        "\"systemProductHash\":\"" + hex(systemHash) + "\"," +
        "\"baseBoardHash\":\"" + hex(boardHash) + "\"," +
        "\"biosHash\":\"" + hex(biosHash) + "\"," +
        "\"firmwareHash\":\"" + hex(firmwareHash) + "\"}";
    return result;
}

std::wstring RandomHex(std::size_t bytes) {
    std::vector<UCHAR> random(bytes);
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return L"";
    std::wostringstream out;
    out << std::hex << std::setfill(L'0');
    for (UCHAR b : random) out << std::setw(2) << static_cast<unsigned>(b);
    return out.str();
}

unsigned long long RandomSessionToken() {
    unsigned long long token = 0;
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&token), sizeof(token),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return 0;
    return token ? token : 0xD6E8FEB86659FD93ull;
}

std::filesystem::path LicensePath() {
    wchar_t localAppData[32768]{};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    std::filesystem::path base;
    if (len > 0 && len < std::size(localAppData)) base = localAppData;
    else base = std::filesystem::temp_directory_path();
    return base / L"AUTOThanLongPro" / L"license.dat";
}

bool SaveLocalKey(const std::wstring& key) {
    const std::string utf8 = WideToUtf8(key);
    if (utf8.empty()) return false;
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.data()));
    input.cbData = static_cast<DWORD>(utf8.size());
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"AUTO Thần Long Pro license", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE, &output)) return false;

    const auto path = LicensePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        LocalFree(output.pbData);
        return false;
    }
    file.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
    const bool ok = file.good();
    LocalFree(output.pbData);
    return ok;
}

std::wstring LoadLocalKey() {
    const auto path = LicensePath();
    std::ifstream file(path, std::ios::binary);
    if (!file) return L"";
    std::vector<BYTE> encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (encrypted.empty() || encrypted.size() > 64 * 1024) return L"";
    DATA_BLOB input{};
    input.pbData = encrypted.data();
    input.cbData = static_cast<DWORD>(encrypted.size());
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) return L"";
    std::string utf8(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return NormalizeKey(Utf8ToWide(utf8));
}

struct LicenseResult {
    bool transportOk = false;
    bool accepted = false;
    std::wstring message;
    long long remainingSeconds = -1;
};

unsigned long long LicenseGuardCanary(unsigned long long token, ULONGLONG syncTick, long long baseSeconds) {
    volatile unsigned long long p0 = 0x6a09e667f3bcc909ULL;
    volatile unsigned long long p1 = 0xbb67ae8584caa73bULL;
    const auto base = static_cast<unsigned long long>(baseSeconds < 0 ? 0 : baseSeconds);
    return tlobf::RuntimeMix(token ^ static_cast<unsigned long long>(syncTick) ^
                             (base * p0) ^ (p1 + 0x3c6ef372fe94f82bULL));
}

void SyncLicenseRemaining(const LicenseResult& result) {
    if (!result.accepted || result.remainingSeconds < 0) return;
    const ULONGLONG now = GetTickCount64();
    gRemainingSecondsAtSync.store(result.remainingSeconds, std::memory_order_relaxed);
    gRemainingSyncTick.store(now, std::memory_order_relaxed);
    const auto token = gLicenseSessionToken.load(std::memory_order_relaxed);
    gLicenseGuardCanary.store(LicenseGuardCanary(token, now, result.remainingSeconds),
                              std::memory_order_release);
}

long long CurrentLicenseRemainingSeconds() {
    const long long base = gRemainingSecondsAtSync.load(std::memory_order_relaxed);
    const ULONGLONG tick = gRemainingSyncTick.load(std::memory_order_relaxed);
    if (base < 0 || tick == 0) return -1;
    const ULONGLONG elapsed = (GetTickCount64() - tick) / 1000ULL;
    if (elapsed >= static_cast<ULONGLONG>(base)) return 0;
    return base - static_cast<long long>(elapsed);
}

__declspec(noinline) bool LicenseGateLinear() {
    if (!gLicenseActionGate.load(std::memory_order_acquire)) return false;
    if (gActiveKey.empty() || gMachineHash.empty() || gLegacyMachineHash.empty() ||
        gMachineProfileJson.empty() || gSessionId.empty()) return false;
    const auto token = gLicenseSessionToken.load(std::memory_order_relaxed);
    if (token == 0 || CurrentLicenseRemainingSeconds() <= 0) return false;
    const ULONGLONG syncTick = gRemainingSyncTick.load(std::memory_order_relaxed);
    const long long base = gRemainingSecondsAtSync.load(std::memory_order_relaxed);
    if (syncTick == 0 || base < 0 || GetTickCount64() - syncTick > kLicenseActionFreshnessMs) return false;
    const auto canary = gLicenseGuardCanary.load(std::memory_order_acquire);
    return canary != 0 && canary == LicenseGuardCanary(token, syncTick, base);
}

__declspec(noinline) bool LicenseGateStateMachine() {
    volatile unsigned state = 0x53u;
    bool ok = true;
    for (;;) {
        switch (state) {
            case 0x53u:
                ok = ok && gLicenseActionGate.load(std::memory_order_acquire);
                state = 0xC1u;
                break;
            case 0xC1u:
                ok = ok && !gActiveKey.empty() && !gSessionId.empty();
                state = 0x2Fu;
                break;
            case 0x2Fu:
                ok = ok && !gMachineHash.empty() && !gLegacyMachineHash.empty() && !gMachineProfileJson.empty();
                state = 0xA8u;
                break;
            case 0xA8u: {
                const auto token = gLicenseSessionToken.load(std::memory_order_relaxed);
                const auto tick = gRemainingSyncTick.load(std::memory_order_relaxed);
                const auto base = gRemainingSecondsAtSync.load(std::memory_order_relaxed);
                const auto canary = gLicenseGuardCanary.load(std::memory_order_acquire);
                ok = ok && token != 0 && tick != 0 && base >= 0 && canary != 0 &&
                     canary == LicenseGuardCanary(token, tick, base);
                state = 0x17u;
                break;
            }
            case 0x17u:
                ok = ok && CurrentLicenseRemainingSeconds() > 0;
                state = 0xE4u;
                break;
            case 0xE4u: {
                const auto tick = gRemainingSyncTick.load(std::memory_order_relaxed);
                ok = ok && tick != 0 && GetTickCount64() - tick <= kLicenseActionFreshnessMs;
                state = 0x6Bu;
                break;
            }
            case 0x6Bu:
                return ok;
            default:
                return false;
        }
    }
}

bool LicenseActionGateOpen() {
    // Two deliberately different evaluation paths must agree. Patching a single
    // branch or one obvious return site no longer opens the action dispatcher.
    const bool linear = LicenseGateLinear();
    const bool stateMachine = LicenseGateStateMachine();
    return linear && stateMachine;
}

LicenseResult CallLicenseRoute(const wchar_t* route, const std::wstring& key) {
    LicenseResult result{};
    const std::string body = std::string("{") +
        "\"key\":\"" + JsonEscape(key) + "\"," +
        "\"deviceHash\":\"" + JsonEscape(gMachineHash) + "\"," +
        "\"legacyDeviceHash\":\"" + JsonEscape(gLegacyMachineHash) + "\"," +
        "\"deviceProfile\":" + (gMachineProfileJson.empty() ? std::string("{}") : gMachineProfileJson) + "," +
        "\"machineName\":\"" + JsonEscape(gMachineName) + "\"," +
        "\"sessionId\":\"" + JsonEscape(gSessionId) + "\"," +
        "\"appVersion\":\"9.9-special\"}";

    std::string response;
    DWORD status = 0;
    std::wstring error;
    if (!PostJson(route, body, response, status, error)) {
        result.message = error.empty() ? L"Không liên lạc được server license." : error;
        return result;
    }

    bool ok = false;
    if (!JsonBool(response, "ok", ok)) {
        result.message = L"Server license trả về dữ liệu không hợp lệ.";
        return result;
    }
    result.transportOk = true;
    result.accepted = ok;
    if (!JsonString(response, "message", result.message)) {
        result.message = ok ? L"License hợp lệ." : L"License không hợp lệ.";
    }
    long long remaining = -1;
    if (JsonInt64(response, "remainingSeconds", remaining)) result.remainingSeconds = std::max(0LL, remaining);
    return result;
}

struct ActivationState {
    HINSTANCE instance = nullptr;
    std::wstring initialKey;
    std::wstring initialStatus;
    std::wstring grantedKey;
    bool granted = false;
    bool closed = false;
    HWND edit = nullptr;
    HWND status = nullptr;
    HWND button = nullptr;
};

void ApplyDefaultFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

LRESULT CALLBACK LicenseWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ActivationState* state = reinterpret_cast<ActivationState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lp);
        state = reinterpret_cast<ActivationState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    switch (msg) {
        case WM_CREATE: {
            if (!state) return -1;
            HWND title = CreateWindowExW(0, L"STATIC", L"Công cụ hỗ trợ game rảnh tay • 10.6",
                WS_CHILD | WS_VISIBLE, 18, 16, 470, 22, hwnd, nullptr, state->instance, nullptr);
            HWND hint = CreateWindowExW(0, L"STATIC", L"Nhập key kích hoạt. Mỗi key chỉ được khóa vào một máy.",
                WS_CHILD | WS_VISIBLE, 18, 43, 470, 20, hwnd, nullptr, state->instance, nullptr);
            state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->initialKey.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_UPPERCASE,
                18, 69, 360, 27, hwnd, reinterpret_cast<HMENU>(IDC_LICENSE_KEY), state->instance, nullptr);
            state->button = CreateWindowExW(0, L"BUTTON", L"KÍCH HOẠT",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                386, 69, 103, 27, hwnd, reinterpret_cast<HMENU>(IDC_LICENSE_ACTIVATE), state->instance, nullptr);
            state->status = CreateWindowExW(0, L"STATIC", state->initialStatus.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 106, 470, 36, hwnd, reinterpret_cast<HMENU>(IDC_LICENSE_STATUS), state->instance, nullptr);
            for (HWND child : {title, hint, state->edit, state->button, state->status}) ApplyDefaultFont(child);
            SendMessageW(state->edit, EM_SETLIMITTEXT, 128, 0);
            SetFocus(state->edit);
            SendMessageW(state->edit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_COMMAND:
            if (state && LOWORD(wp) == IDC_LICENSE_ACTIVATE && HIWORD(wp) == BN_CLICKED) {
                wchar_t buffer[256]{};
                GetWindowTextW(state->edit, buffer, static_cast<int>(std::size(buffer)));
                const std::wstring key = NormalizeKey(buffer);
                if (key.empty()) {
                    SetWindowTextW(state->status, L"Chưa nhập key.");
                    return 0;
                }
                EnableWindow(state->button, FALSE);
                SetWindowTextW(state->status, L"Đang kiểm tra key với server...");
                UpdateWindow(hwnd);
                const LicenseResult check = CallLicenseRoute(ValidateRoute().c_str(), key);
                if (check.transportOk && check.accepted) {
                    SyncLicenseRemaining(check);
                    if (!SaveLocalKey(key)) {
                        SetWindowTextW(state->status, L"Key hợp lệ nhưng không lưu được license trên máy.");
                        EnableWindow(state->button, TRUE);
                        return 0;
                    }
                    state->granted = true;
                    state->grantedKey = key;
                    DestroyWindow(hwnd);
                    return 0;
                }
                SetWindowTextW(state->status, check.message.c_str());
                EnableWindow(state->button, TRUE);
                SetFocus(state->edit);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state) state->closed = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RunActivationWindow(HINSTANCE instance, const std::wstring& initialKey,
                         const std::wstring& initialStatus, std::wstring& grantedKey) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = LicenseWndProc;
    wc.lpszClassName = kLicenseWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    ActivationState state{};
    state.instance = instance;
    state.initialKey = initialKey;
    state.initialStatus = initialStatus.empty() ? L"Tool chỉ mở khi server xác nhận key hợp lệ." : initialStatus;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kLicenseWindowClass, kProductTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 525, 190, nullptr, nullptr, instance, &state);
    if (!hwnd) return false;

    RECT rc{};
    GetWindowRect(hwnd, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (!state.closed && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (state.granted) grantedKey = state.grantedKey;
    return state.granted;
}

bool AcquireLicense(HINSTANCE instance) {
    gLicenseActionGate.store(false, std::memory_order_release);
    gLicenseSessionToken.store(0, std::memory_order_relaxed);
    if (ApiBase().empty()) {
        MessageBoxW(nullptr,
            L"Bản 10.6 chưa được cấu hình địa chỉ HTTPS của Supabase License API.\n"
            L"Tool dừng fail-closed và không mở lõi AUTO.",
            kProductTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    const MachineFingerprint fingerprint = BuildMachineFingerprint();
    gMachineHash = fingerprint.deviceHash;
    gLegacyMachineHash = fingerprint.legacyDeviceHash;
    gMachineProfileJson = fingerprint.profileJson;
    gMachineName = CurrentMachineName();
    gSessionId = RandomHex(16);
    gLicenseSessionToken.store(RandomSessionToken(), std::memory_order_relaxed);
    if (gMachineHash.empty() || gLegacyMachineHash.empty() || gMachineProfileJson.empty() || gSessionId.empty() ||
        gLicenseSessionToken.load(std::memory_order_relaxed) == 0) {
        MessageBoxW(nullptr, L"Không tạo được định danh máy an toàn. Tool không chạy.", kProductTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    const std::wstring saved = LoadLocalKey();
    std::wstring status;
    if (!saved.empty()) {
        const LicenseResult check = CallLicenseRoute(ValidateRoute().c_str(), saved);
        if (check.transportOk && check.accepted) {
            SyncLicenseRemaining(check);
            gActiveKey = saved;
            gLicenseActionGate.store(true, std::memory_order_release);
            return true;
        }
        status = check.message;
    }

    std::wstring key;
    if (!RunActivationWindow(instance, saved, status, key)) return false;
    gActiveKey = key;
    gLicenseActionGate.store(CurrentLicenseRemainingSeconds() > 0, std::memory_order_release);
    return gLicenseActionGate.load(std::memory_order_acquire);
}

DWORD WINAPI HeartbeatThread(LPVOID) {
    int transportFailures = 0;
    for (;;) {
        Sleep(kHeartbeatMs);
        const LicenseResult check = CallLicenseRoute(HeartbeatRoute().c_str(), gActiveKey);
        if (!check.transportOk) {
            ++transportFailures;
            if (transportFailures < kMaxHeartbeatTransportFailures) continue;
            gLicenseActionGate.store(false, std::memory_order_release);
            MessageBoxW(nullptr,
                L"Không thể xác minh license ở kỳ kiểm tra 12 giờ. Lõi tính năng đã khóa và tool sẽ đóng fail-closed.",
                kProductTitle, MB_ICONERROR | MB_OK);
            ExitProcess(31);
        }
        transportFailures = 0;
        if (!check.accepted) {
            gLicenseActionGate.store(false, std::memory_order_release);
            MessageBoxW(nullptr, check.message.c_str(), kProductTitle, MB_ICONERROR | MB_OK);
            ExitProcess(32);
        }
        SyncLicenseRemaining(check);
        gLicenseActionGate.store(true, std::memory_order_release);
    }
}

} // namespace

extern "C" long long ThanLongLicenseRemainingSeconds() {
    return CurrentLicenseRemainingSeconds();
}

extern "C" int ThanLongLicenseActionAllowed() {
    return LicenseActionGateOpen() ? 1 : 0;
}

extern "C" unsigned long long ThanLongLicenseSessionToken() {
    return LicenseActionGateOpen() ? gLicenseSessionToken.load(std::memory_order_relaxed) : 0ull;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR commandLine, int show) {
    if (!AcquireLicense(instance)) return 30;

    HANDLE heartbeat = CreateThread(nullptr, 0, HeartbeatThread, nullptr, 0, nullptr);
    if (heartbeat) CloseHandle(heartbeat);

    return ThanLongCoreMain(instance, previous, commandLine, show);
}
