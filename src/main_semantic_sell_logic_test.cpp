#include "main_semantic_sell_logic.h"
#include <cassert>
#include <iostream>
int main(){using namespace main_semantic_sell_logic;
assert(NormalizeMode(0)==Mode::UnlockedWeaponsOnly);assert(NormalizeMode(1)==Mode::AllUnlockedEquipment);assert(NormalizeMode(99)==Mode::UnlockedWeaponsOnly);
assert(ClampMinimumIntervalMs(-1)==0&&ClampMinimumIntervalMs(50)==50&&ClampMinimumIntervalMs(70000)==60000);
assert(MinimumIntervalReady(1000,0,50));assert(MinimumIntervalReady(1000,999,0));assert(!MinimumIntervalReady(1049,1000,50));assert(MinimumIntervalReady(1050,1000,50));
assert(!Candidate(false,false,true,true,0,Mode::UnlockedWeaponsOnly));assert(!Candidate(true,true,true,true,0,Mode::UnlockedWeaponsOnly));assert(!Candidate(true,false,false,true,0,Mode::UnlockedWeaponsOnly));
assert(Candidate(true,false,true,false,-1,Mode::AllUnlockedEquipment));assert(Candidate(true,false,true,true,0,Mode::UnlockedWeaponsOnly));assert(!Candidate(true,false,true,false,-1,Mode::UnlockedWeaponsOnly));assert(!Candidate(true,false,true,true,2,Mode::UnlockedWeaponsOnly));
assert(CanFinishForTrade(9)&&CanFinishForTrade(12)&&!CanFinishForTrade(8));assert(NeedsCapacitySell(8)&&NeedsCapacitySell(0)&&!NeedsCapacitySell(9));
assert(static_cast<int>(Phase::ResetSwipe)>static_cast<int>(Phase::AwaitSellGone));
std::cout<<"main_semantic_sell_logic_tests PASS\n";}
