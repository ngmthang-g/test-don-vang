#include "con_filter_nosleep_logic.h"

#include <cassert>

int main() {
    using namespace con_filter_nosleep_logic;
    assert(ClampMinActionMs(-1) == 0);
    assert(ClampMinActionMs(0) == 0);
    assert(ClampMinActionMs(50) == 50);
    assert(ClampMinActionMs(999999) == 10000);

    assert(ActionDue(100, 0, 50));
    assert(ActionDue(100, 100, 0));
    assert(!ActionDue(149, 100, 50));
    assert(ActionDue(150, 100, 50));
    assert(ActionDue(230, 100, 50));

    assert(NotBefore(110, 100, 50) == 150);
    assert(NotBefore(180, 100, 50) == 180);
    assert(NotBefore(180, 0, 50) == 180);

    assert(ClassifySemanticFailure(L"SEMANTIC_SCAN NOT_FOUND • active=4") == SemanticProbeFailure::NotFound);
    assert(ClassifySemanticFailure(L"SEMANTIC_SCAN AMBIGUOUS • exact candidates=2") == SemanticProbeFailure::Ambiguous);
    assert(ClassifySemanticFailure(L"Bridge busy") == SemanticProbeFailure::Retry);
    return 0;
}
