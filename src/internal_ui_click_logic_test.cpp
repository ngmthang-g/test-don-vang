#include "internal_ui_click_logic.h"

#include <cassert>

using namespace internal_ui_click_logic;

int main() {
    constexpr auto plan = DispatchPlan();
    static_assert(plan.size() == 2);
    static_assert(plan[0].phase == DispatchPhase::Press);
    static_assert(plan[0].parameterCount == 2);
    static_assert(plan[1].phase == DispatchPhase::Release);
    static_assert(plan[1].parameterCount == 1);
    static_assert(kLeftButton == 0);

    constexpr auto autoStart = AutoStartPlan();
    static_assert(autoStart.size() == 2);
    static_assert(autoStart[0].point == AutoStartPoint::AutoMenu);
    static_assert(autoStart[0].waitBeforeMs == 0);
    static_assert(autoStart[1].point == AutoStartPoint::Attack);
    static_assert(autoStart[1].waitBeforeMs == 500);

    constexpr auto autoChoice = AutoMenuChoicePlan();
    static_assert(autoChoice.size() == 2);
    static_assert(autoChoice[0].point == AutoMenuChoicePoint::AutoMenu);
    static_assert(autoChoice[0].waitBeforeMs == 0);
    static_assert(autoChoice[1].point == AutoMenuChoicePoint::Choice);
    static_assert(autoChoice[1].waitBeforeMs == 500);

    assert(plan[0].methodName[0] == 'T');
    assert(plan[1].methodName[0] == 'E');
    assert(IsNormalizedCoordinate(0, 1000000));
    assert(IsNormalizedCoordinate(999999, 1000000));
    assert(!IsNormalizedCoordinate(-1, 1000000));
    assert(!IsNormalizedCoordinate(1000000, 1000000));
    return 0;
}
