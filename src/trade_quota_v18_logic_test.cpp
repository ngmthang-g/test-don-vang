#include "trade_quota_v18_logic.h"
#include <cassert>
#include <iostream>
using namespace trade_quota_v18_logic;
int main(){
    assert(EligibleEquipment(0,true,false));
    assert(EligibleEquipment(99,true,false));
    assert(!EligibleEquipment(-1,true,false));
    assert(!EligibleEquipment(100,true,false));
    assert(!EligibleEquipment(5,false,false));
    assert(!EligibleEquipment(5,true,true));

    auto m=MakeMainPlan(11,95,1);
    assert(m.valid&&m.passesTotal==10&&m.passesRemaining==10&&m.freeSlotsAtPlan==95&&m.epoch==1);
    auto c=MakeChildPlan(22,29);
    assert(c.valid&&c.passesTotal==3&&c.passesRemaining==3&&c.remainder==2);
    assert(CanRunFullPass(m,c));
    assert(ConsumeFullPass(m,c)); assert(m.passesRemaining==9&&c.passesRemaining==2);
    assert(ConsumeFullPass(m,c)); assert(m.passesRemaining==8&&c.passesRemaining==1);
    assert(ConsumeFullPass(m,c)); assert(m.passesRemaining==7&&c.passesRemaining==0);
    assert(!ConsumeFullPass(m,c));
    assert(ChildDone(c)); assert(!MainNeedsSell(m));

    auto c8=MakeChildPlan(33,8);
    assert(c8.valid&&c8.passesTotal==0&&c8.remainder==8&&ChildDone(c8));
    auto m8=MakeMainPlan(11,8,2);
    assert(m8.valid&&m8.passesRemaining==0&&MainNeedsSell(m8));
    auto m18=MakeMainPlan(11,18,3); assert(m18.passesRemaining==2);
    auto c27=MakeChildPlan(44,27); assert(c27.passesRemaining==3);
    assert(ConsumeFullPass(m18,c27));
    assert(ConsumeFullPass(m18,c27));
    assert(MainNeedsSell(m18)&&c27.passesRemaining==1);
    auto m93=MakeMainPlan(11,93,4); assert(m93.passesRemaining==10);
    assert(CanRunFullPass(m93,c27));
    assert(ConsumeFullPass(m93,c27));
    assert(c27.passesRemaining==0&&m93.passesRemaining==9);

    auto badM=MakeMainPlan(0,90,1); assert(!badM.valid);
    auto badF=MakeMainPlan(1,-1,1); assert(!badF.valid);
    auto badC=MakeChildPlan(0,90); assert(!badC.valid);
    std::cout<<"trade_quota_v18_logic_tests PASS\n";
}
