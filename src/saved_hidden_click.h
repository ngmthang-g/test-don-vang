#pragma once
#include <cstdint>
#include "fixed_slot_sell_logic.h"
namespace saved_hidden_click {
// Keep original 10.6 ClickPoint: client pixels and client dimensions at F8.
struct Point {
    int x=-1, y=-1, baseW=0, baseH=0;
    bool Valid() const { return x>=0 && y>=0 && baseW>0 && baseH>0 && x<baseW && y<baseH; }
};
inline bool Normalize(const Point& saved, int clientW,int clientH,int& outX,int& outY) {
    outX=outY=-1;
    if(!saved.Valid()||clientW<=0||clientH<=0)return false;
    // Same rounded pixel resize as Win32 MulDiv in version 10.6.
    const auto scaledX=(static_cast<std::int64_t>(saved.x)*clientW+saved.baseW/2)/saved.baseW;
    const auto scaledY=(static_cast<std::int64_t>(saved.y)*clientH+saved.baseH/2)/saved.baseH;
    if(scaledX<0||scaledX>=clientW||scaledY<0||scaledY>=clientH)return false;
    // CRITICAL: Bridge decodes using 10.6 kCoordinateScale=1,000,000, NOT 10,000.
    outX=fixed_slot_sell_logic::NormalizeClientCoordinate(static_cast<int>(scaledX),clientW);
    outY=fixed_slot_sell_logic::NormalizeClientCoordinate(static_cast<int>(scaledY),clientH);
    return outX>=0&&outX<fixed_slot_sell_logic::kCoordinateScale &&
           outY>=0&&outY<fixed_slot_sell_logic::kCoordinateScale;
}
} // namespace saved_hidden_click
