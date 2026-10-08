#include "auto_loot_logic.h"
#include "trade_coordinator_logic.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void TestAutoLootGatesAndInterval() {
    using namespace auto_loot_logic;
    assert(NormalizeIntervalMs(0) == kDefaultIntervalMs);
    assert(NormalizeIntervalMs(-5) == kDefaultIntervalMs);
    assert(NormalizeIntervalMs(1) == 1);
    assert(NormalizeIntervalMs(250) == 250);
    assert(NormalizeIntervalMs(1000) == 1000);
    assert(NormalizeIntervalMs(60001) == 60001); // user-entered positive value is not clamped.

    Inputs in{};
    in.enabled = true;
    in.toolRunning = true;
    in.snapshotValid = true;
    in.mapReady = true;
    in.lifeValid = true;
    in.autoFightValid = true;
    in.autoFight = true;
    in.bagValid = true;
    in.freeBagSpace = 1;
    assert(Evaluate(in) == RuntimeGate::Active);

    in.freeBagSpace = 0;
    assert(Evaluate(in) == RuntimeGate::BagFull);
    in.freeBagSpace = 1;
    in.criticalBusy = true;
    assert(Evaluate(in) == RuntimeGate::CriticalBusy);
    in.criticalBusy = false;
    in.autoFight = false;
    assert(Evaluate(in) == RuntimeGate::AutoFightOff);
}

void TestAutoSellRoles() {
    using namespace itemtrade_coordinator;
    // Standalone/NONE: only full bag triggers independent auto sell.
    assert(ShouldAutoSell(true, 0, true, 0));
    assert(!ShouldAutoSell(true, 0, true, 1));
    assert(!ShouldAutoSell(true, 0, false, 0));
    // MAIN keeps capacity policy; CON never starts independent sell.
    assert(ShouldAutoSell(true, 1, true, 8));
    assert(!ShouldAutoSell(true, 1, true, 9));
    assert(!ShouldAutoSell(true, 2, true, 0));
    assert(!ShouldAutoSell(true, 3, true, 0));
}

void MultiAccountSoak() {
    using namespace auto_loot_logic;
    constexpr std::size_t kAccounts = 24;
    constexpr std::uint32_t kTicks = 250000;
    std::array<std::uint32_t, kAccounts> activeCount{};

    for (std::uint32_t tick = 1; tick <= kTicks; ++tick) {
        for (std::size_t i = 0; i < kAccounts; ++i) {
            Inputs in{};
            in.enabled = true;
            in.toolRunning = true;
            in.snapshotValid = true;
            in.mapReady = true;
            in.lifeValid = true;
            in.autoFightValid = true;
            in.autoFight = ((tick + i) % 131u) != 0u;
            in.bagValid = true;
            in.freeBagSpace = ((tick + i * 3u) % 97u) == 0u ? 0 : 1;
            // Represents any exclusive owner: trade/filter/sell/recovery/PK/TL/shortcut.
            in.criticalBusy = ((tick + i * 7u) % 29u) == 0u;
            const auto gate = Evaluate(in);
            if (gate == RuntimeGate::Active) {
                ++activeCount[i];
                assert(in.autoFight && in.freeBagSpace > 0 && !in.criticalBusy);
            } else if (in.freeBagSpace == 0 && in.autoFight) {
                assert(gate == RuntimeGate::BagFull);
            }
        }
    }

    // Every account must both pause and resume repeatedly; none may starve permanently.
    for (std::size_t i = 0; i < kAccounts; ++i) {
        assert(activeCount[i] > kTicks / 2u);
        const auto stagger = InitialStaggerMs(i, kAccounts, 1000);
        assert(stagger < 1000u);
    }
}

} // namespace

int main() {
    TestAutoLootGatesAndInterval();
    TestAutoSellRoles();
    MultiAccountSoak();
    return 0;
}
