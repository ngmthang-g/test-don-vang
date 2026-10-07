#include "unity_geometry_logic.h"

#include <cassert>

using namespace unity_geometry_logic;

int main() {
    const SearchPlan rect = PlanFor(GeometryClass::RectTransform);
    assert(rect.images[0] == ImageSlot::CoreModule);
    assert(rect.images[1] == ImageSlot::LegacyUnity);

    const SearchPlan utility = PlanFor(GeometryClass::RectTransformUtility);
    assert(utility.images[0] == ImageSlot::UiModule);
    assert(utility.images[1] == ImageSlot::CoreModule);
    assert(utility.images[2] == ImageSlot::LegacyUnity);

    const SearchPlan screen = PlanFor(GeometryClass::Screen);
    assert(screen.images[0] == ImageSlot::CoreModule);
    assert(screen.images[1] == ImageSlot::LegacyUnity);

    assert(PlanFor(GeometryClass::GameObject).className[0] == 'G');
    assert(ClassLabel(GeometryClass::RectTransformUtility)[0] == L'R');
    return 0;
}
