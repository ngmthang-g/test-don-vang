#include "automation_bulk_logic.h"
#include <iostream>

int main() {
    using automation_bulk_logic::EligibleForGather;
    using automation_bulk_logic::EligibleForSpot;
    using automation_bulk_logic::SpotScope;

    if (EligibleForSpot(true, 1, 1, SpotScope::Party)) return 1;
    if (!EligibleForSpot(false, 3, 3, SpotScope::Party)) return 2;
    if (EligibleForSpot(false, 2, 3, SpotScope::Party)) return 3;
    if (EligibleForSpot(false, 0, 0, SpotScope::Party)) return 4;
    if (!EligibleForSpot(false, 0, 0, SpotScope::AllCon)) return 5;
    if (EligibleForSpot(true, 0, 0, SpotScope::AllCon)) return 6;
    if (!EligibleForGather(false)) return 7;
    if (EligibleForGather(true)) return 8;

    std::cout << "automation_bulk_logic_tests: PASS\n";
    return 0;
}
