#include "target_loop_logic.h"
#include <cassert>
int main() {
    target_loop::State a, b;
    a.targetRoleID = b.targetRoleID = 2345; // shared targets are allowed
    for (int i = 0; i < 24; ++i) {
        a.Finish((i % 3) == 0); // even repeated errors keep stepping
        if (i < 8) b.Finish(true);
    }
    assert(a.cycles == 6 && a.errors == 16);
    assert(b.cycles == 2 && b.errors == 0);
    assert(a.step == target_loop::Step::Target && b.step == target_loop::Step::Target);
    target_loop::Settings settings;
    assert(settings.Valid() && !settings.Finished(a.cycles));
    settings.repeatCycles = 6;
    settings.delayTargetMs = 101;
    settings.delayClick1Ms = 202;
    settings.delayTradeMs = 303;
    settings.delayClick2Ms = 404;
    settings.delayCycleMs = 505;
    assert(settings.Valid() && settings.Finished(a.cycles) && !settings.Finished(b.cycles));
    assert(settings.AfterStep(target_loop::Step::Target) == 101);
    assert(settings.AfterStep(target_loop::Step::Face) == 202);
    assert(settings.AfterStep(target_loop::Step::Trade) == 303);
    assert(settings.AfterStep(target_loop::Step::Click2) == 404);
    settings.delayTradeMs = 60001;
    assert(!settings.Valid());
    settings.delayTradeMs = 0;
    settings.repeatCycles = 0;
    assert(settings.Valid() && !settings.Finished(100000000));
    // Two ACCs can have the same target but independent repeat/clock settings.
    target_loop::Settings other;
    other.repeatCycles = 2;
    assert(other.Finished(b.cycles) && !settings.Finished(b.cycles));
    b.targetRoleID = 9876;
    assert(a.targetRoleID == 2345 && b.targetRoleID == 9876);
}
