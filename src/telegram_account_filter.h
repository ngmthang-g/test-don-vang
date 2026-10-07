#pragma once

#include <iterator>
#include <string>

namespace telegram_account_filter {

// Applies the TELE tab account selection before a Telegram SendMessage request
// reaches WinHTTP. Returns false when the request must be skipped.
// System/test requests (account == "-") remain available; summary messages are
// stripped of detail lines that name unticked accounts.
bool PrepareRequest(const std::wstring& account,
                    const std::wstring& eventType,
                    std::wstring& message,
                    std::wstring& skipReason);

} // namespace telegram_account_filter
