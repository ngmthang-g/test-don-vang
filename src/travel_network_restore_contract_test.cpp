// Original 10.6 TravelNetwork routing contract: assertions remain active in Release /DNDEBUG.
// This complements (does not replace) the byte-identical original travel_network_logic_test.cpp.
#include "travel_network_logic.h"
#include <iostream>

using namespace travel_network_logic;

static_assert(kDaiLyMap == 2 && kNamHaiMap == 85 && kQuynhChauMap == 86);
static_assert(kMieuCuongMap == 64 && kNamChieuMap == 63);
static_assert(kHoangLongPhuMap == 49 && kTruongBachSonMap == 48);
static_assert(kThachLamMap == 60 && kNgocKheMap == 61);
static_assert(kDiemHoMap == 65 && kBachSaDiemKhanhMap == 66);

int main() {
    int checked = 0;
    const auto check = [&](bool ok, const char* label) {
        ++checked;
        if (!ok) std::cerr << "TravelNetwork RESTORE FAIL: " << label << "\n";
        return ok;
    };
    bool ok = true;
    const auto outbound = [&](int dest, int expectedStage, Semantic semantic) {
        const auto p = SelectNpcTeleport(kDaiLyMap, dest);
        return check(p.valid && p.fromMap == kDaiLyMap &&
            p.expectedMap == expectedStage && p.npcID == kXaTruyenChiNpcId &&
            p.npcX == 8308 && p.npcY == 7554 &&
            p.semantic == semantic && p.needConfirm, "outbound route");
    };
    ok &= outbound(kNamHaiMap, 85, Semantic::NamHai);
    ok &= outbound(kQuynhChauMap, 85, Semantic::NamHai);
    ok &= outbound(kMieuCuongMap, 64, Semantic::MieuCuong);
    ok &= outbound(kNamChieuMap, 64, Semantic::MieuCuong);
    ok &= outbound(kHoangLongPhuMap, 49, Semantic::HoangLongPhu);
    ok &= outbound(kTruongBachSonMap, 49, Semantic::HoangLongPhu);
    ok &= outbound(kThachLamMap, 60, Semantic::ThachLam);
    ok &= outbound(kNgocKheMap, 60, Semantic::ThachLam);
    ok &= outbound(kDiemHoMap, 60, Semantic::ThachLam);
    ok &= outbound(kBachSaDiemKhanhMap, 60, Semantic::ThachLam);

    const auto inbound = [&](int current, ExitKind kind, int stage, int x, int y) {
        const auto p = SelectReturnExit(current, 2);
        return check(p.valid && p.kind == kind && p.stagingMap == stage &&
            p.defaultX == x && p.defaultY == y, "return EXIT");
    };
    ok &= inbound(85, ExitKind::NamHai, 85, 7259, 1899);
    ok &= inbound(86, ExitKind::NamHai, 85, 7259, 1899);
    ok &= inbound(64, ExitKind::MieuCuong, 64, 6306, 4997);
    ok &= inbound(63, ExitKind::MieuCuong, 64, 6306, 4997);
    ok &= inbound(49, ExitKind::HoangLongPhu, 49, 4705, 6997);
    ok &= inbound(48, ExitKind::HoangLongPhu, 49, 4705, 6997);
    ok &= inbound(60, ExitKind::ThachLam, 60, 6556, 6394);
    ok &= inbound(61, ExitKind::ThachLam, 60, 6556, 6394);
    ok &= inbound(65, ExitKind::ThachLam, 60, 6556, 6394);
    ok &= inbound(66, ExitKind::ThachLam, 60, 6556, 6394);

    const auto home = SelectNpcTeleport(85, 2);
    ok &= check(home.valid && home.npcID == 522 && home.semantic == Semantic::DaiLy,
                "NamHai -> DaiLy existing NPC");
    ok &= check(!SelectNpcTeleport(5, 85).valid, "no obsolete LauLan NPC hub");
    ok &= check(!SelectNpcTeleport(60, 61).valid &&
                !SelectNpcTeleport(64, 2).valid &&
                !SelectNpcTeleport(49, 2).valid, "do not invent unproven NPC routes");
    ok &= check(!SelectReturnExit(60, 61).valid &&
                !SelectReturnExit(85, 86).valid, "no exit to non-central target");
    ok &= check(!SelectNpcTeleport(2, 10005).valid &&
                !SelectNpcTeleport(2, 10014).valid &&
                !SelectReturnExit(85, 10014).valid, "THDC/interserver explicitly excluded");
    ok &= check(!SelectNpcTeleport(2, 75).valid &&
                !SelectNpcTeleport(2, 55).valid, "Côn Lôn/Fire remain separate paths");
    if (!ok) return 1;
    std::cout << "TravelNetwork 10.6 RESTORE PASS: " << checked
              << " original routes/return groups and fail-closed boundaries (Release)\n";
    return 0;
}
