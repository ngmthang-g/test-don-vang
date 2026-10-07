#include "auto_loot_logic.h"
#include "trade_coordinator_logic.h"
#include "pk_tl_lm_logic.h"
#include "travel_network_logic.h"

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

void TestPkTlLmPerAccountAndSharedCoordinates() {
    using namespace pk_tl_lm_logic;
    assert(kSharedPointCount == 8);
    std::array<bool, kSharedPointCount> seen{};
    for (const int slot : {5, 10, 6, 7, 8, 9, 11, 12}) {
        const int i = SharedPointIndexForClickSlot(slot);
        assert(i >= 0 && i < kSharedPointCount);
        assert(!seen[static_cast<std::size_t>(i)]);
        seen[static_cast<std::size_t>(i)] = true;
    }
    for (bool v : seen) assert(v);

    // Enable state remains per account even though the 8 click points are shared.
    assert(ShouldArmTrainPk(true, false, 0));
    assert(!ShouldArmTrainPk(false, false, 0));
    assert(!ShouldArmTrainPk(true, true, 0));
    assert(FirstTrainPkPhase(true) == 1);
    assert(FirstTrainPkPhase(false) == 3);
    assert(TreatmentRequested(true, true));
    assert(!TreatmentRequested(false, true));
}

void TestTravelNetworkFailClosed() {
    using namespace travel_network_logic;
    auto p = SelectNpcTeleport(kDaiLyMap, kNamHaiMap);
    assert(p.valid && p.npcID == kXaTruyenChiNpcId && p.npcX == kXaTruyenChiX && p.npcY == kXaTruyenChiY && p.semantic == Semantic::NamHai);
    p = SelectNpcTeleport(kDaiLyMap, kMieuCuongMap);
    assert(p.valid && p.semantic == Semantic::MieuCuong);
    p = SelectNpcTeleport(kDaiLyMap, kHoangLongPhuMap);
    assert(p.valid && p.semantic == Semantic::HoangLongPhu);
    p = SelectNpcTeleport(kDaiLyMap, kNgocKheMap);
    assert(p.valid && p.expectedMap == kThachLamMap && p.semantic == Semantic::ThachLam);
    p = SelectNpcTeleport(kNamHaiMap, kDaiLyMap);
    assert(p.valid && p.npcID == kXaTruyenTinNpcId && p.semantic == Semantic::DaiLy);

    // Unproven reverse routes must remain absent so controller falls back/fails closed.
    assert(!SelectNpcTeleport(kMieuCuongMap, kLauLanMap).valid);
    assert(!SelectNpcTeleport(kHoangLongPhuMap, kLauLanMap).valid);
    assert(!SelectNpcTeleport(kThachLamMap, kLauLanMap).valid);
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
    TestPkTlLmPerAccountAndSharedCoordinates();
    TestTravelNetworkFailClosed();
    MultiAccountSoak();
    return 0;
}
