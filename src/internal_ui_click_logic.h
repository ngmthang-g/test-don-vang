#pragma once

#include <array>

namespace internal_ui_click_logic {

constexpr int kLeftButton = 0;

enum class DispatchPhase {
    Press,
    Release,
};

enum class AutoStartPoint {
    AutoMenu,
    Attack,
};

struct DispatchStep {
    DispatchPhase phase;
    const char* methodName;
    int parameterCount;
};

inline constexpr std::array<DispatchStep, 2> DispatchPlan() {
    return {{
        {DispatchPhase::Press, "TryClickUI", 2},
        {DispatchPhase::Release, "EndUIDrag", 1},
    }};
}

struct AutoStartStep {
    AutoStartPoint point;
    int waitBeforeMs;
};

// The small AUTO choice group needs one UI frame/update window after AUTO is
// opened before the configured ĐÁNH QUÁI point can be dispatched.
inline constexpr std::array<AutoStartStep, 2> AutoStartPlan() {
    return {{
        {AutoStartPoint::AutoMenu, 0},
        {AutoStartPoint::Attack, 500},
    }};
}

enum class AutoMenuChoicePoint {
    AutoMenu,
    Choice,
};

struct AutoMenuChoiceStep {
    AutoMenuChoicePoint point;
    int waitBeforeMs;
};

// Both starting and stopping AutoFight select an item from the small AUTO menu.
// The second point is not reliably raycastable until AUTO has opened the menu.
inline constexpr std::array<AutoMenuChoiceStep, 2> AutoMenuChoicePlan() {
    return {{
        {AutoMenuChoicePoint::AutoMenu, 0},
        {AutoMenuChoicePoint::Choice, 500},
    }};
}

inline constexpr bool IsNormalizedCoordinate(int value, int scale) {
    return scale > 0 && value >= 0 && value < scale;
}

} // namespace internal_ui_click_logic
