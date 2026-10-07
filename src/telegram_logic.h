#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace telegram_logic {


inline std::size_t PhraseIndex(std::uint64_t seed, std::size_t count) {
    if (count == 0) return 0;
    // SplitMix64 finalizer: tiny, deterministic, allocation-free and sufficient for
    // rotating human-facing phrase templates without introducing RNG state/locks.
    seed += 0x9E3779B97F4A7C15ULL;
    seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
    seed ^= seed >> 31;
    return static_cast<std::size_t>(seed % static_cast<std::uint64_t>(count));
}

inline bool IsDeathBurst(std::size_t deathsInTenMinutes) {
    return deathsInTenMinutes >= 3;
}

inline constexpr unsigned kCurrencyMilestoneCount = 5u;

inline constexpr std::int64_t kCurrencyUnitsPerGold = 10000LL;

inline std::int64_t WholeGoldFromRaw(std::int64_t raw) {
    return raw / kCurrencyUnitsPerGold;
}

inline std::int64_t WholeGoldDeltaFromRaw(std::int64_t currentRaw, std::int64_t baselineRaw) {
    return (currentRaw - baselineRaw) / kCurrencyUnitsPerGold;
}
inline constexpr std::uint64_t kCurrencyMinuteMs = 60ULL * 1000ULL;
inline constexpr std::uint64_t kCurrencyMilestoneThresholds[kCurrencyMilestoneCount] = {
    1ULL * kCurrencyMinuteMs,
    5ULL * kCurrencyMinuteMs,
    60ULL * kCurrencyMinuteMs,
    6ULL * 60ULL * kCurrencyMinuteMs,
    24ULL * 60ULL * kCurrencyMinuteMs,
};

inline std::uint64_t CurrencyMilestoneCycle(std::uint64_t elapsedMs, unsigned index) {
    if (index >= kCurrencyMilestoneCount || kCurrencyMilestoneThresholds[index] == 0) return 0;
    return elapsedMs / kCurrencyMilestoneThresholds[index];
}

// Recurring milestone scheduler. lastCycles[i] stores the most recent completed
// interval consumed for milestone i. When several enabled intervals become due at
// the same sample, return the longest interval so the caller emits one message.
inline int CurrencyMilestoneDue(std::uint64_t elapsedMs, const std::uint64_t* lastCycles,
                                std::size_t cycleCount, unsigned enabledMask) {
    if (!lastCycles) return -1;
    const std::size_t limit = std::min<std::size_t>(cycleCount, kCurrencyMilestoneCount);
    for (int i = static_cast<int>(limit) - 1; i >= 0; --i) {
        const unsigned bit = 1u << static_cast<unsigned>(i);
        if (!(enabledMask & bit)) continue;
        const std::uint64_t cycle = CurrencyMilestoneCycle(elapsedMs, static_cast<unsigned>(i));
        if (cycle > lastCycles[static_cast<std::size_t>(i)]) return i;
    }
    return -1;
}

// Consume every enabled interval already crossed at this sample. This prevents a
// PC wake-up / busy period from producing a burst of smaller stale milestones on
// subsequent 30-second samples. Fresh cycles still become due normally later.
inline void ConsumeCurrencyMilestoneCycles(std::uint64_t elapsedMs, std::uint64_t* lastCycles,
                                           std::size_t cycleCount, unsigned enabledMask) {
    if (!lastCycles) return;
    const std::size_t limit = std::min<std::size_t>(cycleCount, kCurrencyMilestoneCount);
    for (std::size_t i = 0; i < limit; ++i) {
        const unsigned bit = 1u << static_cast<unsigned>(i);
        if (!(enabledMask & bit)) continue;
        const std::uint64_t cycle = CurrencyMilestoneCycle(elapsedMs, static_cast<unsigned>(i));
        if (cycle > lastCycles[i]) lastCycles[i] = cycle;
    }
}

inline unsigned CurrencyMilestoneEnabledMask(const bool* enabled, std::size_t count) {
    unsigned mask = 0;
    const std::size_t limit = std::min<std::size_t>(count, kCurrencyMilestoneCount);
    for (std::size_t i = 0; i < limit; ++i) if (enabled[i]) mask |= 1u << static_cast<unsigned>(i);
    return mask;
}

inline bool ParseDailyTimeMinutes(const std::wstring& text, int& minutes) {
    if (text.size() != 5 || text[2] != L':') return false;
    if (text[0] < L'0' || text[0] > L'9' || text[1] < L'0' || text[1] > L'9' ||
        text[3] < L'0' || text[3] > L'9' || text[4] < L'0' || text[4] > L'9') return false;
    const int hh = (text[0] - L'0') * 10 + (text[1] - L'0');
    const int mm = (text[3] - L'0') * 10 + (text[4] - L'0');
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
    minutes = hh * 60 + mm;
    return true;
}

inline std::wstring NormalizeDailyTime(const std::wstring& text, const std::wstring& fallback) {
    int ignored = 0;
    return ParseDailyTimeMinutes(text, ignored) ? text : fallback;
}

inline int ClampSummaryIntervalMinutes(int value) {
    return std::clamp(value, 5, 1440);
}

inline int ClampWorldFlowTimeoutSeconds(int value) {
    return std::clamp(value, 30, 3600);
}

inline bool LooksLikeTelegramChatId(const std::wstring& text) {
    if (text.empty()) return false;
    if (text[0] == L'@') return text.size() >= 2;
    std::size_t i = text[0] == L'-' ? 1u : 0u;
    if (i >= text.size()) return false;
    for (; i < text.size(); ++i) if (text[i] < L'0' || text[i] > L'9') return false;
    return true;
}

inline bool LooksLikeBotToken(const std::wstring& text) {
    const std::size_t colon = text.find(L':');
    if (colon == std::wstring::npos || colon < 5 || colon + 10 >= text.size()) return false;
    for (std::size_t i = 0; i < colon; ++i) if (text[i] < L'0' || text[i] > L'9') return false;
    return true;
}

inline std::string FindLatestChatIdInJson(const std::string& json) {
    std::string latest;
    std::size_t search = 0;
    while (true) {
        const std::size_t chat = json.find("\"chat\"", search);
        if (chat == std::string::npos) break;
        const std::size_t windowEnd = std::min(json.size(), chat + 512);
        std::size_t id = json.find("\"id\"", chat + 6);
        if (id == std::string::npos || id >= windowEnd) { search = chat + 6; continue; }
        std::size_t colon = json.find(':', id + 4);
        if (colon == std::string::npos || colon >= windowEnd) { search = chat + 6; continue; }
        std::size_t p = colon + 1;
        while (p < windowEnd && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
        const std::size_t begin = p;
        if (p < windowEnd && json[p] == '-') ++p;
        const std::size_t digits = p;
        while (p < windowEnd && std::isdigit(static_cast<unsigned char>(json[p]))) ++p;
        if (p > digits) latest = json.substr(begin, p - begin);
        search = chat + 6;
    }
    return latest;
}

inline bool BotApiOk(const std::string& json) {
    const auto p = json.find("\"ok\"");
    if (p == std::string::npos) return false;
    const auto colon = json.find(':', p + 4);
    if (colon == std::string::npos) return false;
    const auto t = json.find("true", colon + 1);
    return t != std::string::npos && t - colon < 16;
}

} // namespace telegram_logic
