#pragma once

namespace automation_bulk_logic {

enum class SpotScope {
    Party,
    AllCon,
};

inline bool EligibleForSpot(bool isMain, int accountParty, int selectedParty, SpotScope scope) {
    if (isMain) return false;
    if (scope == SpotScope::AllCon) return true;
    return selectedParty > 0 && accountParty == selectedParty;
}

inline bool EligibleForGather(bool isMain) {
    return !isMain;
}

} // namespace automation_bulk_logic
