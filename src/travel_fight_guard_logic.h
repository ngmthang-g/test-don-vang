#pragma once

namespace travel_fight_guard_logic {

// The travel contract is intentionally small and deterministic:
//   Stop #1 -> verify -> Stop #2 -> verify -> AUTO/Attack reset -> repeat.
// A route may be started only after an authoritative AutoFight=OFF snapshot.
constexpr int kStopAttemptsBeforeReset = 2;

inline bool NeedsAnotherStopBeforeReset(int completedStopAttempts) {
    return completedStopAttempts < kStopAttemptsBeforeReset;
}

inline bool HasAutoPathFightConflict(bool autoPathing, bool autoFight) {
    return autoPathing && autoFight;
}

// Any movement command that changes locomotion state (Mount or StartPath) is
// fail-closed unless AutoFight is authoritatively OFF. Unknown is unsafe.
inline bool CanDispatchMovement(bool autoFightStateKnown, bool autoFight) {
    return autoFightStateKnown && !autoFight;
}

// A queued AUTO -> Attack request can outlive the state that created it. Recheck
// AutoPath at dispatch time so a stale request cannot turn fight on after travel
// has started. Unknown is unsafe; the caller waits for an authoritative snapshot.
inline bool CanDispatchFightStart(bool autoPathStateKnown, bool autoPathing) {
    return autoPathStateKnown && !autoPathing;
}

// Route ownership is released only at the physical final destination. A map change
// alone is not completion because cross-map routes must retain ownership through
// intermediate maps (notably Lâu Lan gate confirmation). CP9 keeps the rider mounted at arrival.
inline bool IsPhysicalRouteCompletion(bool atTarget, bool autoPathing, bool riding) {
    (void)riding;
    // CP9: route ownership completes at the physical target once AutoPath stops; no dismount required.
    return atTarget && !autoPathing;
}

// Map 87 has no special right to disable training. Its proactive guard applies only
// while the current workflow has a real destination outside Map 87. Unknown/no
// destination and an M87-local target are intentionally not guarded here; every actual
// Mount/StartPath still passes through the shared movement guard separately.
inline bool ShouldGuardUnderworldExit(int currentMap, int destinationMap,
                                      bool hasTravelDestination, int underworldMapId = 87) {
    return currentMap == underworldMapId && hasTravelDestination &&
           destinationMap > 0 && destinationMap != underworldMapId;
}

inline bool ConflictRecoveryComplete(bool conflictLatched,
                                     bool autoPathing,
                                     bool autoFight) {
    return conflictLatched && !autoPathing && !autoFight;
}

} // namespace travel_fight_guard_logic
