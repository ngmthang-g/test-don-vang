#pragma once
#include <algorithm>
#include <cstdint>
namespace main_semantic_sell_logic {
enum class Mode : int { UnlockedWeaponsOnly=0, AllUnlockedEquipment=1 };
enum class Phase : int { Idle=0, ScanBag, Swipe, ClickItem, AwaitSellPopup, AwaitSellGone, ResetSwipe, Blocked };
inline Mode NormalizeMode(int raw){return raw==static_cast<int>(Mode::AllUnlockedEquipment)?Mode::AllUnlockedEquipment:Mode::UnlockedWeaponsOnly;}
inline int ClampMinimumIntervalMs(int value){return std::clamp(value,0,60000);}
inline bool MinimumIntervalReady(std::uint64_t now,std::uint64_t lastActionAt,int minimumMs){
    const auto min=static_cast<std::uint64_t>(ClampMinimumIntervalMs(minimumMs));
    if(lastActionAt==0||min==0)return true;
    return now>=lastActionAt+min;
}
inline bool Candidate(bool validPosition,bool bound,bool isEquip,bool equipPointKnown,int equipPoint,Mode mode){
    if(!validPosition||bound||!isEquip)return false;
    if(mode==Mode::AllUnlockedEquipment)return true;
    return equipPointKnown&&equipPoint==0;
}
inline bool CanFinishForTrade(int freeBagSpace){return freeBagSpace>=9;}
inline bool NeedsCapacitySell(int freeBagSpace){return freeBagSpace>=0&&freeBagSpace<9;}
}
