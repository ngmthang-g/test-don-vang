#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace itemtrade_coordinator {

// v9.9 customer build: at most four FULL children may leave their train spots.
// A child receives its FIFO ticket only after it is physically parked at the
// trade rendezvous; travelling children do not have queue priority yet.
constexpr std::size_t kMaxTravelingChildren = 4;
constexpr std::size_t kMaxQueuedChildren = kMaxTravelingChildren;
constexpr int kReceivedSlotsFinishThreshold = 8;
constexpr int kTradePassMaxItems = kReceivedSlotsFinishThreshold + 1; // 9

enum class PassDecision { FinishChild, RepeatSameChild };
enum class PostPassAction { SellPauseSameChild, FinishChild, RepeatSameChild };

// A new trade pass is allowed only when MAIN has room for the full 9-item pass.
// This is intentionally capacity-first: 1..8 free slots are not enough.
inline bool CanStartTradePass(int freeBagSpace) {
    return freeBagSpace >= kTradePassMaxItems;
}

inline bool MainNeedsCapacitySell(int freeBagSpace) {
    return freeBagSpace >= 0 && freeBagSpace < kTradePassMaxItems;
}

// Kept for exact 0-free diagnostics only. Trade scheduling must use
// MainNeedsCapacitySell/CanStartTradePass, never this predicate.
inline bool MainBagIsFull(int freeBagSpace) {
    return freeBagSpace == 0;
}

inline int ReceivedSlots(int beforeFree, int afterFree) {
    if (beforeFree < 0 || afterFree < 0) return 0;
    return std::max(0, beforeFree - afterFree);
}

// A changed valid snapshot is conclusive before timeout. X→X remains
// ambiguous only while the caller's bounded verification window is open.
inline bool HasConclusivePostPassBagSnapshot(int beforeFree, int afterFree) {
    return beforeFree >= 0 && afterFree >= 0 && beforeFree != afterFree;
}

inline bool CanFinalizeUnchangedPostPassBagSnapshot(int beforeFree, int afterFree,
                                                    bool verifyTimedOut) {
    return verifyTimedOut && beforeFree >= 0 && afterFree >= 0 && beforeFree == afterFree;
}

inline PassDecision DecidePass(int beforeFree, int afterFree) {
    return ReceivedSlots(beforeFree, afterFree) <= kReceivedSlotsFinishThreshold
        ? PassDecision::FinishChild
        : PassDecision::RepeatSameChild;
}

// Capacity wins over a short delta. If MAIN ends a completed pass with fewer
// than 9 free slots, retain the same CON/ticket/slot, run the configured sell
// batch, and only resume after MAIN is back to >=9 free slots.
inline PostPassAction DecidePostPass(int beforeFree, int afterFree) {
    if (MainNeedsCapacitySell(afterFree)) return PostPassAction::SellPauseSameChild;
    return DecidePass(beforeFree, afterFree) == PassDecision::RepeatSameChild
        ? PostPassAction::RepeatSameChild
        : PostPassAction::FinishChild;
}

inline bool EarlierWorkflowEntry(std::uint64_t aEntry, int aSlot,
                                 std::uint64_t bEntry, int bSlot) {
    if (aEntry == 0) return false;
    if (bEntry == 0) return true;
    if (aEntry != bEntry) return aEntry < bEntry;
    return aSlot < bSlot;
}

inline bool ShouldAdmitFullChild(bool stateReady, int freeBagSpace, std::size_t queuedCount) {
    return stateReady && freeBagSpace == 0 && queuedCount < kMaxTravelingChildren;
}

inline bool ShouldAssignArrivalTicket(bool arrivedAtRendezvous, bool alreadyQueued) {
    return arrivedAtRendezvous && !alreadyQueued;
}

// T18: role NONE and CON have no autonomous seller. MAIN only.
// MAIN idle sell uses its own timer; quota capacity below nine reserves
// a MAIN-only sell batch while CON trade is waiting/active.
inline bool ShouldAutoSell(bool consolidationEnabled, int tradeRole, bool enableSell,
                           int freeBagSpace) {
    return consolidationEnabled && tradeRole == 1 && enableSell &&
           MainNeedsCapacitySell(freeBagSpace);
}

} // namespace itemtrade_coordinator
