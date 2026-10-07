#include "fixed_slot_sell_logic.h"

#include <cassert>

using namespace fixed_slot_sell_logic;

int main() {
    // Old multi-row profiles are migrated once; only former row #5 survives.
    assert(LegacyIdleClickSourceIndex(0) == -1);
    assert(LegacyIdleClickSourceIndex(1) == 0);
    assert(LegacyIdleClickSourceIndex(4) == 3);
    assert(LegacyIdleClickSourceIndex(5) == 4);
    assert(LegacyIdleClickSourceIndex(12) == 4);

    // Repeat is fixed/configurable only for a true MAIN-full batch. Idle clicking
    // is infinite and therefore does not use an adaptive post-trade count.
    assert(ClampFullBatchClickCount(0) == 90);
    assert(ClampFullBatchClickCount(1) == 1);
    assert(ClampFullBatchClickCount(90) == 90);
    assert(ClampFullBatchClickCount(120) == 120);
    assert(ClampFullBatchClickCount(1000) == 999);

    // 10.2 NONE independent seller keeps the final 3.6 adaptive fixed-slot ceiling.
    assert(EffectiveClickCount(-1) == 90);
    assert(EffectiveClickCount(0) == 90);
    assert(EffectiveClickCount(1) == 1);
    assert(EffectiveClickCount(37) == 37);
    assert(EffectiveClickCount(90) == 90);
    assert(EffectiveClickCount(100) == 90);

    assert(ClampClickDelayMs(0) == 600);
    assert(ClampClickDelayMs(20) == 50);
    assert(ClampClickDelayMs(600) == 600);
    assert(ClampClickDelayMs(70000) == 60000);

    assert(NormalizeClientCoordinate(0, 930) == 0);
    assert(NormalizeClientCoordinate(929, 930) < kCoordinateScale);
    assert(NormalizeClientCoordinate(465, 930) == 500000);
    assert(NormalizeClientCoordinate(10, 0) == -1);
    return 0;
}
