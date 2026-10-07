#include "travel_network_logic.h"
#include <cassert>

int main() {
    using namespace travel_network_logic;

    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kNamHaiMap);
        assert(p.valid && p.npcID == 45 && p.expectedMap == 85);
        assert(p.npcX == 8308 && p.npcY == 7554);
        assert(!p.useSharedXaTruyenBinhPosition && p.semantic == Semantic::NamHai && p.needConfirm);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kMieuCuongMap);
        assert(p.valid && p.expectedMap == 64 && p.semantic == Semantic::MieuCuong);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kHoangLongPhuMap);
        assert(p.valid && p.expectedMap == 49 && p.semantic == Semantic::HoangLongPhu);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kBachSaDiemKhanhMap);
        assert(p.valid && p.expectedMap == kThachLamMap && p.semantic == Semantic::ThachLam);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kNgocKheMap);
        assert(p.valid && p.expectedMap == kThachLamMap);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kQuynhChauMap);
        assert(p.valid && p.expectedMap == kNamHaiMap && p.semantic == Semantic::NamHai);
    }
    {
        const auto p = SelectNpcTeleport(kDaiLyMap, kTruongBachSonMap);
        assert(p.valid && p.expectedMap == kHoangLongPhuMap && p.semantic == Semantic::HoangLongPhu);
    }
    {
        const auto p = SelectNpcTeleport(kNamHaiMap, kDaiLyMap);
        assert(p.valid && p.npcID == 522 && p.npcX == 7236 && p.npcY == 1908);
        assert(!p.useSharedXaTruyenBinhPosition && p.semantic == Semantic::DaiLy);
    }

    // Current map already inside the Thạch Lâm portal network: never loop back
    // through Đại Lý. Existing AutoPath owns the direct portal chain.
    assert(!SelectNpcTeleport(kDiemHoMap, kBachSaDiemKhanhMap).valid);
    assert(!SelectNpcTeleport(kThachLamMap, kNgocKheMap).valid);
    assert(!SelectNpcTeleport(kNamChieuMap, kThachLamMap).valid);

    // Unproven return NPCs must never be guessed/hardcoded by this planner.
    assert(!SelectNpcTeleport(kMieuCuongMap, kDaiLyMap).valid);
    assert(!SelectNpcTeleport(kHoangLongPhuMap, kDaiLyMap).valid);
    assert(!SelectNpcTeleport(kThachLamMap, kDaiLyMap).valid);


    // 10.6 return-exit planner: exact approved staging maps/default coordinates.
    { auto p = SelectReturnExit(kQuynhChauMap, kDaiLyMap); assert(p.valid && p.kind == ExitKind::NamHai && p.stagingMap == kNamHaiMap && p.defaultX == 7259 && p.defaultY == 1899); }
    { auto p = SelectReturnExit(kNamChieuMap, 4); assert(p.valid && p.kind == ExitKind::MieuCuong && p.stagingMap == kMieuCuongMap && p.defaultX == 6306 && p.defaultY == 4997); }
    { auto p = SelectReturnExit(kTruongBachSonMap, 6); assert(p.valid && p.kind == ExitKind::HoangLongPhu && p.stagingMap == kHoangLongPhuMap); }
    { auto p = SelectReturnExit(kNgocKheMap, 14); assert(p.valid && p.kind == ExitKind::ThachLam && p.stagingMap == kThachLamMap); }
    assert(!SelectReturnExit(kThachLamMap, kNgocKheMap).valid);
    assert(!SelectReturnExit(kNamHaiMap, kQuynhChauMap).valid);
    { auto p = SelectNpcTeleport(kDaiLyMap, kNamChieuMap); assert(p.valid && p.expectedMap == kMieuCuongMap && p.semantic == Semantic::MieuCuong); }

    // Existing unrelated shortcut families remain outside this planner.
    assert(!SelectNpcTeleport(kDaiLyMap, 75).valid);
    assert(!SelectNpcTeleport(kDaiLyMap, 55).valid);
    // Old Lâu Lan hub must no longer arm TravelNetwork outbound.
    assert(!SelectNpcTeleport(kLauLanMap, kNamHaiMap).valid);
    return 0;
}
