#pragma once
#include <cstdint>
#include <cstdlib>

namespace cleanroute_logic {

struct State {
    bool valid = false;
    bool mapReady = false;
    bool waitingMap = false;
    int mapID = 0;
    int x = 0;
    int y = 0;
    bool riding = false;
    bool autoPathing = false;
};

struct Target {
    int mapID = 0;
    int x = 0;
    int y = 0;
    int tolerance = 120;
};

enum class Action {
    Wait,
    Mount,
    StartPath,
    StopPath,
    Dismount,
    Hold,
};

enum class MountAssistAction {
    Wait,
    Mount,
    StartPath,
    MountCycleFailed,
    FinishFootCycle,
};

inline bool CanStartPath(bool riding) {
    return riding;
}

inline MountAssistAction DecideMountAssist(bool riding, bool autoPathing, int mountAttempts,
                                           bool footFallback, std::uint32_t phaseElapsedMs,
                                           std::uint32_t mountRetryWaitMs = 5000,
                                           std::uint32_t footWalkMaxMs = 15000) {
    if (riding) return autoPathing ? MountAssistAction::Wait : MountAssistAction::StartPath;
    if (footFallback) {
        // StartPath is a hard riding-only invariant. A stale fallback state must
        // restart the mount cycle; it must never authorize walking AutoPath.
        (void)autoPathing;
        (void)phaseElapsedMs;
        (void)footWalkMaxMs;
        return MountAssistAction::MountCycleFailed;
    }
    if (mountAttempts <= 0) return MountAssistAction::Mount;
    if (mountAttempts == 1) {
        return phaseElapsedMs >= mountRetryWaitMs ? MountAssistAction::Mount : MountAssistAction::Wait;
    }
    return phaseElapsedMs >= mountRetryWaitMs ? MountAssistAction::MountCycleFailed : MountAssistAction::Wait;
}

inline bool AtTarget(const State& s, const Target& t) {
    if (!s.valid || s.mapID != t.mapID) return false;
    const long long dx = static_cast<long long>(s.x) - t.x;
    const long long dy = static_cast<long long>(s.y) - t.y;
    const long long rr = static_cast<long long>(t.tolerance) * t.tolerance;
    return dx * dx + dy * dy <= rr;
}

inline Action Decide(const State& s, const Target& t) {
    if (!s.valid || !s.mapReady || s.waitingMap) return Action::Wait;
    if (AtTarget(s, t)) {
        if (s.autoPathing) return Action::StopPath;
        // CP9: arriving at any AutoPath destination keeps the mount state.
        return Action::Hold;
    }
    if (!s.riding) return Action::Mount;
    if (!s.autoPathing) return Action::StartPath;
    return Action::Wait;
}

} // namespace cleanroute_logic
