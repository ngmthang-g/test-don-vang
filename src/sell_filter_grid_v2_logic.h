#pragma once
#include <algorithm>

namespace sell_filter_grid_v2_logic {

constexpr int kGridFirst = 0;
constexpr int kGridLast = 41;
constexpr int kBagFirst = 0;
constexpr int kBagLast = 99;
constexpr int kGridCount = 42;
constexpr int kBagCount = 100;

inline bool ValidGrid(int grid) { return grid >= kGridFirst && grid <= kGridLast; }
inline bool ValidPosition(int position) { return position >= kBagFirst && position <= kBagLast; }

inline int RequiredDepth(int position) {
    if (!ValidPosition(position)) return -1;
    if (position <= 41) return 0;
    if (position <= 83) return 1;
    return 2;
}

inline int PhysicalGrid(int position) {
    if (!ValidPosition(position)) return -1;
    if (position <= 41) return position;
    if (position <= 83) return position - 42;
    return position - 63;
}

inline int ResetSwipeCount(int reachedDepth) {
    if (reachedDepth < 0 || reachedDepth > 2) return -1;
    return reachedDepth;
}

inline int NewPositionFirstForDepth(int depth) {
    if (depth == 0) return 0;
    if (depth == 1) return 42;
    if (depth == 2) return 84;
    return -1;
}

inline int NewPositionLastForDepth(int depth) {
    if (depth == 0) return 41;
    if (depth == 1) return 83;
    if (depth == 2) return 99;
    return -1;
}

inline bool ReadyToResumeAfterSell(bool sellerIdle, bool resetComplete, bool passSatisfied) {
    return sellerIdle && resetComplete && passSatisfied;
}

} // namespace sell_filter_grid_v2_logic
