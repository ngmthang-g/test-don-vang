#include "route_logic.h"
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

    std::printf("RESULT %d/19 PASS\n", 19 - g_fail);
    return g_fail ? 1 : 0;
}
