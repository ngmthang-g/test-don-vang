#pragma once

#include <windows.h>
#include <cstddef>
#include <string>
#include <vector>
#include "protocol.h"

namespace image_scan_test {

using HiddenClickFn = bool (*)(void* context,
                               int clientX, int clientY,
                               int clientWidth, int clientHeight,
                               std::wstring& detail);
using HiddenDragFn = bool (*)(void* context,
                              int startClientX, int startClientY,
                              int endClientX, int endClientY,
                              int clientWidth, int clientHeight,
                              std::wstring& detail);
using ReadDropCandidatesFn = bool (*)(void* accountContext, void* bagContext,
                                      std::vector<int>& positions, int& freeBagSpace,
                                      std::wstring& detail);
using SemanticDiscardFn = bool (*)(void* accountContext, bool probeOnly, std::wstring& detail);
using SemanticDiscardConfirmFn = bool (*)(void* accountContext, bool probeOnly, std::wstring& detail);
// -1=bridge/error/ambiguous (fail-closed), 0=target not present, 1=found/invoked.
using UiDirectFn = int (*)(void* accountContext, cleanroute::UiDirectTarget target, bool invoke, std::wstring& detail);

enum class FilterMode { V4 = 1, SemanticBag = 2 };

struct Target {
    HWND owner = nullptr;
    HWND gameWindow = nullptr;
    std::wstring accountLabel;
    HiddenClickFn hiddenClick = nullptr;
    HiddenDragFn hiddenDrag = nullptr;
    ReadDropCandidatesFn readDropCandidates = nullptr;
    SemanticDiscardFn semanticDiscard = nullptr;
    SemanticDiscardConfirmFn semanticDiscardConfirm = nullptr;
    UiDirectFn uiDirect = nullptr;
    void* context = nullptr;
    void* bagContext = nullptr;
};

// Modal FILTER V4 settings/test dialog. It never foregrounds the game window.
void RunDialog(const Target& target);

enum class AutoFilterState {
    Disabled,
    Idle,
    Busy,
    WaitingTurn,
    WaitingEmpty,
    CompletedUntilFull,
    FullYieldReady,
    Error,
};

struct AutoFilterResult {
    AutoFilterState state = AutoFilterState::Idle;
    bool ownsInput = false;
    bool fullYieldReady = true;
    std::size_t slotNumber = 0; // 1-based, 0 when no slot is active.
    std::wstring status;
};

// Scan settings are auto-loaded/saved in LOCALAPPDATA; export/import remains manual backup.
void EnsurePersistentConfigLoaded();
void SavePersistentConfig();

// CON1..CON30 enable switches live in the shared FILTER settings/config.
bool IsChildAutoFilterEnabled(int childSlot);
void SetAutoFilterMode(FilterMode mode);
FilterMode GetAutoFilterMode();

// Background FILTER V4 tick. Call only after the CON has reached its train spot and
// initial AutoFight startup has succeeded. freeBagSpace==0 is passed as bagFull.
AutoFilterResult TickAutoFilter(const Target& target, int childSlot, bool bagFull);

// FULL is latched once seen so discarding the final bad item cannot cancel travel.
bool FullBagTravelLatched(HWND gameWindow, int childSlot);

// FULL-bag coordinator gate: false only while FILTER is finishing the current item
// and/or closing X popup item + X Tay nải through UI DIRECT.
bool FullBagYieldReady(HWND gameWindow, int childSlot);

// Death owns priority. NotifyDeath aborts scan immediately without closing the bag;
// after the existing revive callback succeeds, NotifyReviveClicked performs best-effort
// UI DIRECT cleanup (X popup item + X Tay nải) and resets scan.
void NotifyDeath(HWND gameWindow, int childSlot);
void NotifyReviveClicked(const Target& target, int childSlot);
// Entering CON→MAIN WorldFlow invalidates the old scrolled-bag cursor.
void NotifyTradeFlowStarted(HWND gameWindow, int childSlot);

// START/STOP session boundaries.
void ResetAutoFilter(HWND gameWindow, int childSlot);
void StopAutoFilter(HWND gameWindow, int childSlot);

} // namespace image_scan_test
