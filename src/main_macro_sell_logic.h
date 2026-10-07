#pragma once
#include <algorithm>

namespace main_macro_sell_logic {

inline bool ValidPosition(int position) { return position >= 0 && position <= 99; }
inline bool Eligible(int position, bool isEquip, bool bound) {
    return ValidPosition(position) && isEquip && !bound;
}
inline int ClampDelayMs(int ms) { return std::clamp(ms, 0, 60000); }
inline bool CapacityReady(int freeBagSpace) { return freeBagSpace >= 9; }

} // namespace main_macro_sell_logic
