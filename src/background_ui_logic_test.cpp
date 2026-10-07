#include "background_ui_logic.h"

#include <cassert>

using namespace background_ui_logic;

int main() {
    assert(Key(L"ĐẦU THAI") == L"dauthai");
    assert(Key(L"Bán vật phẩm nhanh") == L"banvatphamnhanh");

    Labels confirm{L"ButtonOK", L"Xác nhận", L"", L"Root/MessageBox", L""};
    Labels cancel{L"ButtonCancel", L"Hủy", L"", L"Root/MessageBox", L""};
    Labels agree{L"ButtonYes", L"Đồng ý", L"", L"Root/MessageBox", L""};
    Labels closeMap{L"ButtonClose", L"Đóng", L"", L"Root/MessageBox", L""};
    Labels unrelated{L"ButtonOK", L"Xác nhận", L"", L"ShopPanel", L""};
    assert(Score(confirm, Role::ConfirmMap) > 0);
    assert(Score(cancel, Role::ConfirmMap) < 0);
    assert(Score(agree, Role::ConfirmMap) > 0);
    assert(Score(closeMap, Role::ConfirmMap) < 0);
    assert(Score(unrelated, Role::ConfirmMap) == 0);

    Labels revive{L"BtnRevive", L"Đầu thai", L"", L"DeathPanel", L""};
    assert(Score(revive, Role::Revive) > 0);
    Labels nestedRevive{L"Button_12", L"", L"", L"DeathPanel", L"Label/Đầu thai"};
    assert(Score(nestedRevive, Role::Revive) > 0);

    Labels quickSell{L"QuickSell", L"Bán vật phẩm nhanh", L"", L"Shop/Bag", L""};
    assert(Score(quickSell, Role::QuickSell) > Score(quickSell, Role::SellTab));

    Labels mountShop{L"NpcFunction", L"Mua thú cưỡi", L"", L"Npc/Dialog", L""};
    Labels medicineShop{L"NpcFunction", L"Mua thuốc", L"", L"Npc/Dialog", L""};
    Labels genericShop{L"NpcFunction", L"Mua vật phẩm", L"", L"Npc/Dialog", L""};
    assert(Score(mountShop, Role::MountShopEntry) > 0);
    assert(Score(medicineShop, Role::MedicineShopEntry) > 0);
    assert(Score(medicineShop, Role::MountShopEntry) == 0);
    assert(Score(mountShop, Role::MedicineShopEntry) == 0);
    assert(Score(genericShop, Role::ShopEntry) > 0);

    Labels bagItem{L"BagItemCell_04", L"", L"OnItemClick", L"Bag/Equipment/ItemGrid", L""};
    Labels shopItem{L"ShopItemCell_04", L"", L"OnItemClick", L"Shop/ProductItemList", L""};
    assert(SafeBagItem(bagItem));
    assert(!SafeBagItem(shopItem));

    Labels treatment{L"BtnTreatment", L"Trị liệu", L"", L"Npc/TreatmentPanel", L""};
    Labels treatmentConfirm{L"ButtonOK", L"Xác nhận", L"", L"Npc/TreatmentPanel", L""};
    Labels treatmentAck{L"BtnKnow", L"Ta biết rồi", L"", L"Npc/TreatmentPanel", L""};
    assert(Score(treatment, Role::Treatment) > 0);
    assert(Score(treatmentConfirm, Role::TreatmentConfirm) > 0);
    assert(Score(treatmentAck, Role::TreatmentAck) > 0);

    Labels safeClose{L"BtnClose", L"", L"", L"Shop/BagPanel", L""};
    Labels unsafeClose{L"BtnClose", L"", L"", L"QuestPanel", L""};
    assert(Score(safeClose, Role::CloseTradeOrBag) > 0);
    assert(Score(unsafeClose, Role::CloseTradeOrBag) == 0);

    return 0;
}
