#pragma once

#include <cwctype>
#include <initializer_list>
#include <string>

namespace background_ui_logic {

enum class Role {
    ConfirmMap,
    Revive,
    ShopEntry,
    MountShopEntry,
    MedicineShopEntry,
    SellTab,
    QuickSell,
    EquipmentTab,
    Treatment,
    TreatmentConfirm,
    TreatmentAck,
    CloseTradeOrBag,
};

struct Labels {
    std::wstring name;
    std::wstring text;
    std::wstring handler;
    std::wstring ancestors;
    std::wstring descendants;
};

inline wchar_t FoldVietnamese(wchar_t c) {
    c = static_cast<wchar_t>(std::towlower(c));
    switch (c) {
        case L'À': case L'Á': case L'Ả': case L'Ã': case L'Ạ':
        case L'Ă': case L'Ằ': case L'Ắ': case L'Ẳ': case L'Ẵ': case L'Ặ':
        case L'Â': case L'Ầ': case L'Ấ': case L'Ẩ': case L'Ẫ': case L'Ậ':
        case L'à': case L'á': case L'ả': case L'ã': case L'ạ':
        case L'ă': case L'ằ': case L'ắ': case L'ẳ': case L'ẵ': case L'ặ':
        case L'â': case L'ầ': case L'ấ': case L'ẩ': case L'ẫ': case L'ậ': return L'a';
        case L'Đ': case L'đ': return L'd';
        case L'È': case L'É': case L'Ẻ': case L'Ẽ': case L'Ẹ':
        case L'Ê': case L'Ề': case L'Ế': case L'Ể': case L'Ễ': case L'Ệ':
        case L'è': case L'é': case L'ẻ': case L'ẽ': case L'ẹ':
        case L'ê': case L'ề': case L'ế': case L'ể': case L'ễ': case L'ệ': return L'e';
        case L'Ì': case L'Í': case L'Ỉ': case L'Ĩ': case L'Ị':
        case L'ì': case L'í': case L'ỉ': case L'ĩ': case L'ị': return L'i';
        case L'Ò': case L'Ó': case L'Ỏ': case L'Õ': case L'Ọ':
        case L'Ô': case L'Ồ': case L'Ố': case L'Ổ': case L'Ỗ': case L'Ộ':
        case L'Ơ': case L'Ờ': case L'Ớ': case L'Ở': case L'Ỡ': case L'Ợ':
        case L'ò': case L'ó': case L'ỏ': case L'õ': case L'ọ':
        case L'ô': case L'ồ': case L'ố': case L'ổ': case L'ỗ': case L'ộ':
        case L'ơ': case L'ờ': case L'ớ': case L'ở': case L'ỡ': case L'ợ': return L'o';
        case L'Ù': case L'Ú': case L'Ủ': case L'Ũ': case L'Ụ':
        case L'Ư': case L'Ừ': case L'Ứ': case L'Ử': case L'Ữ': case L'Ự':
        case L'ù': case L'ú': case L'ủ': case L'ũ': case L'ụ':
        case L'ư': case L'ừ': case L'ứ': case L'ử': case L'ữ': case L'ự': return L'u';
        case L'Ỳ': case L'Ý': case L'Ỷ': case L'Ỹ': case L'Ỵ':
        case L'ỳ': case L'ý': case L'ỷ': case L'ỹ': case L'ỵ': return L'y';
        default: return c;
    }
}

inline std::wstring Key(const std::wstring& input) {
    std::wstring output;
    output.reserve(input.size());
    for (wchar_t c : input) {
        const wchar_t folded = FoldVietnamese(c);
        if (std::iswalnum(folded)) output.push_back(folded);
    }
    return output;
}

inline bool Has(const std::wstring& key,
                std::initializer_list<const wchar_t*> needles) {
    for (const wchar_t* needle : needles) {
        if (needle && *needle && key.find(needle) != std::wstring::npos) return true;
    }
    return false;
}

inline int Score(const Labels& labels, Role role) {
    const std::wstring name = Key(labels.name);
    const std::wstring text = Key(labels.text);
    const std::wstring handler = Key(labels.handler);
    const std::wstring parents = Key(labels.ancestors);
    const std::wstring all = name + text + handler + parents + Key(labels.descendants);

    switch (role) {
        case Role::ConfirmMap: {
            const bool messageBox = Has(parents + name, {L"messagebox", L"messagedialog", L"uimessagebox"});
            if (!messageBox) return 0;
            const bool exactNegative = text == L"huy" || text == L"khong" || text == L"cancel" || text == L"dong" || text == L"close" ||
                                       name == L"huy" || name == L"khong" || name == L"cancel" || name == L"dong" || name == L"close";
            if (exactNegative || Has(name + handler, {L"buttonno", L"btnno", L"buttoncancel", L"btncancel", L"buttonclose", L"btnclose"})) return -1000;
            if (text == L"xacnhan" || text == L"dongy" || text == L"ok" || text == L"yes" || text == L"co") return 900;
            if (Has(all, {L"xacnhan", L"dongy", L"confirm", L"buttonok", L"btnok", L"buttonyes", L"btnyes"})) return 760;
            return 0;
        }
        case Role::Revive:
            if (text == L"dauthai" || name == L"dauthai") return 900;
            if (Has(all, {L"dauthai", L"buttonrevive", L"btnrevive"}) &&
                !Has(all, {L"cancel", L"close", L"huy", L"thoat"})) return 720;
            return 0;
        case Role::MountShopEntry:
            // Mã Kiêu Minh (ResID 373): the vendor-entry callback is specifically
            // "Mua thú cưỡi". Keep this strict so another NPC-function button cannot win.
            if (text == L"muathucuoi") return 980;
            return Has(all, {L"muathucuoi", L"mountshop", L"buymount"}) ? 780 : 0;
        case Role::MedicineShopEntry:
            // Ba Nhĩ (ResID 328) and Dược Đại Phu (ResID 279): the vendor-entry
            // callback is specifically "Mua thuốc". Do not accept a generic function row.
            if (text == L"muathuoc") return 980;
            return Has(all, {L"muathuoc", L"medicineshop", L"buymedicine"}) ? 780 : 0;
        case Role::ShopEntry:
            // Generic fallback retained for the other existing vendor presets only.
            if (text == L"muavatpham" || text == L"muaduocpham" || text == L"muathuocpham" ||
                text == L"muataphoa" || text == L"muahanghoa" || text == L"muathuongpham" ||
                text == L"muanhuyeupham" || text == L"cuahang") return 920;
            return Has(all, {L"muavatpham", L"muaduocpham", L"muathuocpham", L"muataphoa",
                             L"muahanghoa", L"muathuongpham", L"muanhuyeupham", L"shopentry",
                             L"openshop", L"openstore"}) ? 740 : 0;
        case Role::SellTab:
            if (text == L"banvatpham") return 900;
            if (Has(all, {L"banvatphamnhanh", L"quicksell"})) return -1000;
            return Has(all, {L"banvatpham", L"sellitem", L"itemsell", L"selltab", L"tabsell"}) ? 700 : 0;
        case Role::QuickSell:
            if (text == L"banvatphamnhanh") return 920;
            return Has(all, {L"banvatphamnhanh", L"quicksell", L"fastsell", L"sellquick"}) ? 740 : 0;
        case Role::EquipmentTab:
            if (text == L"trangbi") return 920;
            if (Has(all, {L"muavatpham", L"buyitem", L"shopitem"})) return -1000;
            return Has(all, {L"trangbi", L"equipmenttab", L"bagequipment", L"equiptab", L"tabquip"}) ? 730 : 0;
        case Role::Treatment:
            if (text == L"trilieu" || text == L"trithuong" || text == L"hoiphuc") return 920;
            return Has(all, {L"trilieu", L"trithuong", L"hoiphuc", L"treatment", L"recoverhp"}) ? 720 : 0;
        case Role::TreatmentConfirm:
            if (text == L"xacnhan") return 920;
            if (Has(all, {L"suynghi", L"cancel", L"huy", L"khong"})) return -1000;
            return Has(all, {L"xacnhan", L"confirm", L"buttonok", L"btnok"}) ? 720 : 0;
        case Role::TreatmentAck:
            if (text == L"tabietroi") return 940;
            return Has(all, {L"tabietroi", L"iknow", L"understand", L"gotit"}) ? 740 : 0;
        case Role::CloseTradeOrBag: {
            const bool safeParent = Has(parents, {L"shop", L"store", L"sell", L"trade", L"npc", L"business", L"bag", L"inventory", L"package", L"itempack", L"pack"});
            if (!safeParent) return 0;
            const int contextBonus = Has(parents, {L"bag", L"inventory", L"package", L"itempack"})
                ? 80 : (Has(parents, {L"shop", L"store", L"sell"}) ? 40 : 0);
            if (text == L"dong" || text == L"close" || text == L"x") return 820 + contextBonus;
            return Has(all, {L"btnclose", L"buttonclose", L"closebutton", L"shopclose", L"storeclose", L"bagclose", L"inventoryclose", L"packageclose"}) ? 700 + contextBonus : 0;
        }
    }
    return 0;
}

inline bool SafeBagItem(const Labels& labels) {
    const std::wstring name = Key(labels.name);
    const std::wstring parents = Key(labels.ancestors);
    const std::wstring combined = name + parents;
    if (!Has(name, {L"item", L"slot", L"cell", L"griditem", L"bagitem", L"packitem"})) return false;
    if (!Has(parents, {L"bag", L"inventory", L"package", L"itempack", L"packitem", L"bagitem", L"itemgrid", L"itemlist"})) return false;
    return !Has(combined, {L"buyitem", L"productitem", L"shoplistitem", L"npcitem"});
}

} // namespace background_ui_logic
