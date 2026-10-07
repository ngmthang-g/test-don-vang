#include "pk_tl_lm_logic.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace pk_tl_lm_logic;
    assert(kSharedPointCount == 8);
    assert(kTreatmentClickIntervalMs == 1000);
    assert(SharedPointIndexForClickSlot(5) == 0);
    assert(SharedPointIndexForClickSlot(10) == 1);
    assert(SharedPointIndexForClickSlot(6) == 2);
    assert(SharedPointIndexForClickSlot(9) == 5);
    assert(SharedPointIndexForClickSlot(11) == 6);
    assert(SharedPointIndexForClickSlot(12) == 7);
    assert(SharedPointIndexForClickSlot(2) == -1);
    assert(FirstTrainPkPhase(false) == 3);
    assert(FirstTrainPkPhase(true) == 1);
    assert(ShouldArmTrainPk(true, false, 0));
    assert(!ShouldArmTrainPk(false, false, 0));
    assert(!ShouldArmTrainPk(true, true, 0));
    assert(!ShouldArmTrainPk(true, false, 2));
    assert(TreatmentRequested(true, true));
    assert(!TreatmentRequested(false, true));
    std::cout << "pk_tl_lm_logic_tests PASS\n";
    return 0;
}
