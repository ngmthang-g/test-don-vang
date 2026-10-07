#pragma once

#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace telegram_notify {

enum class TaskKind : int {
    SendMessage = 0,
    TestBot = 1,
    DiscoverChatId = 2,
};

struct Request {
    std::uint64_t id = 0;
    TaskKind kind = TaskKind::SendMessage;
    std::wstring botToken;
    std::wstring chatId;
    std::wstring message;
    std::wstring eventType;
    std::wstring account;
};

struct Result {
    std::uint64_t id = 0;
    TaskKind kind = TaskKind::SendMessage;
    bool ok = false;
    bool sent = false; // true only when Telegram sendMessage actually returned ok=true
    DWORD httpStatus = 0;
    std::wstring message; // exact final payload after account filtering; used by TELE LOG mirror
    std::wstring detail;
    std::wstring discoveredChatId;
    std::wstring eventType;
    std::wstring account;
};

bool ProtectTokenForCurrentUser(const std::wstring& plain, std::wstring& encoded, std::wstring& error);
bool UnprotectTokenForCurrentUser(const std::wstring& encoded, std::wstring& plain, std::wstring& error);

class Worker {
public:
    Worker() = default;
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    bool Start(HWND notifyWindow, UINT resultMessage);
    void Stop();
    bool Enqueue(Request request);
    std::size_t Pending() const;

private:
    void ThreadMain();
    Result Execute(const Request& request);

    HWND notifyWindow_ = nullptr;
    UINT resultMessage_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Request> queue_;
    std::thread thread_;
    bool stop_ = false;
};

} // namespace telegram_notify
