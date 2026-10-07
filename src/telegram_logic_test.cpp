#include "telegram_logic.h"
#include <iostream>

int main() {
    if (telegram_logic::WholeGoldFromRaw(8547143) != 854) return 101;
    if (telegram_logic::WholeGoldFromRaw(209239) != 20) return 102;
    if (telegram_logic::WholeGoldDeltaFromRaw(8550999, 8547143) != 0) return 103;
    if (telegram_logic::WholeGoldDeltaFromRaw(8647143, 8547143) != 10) return 104;
    if (telegram_logic::WholeGoldDeltaFromRaw(8447143, 8547143) != -10) return 105;
    int m = -1;
    if (!telegram_logic::ParseDailyTimeMinutes(L"08:05", m) || m != 485) return 1;
    if (telegram_logic::ParseDailyTimeMinutes(L"24:00", m)) return 2;
    if (telegram_logic::ParseDailyTimeMinutes(L"8:05", m)) return 3;
    if (telegram_logic::ClampSummaryIntervalMinutes(1) != 5) return 4;
    if (telegram_logic::ClampSummaryIntervalMinutes(2000) != 1440) return 5;
    if (telegram_logic::ClampWorldFlowTimeoutSeconds(1) != 30) return 6;
    if (!telegram_logic::LooksLikeBotToken(L"123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ")) return 7;
    if (!telegram_logic::LooksLikeTelegramChatId(L"-100123456789")) return 8;
    if (!telegram_logic::LooksLikeTelegramChatId(L"@channel")) return 9;
    const std::string sample = R"({"ok":true,"result":[{"message":{"chat":{"id":123,"type":"private"}}},{"message":{"chat":{"id":-100456,"type":"supergroup"}}}]})";
    if (!telegram_logic::BotApiOk(sample)) return 10;
    if (telegram_logic::FindLatestChatIdInJson(sample) != "-100456") return 11;
    if (telegram_logic::PhraseIndex(123, 5) >= 5) return 12;
    if (telegram_logic::PhraseIndex(123, 5) != telegram_logic::PhraseIndex(123, 5)) return 13;
    bool varied = false;
    const auto first = telegram_logic::PhraseIndex(1, 7);
    for (std::uint64_t seed = 2; seed < 32; ++seed) if (telegram_logic::PhraseIndex(seed, 7) != first) { varied = true; break; }
    if (!varied) return 14;
    if (telegram_logic::IsDeathBurst(2)) return 15;
    if (!telegram_logic::IsDeathBurst(3)) return 16;
    constexpr std::uint64_t minute = 60ULL * 1000ULL;
    constexpr std::uint64_t hour = 60ULL * minute;
    constexpr unsigned allMilestones = 0x1Fu;
    std::uint64_t cycles[telegram_logic::kCurrencyMilestoneCount]{};
    if (telegram_logic::CurrencyMilestoneDue(minute - 1, cycles, 5, allMilestones) != -1) return 17;
    if (telegram_logic::CurrencyMilestoneDue(minute, cycles, 5, allMilestones) != 0) return 18;
    telegram_logic::ConsumeCurrencyMilestoneCycles(minute, cycles, 5, allMilestones);
    if (telegram_logic::CurrencyMilestoneDue(minute + 30000ULL, cycles, 5, allMilestones) != -1) return 19;
    if (telegram_logic::CurrencyMilestoneDue(2ULL * minute, cycles, 5, allMilestones) != 0) return 20;

    std::uint64_t hourCycles[telegram_logic::kCurrencyMilestoneCount]{};
    constexpr unsigned oneHourOnly = 1u << 2;
    if (telegram_logic::CurrencyMilestoneDue(hour, hourCycles, 5, oneHourOnly) != 2) return 21;
    telegram_logic::ConsumeCurrencyMilestoneCycles(hour, hourCycles, 5, oneHourOnly);
    if (telegram_logic::CurrencyMilestoneDue(hour + minute, hourCycles, 5, oneHourOnly) != -1) return 22;
    if (telegram_logic::CurrencyMilestoneDue(2ULL * hour, hourCycles, 5, oneHourOnly) != 2) return 23;

    std::uint64_t overlapCycles[telegram_logic::kCurrencyMilestoneCount]{};
    constexpr unsigned hourSixDay = (1u << 2) | (1u << 3) | (1u << 4);
    if (telegram_logic::CurrencyMilestoneDue(6ULL * hour, overlapCycles, 5, hourSixDay) != 3) return 24;
    telegram_logic::ConsumeCurrencyMilestoneCycles(6ULL * hour, overlapCycles, 5, hourSixDay);
    if (telegram_logic::CurrencyMilestoneDue(6ULL * hour + minute, overlapCycles, 5, hourSixDay) != -1) return 25;
    if (telegram_logic::CurrencyMilestoneDue(7ULL * hour, overlapCycles, 5, hourSixDay) != 2) return 26;
    telegram_logic::ConsumeCurrencyMilestoneCycles(7ULL * hour, overlapCycles, 5, hourSixDay);
    if (telegram_logic::CurrencyMilestoneDue(24ULL * hour, overlapCycles, 5, hourSixDay) != 4) return 27;

    std::uint64_t wakeCycles[telegram_logic::kCurrencyMilestoneCount]{};
    constexpr unsigned hourAndSix = (1u << 2) | (1u << 3);
    if (telegram_logic::CurrencyMilestoneDue(8ULL * hour, wakeCycles, 5, hourAndSix) != 3) return 28;
    telegram_logic::ConsumeCurrencyMilestoneCycles(8ULL * hour, wakeCycles, 5, hourAndSix);
    if (telegram_logic::CurrencyMilestoneDue(8ULL * hour + minute, wakeCycles, 5, hourAndSix) != -1) return 29;
    if (telegram_logic::CurrencyMilestoneDue(9ULL * hour, wakeCycles, 5, hourAndSix) != 2) return 30;

    const bool testOnly[5] = {true, true, false, false, false};
    if (telegram_logic::CurrencyMilestoneEnabledMask(testOnly, 5) != 0x03u) return 31;
    std::cout << "telegram_logic_tests PASS\n";
    return 0;
}
