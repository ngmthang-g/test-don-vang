#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>

namespace cleanroute {

constexpr std::uint32_t kMagic = 0x4352544Cu; // CRTL
// Protocol 3.5 is the cumulative 10.1 wire contract. It keeps V23 UI DIRECT and
// reserves command 29 for semantic ItemPack auto-loot after the old pickup-settings
// subsystem was removed in 10. Controller + Bridge must be shipped as one pair.
constexpr std::uint32_t kProtocolVersion = 0x00030500u;
constexpr UINT kWakeMessage = WM_APP + 0x531;
constexpr wchar_t kMappingPrefix[] = L"Local\\ThanLongCleanRoute_";

enum class Command : std::uint32_t {
    None = 0,
    ReadState = 1,
    ToggleRide = 2,
    StartPath = 3,
    StopPath = 4,
    ClickNpc = 5,
    ConfirmMap = 6,
    Revive = 7,
    StartAutoFight = 8,
    StopAutoFight = 9,
    BeginBackgroundSell = 10,
    AdvanceBackgroundSell = 11,
    SellNextBagItem = 12,
    CloseBackgroundSell = 13,
    ClickInternalPoint = 14,
    BeginBackgroundTreatment = 15,
    AdvanceBackgroundTreatment = 16,
    CloseBackgroundTreatment = 17,
    ReadCurrency = 18,
    ReadBagPage = 19,
    DropBagItem = 20,
    SellBagItem = 21,
    SelectTargetByRoleID = 22,
    ClickTravelSemantic = 23,
    ConfirmTravelSemantic = 24,
    TestOpenBag = 25,
    // FILTER V4 only: image recognition -> raw InputSync click, independent of AUTO state.
    ClickInternalPointRawTest = 26,
    DragInternalPoint = 27,
    ProbeNearbyLoot = 28,
    PickNearestLoot = 29,
    ProbeUiDirect = 32,
    InvokeUiDirect = 33,
};

enum class UiDirectTarget : std::int32_t {
    None = 0,
    CloseItemPopup = 1,
    CloseBag = 2,
    CloseTrade = 3,
    TradeConfirm = 4,
    TradeTabEquip = 5,
    TradeLock = 6,
    TradeSubmit = 7,
    OpenBag = 8,
    TradeRequestCancel = 9,
};

enum class TravelSemantic : std::int32_t {
    None = 0,
    KunLunSon = 1,
    TinhTucHai = 2,
    DenCacMonPhai = 3,
    Trade = 4,
    InviteParty = 5,
    ThienSon = 6,
    SellPopup = 7,
    DiscardPopup = 8,
    DiscardConfirm = 10,
    PutUpPopup = 9,
    NamHai = 11,
    MieuCuong = 12,
    HoangLongPhu = 13,
    ThachLam = 14,
    DaiLy = 15,
};

enum class ActionResult : std::int32_t {
    None = 0,
    ActionInvoked = 1,
    StageReady = 2,
    NoCandidate = 3,
    UiClosed = 4,
    NothingToClose = 5,
};

enum SnapshotValid : std::uint32_t {
    ValidMapTransition = 1u << 0,
    ValidIdentity      = 1u << 1,
    ValidMap           = 1u << 2,
    ValidPosition      = 1u << 3,
    ValidRiding        = 1u << 4,
    ValidAutoPath      = 1u << 5,
    ValidLifeState     = 1u << 6,
    ValidAutoFight     = 1u << 7,
    ValidBagSpace      = 1u << 8,
};

struct Snapshot {
    std::uint32_t validMask = 0;
    std::int32_t roleID = 0;
    std::int32_t mapID = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t riding = 0;
    std::int32_t autoPathing = 0;
    std::int32_t mapReady = 0;
    std::int32_t waitingChangeMap = 0;
    std::int32_t dead = 0;
    std::int32_t autoFight = 0;
    std::int32_t freeBagSpace = -1;
    wchar_t characterName[64]{};
};

struct Request {
    std::uint32_t command = 0;
    std::int32_t arg0 = 0;
    std::int32_t arg1 = 0;
    std::int32_t arg2 = 0;
};

inline bool IsLicenseProtectedCommand(Command command) {
    switch (command) {
        case Command::None:
        case Command::ReadState:
        case Command::ReadCurrency:
        case Command::ReadBagPage:
            return false;
        default:
            return true;
    }
}

inline std::uint64_t Rotl64(std::uint64_t v, unsigned r) {
    return (v << (r & 63u)) | (v >> ((64u - r) & 63u));
}
inline std::uint64_t LicenseProofPepper() {
    volatile std::uint64_t p0 = 0x243F6A8885A308D3ull;
    volatile std::uint64_t p1 = 0x13198A2E03707344ull;
    volatile std::uint64_t p2 = 0xA4093822299F31D0ull;
    std::uint64_t v = p0 ^ Rotl64(p1, 19) ^ Rotl64(p2, 41);
    v ^= 0x082EFA98EC4E6C89ull;
    v ^= v >> 29;
    v *= 0x9E3779B185EBCA87ull;
    v ^= v >> 32;
    return v;
}
inline std::uint64_t LicenseProofMulA() {
    volatile std::uint64_t a = 0xD1B54A32D192ED03ull;
    volatile std::uint64_t b = 0xABC98388FB8FAC03ull;
    return (a ^ Rotl64(b, 23)) | 1ull;
}
inline std::uint64_t LicenseProofMulB() {
    volatile std::uint64_t a = 0x8CB92BA72F3D8DD7ull;
    volatile std::uint64_t b = 0xDB4F0B9175AE2165ull;
    return (a ^ Rotl64(b, 31)) | 1ull;
}
inline std::uint64_t LicenseRequestProof(std::uint64_t sessionToken, LONG seq, const Request& request) {
    std::uint64_t v = sessionToken ^ LicenseProofPepper();
    v ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(seq)) * LicenseProofMulA();
    v = Rotl64(v, 17) ^ (static_cast<std::uint64_t>(request.command) * LicenseProofMulB());
    v = Rotl64(v, 23) ^ static_cast<std::uint32_t>(request.arg0);
    v = Rotl64(v, 29) ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(request.arg1)) << 1);
    v = Rotl64(v, 31) ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(request.arg2)) << 7);
    v ^= v >> 33; v *= LicenseProofMulB();
    v ^= v >> 33; v *= LicenseProofMulA();
    v ^= v >> 33;
    return v ? v : 1ull;
}


struct BagItemSnapshot {
    std::int64_t instanceID = 0;
    std::int32_t itemID = 0;
    std::int32_t site = 0;
    std::int32_t position = -1;
    std::int32_t quantity = 0;
    std::int32_t bound = 0;
    std::int32_t throwable = 0;
    std::int32_t sellable = 0;
    std::int32_t isEquip = 0;
    std::int32_t isWeapon = 0;
    std::int32_t itemTypeCode = 0;
    std::int32_t equipTypeCode = 0;
    wchar_t name[96]{};
    wchar_t itemType[32]{};
    wchar_t equipType[32]{};
};

constexpr std::size_t kBagPageCapacity = 20;

struct BagPageSnapshot {
    std::int32_t totalCount = 0;
    std::int32_t pageStart = 0;
    std::int32_t pageCount = 0;
    std::int32_t freeBagSpace = -1;
    BagItemSnapshot items[kBagPageCapacity]{};
};

struct Response {
    std::int32_t ok = 0;
    std::int32_t resultCode = 0;
    std::int32_t value0 = 0;
    std::int32_t value1 = 0;
    std::int64_t value64_0 = 0;
    std::int64_t value64_1 = 0;
    Snapshot snapshot{};
    BagPageSnapshot bagPage{};
    wchar_t detail[512]{};
};

struct SharedBlock {
    std::uint32_t magic = kMagic;
    std::uint32_t protocolVersion = kProtocolVersion;
    std::uint32_t targetPid = 0;
    std::uint32_t targetWindowThreadId = 0;
    volatile LONG requestSeq = 0;
    volatile LONG completedSeq = 0;
    volatile LONG bridgeLoaded = 0;
    volatile LONG bridgeBusy = 0;
    volatile LONG licenseGate = 0;
    std::uint64_t licenseSessionToken = 0;
    std::uint64_t requestLicenseProof = 0;
    Request request{};
    Response response{};
};

inline void MappingName(DWORD pid, wchar_t* output, std::size_t count) {
    if (!output || count == 0) return;
    wsprintfW(output, L"%s%lu", kMappingPrefix, static_cast<unsigned long>(pid));
}

} // namespace cleanroute
