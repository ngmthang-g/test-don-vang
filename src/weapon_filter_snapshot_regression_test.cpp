// T16 deterministic C1/C2 bag/grid snapshot fixtures. No Windows game needed.
// This covers decision/layout/FULL contracts, NOT real-frame template matching.
#include "bag_filter_v2_logic.h"
#include "con_filter_nosleep_logic.h"
#include "sell_filter_grid_v2_logic.h"
#include "image_scan_test.h"

#include <array>
#include <iostream>
#include <vector>

namespace {
int failures = 0;

void Expect(bool ok, const char* context) {
    if (!ok) {
        ++failures;
        std::cerr << "SNAPSHOT FAIL: " << context << "\n";
    }
}

struct GridSnapshot {
    int position;
    int expectedDepth;
    int expectedPhysical;
};

// Golden 42-slot UI grid: depth0 0..41, depth1 42..83, depth2
// 84..99 mapped to physical slots 21..36. Sentinel inputs fail closed.
constexpr std::array<GridSnapshot, 14> kC1Grid = {{
    {-1,-1,-1}, {0,0,0}, {1,0,1}, {20,0,20}, {41,0,41},
    {42,1,0}, {43,1,1}, {63,1,21}, {83,1,41},
    {84,2,21}, {90,2,27}, {98,2,35}, {99,2,36}, {100,-1,-1}
}};

void TestC1GridSnapshot() {
    for (const auto& row : kC1Grid) {
        Expect(sell_filter_grid_v2_logic::RequiredDepth(row.position) == row.expectedDepth,
               "C1: required depth");
        Expect(sell_filter_grid_v2_logic::PhysicalGrid(row.position) == row.expectedPhysical,
               "C1: physical grid");
        // C2's semantic Position -> F8 coordinate mapping must remain identical.
        Expect(bag_filter_v2_logic::RequiredScrollDepth(row.position) == row.expectedDepth,
               "C2: semantic required depth");
        Expect(bag_filter_v2_logic::PhysicalIndex(row.position) == row.expectedPhysical,
               "C2: semantic physical index");
    }
    Expect(sell_filter_grid_v2_logic::NewPositionFirstForDepth(0)==0 &&
           sell_filter_grid_v2_logic::NewPositionLastForDepth(0)==41, "C1: first page");
    Expect(sell_filter_grid_v2_logic::NewPositionFirstForDepth(1)==42 &&
           sell_filter_grid_v2_logic::NewPositionLastForDepth(1)==83, "C1: second page");
    Expect(sell_filter_grid_v2_logic::NewPositionFirstForDepth(2)==84 &&
           sell_filter_grid_v2_logic::NewPositionLastForDepth(2)==99, "C1: third page");
    Expect(sell_filter_grid_v2_logic::ResetSwipeCount(2)==2 &&
           sell_filter_grid_v2_logic::ResetSwipeCount(3)==-1, "C1: swipe reset");
    Expect(!sell_filter_grid_v2_logic::ReadyToResumeAfterSell(true,false,true),
           "C1: seller not reset");
    Expect(sell_filter_grid_v2_logic::ReadyToResumeAfterSell(true,true,true),
           "C1: seller resumed after reset");
}

struct BagSnapshot {
    int position;
    bool equip;
    bool bound;
    bool pointKnown;
    int equipPoint;
    bool exactNameOverride;
    bool drop;
    bool weapon;
};

constexpr std::array<BagSnapshot, 10> kC2Bag = {{
    {0, true, false, true, 0, false, false, true},   // unlocked weapon kept
    {41, true, false, true, 4, false, true, false},  // unlocked nonweapon equip
    {42, true, true, true, 4, false, false, false},  // bound never dropped
    {83, true, false, false, -1, false, false, false}, // unknown point kept
    {84, false, false, false, -1, true, true, false}, // user exact-name opt-in
    {99, true, false, true, 0, true, true, true},   // override even if weapon
    {5, false, true, false, -1, true, false, false}, // bound beats override
    {-1, true, false, true, 4, true, false, false}, // invalid position
    {100, true, false, true, 4, false, false, false}, // invalid position
    {50, false, false, false, -1, false, false, false} // common item kept
}};

void TestC2BagSnapshot() {
    std::vector<int> candidates;
    for (const auto& row : kC2Bag) {
        cleanroute::BagItemSnapshot bag{};
        bag.position=row.position;
        bag.isEquip=row.equip?1:0;
        bag.bound=row.bound?1:0;
        bag.isWeapon=row.weapon?1:0;

        bag_filter_v2_logic::ItemDecisionInput decision{};
        decision.position=bag.position;
        decision.isEquip=bag.isEquip!=0;
        decision.bound=bag.bound!=0;
        decision.equipPointKnown=row.pointKnown;
        decision.equipPoint=row.equipPoint;
        decision.exactNameOverride=row.exactNameOverride;

        const bool drop=bag_filter_v2_logic::ShouldDrop(decision);
        const bool weapon=bag_filter_v2_logic::IsWeapon(decision);
        Expect(drop==row.drop, "C2: should-drop golden bag row");
        Expect(weapon==row.weapon, "C2: equip-point weapon classification");
        if(drop)candidates.push_back(bag.position);
    }
    candidates.push_back(99); // repeated Position from a duplicate page must normalize.
    candidates.push_back(100); // invalid Position must never enter a candidate list.
    const auto normalized=bag_filter_v2_logic::NormalizeCandidates(candidates);
    Expect(normalized==std::vector<int>({41,84,99}), "C2: sorted unique positions");
}

void TestModeAndFullHandshakeSnapshot() {
    Expect(static_cast<int>(image_scan_test::FilterMode::V4)==1 &&
           static_cast<int>(image_scan_test::FilterMode::SemanticBag)==2,
           "C1/C2: persisted mode IDs");
    Expect(bag_filter_v2_logic::NextScrollDepth(0,2)==1 &&
           bag_filter_v2_logic::NextScrollDepth(1,2)==2 &&
           bag_filter_v2_logic::NextScrollDepth(2,1)==-1,
           "C2: no skip over intermediate swipe");
    Expect(!bag_filter_v2_logic::FullBoundaryReady(true,true),
           "C2: FULL must wait for active item transaction");
    Expect(bag_filter_v2_logic::FullBoundaryReady(true,false),
           "C2: FULL yields at safe boundary");
    Expect(!bag_filter_v2_logic::FullBoundaryReady(false,false),
           "C2: no FULL, no yield");

    using namespace con_filter_nosleep_logic;
    Expect(ClassifySemanticFailure(L"SEMANTIC_SCAN AMBIGUOUS") ==
           SemanticProbeFailure::Ambiguous, "C2: ambiguous fail closed");
    Expect(ClassifySemanticFailure(L"SEMANTIC_SCAN NOT_FOUND") ==
           SemanticProbeFailure::NotFound, "C2: missing target classification");
    Expect(ClassifySemanticFailure(L"Bridge busy") ==
           SemanticProbeFailure::Retry, "C2: transient retry classification");
    Expect(!ActionDue(149,100,50) && ActionDue(150,100,50),
           "C2: nonblocking minimum click interval");
}
} // namespace

int main() {
    TestC1GridSnapshot();
    TestC2BagSnapshot();
    TestModeAndFullHandshakeSnapshot();
    if(failures) {
        std::cerr << "C1/C2 golden snapshots FAILED: " << failures << " invariant(s)\n";
        return 1; // Explicit failure, including Release /DNDEBUG builds.
    }
    std::cout << "C1/C2 golden snapshots PASS (synthetic bag + grid, not game-image matching)\n";
    return 0;
}
