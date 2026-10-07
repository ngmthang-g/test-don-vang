#include "route_logic.h"
#include "thdc_route_logic.h"
#include <cstdio>
using namespace cleanroute_logic;

static int g_fail = 0;
static void Check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++g_fail;
}

int main() {
    Target t{50, 1000, 2000, 120};
    State s{};
    Check(Decide(s, t) == Action::Wait, "invalid->wait");
    s = {true, false, true, 1, 0, 0, false, false};
    Check(Decide(s, t) == Action::Wait, "transition->wait");
    s = {true, true, false, 1, 0, 0, false, false};
    Check(Decide(s, t) == Action::Mount, "wrong-place-foot->mount");
    s.riding = true;
    Check(Decide(s, t) == Action::StartPath, "wrong-place-mounted->startpath");
    s.autoPathing = true;
    Check(Decide(s, t) == Action::Wait, "routing->wait");
    s = {true, true, false, 50, 1005, 2004, true, true};
    Check(Decide(s, t) == Action::StopPath, "arrive-pathing->stoppath");
    s.autoPathing = false;
    Check(Decide(s, t) == Action::Hold, "arrive-mounted->hold-mounted");
    s.riding = false;
    Check(Decide(s, t) == Action::Hold, "arrive-foot->hold");

    Check(DecideMountAssist(false, false, 0, false, 0) == MountAssistAction::Mount, "mount-assist:first-mount");
    Check(DecideMountAssist(false, false, 1, false, 4999) == MountAssistAction::Wait, "mount-assist:wait-first-5s");
    Check(DecideMountAssist(false, false, 1, false, 5000) == MountAssistAction::Mount, "mount-assist:second-mount-at-5s");
    Check(DecideMountAssist(false, false, 2, false, 4999) == MountAssistAction::Wait, "mount-assist:wait-second-5s");
    Check(DecideMountAssist(false, false, 2, false, 5000) == MountAssistAction::MountCycleFailed, "mount-assist:two-mount-cycle-failed");
    Check(!CanStartPath(false), "startpath:foot-rejected");
    Check(CanStartPath(true), "startpath:riding-accepted");
    Check(DecideMountAssist(false, true, 2, true, 14999) == MountAssistAction::MountCycleFailed,
          "mount-assist:stale-foot-fallback-rejected");
    Check(DecideMountAssist(false, true, 2, true, 15000) == MountAssistAction::MountCycleFailed,
          "mount-assist:no-foot-startpath");

    State preciseState{true, true, false, 50, 1050, 2000, false, false};
    Target trainTolerance{50, 1000, 2000, 120};
    Target preciseTolerance{50, 1000, 2000, 20};
    Check(AtTarget(preciseState, trainTolerance), "tolerance-split:train-120-allows-near");
    Check(!AtTarget(preciseState, preciseTolerance), "tolerance-split:precise-20-rejects-50-away");

    using thdc_route_logic::NextGate;
    auto gate = NextGate(10000, 10017);
    Check(gate.valid && gate.sourceMap == 10000 && gate.expectedMap == 10014 &&
          gate.coordinateIndex == 0 && gate.confirmAfterTransition,
          "thdc:interserver-enters-floor1-first");
    gate = NextGate(10014, 10017);
    Check(gate.valid && gate.sourceMap == 10014 && gate.expectedMap == 10015 && gate.coordinateIndex == 1,
          "thdc:floor1-up-uses-m10014-gate");
    gate = NextGate(10015, 10017);
    Check(gate.valid && gate.sourceMap == 10015 && gate.expectedMap == 10016 && gate.coordinateIndex == 2,
          "thdc:floor2-up-uses-m10015-gate");
    gate = NextGate(10015, 10014);
    Check(gate.valid && gate.sourceMap == 10015 && gate.expectedMap == 10014 && gate.coordinateIndex == 3,
          "thdc:floor2-down-uses-second-m10015-gate");
    gate = NextGate(10016, 10017);
    Check(gate.valid && gate.sourceMap == 10016 && gate.expectedMap == 10017 && gate.coordinateIndex == 4,
          "thdc:floor3-up-uses-m10016-gate");
    gate = NextGate(10016, 10014);
    Check(gate.valid && gate.sourceMap == 10016 && gate.expectedMap == 10015 && gate.coordinateIndex == 5,
          "thdc:floor3-down-uses-second-m10016-gate");
    gate = NextGate(10017, 10014);
    Check(gate.valid && gate.sourceMap == 10017 && gate.expectedMap == 10016 && gate.coordinateIndex == 6,
          "thdc:floor4-down-uses-m10017-gate");
    Check(!NextGate(10017, 10017).valid, "thdc:destination-floor-does-not-route");
    Check(!NextGate(10016, 10005).valid, "thdc:non-thdc-target-rejected");

    std::printf("RESULT %d/28 PASS\n", 28 - g_fail);
    return g_fail ? 1 : 0;
}
