#pragma once
#include <cstdint>

namespace pk_tl_lm_logic {

inline constexpr std::uint32_t kTreatmentClickIntervalMs = 1000;
inline constexpr int kSharedPointCount = 8;

// ClickSlot numeric contract in controller.cpp:
// 5=PK1, 6=TL1, 7=TL2, 8=TL3, 9=TL4, 10=PK2, 11=LM1, 12=LM2.
constexpr int SharedPointIndexForClickSlot(int slot) {
    switch (slot) {
        case 5: return 0;  // PK1
        case 10: return 1; // PK2
        case 6: return 2;  // TL1
        case 7: return 3;  // TL2
        case 8: return 4;  // TL3
        case 9: return 5;  // TL4
        case 11: return 6; // LM1
        case 12: return 7; // LM2
        default: return -1;
    }
}

constexpr int FirstTrainPkPhase(bool enableAlliancePk) {
    return enableAlliancePk ? 1 : 3;
}

constexpr bool ShouldArmTrainPk(bool enableAutoPk, bool pending, int phase) {
    return enableAutoPk && !pending && phase == 0;
}

constexpr bool TreatmentRequested(bool enableTreatment, bool pending) {
    return enableTreatment && pending;
}

} // namespace pk_tl_lm_logic
