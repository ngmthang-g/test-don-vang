#include "bag_filter_v2_logic.h"
#include <cassert>
#include <vector>

using namespace bag_filter_v2_logic;

int main() {
    ItemDecisionInput weapon{0,true,false,true,0,false};
    assert(IsWeapon(weapon));
    assert(!ShouldDrop(weapon));

    ItemDecisionInput ring{1,true,false,true,6,false};
    assert(ShouldDrop(ring));

    ItemDecisionInput locked{2,true,true,true,6,false};
    assert(!ShouldDrop(locked));

    ItemDecisionInput unknown{3,true,false,false,-1,false};
    assert(!ShouldDrop(unknown));

    ItemDecisionInput namedCommon{4,false,false,false,-1,true};
    assert(ShouldDrop(namedCommon));
    namedCommon.bound = true;
    assert(!ShouldDrop(namedCommon));

    assert(RequiredScrollDepth(0)==0 && PhysicalIndex(0)==0);
    assert(RequiredScrollDepth(41)==0 && PhysicalIndex(41)==41);
    assert(RequiredScrollDepth(42)==1 && PhysicalIndex(42)==0);
    assert(RequiredScrollDepth(50)==1 && PhysicalIndex(50)==8);
    assert(RequiredScrollDepth(83)==1 && PhysicalIndex(83)==41);
    assert(RequiredScrollDepth(84)==2 && PhysicalIndex(84)==21);
    assert(RequiredScrollDepth(99)==2 && PhysicalIndex(99)==36);

    // Depth-2 candidates never require a fake depth-1 candidate. Each call advances one swipe.
    assert(NextScrollDepth(0,2)==1);
    assert(NextScrollDepth(1,2)==2);
    assert(NextScrollDepth(0,1)==1);
    assert(NextScrollDepth(2,2)==2);
    assert(NextScrollDepth(2,1)==-1);
    assert(NextScrollDepth(-1,2)==-1);

    // FULL never hard-aborts an active item transaction; it exits at the next safe boundary.
    assert(FullBoundaryReady(true,false));
    assert(!FullBoundaryReady(true,true));
    assert(!FullBoundaryReady(false,false));

    const auto v = NormalizeCandidates({99,50,-1,50,0,100,84,41});
    assert((v == std::vector<int>{0,41,50,84,99}));
    return 0;
}
