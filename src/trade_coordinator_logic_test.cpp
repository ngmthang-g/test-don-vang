#include "trade_coordinator_logic.h"
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace itemtrade_coordinator;

int main() {
    // Base pass rule: MAIN delta <=8 finishes, >8 repeats the same active CON.
    assert(kReceivedSlotsFinishThreshold == 8);
    assert(kTradePassMaxItems == 9);
    assert(kMaxTravelingChildren == 4);
    assert(ReceivedSlots(40, 32) == 8);
    assert(ReceivedSlots(40, 31) == 9);
    assert(ReceivedSlots(31, 22) == 9);
    assert(ReceivedSlots(20, 12) == 8);
    assert(ReceivedSlots(20, 11) == 9);
    assert(ReceivedSlots(40, 40) == 0);
    assert(ReceivedSlots(40, 42) == 0);
    assert(!HasConclusivePostPassBagSnapshot(40, 40));
    assert(HasConclusivePostPassBagSnapshot(40, 39));
    assert(HasConclusivePostPassBagSnapshot(40, 42));
    assert(!CanFinalizeUnchangedPostPassBagSnapshot(40, 40, false));
    assert(CanFinalizeUnchangedPostPassBagSnapshot(40, 40, true));
    assert(!CanFinalizeUnchangedPostPassBagSnapshot(-1, -1, true));
    assert(!CanFinalizeUnchangedPostPassBagSnapshot(40, 39, true));
    assert(DecidePass(40, 32) == PassDecision::FinishChild);
    assert(DecidePass(20, 12) == PassDecision::FinishChild);
    assert(DecidePass(40, 31) == PassDecision::RepeatSameChild);
    assert(DecidePass(31, 22) == PassDecision::RepeatSameChild);
    assert(DecidePass(20, 11) == PassDecision::RepeatSameChild);

    // Approved MAIN policy: <9 free slots is a trade-capacity pause. The same
    // CON/ticket/slot is retained; >=9 is required before a new pass resumes.
    assert(DecidePostPass(40, 31) == PostPassAction::RepeatSameChild);
    assert(DecidePostPass(20, 12) == PostPassAction::FinishChild);
    assert(DecidePostPass(15, 9) == PostPassAction::FinishChild);
    assert(DecidePostPass(12, 8) == PostPassAction::SellPauseSameChild);
    assert(DecidePostPass(8, 1) == PostPassAction::SellPauseSameChild);
    assert(DecidePostPass(8, 0) == PostPassAction::SellPauseSameChild);
    assert(DecidePostPass(9, 0) == PostPassAction::SellPauseSameChild);

    assert(!CanStartTradePass(0));
    assert(!CanStartTradePass(1));
    assert(!CanStartTradePass(8));
    assert(CanStartTradePass(9));
    assert(CanStartTradePass(30));
    assert(!CanStartTradePass(-1));
    assert(MainNeedsCapacitySell(0));
    assert(MainNeedsCapacitySell(1));
    assert(MainNeedsCapacitySell(8));
    assert(!MainNeedsCapacitySell(9));
    assert(!MainNeedsCapacitySell(30));
    assert(!MainNeedsCapacitySell(-1));
    assert(MainBagIsFull(0));
    assert(!MainBagIsFull(1));

    // FIFO behavior is unchanged.
    assert(EarlierWorkflowEntry(1, 3, 2, 1));
    assert(!EarlierWorkflowEntry(2, 1, 1, 3));
    assert(EarlierWorkflowEntry(10, 1, 10, 2));
    assert(!EarlierWorkflowEntry(10, 2, 10, 1));

    // FULL is admission-only for CON; repeat logic never consults CON fullness again.
    assert(ShouldAdmitFullChild(true, 0, 0));
    assert(ShouldAdmitFullChild(true, 0, 3));
    assert(!ShouldAdmitFullChild(true, 1, 0));
    assert(!ShouldAdmitFullChild(true, 0, 4));
    assert(!ShouldAdmitFullChild(false, 0, 0));
    assert(ShouldAssignArrivalTicket(true, false));
    assert(!ShouldAssignArrivalTicket(false, false));
    assert(!ShouldAssignArrivalTicket(true, true));

    // T18 golden role/capacity regression: NONE and CON never start auto seller.
    assert(!ShouldAutoSell(false, 0, true, 0));
    assert(!ShouldAutoSell(true, 0, false, 0));
    assert(!ShouldAutoSell(true, 0, true, 0));
    assert(!ShouldAutoSell(true, 0, true, 1));
    assert(!ShouldAutoSell(true, 2, true, 0));
    assert(!ShouldAutoSell(true, 31, true, 0));
    assert(!ShouldAutoSell(false, 1, true, 0));
    assert(!ShouldAutoSell(true, 1, false, 0));
    assert(!ShouldAutoSell(true, 1, true, -1));
    assert(ShouldAutoSell(true, 1, true, 0));
    assert(ShouldAutoSell(true, 1, true, 8));
    assert(!ShouldAutoSell(true, 1, true, 9));
    assert(!ShouldAutoSell(true, 1, true, 30));

    // This explicit runtime check remains enforced in Release /DNDEBUG builds.
    if (ShouldAutoSell(true, 0, true, 0) ||
        ShouldAutoSell(false, 1, true, 0) ||
        ShouldAutoSell(true, 2, true, 0) ||
        !ShouldAutoSell(true, 1, true, 8) ||
        ShouldAutoSell(true, 1, true, 9) ||
        !MainNeedsCapacitySell(8) || MainNeedsCapacitySell(9) ||
        !CanStartTradePass(9) || CanStartTradePass(8)) {
        std::cerr << "T18 FAIL: NONE/CON must never auto-sell; MAIN quota must remain 9" << std::endl;
        return 1;
    }
    std::cout << "trade_coordinator_logic_tests PASS\n";
    return 0;
}
