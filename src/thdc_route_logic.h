#pragma once

namespace thdc_route_logic {

struct GatePlan {
    bool valid = false;
    int sourceMap = 0;
    int expectedMap = 0;
    int coordinateIndex = -1;
    bool confirmAfterTransition = false;
};

inline constexpr bool IsThdcFloor(int mapID) {
    return mapID >= 10014 && mapID <= 10017;
}

// Stable config/export order:
// 0 M10000 entry, 1 M10014 up, 2 M10015 up, 3 M10015 down,
// 4 M10016 up, 5 M10016 down, 6 M10017 down.
inline constexpr GatePlan NextGate(int currentMap, int targetMap) {
    if (!IsThdcFloor(targetMap) || currentMap == targetMap) return {};
    if (currentMap == 10000) return {true, 10000, 10014, 0, true};
    if (!IsThdcFloor(currentMap)) return {};

    if (currentMap == 10014 && targetMap > currentMap)
        return {true, 10014, 10015, 1, false};
    if (currentMap == 10015)
        return targetMap > currentMap
            ? GatePlan{true, 10015, 10016, 2, false}
            : GatePlan{true, 10015, 10014, 3, false};
    if (currentMap == 10016)
        return targetMap > currentMap
            ? GatePlan{true, 10016, 10017, 4, false}
            : GatePlan{true, 10016, 10015, 5, false};
    if (currentMap == 10017 && targetMap < currentMap)
        return {true, 10017, 10016, 6, false};
    return {};
}

} // namespace thdc_route_logic
