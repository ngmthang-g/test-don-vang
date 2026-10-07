#include "main_macro_sell_logic.h"
#include <cassert>
int main(){
    using namespace main_macro_sell_logic;
    assert(Eligible(0,true,false));
    assert(Eligible(99,true,false));
    assert(!Eligible(-1,true,false));
    assert(!Eligible(100,true,false));
    assert(!Eligible(10,false,false));
    assert(!Eligible(10,true,true));
    assert(ClampDelayMs(-1)==0);
    assert(ClampDelayMs(600)==600);
    assert(ClampDelayMs(70000)==60000);
    assert(!CapacityReady(8));
    assert(CapacityReady(9));
    return 0;
}
