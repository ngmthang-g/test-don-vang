#include "auto_loot_logic.h"
#include <cassert>

using namespace auto_loot_logic;

int main() {
    assert(NormalizeIntervalMs(0) == 1000);
    assert(NormalizeIntervalMs(-1) == 1000);
    assert(NormalizeIntervalMs(1) == 1);
    assert(NormalizeIntervalMs(250) == 250);
    assert(NormalizeIntervalMs(70000) == 70000);

    Inputs in{};
    assert(Evaluate(in) == RuntimeGate::Disabled);
    in.enabled = true;
    assert(Evaluate(in) == RuntimeGate::ToolStopped);
    in.toolRunning = true;
    assert(Evaluate(in) == RuntimeGate::SnapshotUnknown);
    in.snapshotValid = true; in.mapReady = true;
    assert(Evaluate(in) == RuntimeGate::LifeUnknown);
    in.lifeValid = true;
    assert(Evaluate(in) == RuntimeGate::AutoFightUnknown);
    in.autoFightValid = true;
    assert(Evaluate(in) == RuntimeGate::AutoFightOff);
    in.autoFight = true;
    assert(Evaluate(in) == RuntimeGate::BagUnknown);
    in.bagValid = true; in.freeBagSpace = 0;
    assert(Evaluate(in) == RuntimeGate::BagFull);
    in.freeBagSpace = 1; in.criticalBusy = true;
    assert(Evaluate(in) == RuntimeGate::CriticalBusy);
    in.criticalBusy = false;
    assert(Evaluate(in) == RuntimeGate::Active);
    in.dead = true;
    assert(Evaluate(in) == RuntimeGate::Dead);

    assert(InitialStaggerMs(0, 4, 1000) == 0);
    assert(InitialStaggerMs(1, 4, 1000) == 250);
    assert(InitialStaggerMs(2, 4, 1000) == 500);
    assert(InitialStaggerMs(3, 4, 1000) == 750);
    assert(InitialStaggerMs(2, 4, 200) == 100);

    assert(TickDue(1000, 0));
    assert(!TickDue(999, 1000));
    assert(TickDue(1000, 1000));
    assert(TickDue(1001, 1000));
    assert(TickDue(5, 0xFFFFFFF0u)); // DWORD wrap-around safe.
    return 0;
}
