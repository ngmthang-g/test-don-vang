#include "sell_filter_grid_v2_logic.h"
#include <cassert>
#include <iostream>
int main(){using namespace sell_filter_grid_v2_logic;
assert(ValidGrid(0)&&ValidGrid(41)&&!ValidGrid(-1)&&!ValidGrid(42));
assert(ValidPosition(0)&&ValidPosition(99)&&!ValidPosition(-1)&&!ValidPosition(100));
assert(RequiredDepth(0)==0&&RequiredDepth(41)==0&&RequiredDepth(42)==1&&RequiredDepth(83)==1&&RequiredDepth(84)==2&&RequiredDepth(99)==2);
assert(PhysicalGrid(0)==0&&PhysicalGrid(41)==41&&PhysicalGrid(42)==0&&PhysicalGrid(83)==41&&PhysicalGrid(84)==21&&PhysicalGrid(99)==36);
assert(ResetSwipeCount(0)==0&&ResetSwipeCount(1)==1&&ResetSwipeCount(2)==2&&ResetSwipeCount(3)==-1);
assert(NewPositionFirstForDepth(0)==0&&NewPositionLastForDepth(0)==41);
assert(NewPositionFirstForDepth(1)==42&&NewPositionLastForDepth(1)==83);
assert(NewPositionFirstForDepth(2)==84&&NewPositionLastForDepth(2)==99);
assert(ReadyToResumeAfterSell(true,true,true));
assert(!ReadyToResumeAfterSell(false,true,true));
assert(!ReadyToResumeAfterSell(true,false,true));
assert(!ReadyToResumeAfterSell(true,true,false));
std::cout<<"sell_filter_grid_v2_logic_tests PASS\n";
}
