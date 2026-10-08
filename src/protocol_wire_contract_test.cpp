// T19: compile- and Release-runtime gates for the locked 10.6 wire protocol.
// Retired command numbers remain holes; no surviving numeric opcode moves.
#include "protocol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

using cleanroute::Command;
using cleanroute::TravelSemantic;
using cleanroute::UiDirectTarget;

static_assert(cleanroute::kMagic == 0x4352544Cu, "wire magic changed");
static_assert(cleanroute::kProtocolVersion == 0x00030500u, "wire protocol version changed");
static_assert(sizeof(cleanroute::Request) == 16, "wire Request layout changed");
static_assert(offsetof(cleanroute::Request, command) == 0, "request command offset changed");
static_assert(offsetof(cleanroute::Request, arg0) == 4, "request arg0 offset changed");
static_assert(offsetof(cleanroute::Request, arg1) == 8, "request arg1 offset changed");
static_assert(offsetof(cleanroute::Request, arg2) == 12, "request arg2 offset changed");
static_assert(cleanroute::kBagPageCapacity == 20, "bag page wire count changed");

#define LOCK_CMD(name, id) static_assert(static_cast<std::uint32_t>(Command::name) == id, "command " #name " renumbered")
LOCK_CMD(None, 0);
LOCK_CMD(ReadState, 1);
LOCK_CMD(ToggleRide, 2);
LOCK_CMD(StartPath, 3);
LOCK_CMD(StopPath, 4);
LOCK_CMD(ClickNpc, 5);
LOCK_CMD(ConfirmMap, 6);
LOCK_CMD(Revive, 7);
LOCK_CMD(StartAutoFight, 8);
LOCK_CMD(StopAutoFight, 9);
LOCK_CMD(ClickInternalPoint, 14);
LOCK_CMD(ReadCurrency, 18);
LOCK_CMD(ReadBagPage, 19);
LOCK_CMD(DropBagItem, 20);
LOCK_CMD(SellBagItem, 21);
LOCK_CMD(SelectTargetByRoleID, 22);
LOCK_CMD(ClickTravelSemantic, 23);
LOCK_CMD(ConfirmTravelSemantic, 24);
LOCK_CMD(ClickInternalPointRawTest, 26);
LOCK_CMD(DragInternalPoint, 27);
LOCK_CMD(ProbeNearbyLoot, 28);
LOCK_CMD(PickNearestLoot, 29);
LOCK_CMD(ProbeUiDirect, 32);
LOCK_CMD(InvokeUiDirect, 33);
#undef LOCK_CMD

static_assert(static_cast<int>(UiDirectTarget::CloseItemPopup) == 1, "UI close id drift");
static_assert(static_cast<int>(UiDirectTarget::CloseBag) == 2, "UI bag id drift");
static_assert(static_cast<int>(UiDirectTarget::CloseTrade) == 3, "UI trade id drift");
static_assert(static_cast<int>(UiDirectTarget::TradeRequestCancel) == 9, "UI cancel id drift");
static_assert(static_cast<int>(TravelSemantic::DenCacMonPhai) == 3, "KunLun semantic id drift");
static_assert(static_cast<int>(TravelSemantic::DiscardPopup) == 8, "filter discard id drift");
static_assert(static_cast<int>(TravelSemantic::DiscardConfirm) == 10, "filter confirm id drift");
static_assert(static_cast<int>(TravelSemantic::PutUpPopup) == 9, "trade put-up id drift");

int main() {
    constexpr std::array<std::uint32_t, 7> retired{{10, 11, 12, 13, 15, 16, 17}};
    constexpr std::array<Command, 24> active{{
        Command::None, Command::ReadState, Command::ToggleRide, Command::StartPath,
        Command::StopPath, Command::ClickNpc, Command::ConfirmMap, Command::Revive,
        Command::StartAutoFight, Command::StopAutoFight, Command::ClickInternalPoint,
        Command::ReadCurrency, Command::ReadBagPage, Command::DropBagItem,
        Command::SellBagItem, Command::SelectTargetByRoleID, Command::ClickTravelSemantic,
        Command::ConfirmTravelSemantic, Command::ClickInternalPointRawTest,
        Command::DragInternalPoint, Command::ProbeNearbyLoot, Command::PickNearestLoot,
        Command::ProbeUiDirect, Command::InvokeUiDirect
    }};
    for (const auto id : retired)
        for (const auto cmd : active)
            if (id == static_cast<std::uint32_t>(cmd)) {
                std::cerr << "T19 FAIL: retired opcode reused " << id << "\n";
                return 1;
            }

    if (cleanroute::IsLicenseProtectedCommand(Command::ReadState) ||
        cleanroute::IsLicenseProtectedCommand(Command::ReadBagPage) ||
        !cleanroute::IsLicenseProtectedCommand(Command::ClickInternalPoint) ||
        !cleanroute::IsLicenseProtectedCommand(Command::DropBagItem) ||
        !cleanroute::IsLicenseProtectedCommand(Command::StartPath)) {
        std::cerr << "T19 FAIL: bridge license command gate changed\n";
        return 1;
    }
    cleanroute::Request request{};
    request.command = static_cast<std::uint32_t>(Command::ClickInternalPoint);
    request.arg0 = 123;
    const auto a = cleanroute::LicenseRequestProof(0x1111222233334444ULL, 42, request);
    const auto b = cleanroute::LicenseRequestProof(0x1111222233334444ULL, 42, request);
    const auto c = cleanroute::LicenseRequestProof(0x1111222233334444ULL, 43, request);
    if (a == 0 || a != b || a == c) {
        std::cerr << "T19 FAIL: license proof determinism/sequence binding\n";
        return 1;
    }
    std::cout << "T19 protocol wire PASS: 24 active IDs, 7 retired holes, bridge request/license contract\n";
    return 0;
}
