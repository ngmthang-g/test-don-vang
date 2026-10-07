#pragma once
#include <algorithm>

namespace trade_quota_v18_logic {

constexpr int kItemsPerPass = 9;

struct MainCapacityPlan {
    bool valid = false;
    unsigned long mainPid = 0;
    int freeSlotsAtPlan = -1;
    int passesTotal = 0;
    int passesRemaining = 0;
    unsigned epoch = 0;
};

struct ChildTradePlan {
    bool valid = false;
    unsigned long pid = 0;
    int eligibleCount = 0;
    int passesTotal = 0;
    int passesRemaining = 0;
    int remainder = 0;
};

inline bool EligibleEquipment(int position, bool isEquip, bool bound) {
    return position >= 0 && position <= 99 && isEquip && !bound;
}

inline MainCapacityPlan MakeMainPlan(unsigned long pid, int freeSlots, unsigned nextEpoch) {
    MainCapacityPlan p{};
    if (pid == 0 || freeSlots < 0) return p;
    p.valid = true;
    p.mainPid = pid;
    p.freeSlotsAtPlan = freeSlots;
    p.passesTotal = freeSlots / kItemsPerPass;
    p.passesRemaining = p.passesTotal;
    p.epoch = nextEpoch;
    return p;
}

inline ChildTradePlan MakeChildPlan(unsigned long pid, int eligibleCount) {
    ChildTradePlan p{};
    if (pid == 0) return p;
    p.valid = true;
    p.pid = pid;
    p.eligibleCount = std::max(0, eligibleCount);
    p.passesTotal = p.eligibleCount / kItemsPerPass;
    p.passesRemaining = p.passesTotal;
    p.remainder = p.eligibleCount % kItemsPerPass;
    return p;
}

inline bool CanRunFullPass(const MainCapacityPlan& main, const ChildTradePlan& child) {
    return main.valid && child.valid && main.passesRemaining > 0 && child.passesRemaining > 0;
}

inline bool ConsumeFullPass(MainCapacityPlan& main, ChildTradePlan& child) {
    if (!CanRunFullPass(main, child)) return false;
    --main.passesRemaining;
    --child.passesRemaining;
    return true;
}

inline bool MainNeedsSell(const MainCapacityPlan& main) {
    return !main.valid || main.passesRemaining <= 0;
}

inline bool ChildDone(const ChildTradePlan& child) {
    return !child.valid || child.passesRemaining <= 0;
}

} // namespace trade_quota_v18_logic
