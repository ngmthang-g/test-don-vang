#pragma once

#include <cstddef>
#include <cstdint>

namespace auto_loot_logic {

constexpr int kDefaultIntervalMs = 1000;

enum class RuntimeGate {
    Disabled,
    ToolStopped,
    SnapshotUnknown,
    MapTransition,
    LifeUnknown,
    Dead,
    AutoFightUnknown,
    AutoFightOff,
    BagUnknown,
    BagFull,
    CriticalBusy,
    Active,
};

struct Inputs {
    bool enabled = false;
    bool toolRunning = false;
    bool snapshotValid = false;
    bool mapReady = false;
    bool waitingChangeMap = false;
    bool lifeValid = false;
    bool dead = false;
    bool autoFightValid = false;
    bool autoFight = false;
    bool bagValid = false;
    int freeBagSpace = -1;
    bool criticalBusy = false;
};

inline int NormalizeIntervalMs(int value) {
    if (value <= 0) return kDefaultIntervalMs;
    return value;
}

inline RuntimeGate Evaluate(const Inputs& in) {
    if (!in.enabled) return RuntimeGate::Disabled;
    if (!in.toolRunning) return RuntimeGate::ToolStopped;
    if (!in.snapshotValid) return RuntimeGate::SnapshotUnknown;
    if (!in.mapReady || in.waitingChangeMap) return RuntimeGate::MapTransition;
    if (!in.lifeValid) return RuntimeGate::LifeUnknown;
    if (in.dead) return RuntimeGate::Dead;
    if (!in.autoFightValid) return RuntimeGate::AutoFightUnknown;
    if (!in.autoFight) return RuntimeGate::AutoFightOff;
    if (!in.bagValid || in.freeBagSpace < 0) return RuntimeGate::BagUnknown;
    if (in.freeBagSpace == 0) return RuntimeGate::BagFull;
    if (in.criticalBusy) return RuntimeGate::CriticalBusy;
    return RuntimeGate::Active;
}

inline std::uint32_t InitialStaggerMs(std::size_t index, std::size_t count, int intervalMs) {
    const std::uint32_t interval = static_cast<std::uint32_t>(NormalizeIntervalMs(intervalMs));
    if (count <= 1 || index == 0) return 0;
    if (index >= count) index = count - 1;
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(interval) * index) / count);
}

inline bool TickDue(std::uint32_t now, std::uint32_t due) {
    if (due == 0) return true;
    return static_cast<std::int32_t>(now - due) >= 0;
}

} // namespace auto_loot_logic
