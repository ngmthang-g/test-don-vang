#include "saved_hidden_click.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main() {
    using namespace saved_hidden_click;
    int x,y;
    assert(fixed_slot_sell_logic::kCoordinateScale==1000000);
    assert(Normalize({250,125,1000,500},1000,500,x,y)&&x==250000&&y==250000);
    assert(Normalize({250,125,1000,500},2000,1000,x,y)&&x==250000&&y==250000);
    assert(Normalize({2500,2500,10000,10000},1000,500,x,y)&&x==250000&&y==250000);
    // Real F8 logs, PID 5880, client 951x498.
    assert(Normalize({457,108,951,498},951,498,x,y)&&x==480546&&y==216867);
    assert(Normalize({450,111,951,498},951,498,x,y)&&x==473186&&y==222891);
    assert(!Normalize({-1,-1,0,0},1000,500,x,y));
    assert(!Normalize({1000,0,1000,500},1000,500,x,y));
    assert(!Normalize({12,30,1000,500},0,500,x,y));
    assert(Normalize({999,499,1000,500},1000,500,x,y)&&x==999000&&y==998000);
}
