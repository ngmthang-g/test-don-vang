#include "telegram_account_filter.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cwctype>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace telegram_account_filter {
namespace {

constexpr int kMainTabId = 207;
constexpr int kClientListId = 100;
constexpr int kTelegramLogId = 509;
constexpr int kFilterCaptionId = 7500;
constexpr int kFilterListId = 7501;
constexpr int kTelegramTabIndex = 3;

std::mutex g_mutex;
std::map<std::wstring, bool> g_selectedByName;
std::vector<std::wstring> g_rowKeys;
std::vector<std::wstring> g_rowLabels;

HWND g_main = nullptr;
HWND g_caption = nullptr;
HWND g_list = nullptr;
RECT g_originalLogRect{};
bool g_haveOriginalLogRect = false;
UINT_PTR g_timerId = 0;

std::wstring Trim(std::wstring value) {
    auto notSpace = [](wchar_t ch) { return !std::iswspace(static_cast<wint_t>(ch)); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::wstring ItemText(HWND list, int row, int subItem) {
    wchar_t buffer[512]{};
    LVITEMW item{};
    item.iSubItem = subItem;
    item.pszText = buffer;
    item.cchTextMax = static_cast<int>(std::size(buffer));
    SendMessageW(list, LVM_GETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&item));
    return buffer;
}

std::wstring PlainNameFromDisplay(const std::wstring& displayName) {
    const std::size_t bullet = displayName.find(L" • ");
    std::wstring name = bullet == std::wstring::npos ? displayName : displayName.substr(0, bullet);
    name = Trim(std::move(name));
    if (name.empty() || name == L"?") return {};
    return name;
}

std::wstring RequestAccountName(const std::wstring& account) {
    if (account.empty() || account == L"-") return {};
    const std::size_t bullet = account.rfind(L" • ");
    return Trim(bullet == std::wstring::npos ? account : account.substr(bullet + 3));
}

BOOL CALLBACK FindHostProc(HWND hwnd, LPARAM parameter) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!GetDlgItem(hwnd, kMainTabId) || !GetDlgItem(hwnd, kClientListId) || !GetDlgItem(hwnd, kTelegramLogId)) return TRUE;
    *reinterpret_cast<HWND*>(parameter) = hwnd;
    return FALSE;
}

HWND FindHostWindow() {
    HWND found = nullptr;
    EnumWindows(&FindHostProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

void CaptureCurrentChecks() {
    if (!g_list) return;
    const int rows = ListView_GetItemCount(g_list);
    std::lock_guard<std::mutex> lock(g_mutex);
    const int limit = std::min<int>(rows, static_cast<int>(g_rowKeys.size()));
    for (int i = 0; i < limit; ++i) {
        if (!g_rowKeys[static_cast<std::size_t>(i)].empty())
            g_selectedByName[g_rowKeys[static_cast<std::size_t>(i)]] = ListView_GetCheckState(g_list, i) != FALSE;
    }
}

void UpdateCaption() {
    if (!g_caption) return;
    std::size_t selected = 0;
    std::size_t total = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        total = g_rowKeys.size();
        for (const auto& key : g_rowKeys) {
            const auto it = g_selectedByName.find(key);
            if (it != g_selectedByName.end() && it->second) ++selected;
        }
    }
    const std::wstring text = L"ACC BÁO CÁO • đã tick " + std::to_wstring(selected) + L"/" + std::to_wstring(total) +
                              L" • không tick = không gửi";
    SetWindowTextW(g_caption, text.c_str());
}

void RebuildRowsIfNeeded() {
    if (!g_main || !g_list) return;
    HWND source = GetDlgItem(g_main, kClientListId);
    if (!source) return;

    CaptureCurrentChecks();

    std::vector<std::wstring> keys;
    std::vector<std::wstring> labels;
    const int count = ListView_GetItemCount(source);
    keys.reserve(static_cast<std::size_t>(std::max(count, 0)));
    labels.reserve(keys.capacity());

    for (int row = 0; row < count; ++row) {
        const std::wstring displayName = ItemText(source, row, 0);
        const std::wstring name = PlainNameFromDisplay(displayName);
        if (name.empty()) continue;
        std::wstring role = Trim(ItemText(source, row, 1));
        if (role.empty() || role == L"-") role = L"ACC";
        keys.push_back(name);
        labels.push_back(role + L" • " + name);
    }

    if (keys == g_rowKeys && labels == g_rowLabels) {
        UpdateCaption();
        return;
    }

    g_rowKeys = std::move(keys);
    g_rowLabels = std::move(labels);
    ListView_DeleteAllItems(g_list);

    for (int row = 0; row < static_cast<int>(g_rowKeys.size()); ++row) {
        const std::wstring& key = g_rowKeys[static_cast<std::size_t>(row)];
        const std::wstring& label = g_rowLabels[static_cast<std::size_t>(row)];
        bool checked = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto [it, inserted] = g_selectedByName.emplace(key, false);
            (void)inserted;
            checked = it->second;
        }

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(label.c_str());
        ListView_InsertItem(g_list, &item);
        ListView_SetCheckState(g_list, row, checked ? TRUE : FALSE);
    }
    UpdateCaption();
}

void EnsureControls() {
    if (!g_main || !IsWindow(g_main)) return;
    if (g_list && IsWindow(g_list) && g_caption && IsWindow(g_caption)) return;

    HWND telegramLog = GetDlgItem(g_main, kTelegramLogId);
    if (!telegramLog) return;

    if (!g_haveOriginalLogRect) {
        GetWindowRect(telegramLog, &g_originalLogRect);
        MapWindowPoints(HWND_DESKTOP, g_main, reinterpret_cast<POINT*>(&g_originalLogRect), 2);
        g_haveOriginalLogRect = true;
    }

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(GetDlgItem(g_main, kClientListId), WM_GETFONT, 0, 0));
    g_caption = CreateWindowExW(0, L"STATIC", L"ACC BÁO CÁO • không tick = không gửi",
                                WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
                                0, 0, 10, 10, g_main,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFilterCaptionId)),
                                GetModuleHandleW(nullptr), nullptr);
    g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                             WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
                             0, 0, 10, 10, g_main,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFilterListId)),
                             GetModuleHandleW(nullptr), nullptr);
    if (font) {
        SendMessageW(g_caption, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    if (g_list) {
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(L"Tài khoản được phép báo cáo");
        column.cx = 270;
        column.iSubItem = 0;
        ListView_InsertColumn(g_list, 0, &column);
    }
}

void LayoutAndVisibility() {
    if (!g_main || !g_haveOriginalLogRect) return;
    HWND tab = GetDlgItem(g_main, kMainTabId);
    HWND telegramLog = GetDlgItem(g_main, kTelegramLogId);
    if (!tab || !telegramLog) return;

    const bool telegramVisible = TabCtrl_GetCurSel(tab) == kTelegramTabIndex;
    if (!telegramVisible) {
        if (g_caption) ShowWindow(g_caption, SW_HIDE);
        if (g_list) ShowWindow(g_list, SW_HIDE);
        const int w = g_originalLogRect.right - g_originalLogRect.left;
        const int h = g_originalLogRect.bottom - g_originalLogRect.top;
        MoveWindow(telegramLog, g_originalLogRect.left, g_originalLogRect.top, w, h, TRUE);
        return;
    }

    const int fullWidth = g_originalLogRect.right - g_originalLogRect.left;
    const int fullHeight = g_originalLogRect.bottom - g_originalLogRect.top;
    const int sidebarWidth = std::clamp(fullWidth / 3, 250, 300);
    const int gap = 10;
    const int logWidth = std::max(300, fullWidth - sidebarWidth - gap);
    const int sideX = g_originalLogRect.left + logWidth + gap;

    MoveWindow(telegramLog, g_originalLogRect.left, g_originalLogRect.top, logWidth, fullHeight, TRUE);
    if (g_caption) {
        MoveWindow(g_caption, sideX, g_originalLogRect.top, sidebarWidth, 24, TRUE);
        ShowWindow(g_caption, SW_SHOW);
    }
    if (g_list) {
        MoveWindow(g_list, sideX, g_originalLogRect.top + 26, sidebarWidth, std::max(80, fullHeight - 26), TRUE);
        ShowWindow(g_list, SW_SHOW);
    }
}

void ResetHost() {
    g_main = nullptr;
    g_caption = nullptr;
    g_list = nullptr;
    g_haveOriginalLogRect = false;
    g_rowKeys.clear();
    g_rowLabels.clear();
}

VOID CALLBACK Tick(HWND, UINT, UINT_PTR, DWORD) {
    if (!g_main || !IsWindow(g_main)) {
        ResetHost();
        g_main = FindHostWindow();
        if (!g_main) return;
    }
    EnsureControls();
    if (!g_list) return;
    RebuildRowsIfNeeded();
    LayoutAndVisibility();
}

struct Bootstrap {
    Bootstrap() {
        MSG msg{};
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        g_timerId = SetTimer(nullptr, 0, 300, &Tick);
    }
    ~Bootstrap() {
        if (g_timerId) KillTimer(nullptr, g_timerId);
    }
};

Bootstrap g_bootstrap;

std::wstring FilterSummary(const std::wstring& message, const std::map<std::wstring, bool>& selected) {
    std::wstring out;
    std::size_t start = 0;
    while (start <= message.size()) {
        const std::size_t end = message.find(L'\n', start);
        std::wstring line = message.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        bool mentionsUnticked = false;
        for (const auto& kv : selected) {
            if (!kv.second && !kv.first.empty() && line.find(kv.first) != std::wstring::npos) {
                mentionsUnticked = true;
                break;
            }
        }
        if (!mentionsUnticked) {
            if (!out.empty()) out += L"\r\n";
            out += line;
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return out;
}

} // namespace

bool PrepareRequest(const std::wstring& account,
                    const std::wstring& eventType,
                    std::wstring& message,
                    std::wstring& skipReason) {
    skipReason.clear();

    std::map<std::wstring, bool> selected;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        selected = g_selectedByName;
    }

    // Bot diagnostics and tool-wide events are not tied to one game account.
    if (account.empty() || account == L"-") {
        const bool summary = eventType.find(L"SUMMARY") != std::wstring::npos;
        if (!summary) return true;

        const bool anySelected = std::any_of(selected.begin(), selected.end(), [](const auto& kv) { return kv.second; });
        if (!anySelected) {
            skipReason = L"không có acc nào được tick trong Tab TELE";
            return false;
        }
        message = FilterSummary(message, selected);
        return !message.empty();
    }

    const std::wstring name = RequestAccountName(account);
    if (name.empty()) {
        skipReason = L"event không xác định được tên acc • fail-closed";
        return false;
    }

    const auto it = selected.find(name);
    if (it == selected.end() || !it->second) {
        skipReason = L"acc '" + name + L"' chưa được tick báo cáo trong Tab TELE";
        return false;
    }
    return true;
}

} // namespace telegram_account_filter
