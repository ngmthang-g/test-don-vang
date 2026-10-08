#pragma once

namespace travel_network_logic {

constexpr int kDaiLyMap = 2;
constexpr int kLauLanMap = 5;
constexpr int kTruongBachSonMap = 48;
constexpr int kHoangLongPhuMap = 49;
constexpr int kThachLamMap = 60;
constexpr int kNgocKheMap = 61;
constexpr int kNamChieuMap = 63;
constexpr int kMieuCuongMap = 64;
constexpr int kDiemHoMap = 65;
constexpr int kBachSaDiemKhanhMap = 66;
constexpr int kNamHaiMap = 85;
constexpr int kQuynhChauMap = 86;

constexpr int kNamHaiExitDefaultX = 7259;
constexpr int kNamHaiExitDefaultY = 1899;
constexpr int kMieuCuongExitDefaultX = 6306;
constexpr int kMieuCuongExitDefaultY = 4997;
constexpr int kHoangLongPhuExitDefaultX = 4705;
constexpr int kHoangLongPhuExitDefaultY = 6997;
constexpr int kThachLamExitDefaultX = 6556;
constexpr int kThachLamExitDefaultY = 6394;

constexpr int kXaTruyenBinhNpcId = 387; // retained for unrelated existing workflows
constexpr int kXaTruyenChiNpcId = 45;
constexpr int kXaTruyenChiX = 8308;
constexpr int kXaTruyenChiY = 7554;
constexpr int kXaTruyenTinNpcId = 522;
constexpr int kXaTruyenTinX = 7236;
constexpr int kXaTruyenTinY = 1908;

enum class ExitKind : int {
    None = 0,
    NamHai,
    MieuCuong,
    HoangLongPhu,
    ThachLam,
};

struct ExitPlan {
    bool valid = false;
    ExitKind kind = ExitKind::None;
    int stagingMap = 0;
    int defaultX = 0;
    int defaultY = 0;
    const wchar_t* label = L"";
};

constexpr bool IsClassicNineFactionMap(int mapID) { return mapID >= 6 && mapID <= 14; }
constexpr bool IsCentralDestinationMap(int mapID) {
    return mapID == kDaiLyMap || mapID == 3 || mapID == 4 || mapID == kLauLanMap ||
           IsClassicNineFactionMap(mapID);
}

// More-specific pair rules win over the generic M60 network. In particular M63
// (Nam Chiêu) exits through M64 Miêu Cương, matching the explicit approved rule.
constexpr ExitPlan SelectReturnExit(int currentMap, int destinationMap) {
    if (!IsCentralDestinationMap(destinationMap)) return {};
    if (currentMap == kNamHaiMap || currentMap == kQuynhChauMap)
        return {true, ExitKind::NamHai, kNamHaiMap, kNamHaiExitDefaultX, kNamHaiExitDefaultY, L"Nam Hải EXIT"};
    if (currentMap == kMieuCuongMap || currentMap == kNamChieuMap)
        return {true, ExitKind::MieuCuong, kMieuCuongMap, kMieuCuongExitDefaultX, kMieuCuongExitDefaultY, L"Miêu Cương EXIT"};
    if (currentMap == kHoangLongPhuMap || currentMap == kTruongBachSonMap)
        return {true, ExitKind::HoangLongPhu, kHoangLongPhuMap, kHoangLongPhuExitDefaultX, kHoangLongPhuExitDefaultY, L"Hoàng Long Phủ EXIT"};
    if (currentMap == kThachLamMap || currentMap == kNgocKheMap ||
        currentMap == kDiemHoMap || currentMap == kBachSaDiemKhanhMap)
        return {true, ExitKind::ThachLam, kThachLamMap, kThachLamExitDefaultX, kThachLamExitDefaultY, L"M60 EXIT"};
    return {};
}

enum class Semantic : int {
    None = 0,
    NamHai,
    MieuCuong,
    HoangLongPhu,
    ThachLam,
    DaiLy,
};

struct NpcTeleportPlan {
    bool valid = false;
    int fromMap = 0;
    int expectedMap = 0;
    int npcID = 0;
    int npcX = 0;
    int npcY = 0;
    bool useSharedXaTruyenBinhPosition = false; // legacy descriptor flag; TravelNetwork outbound no longer uses it.
    Semantic semantic = Semantic::None;
    bool needConfirm = true;
    const wchar_t* label = L"";
};

constexpr bool IsThachLamPortalNetworkMap(int mapID) {
    return mapID == kThachLamMap || mapID == kNgocKheMap ||
           mapID == kDiemHoMap || mapID == kBachSaDiemKhanhMap;
}

// Select only the special NPC leg proven by source/DATA. Normal portal routing
// remains owned by the existing AutoPath engine after this leg finishes.
// This intentionally does not invent return NPCs whose ResID is still unknown.
constexpr NpcTeleportPlan SelectNpcTeleport(int currentMap, int destinationMap) {
    if (currentMap == kDaiLyMap) {
        if (destinationMap == kNamHaiMap || destinationMap == kQuynhChauMap) {
            return {true, kDaiLyMap, kNamHaiMap, kXaTruyenChiNpcId, kXaTruyenChiX, kXaTruyenChiY, false,
                    Semantic::NamHai, true, L"Xa Truyền Chí → Nam Hải"};
        }
        if (destinationMap == kMieuCuongMap || destinationMap == kNamChieuMap) {
            return {true, kDaiLyMap, kMieuCuongMap, kXaTruyenChiNpcId, kXaTruyenChiX, kXaTruyenChiY, false,
                    Semantic::MieuCuong, true, L"Xa Truyền Chí → Miêu Cương"};
        }
        if (destinationMap == kHoangLongPhuMap || destinationMap == kTruongBachSonMap) {
            return {true, kDaiLyMap, kHoangLongPhuMap, kXaTruyenChiNpcId, kXaTruyenChiX, kXaTruyenChiY, false,
                    Semantic::HoangLongPhu, true, L"Xa Truyền Chí → Hoàng Long Phủ"};
        }
        if (IsThachLamPortalNetworkMap(destinationMap)) {
            return {true, kDaiLyMap, kThachLamMap, kXaTruyenChiNpcId, kXaTruyenChiX, kXaTruyenChiY, false,
                    Semantic::ThachLam, true, L"Xa Truyền Chí → Thạch Lâm"};
        }
    }

    // Only this return NPC is proven. Miêu Cương/Hoàng Long Phủ/Thạch Lâm and
    // downstream maps deliberately fall through to normal AutoPath rather than
    // guessing Xa Truyền Chỉ/Sảng/Quy or any other NPC ID.
    if (currentMap == kNamHaiMap && destinationMap == kDaiLyMap) {
        return {true, kNamHaiMap, kDaiLyMap, kXaTruyenTinNpcId,
                kXaTruyenTinX, kXaTruyenTinY, false,
                Semantic::DaiLy, true, L"Xa Truyền Tín → Đại Lý"};
    }

    return {};
}

} // namespace travel_network_logic
