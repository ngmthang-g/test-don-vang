#include "auto_fight_retry_logic.h"

#include <cassert>
#include <cstdint>

int main() {
    using namespace auto_fight_retry_logic;
    static_assert(kImmediateAttemptLimit == 2);
    assert(DecideExhaustedRetry(1000u, 0u, 60000u) == ExhaustedRetryDecision::StartWait);
    assert(DecideExhaustedRetry(60999u, 1000u, 60000u) == ExhaustedRetryDecision::KeepWaiting);
    assert(DecideExhaustedRetry(61000u, 1000u, 60000u) == ExhaustedRetryDecision::RetryNow);
    const std::uint32_t nearWrap = 0xfffffff0u;
    const std::uint32_t afterWrap = 0x00000020u;
    assert(DecideExhaustedRetry(afterWrap, nearWrap, 0x30u) == ExhaustedRetryDecision::RetryNow);
    return 0;
}
