#pragma once

#include <cstdint>

namespace auto_fight_retry_logic {

constexpr int kImmediateAttemptLimit = 2;

enum class ExhaustedRetryDecision { StartWait, KeepWaiting, RetryNow };

inline ExhaustedRetryDecision DecideExhaustedRetry(std::uint32_t now,
                                                    std::uint32_t waitStartedTick,
                                                    std::uint32_t retryDelayMs) {
    if (waitStartedTick == 0) return ExhaustedRetryDecision::StartWait;
    return static_cast<std::uint32_t>(now - waitStartedTick) >= retryDelayMs
        ? ExhaustedRetryDecision::RetryNow
        : ExhaustedRetryDecision::KeepWaiting;
}

} // namespace auto_fight_retry_logic
