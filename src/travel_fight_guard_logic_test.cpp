#include "travel_fight_guard_logic.h"

#include <cassert>

int main() {
    using namespace travel_fight_guard_logic;

    static_assert(kStopAttemptsBeforeReset == 2);
    assert(NeedsAnotherStopBeforeReset(0));
    assert(NeedsAnotherStopBeforeReset(1));
    assert(!NeedsAnotherStopBeforeReset(2));

    assert(!HasAutoPathFightConflict(false, false));
    assert(!HasAutoPathFightConflict(true, false));
    assert(!HasAutoPathFightConflict(false, true));
    assert(HasAutoPathFightConflict(true, true));

    assert(!CanDispatchMovement(false, false));
    assert(!CanDispatchMovement(false, true));
    assert(!CanDispatchMovement(true, true));
    assert(CanDispatchMovement(true, false));

    assert(!CanDispatchFightStart(false, false));
    assert(!CanDispatchFightStart(false, true));
    assert(!CanDispatchFightStart(true, true));
    assert(CanDispatchFightStart(true, false));

    assert(!IsPhysicalRouteCompletion(false, false, false));
    assert(!IsPhysicalRouteCompletion(true, true, false));
    assert(IsPhysicalRouteCompletion(true, false, true));
    assert(IsPhysicalRouteCompletion(true, false, false));

    assert(!ShouldGuardUnderworldExit(1, 2, true));
    assert(!ShouldGuardUnderworldExit(87, 0, false));
    assert(!ShouldGuardUnderworldExit(87, 87, true));
    assert(ShouldGuardUnderworldExit(87, 5, true));

    assert(!ConflictRecoveryComplete(false, false, false));
    assert(!ConflictRecoveryComplete(true, true, false));
    assert(!ConflictRecoveryComplete(true, false, true));
    assert(ConflictRecoveryComplete(true, false, false));
    return 0;
}
