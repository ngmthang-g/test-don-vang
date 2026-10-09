#pragma once
#include <cstdint>
#include <algorithm>
namespace target_loop {
// One state machine per game account. Failure never locks or aborts the cycle.
enum class Step : std::uint8_t { Target, Face, Trade, Click2 };
inline Step Next(Step step) {
    switch (step) {
        case Step::Target: return Step::Face;
        case Step::Face: return Step::Trade;
        case Step::Trade: return Step::Click2;
        case Step::Click2: return Step::Target;
    }
    return Step::Target;
}
// All timings and repeat limit belong to one ACC. Zero repeats = unbounded.
struct Settings {
    std::int32_t repeatCycles = 0;
    std::int32_t delayTargetMs = 330;
    std::int32_t delayClick1Ms = 330;
    std::int32_t delayTradeMs = 330;
    std::int32_t delayClick2Ms = 0;
    std::int32_t delayCycleMs = 550;
    static constexpr std::int32_t kMaxDelayMs = 60000;
    static constexpr std::int32_t kMaxRepeats = 1000000;
    bool Valid() const {
        return repeatCycles>=0 && repeatCycles<=kMaxRepeats &&
            delayTargetMs>=0 && delayTargetMs<=kMaxDelayMs &&
            delayClick1Ms>=0 && delayClick1Ms<=kMaxDelayMs &&
            delayTradeMs>=0 && delayTradeMs<=kMaxDelayMs &&
            delayClick2Ms>=0 && delayClick2Ms<=kMaxDelayMs &&
            delayCycleMs>=0 && delayCycleMs<=kMaxDelayMs;
    }
    std::int32_t AfterStep(Step step) const {
        switch (step) {
            case Step::Target: return delayTargetMs;
            case Step::Face: return delayClick1Ms;
            case Step::Trade: return delayTradeMs;
            case Step::Click2: return delayClick2Ms;
        }
        return 0;
    }
    bool Finished(std::uint64_t cycles) const {
        return repeatCycles>0 && cycles>=static_cast<std::uint64_t>(repeatCycles);
    }
};
struct State {
    std::int32_t targetRoleID = 0;
    Step step = Step::Target;
    std::uint64_t cycles = 0;
    std::uint64_t errors = 0;
    void Finish(bool ok) {
        if (!ok) ++errors;
        if (step == Step::Click2) ++cycles;
        step = Next(step);
    }
    void ResetStep() { step = Step::Target; }
};
} // namespace target_loop
