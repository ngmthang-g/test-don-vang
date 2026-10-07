#include "image_scan_test.h"
#include "con_filter_nosleep_logic.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace image_scan_test {
namespace {

constexpr wchar_t kClassName[] = L"ThanLongImageFilterTestPocV4";
constexpr wchar_t kRegionClassName[] = L"ThanLongImageScanRegionPickerV2";
constexpr wchar_t kPreviewClassName[] = L"ThanLongImageScanPreviewV2";
constexpr UINT kPwRenderFullContent = 0x00000002u;
constexpr UINT_PTR kF8PollTimer = 91;
constexpr UINT_PTR kRunTimer = 92;
constexpr UINT_PTR kRec42PollTimer = 93;
constexpr UINT kRec42PollMs = 10;
constexpr UINT kProbeIntervalMs = 80;
constexpr std::size_t kDefaultInitialSteps = 42;
constexpr ULONGLONG kEmptyRetryMs = 10000;

constexpr int IDC_TEMPLATE = 1001;
constexpr int IDC_PICK = 1002;
constexpr int IDC_X = 1003;
constexpr int IDC_Y = 1004;
constexpr int IDC_W = 1005;
constexpr int IDC_H = 1006;
constexpr int IDC_THRESHOLD = 1007;
constexpr int IDC_TEST = 1008;
constexpr int IDC_STATUS = 1009;
constexpr int IDC_PICK_REGION = 1010;
constexpr int IDC_PREVIEW_REGION = 1011;
constexpr int IDC_FULL_REGION = 1012;
constexpr int IDC_STEP_LIST = 1020;
constexpr int IDC_STEP_ADD = 1021;
constexpr int IDC_STEP_DELETE = 1022;
constexpr int IDC_STEP_UP = 1023;
constexpr int IDC_STEP_DOWN = 1024;
constexpr int IDC_STEP_X = 1025;
constexpr int IDC_STEP_Y = 1026;
constexpr int IDC_STEP_DELAY = 1027;
constexpr int IDC_STEP_REPEAT = 1028;
constexpr int IDC_STEP_SAVE = 1029;
constexpr int IDC_STEP_CAPTURE = 1030;
constexpr int IDC_STEP_TEST = 1031;
constexpr int IDC_TEMPLATE_DISCARD = 1040;
constexpr int IDC_PICK_DISCARD = 1041;
constexpr int IDC_TEMPLATE_CLOSE = 1042;
constexpr int IDC_PICK_CLOSE = 1043;
constexpr int IDC_DISCARD_X = 1050;
constexpr int IDC_DISCARD_Y = 1051;
constexpr int IDC_DISCARD_W = 1052;
constexpr int IDC_DISCARD_H = 1053;
constexpr int IDC_DISCARD_REGION = 1054;
constexpr int IDC_DISCARD_PREVIEW = 1055;
constexpr int IDC_DISCARD_FULL = 1056;
constexpr int IDC_CLOSE_X = 1060;
constexpr int IDC_CLOSE_Y = 1061;
constexpr int IDC_CLOSE_W = 1062;
constexpr int IDC_CLOSE_H = 1063;
constexpr int IDC_CLOSE_REGION = 1064;
constexpr int IDC_CLOSE_PREVIEW = 1065;
constexpr int IDC_CLOSE_FULL = 1066;
constexpr int IDC_DELAY_DISCARD = 1070;
constexpr int IDC_DELAY_CLOSE = 1072;
constexpr int IDC_EXPORT_SCAN = 1092;
constexpr int IDC_IMPORT_SCAN = 1093;
constexpr int IDC_CHILD_SCAN_BASE = 1200; // 1200..1229 = CON1..CON30
constexpr int IDC_PRECHECK_X = 1135;
constexpr int IDC_PRECHECK_Y = 1136;
constexpr int IDC_PRECHECK_DELAY = 1137;
constexpr int IDC_PRECHECK_CAPTURE = 1138;
constexpr int IDC_SCAN_MAX_CONCURRENT = 1140;
constexpr int IDC_SCAN_MAX_SWIPES = 1141;
constexpr int IDC_SWIPE_START_CAPTURE = 1142;
constexpr int IDC_SWIPE_END_CAPTURE = 1143;
constexpr int IDC_SWIPE_TEST = 1144;
constexpr int IDC_SWIPE_DELAY = 1145;
constexpr int IDC_FILTER_MIN_ACTION = 1146;
constexpr int IDC_STEP_REC42 = 1147;
constexpr int IDC_STEP_REC42_STOP = 1148;
constexpr int IDC_DISCARD_CONFIRM_MIN = 1149;
constexpr int IDC_STEP_REC_FROM_SELECTED = 1149;
constexpr int IDC_STEP_DELETE_GRID = 1150;
constexpr int IDC_STEP_DELETE_ALL_GRID = 1151;
constexpr int IDC_BAG_OPEN_X = 1152;
constexpr int IDC_BAG_OPEN_Y = 1153;
constexpr int IDC_BAG_OPEN_CAPTURE = 1154;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

struct Match {
    bool found = false;
    double score = -1.0;
    int x = 0;
    int y = 0;
};

struct ClickStep {
    int x = -1;
    int y = -1;
    int baseW = 0;
    int baseH = 0;
    int delayMs = 500;
    int repeat = 1;
    bool valid = false;
};

struct ScanRoi {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int baseW = 0;
    int baseH = 0;
};

struct Config {
    // Ảnh 1 = dấu hiệu món đồ ĐÚNG. Chỉ scan, KHÔNG click ảnh 1.
    std::wstring templatePath;
    std::wstring discardTemplatePath;
    std::wstring closeTemplatePath;
    // ROI ẢNH 1 giữ các field cũ để tương thích cấu hình v3.
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int roiBaseW = 0;
    int roiBaseH = 0;

    // V4: NÚT VỨT và DẤU X có ROI riêng.
    ScanRoi discardRoi{};
    ScanRoi closeRoi{};

    int thresholdPercent = 90;
    std::vector<ClickStep> steps;

    // AUTO CON: PRECHECK vẫn dùng UI DIRECT read-only để nhận biết nút MỞ TAY NẢI.
    // bagUiSwitch chỉ chuyển đúng UI khi PRECHECK chưa thấy nút; bagOpenClick là click F8 duy nhất để mở tay nải.
    ClickStep bagUiSwitch;
    ClickStep bagOpenClick;
    // v1.2.1 proven hidden drag primitive, now reused by weapon scan.
    ClickStep swipeStart;
    ClickStep swipeEnd;
    int maxConcurrentScan = 2; // global CON scheduler cap, 0=OFF, 1..30 active
    int maxSwipes = 2;         // user-approved: only 1 or 2; never 3
    int swipeDelayMs = 600; // legacy config/import only; AUTO timing uses minFilterActionMs
    int minFilterActionMs = 0;
    int minDiscardConfirmMs = 0;
    std::array<bool, 30> childEnabled{};

    // Delay cố định sau từng loại click đặc biệt.
    int discardClickDelayMs = 500;
    int closeClickDelayMs = 500;
};

Config g_lastConfig{};

enum class RunPhase {
    Idle,
    WaitItemReady,
    WaitDiscardGone,
    WaitPopupGoneAfterConfirm,
    WaitCloseGone,
    WaitEmptyRetry,
};


struct State {
    Target target{};
    Config config{};
    HWND hwnd = nullptr;
    HWND stepList = nullptr;
    int captureRow = -1;
    bool f8WasDown = false;
    bool runningChain = false;
    bool rec42Active = false;
    bool rec42MouseWasDown = false;
    bool rec42FromSelected = false;
    int rec42StartGrid = 0;
    std::vector<ClickStep> rec42Buffer{};

    // Runtime v4: template load đúng 1 lần khi Start; timer/state không khóa UI thread bằng Sleep.
    Image goodTpl{};
    Image closeTpl{};
    RunPhase runPhase = RunPhase::Idle;
    std::size_t slotIndex = 0;
    std::size_t discardCount = 0;
    ULONGLONG lastActionTick = 0;
    ULONGLONG nextProbeTick = 0;
    ULONGLONG phaseDeadlineTick = 0;
};

struct RegionPickerState {
    const Image* frame = nullptr;
    HWND hwnd = nullptr;
    RECT imageRect{};
    POINT dragStart{};
    POINT dragCurrent{};
    bool dragging = false;
    bool accepted = false;
    RECT selected{};
};

struct PreviewState {
    const Image* image = nullptr;
    HWND hwnd = nullptr;
    RECT imageRect{};
    std::wstring note;
};

template <typename T>
void ReleaseCom(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

std::wstring HrText(HRESULT hr) {
    wchar_t buf[32]{};
    swprintf_s(buf, L"0x%08X", static_cast<unsigned int>(hr));
    return buf;
}

bool CurrentClientSize(HWND hwnd, int& w, int& h) {
    w = 0; h = 0;
    RECT rc{};
    if (!hwnd || !IsWindow(hwnd) || !GetClientRect(hwnd, &rc)) return false;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    return w > 0 && h > 0;
}

int ScaleCoord(int value, int fromExtent, int toExtent) {
    if (value < 0 || fromExtent <= 0 || toExtent <= 0) return value;
    const long long scaled = static_cast<long long>(value) * toExtent;
    return static_cast<int>((scaled + fromExtent / 2) / fromExtent);
}

void RebaseNamedRoi(ScanRoi& r, int cw, int ch) {
    if (r.baseW > 0 && r.baseH > 0 && (r.baseW != cw || r.baseH != ch)) {
        r.x = std::max(0, ScaleCoord(r.x, r.baseW, cw));
        r.y = std::max(0, ScaleCoord(r.y, r.baseH, ch));
        if (r.w > 0) r.w = std::max(1, ScaleCoord(r.w, r.baseW, cw));
        if (r.h > 0) r.h = std::max(1, ScaleCoord(r.h, r.baseH, ch));
    }
    r.baseW = cw;
    r.baseH = ch;
}

void RebaseClickStep(ClickStep& step, int cw, int ch) {
    if (!step.valid || step.baseW <= 0 || step.baseH <= 0) return;
    if (step.baseW != cw || step.baseH != ch) {
        step.x = ScaleCoord(step.x, step.baseW, cw);
        step.y = ScaleCoord(step.y, step.baseH, ch);
        step.baseW = cw;
        step.baseH = ch;
    }
}

void RebaseForCurrentClient(Config& c, HWND gameWindow) {
    int cw = 0, ch = 0;
    if (!CurrentClientSize(gameWindow, cw, ch)) return;
    if (c.roiBaseW > 0 && c.roiBaseH > 0 && (c.roiBaseW != cw || c.roiBaseH != ch)) {
        c.x = std::max(0, ScaleCoord(c.x, c.roiBaseW, cw));
        c.y = std::max(0, ScaleCoord(c.y, c.roiBaseH, ch));
        if (c.w > 0) c.w = std::max(1, ScaleCoord(c.w, c.roiBaseW, cw));
        if (c.h > 0) c.h = std::max(1, ScaleCoord(c.h, c.roiBaseH, ch));
    }
    c.roiBaseW = cw;
    c.roiBaseH = ch;
    RebaseNamedRoi(c.discardRoi, cw, ch);
    RebaseNamedRoi(c.closeRoi, cw, ch);
    for (ClickStep& step : c.steps) RebaseClickStep(step, cw, ch);
    RebaseClickStep(c.bagUiSwitch, cw, ch);
    RebaseClickStep(c.bagOpenClick, cw, ch);
    RebaseClickStep(c.swipeStart, cw, ch);
    RebaseClickStep(c.swipeEnd, cw, ch);
}

bool LoadImageWic(const std::wstring& path, Image& out, std::wstring& error) {
    out = {};
    if (path.empty()) { error = L"Chưa chọn ảnh mẫu"; return false; }
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
        error = L"CoInitializeEx FAIL " + HrText(initHr); return false;
    }
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                                WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                                   WICBitmapPaletteTypeCustom);
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) hr = converter->GetSize(&w, &h);
    if (SUCCEEDED(hr) && (w == 0 || h == 0 || w > 8192 || h > 8192)) hr = E_INVALIDARG;
    if (SUCCEEDED(hr)) {
        const std::size_t bytes = static_cast<std::size_t>(w) * h * 4u;
        out.bgra.resize(bytes);
        hr = converter->CopyPixels(nullptr, w * 4u, static_cast<UINT>(bytes), out.bgra.data());
        if (SUCCEEDED(hr)) { out.width = static_cast<int>(w); out.height = static_cast<int>(h); }
    }
    ReleaseCom(converter); ReleaseCom(frame); ReleaseCom(decoder); ReleaseCom(factory);
    if (uninit) CoUninitialize();
    if (FAILED(hr)) {
        out = {}; error = L"Không đọc được ảnh mẫu bằng WIC: " + HrText(hr); return false;
    }
    return true;
}

bool CaptureClient(HWND hwnd, Image& out, std::wstring& backend, std::wstring& error) {
    out = {}; backend.clear();
    if (!hwnd || !IsWindow(hwnd)) { error = L"HWND game không còn tồn tại"; return false; }
    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) { error = L"GetClientRect FAIL"; return false; }
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) { error = L"Client size không hợp lệ"; return false; }

    HDC src = GetDC(hwnd);
    if (!src) { error = L"GetDC(game) FAIL"; return false; }
    HDC mem = CreateCompatibleDC(src);
    if (!mem) { ReleaseDC(hwnd, src); error = L"CreateCompatibleDC FAIL"; return false; }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        DeleteDC(mem); ReleaseDC(hwnd, src);
        error = L"CreateDIBSection FAIL"; return false;
    }
    HGDIOBJ old = SelectObject(mem, dib);
    PatBlt(mem, 0, 0, w, h, BLACKNESS);
    BOOL ok = PrintWindow(hwnd, mem, PW_CLIENTONLY | kPwRenderFullContent);
    if (ok) backend = L"PrintWindow(HWND/PW_CLIENTONLY)";
    else {
        ok = BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY | CAPTUREBLT);
        backend = L"BitBlt fallback";
    }
    if (ok) {
        out.width = w; out.height = h;
        const std::size_t bytes = static_cast<std::size_t>(w) * h * 4u;
        out.bgra.resize(bytes);
        std::memcpy(out.bgra.data(), bits, bytes);
    }
    SelectObject(mem, old); DeleteObject(dib); DeleteDC(mem); ReleaseDC(hwnd, src);
    if (!ok) { out = {}; error = L"PrintWindow và BitBlt đều FAIL"; return false; }
    return true;
}

inline int ColorDiff(const std::uint8_t* a, const std::uint8_t* b) {
    return std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0])) +
           std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1])) +
           std::abs(static_cast<int>(a[2]) - static_cast<int>(b[2]));
}

double ScoreAt(const Image& frame, const Image& tpl, int ox, int oy, int sampleStep) {
    std::uint64_t diff = 0, samples = 0;
    for (int ty = 0; ty < tpl.height; ty += sampleStep) {
        const std::uint8_t* tr = tpl.bgra.data() + static_cast<std::size_t>(ty) * tpl.width * 4u;
        const std::uint8_t* fr = frame.bgra.data() +
            (static_cast<std::size_t>(oy + ty) * frame.width + ox) * 4u;
        for (int tx = 0; tx < tpl.width; tx += sampleStep) {
            diff += static_cast<std::uint64_t>(ColorDiff(fr + static_cast<std::size_t>(tx) * 4u,
                                                        tr + static_cast<std::size_t>(tx) * 4u));
            ++samples;
        }
    }
    if (samples == 0) return -1.0;
    const double maxDiff = static_cast<double>(samples) * 3.0 * 255.0;
    return 1.0 - static_cast<double>(diff) / maxDiff;
}

Match FindTemplate(const Image& frame, const Image& tpl,
                   int rx, int ry, int rw, int rh, double threshold,
                   std::wstring& error) {
    Match best{};
    if (frame.width <= 0 || frame.height <= 0 || tpl.width <= 0 || tpl.height <= 0) {
        error = L"Ảnh/frame rỗng"; return best;
    }
    rx = std::clamp(rx, 0, frame.width - 1);
    ry = std::clamp(ry, 0, frame.height - 1);
    if (rw <= 0) rw = frame.width - rx;
    if (rh <= 0) rh = frame.height - ry;
    const int right = std::clamp(rx + rw, rx + 1, frame.width);
    const int bottom = std::clamp(ry + rh, ry + 1, frame.height);
    const int maxX = right - tpl.width;
    const int maxY = bottom - tpl.height;
    if (maxX < rx || maxY < ry) { error = L"Ảnh mẫu lớn hơn vùng quét"; return best; }

    const std::int64_t area = static_cast<std::int64_t>(right - rx) * (bottom - ry);
    const int posStep = area > 800000 ? 2 : 1;
    const int sampleStep = std::max(1, std::min(tpl.width, tpl.height) / 18);
    for (int y = ry; y <= maxY; y += posStep) {
        for (int x = rx; x <= maxX; x += posStep) {
            const double score = ScoreAt(frame, tpl, x, y, sampleStep);
            if (score > best.score) { best.score = score; best.x = x; best.y = y; }
        }
    }
    if (posStep > 1 && best.score >= 0.0) {
        const int x0 = std::max(rx, best.x - posStep);
        const int y0 = std::max(ry, best.y - posStep);
        const int x1 = std::min(maxX, best.x + posStep);
        const int y1 = std::min(maxY, best.y + posStep);
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            const double score = ScoreAt(frame, tpl, x, y, sampleStep);
            if (score > best.score) { best.score = score; best.x = x; best.y = y; }
        }
    }
    if (best.score >= 0.0) best.score = ScoreAt(frame, tpl, best.x, best.y, 1);
    best.found = best.score >= threshold;
    return best;
}

bool CropImage(const Image& src, int x, int y, int w, int h, Image& out, std::wstring& error) {
    out = {};
    if (src.width <= 0 || src.height <= 0) { error = L"Frame rỗng"; return false; }
    x = std::clamp(x, 0, src.width - 1);
    y = std::clamp(y, 0, src.height - 1);
    if (w <= 0) w = src.width - x;
    if (h <= 0) h = src.height - y;
    const int right = std::clamp(x + w, x + 1, src.width);
    const int bottom = std::clamp(y + h, y + 1, src.height);
    out.width = right - x; out.height = bottom - y;
    out.bgra.resize(static_cast<std::size_t>(out.width) * out.height * 4u);
    for (int row = 0; row < out.height; ++row) {
        const std::uint8_t* from = src.bgra.data() +
            (static_cast<std::size_t>(y + row) * src.width + x) * 4u;
        std::uint8_t* to = out.bgra.data() + static_cast<std::size_t>(row) * out.width * 4u;
        std::memcpy(to, from, static_cast<std::size_t>(out.width) * 4u);
    }
    return true;
}

void DrawImage(HDC dc, const Image& image, const RECT& dest) {
    if (image.width <= 0 || image.height <= 0 || image.bgra.empty()) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = image.width;
    bi.bmiHeader.biHeight = -image.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, dest.left, dest.top, dest.right - dest.left, dest.bottom - dest.top,
                  0, 0, image.width, image.height, image.bgra.data(), &bi,
                  DIB_RGB_COLORS, SRCCOPY);
}

int ReadInt(HWND hwnd, int id, int fallback) {
    wchar_t buf[64]{};
    GetDlgItemTextW(hwnd, id, buf, static_cast<int>(std::size(buf)));
    wchar_t* end = nullptr;
    long value = wcstol(buf, &end, 10);
    return end == buf ? fallback : static_cast<int>(value);
}

std::wstring ReadText(HWND hwnd, int id) {
    const int n = GetWindowTextLengthW(GetDlgItem(hwnd, id));
    if (n <= 0) return {};
    std::wstring text(static_cast<std::size_t>(n + 1), L'\0');
    GetDlgItemTextW(hwnd, id, text.data(), n + 1);
    text.resize(static_cast<std::size_t>(n));
    return text;
}

void SetStatus(HWND hwnd, const std::wstring& text) {
    HWND h = GetDlgItem(hwnd, IDC_STATUS);
    if (h) { SetWindowTextW(h, text.c_str()); UpdateWindow(h); }
}

HWND Add(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
         int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    if (c) SendMessageW(c, WM_SETFONT,
                         reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return c;
}

void SetEditInt(HWND hwnd, int id, int value) {
    SetDlgItemTextW(hwnd, id, std::to_wstring(value).c_str());
}

int SelectedStep(const State& s) {
    return s.stepList ? ListView_GetNextItem(s.stepList, -1, LVNI_SELECTED) : -1;
}

void AddColumn(HWND list, int index, int width, const wchar_t* text) {
    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = const_cast<wchar_t*>(text);
    c.cx = width; c.iSubItem = index;
    ListView_InsertColumn(list, index, &c);
}

void LoadStepEditor(State& s, int row) {
    if (row < 0 || row >= static_cast<int>(s.config.steps.size())) return;
    const ClickStep& step = s.config.steps[static_cast<std::size_t>(row)];
    if (step.valid) { SetEditInt(s.hwnd, IDC_STEP_X, step.x); SetEditInt(s.hwnd, IDC_STEP_Y, step.y); }
    else { SetDlgItemTextW(s.hwnd, IDC_STEP_X, L""); SetDlgItemTextW(s.hwnd, IDC_STEP_Y, L""); }
}

std::wstring FilterGridMapLabel(int grid) {
    if(grid<0||grid>41)return L"-";std::wstring t=L"P"+std::to_wstring(grid);const int p1=grid+42;if(p1<=83)t+=L" / P"+std::to_wstring(p1);if(grid>=21){const int p2=grid+63;if(p2<=99)t+=L" / P"+std::to_wstring(p2);}return t;
}

void RefreshStepList(State& s, int select = -1) {
    if (!s.stepList) return;
    ListView_DeleteAllItems(s.stepList);
    for (std::size_t ix = 0; ix < s.config.steps.size(); ++ix) {
        const ClickStep& step = s.config.steps[ix];
        std::wstring no = std::to_wstring(ix);
        LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(ix); item.pszText = no.data();
        ListView_InsertItem(s.stepList, &item);
        std::wstring x = step.valid ? std::to_wstring(step.x) : L"CHƯA F8";
        std::wstring y = step.valid ? std::to_wstring(step.y) : L"-";
        std::wstring base = step.valid ? std::to_wstring(step.baseW) + L"x" + std::to_wstring(step.baseH) : L"-";
        std::wstring map = FilterGridMapLabel(static_cast<int>(ix));
        ListView_SetItemText(s.stepList, static_cast<int>(ix), 1, x.data());
        ListView_SetItemText(s.stepList, static_cast<int>(ix), 2, y.data());
        ListView_SetItemText(s.stepList, static_cast<int>(ix), 3, base.data());
        ListView_SetItemText(s.stepList, static_cast<int>(ix), 4, map.data());
    }
    if (select < 0 && !s.config.steps.empty()) select = 0;
    if (select >= 0 && select < static_cast<int>(s.config.steps.size())) {
        ListView_SetItemState(s.stepList, select, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(s.stepList, select, FALSE);
        LoadStepEditor(s, select);
    } else {
        SetDlgItemTextW(s.hwnd, IDC_STEP_X, L""); SetDlgItemTextW(s.hwnd, IDC_STEP_Y, L"");
    }
}

void SaveStepEditor(State& s) {
    const int row = SelectedStep(s);
    if (row < 0 || row >= static_cast<int>(s.config.steps.size())) return;
    ClickStep& step = s.config.steps[static_cast<std::size_t>(row)];
    const int x = ReadInt(s.hwnd, IDC_STEP_X, -1);
    const int y = ReadInt(s.hwnd, IDC_STEP_Y, -1);
    step.repeat = 1;
    int cw = 0, ch = 0;
    if (x >= 0 && y >= 0 && CurrentClientSize(s.target.gameWindow, cw, ch) && x < cw && y < ch) {
        step.x = x; step.y = y; step.baseW = cw; step.baseH = ch; step.valid = true;
    } else if (x < 0 || y < 0) {
        step.valid = false; step.x = -1; step.y = -1;
    }
    g_lastConfig = s.config;
}

void SyncConfig(State& s) {
    SaveStepEditor(s);
    s.config.templatePath = ReadText(s.hwnd, IDC_TEMPLATE);
    s.config.discardTemplatePath = ReadText(s.hwnd, IDC_TEMPLATE_DISCARD);
    s.config.closeTemplatePath = ReadText(s.hwnd, IDC_TEMPLATE_CLOSE);

    s.config.x = std::max(0, ReadInt(s.hwnd, IDC_X, 0));
    s.config.y = std::max(0, ReadInt(s.hwnd, IDC_Y, 0));
    s.config.w = std::max(0, ReadInt(s.hwnd, IDC_W, 0));
    s.config.h = std::max(0, ReadInt(s.hwnd, IDC_H, 0));

    s.config.discardRoi.x = std::max(0, ReadInt(s.hwnd, IDC_DISCARD_X, 0));
    s.config.discardRoi.y = std::max(0, ReadInt(s.hwnd, IDC_DISCARD_Y, 0));
    s.config.discardRoi.w = std::max(0, ReadInt(s.hwnd, IDC_DISCARD_W, 0));
    s.config.discardRoi.h = std::max(0, ReadInt(s.hwnd, IDC_DISCARD_H, 0));

    s.config.closeRoi.x = std::max(0, ReadInt(s.hwnd, IDC_CLOSE_X, 0));
    s.config.closeRoi.y = std::max(0, ReadInt(s.hwnd, IDC_CLOSE_Y, 0));
    s.config.closeRoi.w = std::max(0, ReadInt(s.hwnd, IDC_CLOSE_W, 0));
    s.config.closeRoi.h = std::max(0, ReadInt(s.hwnd, IDC_CLOSE_H, 0));

    s.config.thresholdPercent = std::clamp(ReadInt(s.hwnd, IDC_THRESHOLD, 90), 1, 100);


    for (int i = 0; i < 30; ++i)
        s.config.childEnabled[static_cast<std::size_t>(i)] =
            IsDlgButtonChecked(s.hwnd, IDC_CHILD_SCAN_BASE + i) == BST_CHECKED;
    s.config.maxConcurrentScan = std::clamp(ReadInt(s.hwnd, IDC_SCAN_MAX_CONCURRENT, s.config.maxConcurrentScan), 0, 30);
    s.config.maxSwipes = std::clamp(ReadInt(s.hwnd, IDC_SCAN_MAX_SWIPES, s.config.maxSwipes), 1, 2);
    s.config.minFilterActionMs = con_filter_nosleep_logic::ClampMinActionMs(ReadInt(s.hwnd, IDC_FILTER_MIN_ACTION, s.config.minFilterActionMs));
    s.config.minDiscardConfirmMs = std::max(0, ReadInt(s.hwnd, IDC_DISCARD_CONFIRM_MIN, s.config.minDiscardConfirmMs));

    const int precheckX = ReadInt(s.hwnd, IDC_PRECHECK_X, s.config.bagUiSwitch.x);
    const int precheckY = ReadInt(s.hwnd, IDC_PRECHECK_Y, s.config.bagUiSwitch.y);
    const int bagOpenX = ReadInt(s.hwnd, IDC_BAG_OPEN_X, s.config.bagOpenClick.x);
    const int bagOpenY = ReadInt(s.hwnd, IDC_BAG_OPEN_Y, s.config.bagOpenClick.y);

    int cw = 0, ch = 0;
    if (CurrentClientSize(s.target.gameWindow, cw, ch)) {
        s.config.roiBaseW = cw;
        s.config.roiBaseH = ch;
        s.config.discardRoi.baseW = cw;
        s.config.discardRoi.baseH = ch;
        s.config.closeRoi.baseW = cw;
        s.config.closeRoi.baseH = ch;
        auto syncPoint = [cw, ch](ClickStep& step, int x, int y, bool optional) {
            if (x >= 0 && y >= 0 && x < cw && y < ch) {
                step.x = x; step.y = y; step.baseW = cw; step.baseH = ch; step.valid = true; step.repeat = 1;
            } else if (optional && (x < 0 || y < 0)) {
                step.valid = false; step.x = -1; step.y = -1;
            }
        };
        syncPoint(s.config.bagUiSwitch, precheckX, precheckY, true);
        syncPoint(s.config.bagOpenClick, bagOpenX, bagOpenY, true);
    }
    g_lastConfig = s.config;
}

void PickImageFile(State& s, int editId, std::wstring& dest) {
    SyncConfig(s);
    wchar_t file[MAX_PATH * 4]{};
    if (!dest.empty()) wcsncpy_s(file, dest.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = s.hwnd;
    ofn.lpstrFilter = L"Ảnh mẫu (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0Tất cả file\0*.*\0\0";
    ofn.lpstrFile = file; ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    dest = file;
    SetDlgItemTextW(s.hwnd, editId, file);
    g_lastConfig = s.config;
}

void PickTemplate(State& s) { PickImageFile(s, IDC_TEMPLATE, s.config.templatePath); }
void PickDiscardTemplate(State& s) { PickImageFile(s, IDC_TEMPLATE_DISCARD, s.config.discardTemplatePath); }
void PickCloseTemplate(State& s) { PickImageFile(s, IDC_TEMPLATE_CLOSE, s.config.closeTemplatePath); }

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(std::max(0, n)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(const std::string& u) {
    if (u.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(std::max(0, n)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), out.data(), n);
    return out;
}

std::string StepLine(const ClickStep& p) {
    return std::to_string(p.x)+","+std::to_string(p.y)+","+std::to_string(p.baseW)+","+
           std::to_string(p.baseH)+","+std::to_string(p.delayMs)+","+(p.valid?"1":"0");
}

bool ParseStepLine(const std::string& text, ClickStep& p) {
    std::stringstream ss(text); std::string item; std::array<int,6> v{}; int i=0;
    while (std::getline(ss,item,',') && i<6) { try { v[static_cast<std::size_t>(i++)]=std::stoi(item); } catch (...) { return false; } }
    if (i != 6) return false;
    p.x=v[0]; p.y=v[1]; p.baseW=v[2]; p.baseH=v[3]; p.delayMs=std::clamp(v[4],50,10000); p.valid=v[5]!=0; p.repeat=1;
    return true;
}

std::string RoiLine(int x,int y,int w,int h,int bw,int bh) {
    return std::to_string(x)+","+std::to_string(y)+","+std::to_string(w)+","+std::to_string(h)+","+
           std::to_string(bw)+","+std::to_string(bh);
}

bool ParseSix(const std::string& text, std::array<int,6>& v) {
    std::stringstream ss(text); std::string item; int i=0;
    while (std::getline(ss,item,',') && i<6) { try { v[static_cast<std::size_t>(i++)]=std::stoi(item); } catch (...) { return false; } }
    return i==6;
}

std::filesystem::path CopyTemplateBesideConfig(const std::filesystem::path& configPath,
                                                const std::wstring& source,
                                                const wchar_t* suffix,
                                                std::wstring& error) {
    if (source.empty()) { error=L"thiếu đường dẫn ảnh mẫu"; return {}; }
    const std::filesystem::path src(source);
    if (!std::filesystem::exists(src)) { error=L"không tìm thấy ảnh mẫu: "+source; return {}; }
    std::wstring ext=src.extension().wstring(); if(ext.empty()) ext=L".png";
    const std::filesystem::path dest=configPath.parent_path()/(configPath.stem().wstring()+suffix+ext);
    std::error_code ec; std::filesystem::copy_file(src,dest,std::filesystem::copy_options::overwrite_existing,ec);
    if(ec){error=L"không copy được ảnh mẫu: "+dest.wstring();return {};}
    return dest;
}

void RefreshAutoEditors(State& s) {
    auto setPoint=[&](int xId,int yId,int dId,const ClickStep& p){
        if(p.valid){SetEditInt(s.hwnd,xId,p.x);SetEditInt(s.hwnd,yId,p.y);}else{SetDlgItemTextW(s.hwnd,xId,L"");SetDlgItemTextW(s.hwnd,yId,L"");}
        if(dId>0) SetEditInt(s.hwnd,dId,p.delayMs);
    };
    setPoint(IDC_PRECHECK_X,IDC_PRECHECK_Y,IDC_PRECHECK_DELAY,s.config.bagUiSwitch);
    setPoint(IDC_BAG_OPEN_X,IDC_BAG_OPEN_Y,0,s.config.bagOpenClick);
    SetEditInt(s.hwnd,IDC_SCAN_MAX_CONCURRENT,s.config.maxConcurrentScan);
    SetEditInt(s.hwnd,IDC_SCAN_MAX_SWIPES,s.config.maxSwipes);
    SetEditInt(s.hwnd,IDC_SWIPE_DELAY,s.config.swipeDelayMs);
    SetEditInt(s.hwnd,IDC_FILTER_MIN_ACTION,s.config.minFilterActionMs);
    SetEditInt(s.hwnd,IDC_DISCARD_CONFIRM_MIN,s.config.minDiscardConfirmMs);
    for(int i=0;i<30;++i) CheckDlgButton(s.hwnd,IDC_CHILD_SCAN_BASE+i,s.config.childEnabled[static_cast<std::size_t>(i)]?BST_CHECKED:BST_UNCHECKED);
}

void RefreshAllConfigEditors(State& s) {
    SetDlgItemTextW(s.hwnd,IDC_TEMPLATE,s.config.templatePath.c_str());
    SetDlgItemTextW(s.hwnd,IDC_TEMPLATE_DISCARD,s.config.discardTemplatePath.c_str());
    SetDlgItemTextW(s.hwnd,IDC_TEMPLATE_CLOSE,s.config.closeTemplatePath.c_str());
    SetEditInt(s.hwnd,IDC_X,s.config.x);SetEditInt(s.hwnd,IDC_Y,s.config.y);SetEditInt(s.hwnd,IDC_W,s.config.w);SetEditInt(s.hwnd,IDC_H,s.config.h);
    SetEditInt(s.hwnd,IDC_DISCARD_X,s.config.discardRoi.x);SetEditInt(s.hwnd,IDC_DISCARD_Y,s.config.discardRoi.y);SetEditInt(s.hwnd,IDC_DISCARD_W,s.config.discardRoi.w);SetEditInt(s.hwnd,IDC_DISCARD_H,s.config.discardRoi.h);
    SetEditInt(s.hwnd,IDC_CLOSE_X,s.config.closeRoi.x);SetEditInt(s.hwnd,IDC_CLOSE_Y,s.config.closeRoi.y);SetEditInt(s.hwnd,IDC_CLOSE_W,s.config.closeRoi.w);SetEditInt(s.hwnd,IDC_CLOSE_H,s.config.closeRoi.h);
    SetEditInt(s.hwnd,IDC_THRESHOLD,s.config.thresholdPercent);SetEditInt(s.hwnd,IDC_DELAY_DISCARD,s.config.discardClickDelayMs);
    SetEditInt(s.hwnd,IDC_DELAY_CLOSE,s.config.closeClickDelayMs);
    RefreshAutoEditors(s);RefreshStepList(s);
}

void ExportScanConfig(State& s) {
    SyncConfig(s);
    wchar_t file[MAX_PATH*4]{}; wcscpy_s(file,L"ThanLong_WeaponScan.tlscan");
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=s.hwnd;ofn.lpstrFilter=L"ThanLong Scan (*.tlscan)\0*.tlscan\0Tất cả file\0*.*\0\0";
    ofn.lpstrFile=file;ofn.nMaxFile=static_cast<DWORD>(std::size(file));ofn.lpstrDefExt=L"tlscan";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(!GetSaveFileNameW(&ofn))return;
    const std::filesystem::path cfgPath(file);std::wstring error;
    const auto good=CopyTemplateBesideConfig(cfgPath,s.config.templatePath,L"_good",error);if(good.empty()){SetStatus(s.hwnd,L"XUẤT SCAN FAIL • "+error);return;}
    const auto close=CopyTemplateBesideConfig(cfgPath,s.config.closeTemplatePath,L"_close",error);if(close.empty()){SetStatus(s.hwnd,L"XUẤT SCAN FAIL • "+error);return;}
    std::ofstream out(cfgPath,std::ios::binary|std::ios::trunc);if(!out){SetStatus(s.hwnd,L"XUẤT SCAN FAIL • không tạo được file");return;}
    out<<"TL_SCAN_V7\n";
    out<<"good_file="<<Utf8(good.filename().wstring())<<"\n"<<"close_file="<<Utf8(close.filename().wstring())<<"\n";
    out<<"good_roi="<<RoiLine(s.config.x,s.config.y,s.config.w,s.config.h,s.config.roiBaseW,s.config.roiBaseH)<<"\n";
    out<<"close_roi="<<RoiLine(s.config.closeRoi.x,s.config.closeRoi.y,s.config.closeRoi.w,s.config.closeRoi.h,s.config.closeRoi.baseW,s.config.closeRoi.baseH)<<"\n";
    out<<"precheck_switch="<<StepLine(s.config.bagUiSwitch)<<"\n";
    out<<"swipe_start="<<StepLine(s.config.swipeStart)<<"\n"<<"swipe_end="<<StepLine(s.config.swipeEnd)<<"\n";
    out<<"max_concurrent="<<s.config.maxConcurrentScan<<"\n"<<"max_swipes="<<s.config.maxSwipes<<"\n"<<"swipe_delay="<<s.config.swipeDelayMs<<"\n"<<"min_filter_action_ms="<<s.config.minFilterActionMs<<"\n"<<"min_discard_confirm_ms="<<std::max(0,s.config.minDiscardConfirmMs)<<"\n";
    out<<"threshold="<<s.config.thresholdPercent<<"\n";
    out<<"children=";for(int i=0;i<30;++i){if(i)out<<',';out<<(s.config.childEnabled[static_cast<std::size_t>(i)]?1:0);}out<<"\n";
    out<<"step_count="<<s.config.steps.size()<<"\n";for(std::size_t i=0;i<s.config.steps.size();++i)out<<"step"<<i<<"="<<StepLine(s.config.steps[i])<<"\n";
    out.close();SetStatus(s.hwnd,L"XUẤT SCAN PASS • config + ảnh GOOD/X • mở/đóng tay nải bằng UI DIRECT");
}

void ImportScanConfig(State& s) {
    wchar_t file[MAX_PATH*4]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=s.hwnd;ofn.lpstrFilter=L"ThanLong Scan (*.tlscan)\0*.tlscan\0Tất cả file\0*.*\0\0";
    ofn.lpstrFile=file;ofn.nMaxFile=static_cast<DWORD>(std::size(file));ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(!GetOpenFileNameW(&ofn))return;
    const std::filesystem::path cfgPath(file);std::ifstream in(cfgPath,std::ios::binary);if(!in){SetStatus(s.hwnd,L"NHẬP SCAN FAIL • không mở được file");return;}
    std::string line;if(!std::getline(in,line)||(line!="TL_SCAN_V5"&&line!="TL_SCAN_V6"&&line!="TL_SCAN_V7")){SetStatus(s.hwnd,L"NHẬP SCAN FAIL • sai định dạng TL_SCAN_V5/V6/V7");return;}
    Config c{};std::size_t stepCount=0;std::vector<std::pair<std::size_t,std::string>> pendingSteps;
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();const auto pos=line.find('=');if(pos==std::string::npos)continue;const std::string key=line.substr(0,pos),val=line.substr(pos+1);
        if(key=="good_file")c.templatePath=(cfgPath.parent_path()/std::filesystem::path(FromUtf8(val))).wstring();
        else if(key=="discard_file")c.discardTemplatePath=(cfgPath.parent_path()/std::filesystem::path(FromUtf8(val))).wstring();
        else if(key=="close_file")c.closeTemplatePath=(cfgPath.parent_path()/std::filesystem::path(FromUtf8(val))).wstring();
        else if(key=="good_roi"){std::array<int,6>v{};if(ParseSix(val,v)){c.x=v[0];c.y=v[1];c.w=v[2];c.h=v[3];c.roiBaseW=v[4];c.roiBaseH=v[5];}}
        else if(key=="discard_roi"){std::array<int,6>v{};if(ParseSix(val,v)){c.discardRoi={v[0],v[1],v[2],v[3],v[4],v[5]};}}
        else if(key=="close_roi"){std::array<int,6>v{};if(ParseSix(val,v)){c.closeRoi={v[0],v[1],v[2],v[3],v[4],v[5]};}}
        else if(key=="precheck_switch")ParseStepLine(val,c.bagUiSwitch);
        else if(key=="swipe_start")ParseStepLine(val,c.swipeStart);
        else if(key=="swipe_end")ParseStepLine(val,c.swipeEnd);
        else if(key=="max_concurrent"){try{c.maxConcurrentScan=std::clamp(std::stoi(val),1,30);}catch(...){}}
        else if(key=="max_swipes"){try{c.maxSwipes=std::clamp(std::stoi(val),1,2);}catch(...){}}
        else if(key=="swipe_delay"){try{c.swipeDelayMs=std::clamp(std::stoi(val),100,5000);}catch(...){}}
        else if(key=="min_filter_action_ms"){try{c.minFilterActionMs=con_filter_nosleep_logic::ClampMinActionMs(std::stoi(val));}catch(...){}}else if(key=="min_discard_confirm_ms"){try{c.minDiscardConfirmMs=std::max(0,std::stoi(val));}catch(...){}}
        else if(key=="threshold"){try{c.thresholdPercent=std::clamp(std::stoi(val),1,100);}catch(...){}}
        else if(key=="delay_discard"){try{c.discardClickDelayMs=std::clamp(std::stoi(val),50,10000);}catch(...){}}
        else if(key=="delay_x"){try{c.closeClickDelayMs=std::clamp(std::stoi(val),50,10000);}catch(...){}}
        else if(key=="children"){std::stringstream ss(val);std::string x;int i=0;while(std::getline(ss,x,',')&&i<30){c.childEnabled[static_cast<std::size_t>(i++)]=(x=="1");}}
        else if(key=="step_count"){try{stepCount=static_cast<std::size_t>(std::max(0,std::stoi(val)));}catch(...){}}
        else if(key.rfind("step",0)==0){try{pendingSteps.emplace_back(static_cast<std::size_t>(std::stoul(key.substr(4))),val);}catch(...){}}
    }
    c.steps.resize(stepCount);for(const auto& it:pendingSteps)if(it.first<c.steps.size())ParseStepLine(it.second,c.steps[it.first]);
    if(c.steps.empty())c.steps.resize(kDefaultInitialSteps);RebaseForCurrentClient(c,s.target.gameWindow);s.config=c;g_lastConfig=c;SavePersistentConfig();RefreshAllConfigEditors(s);
    SetStatus(s.hwnd,L"NHẬP SCAN PASS • tọa/ROI/CON/MIN + ảnh GOOD/X đã nạp • UI DIRECT tay nải");
}

RECT NormalizedDragRect(const RegionPickerState& s) {
    RECT r{};
    r.left = std::min(s.dragStart.x, s.dragCurrent.x);
    r.top = std::min(s.dragStart.y, s.dragCurrent.y);
    r.right = std::max(s.dragStart.x, s.dragCurrent.x);
    r.bottom = std::max(s.dragStart.y, s.dragCurrent.y);
    r.left = std::clamp(r.left, s.imageRect.left, s.imageRect.right - 1);
    r.top = std::clamp(r.top, s.imageRect.top, s.imageRect.bottom - 1);
    r.right = std::clamp(r.right, s.imageRect.left + 1, s.imageRect.right);
    r.bottom = std::clamp(r.bottom, s.imageRect.top + 1, s.imageRect.bottom);
    return r;
}

bool PointInsideImage(const RegionPickerState& s, POINT p) {
    return p.x >= s.imageRect.left && p.x < s.imageRect.right &&
           p.y >= s.imageRect.top && p.y < s.imageRect.bottom;
}

POINT ClampToImage(const RegionPickerState& s, POINT p) {
    p.x = std::clamp(p.x, s.imageRect.left, s.imageRect.right - 1);
    p.y = std::clamp(p.y, s.imageRect.top, s.imageRect.bottom - 1);
    return p;
}

LRESULT CALLBACK RegionWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    RegionPickerState* s = reinterpret_cast<RegionPickerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        s = static_cast<RegionPickerState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        if (s) s->hwnd = hwnd;
    }
    if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
            RECT client{}; GetClientRect(hwnd, &client);
            FillRect(dc, &client, GetSysColorBrush(COLOR_WINDOW));
            SetBkMode(dc, TRANSPARENT);
            RECT note{10, 8, client.right - 10, 34};
            DrawTextW(dc, L"KÉO CHUỘT KHOANH VÙNG CẦN SCAN • thả chuột = lưu ngay • ESC = hủy",
                      -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            DrawImage(dc, *s->frame, s->imageRect);
            FrameRect(dc, &s->imageRect, GetSysColorBrush(COLOR_WINDOWFRAME));
            if (s->dragging) {
                RECT r = NormalizedDragRect(*s);
                FrameRect(dc, &r, GetSysColorBrush(COLOR_HIGHLIGHT));
                InflateRect(&r, -1, -1);
                if (r.right > r.left && r.bottom > r.top)
                    FrameRect(dc, &r, GetSysColorBrush(COLOR_HIGHLIGHT));
            }
            EndPaint(hwnd, &ps); return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (!PointInsideImage(*s, p)) return 0;
            s->dragStart = s->dragCurrent = p; s->dragging = true;
            SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0;
        }
        case WM_MOUSEMOVE:
            if (s->dragging) {
                POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                s->dragCurrent = ClampToImage(*s, p); InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (s->dragging) {
                POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                s->dragCurrent = ClampToImage(*s, p); s->dragging = false; ReleaseCapture();
                RECT r = NormalizedDragRect(*s);
                const int dw = s->imageRect.right - s->imageRect.left;
                const int dh = s->imageRect.bottom - s->imageRect.top;
                const int fw = s->frame->width, fh = s->frame->height;
                const int x0 = static_cast<int>((static_cast<long long>(r.left - s->imageRect.left) * fw) / dw);
                const int y0 = static_cast<int>((static_cast<long long>(r.top - s->imageRect.top) * fh) / dh);
                const int x1 = static_cast<int>((static_cast<long long>(r.right - s->imageRect.left) * fw + dw - 1) / dw);
                const int y1 = static_cast<int>((static_cast<long long>(r.bottom - s->imageRect.top) * fh + dh - 1) / dh);
                if (x1 - x0 >= 2 && y1 - y0 >= 2) {
                    s->selected = {x0, y0, std::min(fw, x1), std::min(fh, y1)};
                    s->accepted = true; DestroyWindow(hwnd);
                } else InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
            break;
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_NCDESTROY: s->hwnd = nullptr; return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool PickRegionModal(HWND owner, const Image& frame, RECT& selected) {
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workW = std::max(640L, work.right - work.left);
    const int workH = std::max(480L, work.bottom - work.top);
    const int maxImageW = std::min(1200, workW - 80);
    const int maxImageH = std::min(820, workH - 130);
    double scale = 1.0;
    if (frame.width > maxImageW) scale = std::min(scale, static_cast<double>(maxImageW) / frame.width);
    if (frame.height > maxImageH) scale = std::min(scale, static_cast<double>(maxImageH) / frame.height);
    const int displayW = std::max(1, static_cast<int>(std::lround(frame.width * scale)));
    const int displayH = std::max(1, static_cast<int>(std::lround(frame.height * scale)));

    RegionPickerState state{}; state.frame = &frame;
    state.imageRect = {10, 40, 10 + displayW, 40 + displayH};
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = RegionWndProc;
    wc.hInstance = GetModuleHandleW(nullptr); wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); wc.lpszClassName = kRegionClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    RECT wr{0, 0, displayW + 20, displayH + 55};
    AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    const int winW = wr.right - wr.left, winH = wr.bottom - wr.top;
    const int px = work.left + std::max(0, (workW - winW) / 2);
    const int py = work.top + std::max(0, (workH - winH) / 2);
    EnableWindow(owner, FALSE);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kRegionClassName,
                                L"CHỌN VÙNG SCAN • kéo chuột trực tiếp trên ảnh client",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                px, py, winW, winH, owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (!hwnd) { EnableWindow(owner, TRUE); return false; }
    ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd); SetFocus(hwnd);
    MSG msg{}; bool sawQuit = false; int quitCode = 0;
    while (IsWindow(hwnd)) {
        const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm <= 0) { if (gm == 0) { sawQuit = true; quitCode = static_cast<int>(msg.wParam); } break; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE); SetActiveWindow(owner);
    if (sawQuit) PostQuitMessage(quitCode);
    if (state.accepted) selected = state.selected;
    return state.accepted;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PreviewState* s = reinterpret_cast<PreviewState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        s = static_cast<PreviewState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        if (s) s->hwnd = hwnd;
    }
    if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
            RECT client{}; GetClientRect(hwnd, &client); FillRect(dc, &client, GetSysColorBrush(COLOR_WINDOW));
            SetBkMode(dc, TRANSPARENT); RECT note{10, 8, client.right - 10, 34};
            DrawTextW(dc, s->note.c_str(), -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            DrawImage(dc, *s->image, s->imageRect); FrameRect(dc, &s->imageRect, GetSysColorBrush(COLOR_WINDOWFRAME));
            EndPaint(hwnd, &ps); return 0;
        }
        case WM_KEYDOWN: if (wp == VK_ESCAPE || wp == VK_RETURN) { DestroyWindow(hwnd); return 0; } break;
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_NCDESTROY: s->hwnd = nullptr; return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowPreviewModal(HWND owner, const Image& image, const std::wstring& note) {
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workW = std::max(640L, work.right - work.left), workH = std::max(480L, work.bottom - work.top);
    const int maxW = std::min(760, workW - 100), maxH = std::min(560, workH - 140);
    double scale = 1.0;
    if (image.width > maxW) scale = std::min(scale, static_cast<double>(maxW) / image.width);
    if (image.height > maxH) scale = std::min(scale, static_cast<double>(maxH) / image.height);
    if (image.width < 260 && image.height < 180)
        scale = std::min(3.0, std::min(static_cast<double>(maxW) / image.width,
                                       static_cast<double>(maxH) / image.height));
    const int dw = std::max(1, static_cast<int>(std::lround(image.width * scale)));
    const int dh = std::max(1, static_cast<int>(std::lround(image.height * scale)));
    PreviewState state{}; state.image = &image; state.note = note; state.imageRect = {10, 40, 10 + dw, 40 + dh};
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = PreviewWndProc;
    wc.hInstance = GetModuleHandleW(nullptr); wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); wc.lpszClassName = kPreviewClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    RECT wr{0,0,dw+20,dh+55}; AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
    const int winW=wr.right-wr.left, winH=wr.bottom-wr.top;
    EnableWindow(owner, FALSE);
    HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,kPreviewClassName,L"XEM VÙNG SCAN",
                              WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
                              CW_USEDEFAULT,CW_USEDEFAULT,winW,winH,owner,nullptr,GetModuleHandleW(nullptr),&state);
    if(!hwnd){EnableWindow(owner,TRUE);return;}
    ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);SetFocus(hwnd);
    MSG msg{};bool sawQuit=false;int quitCode=0;
    while(IsWindow(hwnd)){
        const BOOL gm=GetMessageW(&msg,nullptr,0,0);
        if(gm<=0){if(gm==0){sawQuit=true;quitCode=static_cast<int>(msg.wParam);}break;}
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    EnableWindow(owner,TRUE);SetActiveWindow(owner);if(sawQuit)PostQuitMessage(quitCode);
}

void SelectRegion(State& s) {
    SyncConfig(s);
    Image frame{}; std::wstring backend, error;
    if (!CaptureClient(s.target.gameWindow, frame, backend, error)) {
        SetStatus(s.hwnd, L"CHỌN ROI ẢNH 1 FAIL • " + error); return;
    }
    RECT region{};
    if (!PickRegionModal(s.hwnd, frame, region)) {
        SetStatus(s.hwnd, L"ROI ẢNH 1: đã hủy, giữ nguyên vùng cũ"); return;
    }
    s.config.x = region.left; s.config.y = region.top;
    s.config.w = region.right - region.left; s.config.h = region.bottom - region.top;
    s.config.roiBaseW = frame.width; s.config.roiBaseH = frame.height;
    SetEditInt(s.hwnd, IDC_X, s.config.x); SetEditInt(s.hwnd, IDC_Y, s.config.y);
    SetEditInt(s.hwnd, IDC_W, s.config.w); SetEditInt(s.hwnd, IDC_H, s.config.h);
    g_lastConfig = s.config;
    SetStatus(s.hwnd, L"ROI ẢNH 1 PASS • " + std::to_wstring(s.config.x) + L"," + std::to_wstring(s.config.y) +
                       L"/" + std::to_wstring(s.config.w) + L"x" + std::to_wstring(s.config.h) + L" • " + backend);
}

void PreviewRegion(State& s) {
    SyncConfig(s);
    Image frame{}, crop{}; std::wstring backend, error;
    if (!CaptureClient(s.target.gameWindow, frame, backend, error)) { SetStatus(s.hwnd, L"XEM ROI ẢNH 1 FAIL • " + error); return; }
    if (!CropImage(frame, s.config.x, s.config.y, s.config.w, s.config.h, crop, error)) { SetStatus(s.hwnd, L"XEM ROI ẢNH 1 FAIL • " + error); return; }
    ShowPreviewModal(s.hwnd, crop, L"ROI ẢNH 1 • " + std::to_wstring(crop.width) + L"x" + std::to_wstring(crop.height) + L" • " + backend);
}

void ResetFullRegion(State& s) {
    int cw=0,ch=0;CurrentClientSize(s.target.gameWindow,cw,ch);
    s.config.x=0;s.config.y=0;s.config.w=0;s.config.h=0;s.config.roiBaseW=cw;s.config.roiBaseH=ch;
    SetEditInt(s.hwnd,IDC_X,0);SetEditInt(s.hwnd,IDC_Y,0);SetEditInt(s.hwnd,IDC_W,0);SetEditInt(s.hwnd,IDC_H,0);
    g_lastConfig=s.config;SetStatus(s.hwnd,L"ROI ẢNH 1 = FULL CLIENT");
}

void SelectNamedRegion(State& s, ScanRoi& roi, int xId, int yId, int wId, int hId, const wchar_t* label) {
    SyncConfig(s);
    Image frame{}; std::wstring backend, error;
    if (!CaptureClient(s.target.gameWindow, frame, backend, error)) {
        SetStatus(s.hwnd, std::wstring(L"CHỌN ") + label + L" FAIL • " + error); return;
    }
    RECT region{};
    if (!PickRegionModal(s.hwnd, frame, region)) {
        SetStatus(s.hwnd, std::wstring(label) + L": đã hủy, giữ nguyên vùng cũ"); return;
    }
    roi.x = region.left; roi.y = region.top; roi.w = region.right-region.left; roi.h = region.bottom-region.top;
    roi.baseW = frame.width; roi.baseH = frame.height;
    if(xId>0)SetEditInt(s.hwnd,xId,roi.x);if(yId>0)SetEditInt(s.hwnd,yId,roi.y);if(wId>0)SetEditInt(s.hwnd,wId,roi.w);if(hId>0)SetEditInt(s.hwnd,hId,roi.h);
    g_lastConfig=s.config;
    SetStatus(s.hwnd,std::wstring(label)+L" PASS • "+std::to_wstring(roi.x)+L","+std::to_wstring(roi.y)+L"/"+std::to_wstring(roi.w)+L"x"+std::to_wstring(roi.h)+L" • "+backend);
}

void PreviewNamedRegion(State& s, const ScanRoi& roi, const wchar_t* label) {
    SyncConfig(s);
    Image frame{},crop{};std::wstring backend,error;
    if(!CaptureClient(s.target.gameWindow,frame,backend,error)){SetStatus(s.hwnd,std::wstring(L"XEM ")+label+L" FAIL • "+error);return;}
    int rx=roi.x,ry=roi.y,rw=roi.w,rh=roi.h;
    if(roi.baseW>0&&roi.baseH>0&&(roi.baseW!=frame.width||roi.baseH!=frame.height)){
        rx=ScaleCoord(rx,roi.baseW,frame.width);ry=ScaleCoord(ry,roi.baseH,frame.height);
        if(rw>0)rw=std::max(1,ScaleCoord(rw,roi.baseW,frame.width));
        if(rh>0)rh=std::max(1,ScaleCoord(rh,roi.baseH,frame.height));
    }
    if(!CropImage(frame,rx,ry,rw,rh,crop,error)){SetStatus(s.hwnd,std::wstring(L"XEM ")+label+L" FAIL • "+error);return;}
    ShowPreviewModal(s.hwnd,crop,std::wstring(label)+L" • "+std::to_wstring(crop.width)+L"x"+std::to_wstring(crop.height)+L" • "+backend);
}

void ResetNamedRegion(State& s, ScanRoi& roi, int xId, int yId, int wId, int hId, const wchar_t* label) {
    int cw=0,ch=0;CurrentClientSize(s.target.gameWindow,cw,ch);roi={};roi.baseW=cw;roi.baseH=ch;
    SetEditInt(s.hwnd,xId,0);SetEditInt(s.hwnd,yId,0);SetEditInt(s.hwnd,wId,0);SetEditInt(s.hwnd,hId,0);
    g_lastConfig=s.config;SetStatus(s.hwnd,std::wstring(label)+L" = FULL CLIENT");
}

void SelectDiscardRegion(State& s){SelectNamedRegion(s,s.config.discardRoi,IDC_DISCARD_X,IDC_DISCARD_Y,IDC_DISCARD_W,IDC_DISCARD_H,L"ROI VỨT");}
void PreviewDiscardRegion(State& s){PreviewNamedRegion(s,s.config.discardRoi,L"ROI VỨT");}
void ResetDiscardRegion(State& s){ResetNamedRegion(s,s.config.discardRoi,IDC_DISCARD_X,IDC_DISCARD_Y,IDC_DISCARD_W,IDC_DISCARD_H,L"ROI VỨT");}
void SelectCloseRegion(State& s){SelectNamedRegion(s,s.config.closeRoi,IDC_CLOSE_X,IDC_CLOSE_Y,IDC_CLOSE_W,IDC_CLOSE_H,L"ROI DẤU X");}
void PreviewCloseRegion(State& s){PreviewNamedRegion(s,s.config.closeRoi,L"ROI DẤU X");}
void ResetCloseRegion(State& s){ResetNamedRegion(s,s.config.closeRoi,IDC_CLOSE_X,IDC_CLOSE_Y,IDC_CLOSE_W,IDC_CLOSE_H,L"ROI DẤU X");}

void AddStep(State& s) {
    SaveStepEditor(s);
    ClickStep step{};int cw=0,ch=0;if(CurrentClientSize(s.target.gameWindow,cw,ch)){step.baseW=cw;step.baseH=ch;}
    s.config.steps.push_back(step);g_lastConfig=s.config;RefreshStepList(s,static_cast<int>(s.config.steps.size()-1));
    SetStatus(s.hwnd,L"Đã thêm CLICK "+std::to_wstring(s.config.steps.size())+L" • không có giới hạn cứng");
}

void DeleteStep(State& s) {
    const int row=SelectedStep(s);if(row<0||row>=static_cast<int>(s.config.steps.size()))return;
    s.config.steps.erase(s.config.steps.begin()+row);g_lastConfig=s.config;
    RefreshStepList(s,std::min(row,static_cast<int>(s.config.steps.size())-1));
}

void MoveStep(State& s,int delta){
    SaveStepEditor(s);const int row=SelectedStep(s),next=row+delta;
    if(row<0||next<0||row>=static_cast<int>(s.config.steps.size())||next>=static_cast<int>(s.config.steps.size()))return;
    std::swap(s.config.steps[static_cast<std::size_t>(row)],s.config.steps[static_cast<std::size_t>(next)]);
    g_lastConfig=s.config;RefreshStepList(s,next);
}

void SaveSelectedStep(State& s){
    const int row=SelectedStep(s);if(row<0){SetStatus(s.hwnd,L"Chọn một dòng click trước");return;}
    SaveStepEditor(s);RefreshStepList(s,row);SetStatus(s.hwnd,L"Đã lưu GRID "+std::to_wstring(row));
}

void ArmF8(State& s){
    const int row=SelectedStep(s);if(row<0||row>=static_cast<int>(s.config.steps.size())){SetStatus(s.hwnd,L"F8: chọn một dòng CLICK trước");return;}
    s.captureRow=row;s.f8WasDown=(GetAsyncKeyState(VK_F8)&0x8000)!=0;SetTimer(s.hwnd,kF8PollTimer,30,nullptr);
    SetStatus(s.hwnd,L"ĐÃ ARM F8 CHO GRID "+std::to_wstring(row)+L" • đưa chuột vào đúng ô đồ trong GAME rồi F8");
}

void StartRec42At(State& s,int startGrid,bool fromSelected){
    if(s.runningChain){SetStatus(s.hwnd,L"Đang chạy FILTER • dừng trước khi REC");return;}
    if(s.rec42Active){SetStatus(s.hwnd,L"REC đang chạy • bấm DỪNG REC hoặc ESC trước");return;}
    if(startGrid<0||startGrid>=42){SetStatus(s.hwnd,L"REC: GRID bắt đầu không hợp lệ");return;}
    SyncConfig(s);
    if(s.config.steps.size()!=42)s.config.steps.resize(42);
    s.rec42Buffer.clear();s.rec42Buffer.reserve(static_cast<std::size_t>(42-startGrid));
    s.rec42StartGrid=startGrid;s.rec42FromSelected=fromSelected;
    s.rec42MouseWasDown=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;s.rec42Active=true;
    SetTimer(s.hwnd,kRec42PollTimer,kRec42PollMs,nullptr);
    if(fromSelected)SetStatus(s.hwnd,L"REC TỪ GRID "+std::to_wstring(startGrid)+L" • click trong GAME • DỪNG REC để ghi đoạn đã lấy");
    else SetStatus(s.hwnd,L"REC FULL 0–41 • click trong GAME • 0/42");
}
void StartRec42(State& s){StartRec42At(s,0,false);}
void StartRec42FromSelected(State& s){
    const int row=SelectedStep(s);if(row<0||row>=42){SetStatus(s.hwnd,L"REC TỪ GRID: chọn GRID 0..41 trước");return;}
    StartRec42At(s,row,true);
}
void StopRec42(State& s,bool cancel=false){
    KillTimer(s.hwnd,kRec42PollTimer);if(!s.rec42Active)return;s.rec42Active=false;s.rec42MouseWasDown=false;
    const std::size_t n=s.rec42Buffer.size();const int start=s.rec42StartGrid;const bool partial=s.rec42FromSelected;
    s.rec42StartGrid=0;s.rec42FromSelected=false;
    if(cancel){s.rec42Buffer.clear();SetStatus(s.hwnd,L"REC ĐÃ HỦY • GRID cũ giữ nguyên");return;}
    if(!partial&&n!=42){s.rec42Buffer.clear();SetStatus(s.hwnd,L"REC FULL chưa đủ "+std::to_wstring(n)+L"/42 • KHÔNG ghi đè GRID cũ");return;}
    if(partial&&n==0){s.rec42Buffer.clear();SetStatus(s.hwnd,L"REC TỪ GRID chưa có điểm mới • GRID cũ giữ nguyên");return;}
    if(s.config.steps.size()!=42)s.config.steps.resize(42);
    if(partial){
        const std::size_t maxCount=static_cast<std::size_t>(42-start);const std::size_t count=std::min(n,maxCount);
        for(std::size_t i=0;i<count;++i)s.config.steps[static_cast<std::size_t>(start)+i]=s.rec42Buffer[i];
        const int end=start+static_cast<int>(count)-1;s.rec42Buffer.clear();g_lastConfig=s.config;SavePersistentConfig();RefreshStepList(s,end);
        SetStatus(s.hwnd,L"REC ĐOẠN PASS • ghi đè GRID "+std::to_wstring(start)+L".."+std::to_wstring(end)+L" • Grid khác giữ nguyên");return;
    }
    s.config.steps=s.rec42Buffer;s.rec42Buffer.clear();g_lastConfig=s.config;SavePersistentConfig();RefreshStepList(s,41);SetStatus(s.hwnd,L"REC FULL PASS • đã ghi đè GRID 0..41");
}
void PollRec42(State& s){
    if(!s.rec42Active)return;if(GetAsyncKeyState(VK_ESCAPE)&0x8000){StopRec42(s,true);return;}const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    const std::size_t maxCount=static_cast<std::size_t>(42-s.rec42StartGrid);
    if(down&&!s.rec42MouseWasDown&&s.rec42Buffer.size()<maxCount){POINT p{};if(GetCursorPos(&p)){POINT client=p;if(ScreenToClient(s.target.gameWindow,&client)){int cw=0,ch=0;if(CurrentClientSize(s.target.gameWindow,cw,ch)&&client.x>=0&&client.y>=0&&client.x<cw&&client.y<ch){ClickStep step{};step.x=client.x;step.y=client.y;step.baseW=cw;step.baseH=ch;step.valid=true;step.repeat=1;s.rec42Buffer.push_back(step);const std::size_t n=s.rec42Buffer.size();const int captured=s.rec42StartGrid+static_cast<int>(n)-1;const int next=s.rec42StartGrid+static_cast<int>(n);if(n>=maxCount)SetStatus(s.hwnd,L"REC ĐÃ TỚI GRID 41 • bấm DỪNG REC để commit");else SetStatus(s.hwnd,L"REC PASS GRID "+std::to_wstring(captured)+L" • tiếp theo GRID "+std::to_wstring(next)+L"/41");}}}}
    s.rec42MouseWasDown=down;
}
void DeleteSelectedGrid(State& s){
    if(s.runningChain||s.rec42Active){SetStatus(s.hwnd,L"XÓA GRID: dừng FILTER/REC trước");return;}
    const int row=SelectedStep(s);if(row<0||row>=42){SetStatus(s.hwnd,L"XÓA GRID: chọn GRID 0..41 trước");return;}
    if(s.config.steps.size()!=42)s.config.steps.resize(42);s.config.steps[static_cast<std::size_t>(row)]={};g_lastConfig=s.config;SavePersistentConfig();RefreshStepList(s,row);SetStatus(s.hwnd,L"ĐÃ XÓA GRID "+std::to_wstring(row)+L" • không shift Grid khác");
}
void DeleteAllGrid(State& s){
    if(s.runningChain||s.rec42Active){SetStatus(s.hwnd,L"XÓA TẤT CẢ: dừng FILTER/REC trước");return;}
    if(MessageBoxW(s.hwnd,L"Xóa toàn bộ tọa FILTER GRID 0..41?",L"XÁC NHẬN XÓA GRID",MB_ICONWARNING|MB_YESNO|MB_DEFBUTTON2)!=IDYES){SetStatus(s.hwnd,L"Đã hủy XÓA TẤT CẢ");return;}
    s.config.steps.assign(42,ClickStep{});g_lastConfig=s.config;SavePersistentConfig();RefreshStepList(s,0);SetStatus(s.hwnd,L"ĐÃ XÓA TẤT CẢ GRID 0..41 • mapping vẫn cố định");
}

void ArmBagF8(State& s,int code,const wchar_t* label){
    s.captureRow=code;s.f8WasDown=(GetAsyncKeyState(VK_F8)&0x8000)!=0;SetTimer(s.hwnd,kF8PollTimer,30,nullptr);
    SetStatus(s.hwnd,L"ĐÃ ARM F8 CHO "+std::wstring(label)+L" • đưa chuột vào GAME rồi F8");
}

void PollF8(State& s){
    const bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
    if(s.captureRow!=-1&&down&&!s.f8WasDown){
        POINT p{};GetCursorPos(&p);POINT client=p;
        if(!ScreenToClient(s.target.gameWindow,&client)){SetStatus(s.hwnd,L"F8 FAIL • ScreenToClient");}
        else{
            int cw=0,ch=0;if(!CurrentClientSize(s.target.gameWindow,cw,ch)||client.x<0||client.y<0||client.x>=cw||client.y>=ch){
                SetStatus(s.hwnd,L"F8: chuột chưa nằm trong client game • vẫn đang chờ F8");
            }else if(s.captureRow==-3||s.captureRow==-4||s.captureRow==-5||s.captureRow==-6){
                ClickStep* step=s.captureRow==-3?&s.config.bagUiSwitch:(s.captureRow==-4?&s.config.swipeStart:(s.captureRow==-5?&s.config.swipeEnd:&s.config.bagOpenClick));
                const int xId=s.captureRow==-3?IDC_PRECHECK_X:(s.captureRow==-6?IDC_BAG_OPEN_X:0);
                const int yId=s.captureRow==-3?IDC_PRECHECK_Y:(s.captureRow==-6?IDC_BAG_OPEN_Y:0);
                const wchar_t* label=s.captureRow==-3?L"PRECHECK TAY NẢI":(s.captureRow==-4?L"VUỐT START":(s.captureRow==-5?L"VUỐT END":L"MỞ TAY NẢI"));
                step->x=client.x;step->y=client.y;step->baseW=cw;step->baseH=ch;step->valid=true;step->repeat=1;
                s.captureRow=-1;KillTimer(s.hwnd,kF8PollTimer);g_lastConfig=s.config;if(xId>0)SetEditInt(s.hwnd,xId,step->x);if(yId>0)SetEditInt(s.hwnd,yId,step->y);
                SetStatus(s.hwnd,L"F8 PASS • "+std::wstring(label)+L" = "+std::to_wstring(step->x)+L","+std::to_wstring(step->y)+L" @ "+std::to_wstring(cw)+L"x"+std::to_wstring(ch));
            }else if(s.captureRow>=0&&s.captureRow<static_cast<int>(s.config.steps.size())){
                ClickStep& step=s.config.steps[static_cast<std::size_t>(s.captureRow)];
                step.x=client.x;step.y=client.y;step.baseW=cw;step.baseH=ch;step.valid=true;step.repeat=1;
                const int row=s.captureRow;s.captureRow=-1;KillTimer(s.hwnd,kF8PollTimer);g_lastConfig=s.config;RefreshStepList(s,row);
                SetStatus(s.hwnd,L"F8 PASS • GRID "+std::to_wstring(row)+L" = "+std::to_wstring(step.x)+L","+std::to_wstring(step.y)+L" @ "+std::to_wstring(cw)+L"x"+std::to_wstring(ch));
            }
        }
    }
    s.f8WasDown=down;
}

bool ResolveStepPoint(const ClickStep& step,int currentW,int currentH,int& x,int& y){
    if(!step.valid||step.baseW<=0||step.baseH<=0||currentW<=0||currentH<=0)return false;
    x=ScaleCoord(step.x,step.baseW,currentW);y=ScaleCoord(step.y,step.baseH,currentH);
    x=std::clamp(x,0,currentW-1);y=std::clamp(y,0,currentH-1);return true;
}

void TestSwipe(State& s){
    SyncConfig(s);
    if(!s.config.swipeStart.valid||!s.config.swipeEnd.valid){SetStatus(s.hwnd,L"TEST VUỐT: chưa đủ F8 ĐẦU + F8 CUỐI");return;}
    if(!s.target.hiddenDrag){SetStatus(s.hwnd,L"TEST VUỐT: bridge drag chưa sẵn sàng");return;}
    int cw=0,ch=0,sx=0,sy=0,ex=0,ey=0;
    if(!CurrentClientSize(s.target.gameWindow,cw,ch)||!ResolveStepPoint(s.config.swipeStart,cw,ch,sx,sy)||!ResolveStepPoint(s.config.swipeEnd,cw,ch,ex,ey)){
        SetStatus(s.hwnd,L"TEST VUỐT: không resolve được START/END");return;
    }
    std::wstring detail;const bool ok=s.target.hiddenDrag(s.target.context,sx,sy,ex,ey,cw,ch,detail);
    SetStatus(s.hwnd,std::wstring(L"TEST VUỐT ")+(ok?L"PASS • ":L"FAIL • ")+detail);
}

void TestSelectedStep(State& s){
    SaveStepEditor(s);const int row=SelectedStep(s);
    if(row<0||row>=static_cast<int>(s.config.steps.size())){SetStatus(s.hwnd,L"TEST DÒNG: chưa chọn dòng");return;}
    int cw=0,ch=0,x=0,y=0;if(!CurrentClientSize(s.target.gameWindow,cw,ch)||!ResolveStepPoint(s.config.steps[static_cast<std::size_t>(row)],cw,ch,x,y)){
        SetStatus(s.hwnd,L"TEST DÒNG FAIL • dòng chưa có tọa F8 hợp lệ");return;
    }
    std::wstring detail;const bool ok=s.target.hiddenClick&&s.target.hiddenClick(s.target.context,x,y,cw,ch,detail);
    SetStatus(s.hwnd,L"TEST GRID "+std::to_wstring(row)+(ok?L" PASS • no-sleep • ":L" FAIL • ")+std::to_wstring(x)+L","+std::to_wstring(y)+L" • "+detail);
}

bool ValidateFilter(const State& s,std::wstring& error){
    if(s.config.templatePath.empty()){error=L"chưa chọn ẢNH 1 (đồ đúng)";return false;}
    if(s.config.closeTemplatePath.empty()){error=L"chưa chọn ảnh DẤU X";return false;}
    if(s.config.steps.size()!=42){error=L"FILTER cần đúng GRID 0..41 (42 tọa)";return false;}
    for(std::size_t ix=0;ix<s.config.steps.size();++ix){
        const ClickStep& step=s.config.steps[ix];
        if(!step.valid||step.baseW<=0||step.baseH<=0){error=L"GRID "+std::to_wstring(ix)+L" chưa gán tọa F8";return false;}
    }
    return true;
}

void ResolveGoodRoi(const Config& c,int currentW,int currentH,int& rx,int& ry,int& rw,int& rh){
    rx=c.x;ry=c.y;rw=c.w;rh=c.h;
    if(c.roiBaseW>0&&c.roiBaseH>0&&(c.roiBaseW!=currentW||c.roiBaseH!=currentH)){
        rx=ScaleCoord(rx,c.roiBaseW,currentW);ry=ScaleCoord(ry,c.roiBaseH,currentH);
        if(rw>0)rw=std::max(1,ScaleCoord(rw,c.roiBaseW,currentW));
        if(rh>0)rh=std::max(1,ScaleCoord(rh,c.roiBaseH,currentH));
    }
}

void ResolveNamedRoi(const ScanRoi& r,int currentW,int currentH,int& rx,int& ry,int& rw,int& rh){
    rx=r.x;ry=r.y;rw=r.w;rh=r.h;
    if(r.baseW>0&&r.baseH>0&&(r.baseW!=currentW||r.baseH!=currentH)){
        rx=ScaleCoord(rx,r.baseW,currentW);ry=ScaleCoord(ry,r.baseH,currentH);
        if(rw>0)rw=std::max(1,ScaleCoord(rw,r.baseW,currentW));
        if(rh>0)rh=std::max(1,ScaleCoord(rh,r.baseH,currentH));
    }
}

std::wstring ScoreText(double score){
    if(score<0.0)return L"N/A";wchar_t b[40]{};swprintf_s(b,L"%.2f%%",score*100.0);return b;
}

bool ScanFrameRect(const State& s,const Image& frame,const Image& tpl,int rx,int ry,int rw,int rh,Match& match,std::wstring& error){
    match=FindTemplate(frame,tpl,rx,ry,rw,rh,static_cast<double>(s.config.thresholdPercent)/100.0,error);
    return match.score>=0.0;
}

bool ScanGoodOnFrame(const State& s,const Image& frame,Match& match,std::wstring& error){
    int rx=0,ry=0,rw=0,rh=0;ResolveGoodRoi(s.config,frame.width,frame.height,rx,ry,rw,rh);
    return ScanFrameRect(s,frame,s.goodTpl,rx,ry,rw,rh,match,error);
}

bool ScanCloseOnFrame(const State& s,const Image& frame,Match& match,std::wstring& error){
    int rx=0,ry=0,rw=0,rh=0;ResolveNamedRoi(s.config.closeRoi,frame.width,frame.height,rx,ry,rw,rh);
    return ScanFrameRect(s,frame,s.closeTpl,rx,ry,rw,rh,match,error);
}

bool RawClick(State& s,int x,int y,int cw,int ch,std::wstring& error){
    if(!s.target.hiddenClick){error=L"không có RAW hidden-click callback";return false;}
    std::wstring detail;if(!s.target.hiddenClick(s.target.context,x,y,cw,ch,detail)){error=detail;return false;}
    s.lastActionTick=GetTickCount64();
    return true;
}

bool SemanticDiscard(State& s,bool probeOnly,std::wstring& detail){
    if(!s.target.semanticDiscard){detail=L"semantic VỨT callback chưa có";return false;}
    const bool ok=s.target.semanticDiscard(s.target.context,probeOnly,detail);
    if(ok&&!probeOnly)s.lastActionTick=GetTickCount64();
    return ok;
}

bool SemanticDiscardConfirm(State& s,bool probeOnly,std::wstring& detail){
    if(!s.target.semanticDiscardConfirm){detail=L"semantic XÁC NHẬN SAU VỨT callback chưa có";return false;}
    const bool ok=s.target.semanticDiscardConfirm(s.target.context,probeOnly,detail);
    if(ok&&!probeOnly)s.lastActionTick=GetTickCount64();
    return ok;
}

constexpr UINT kStateTimeoutMs = 5000;

void ArmRunPhase(State& s,RunPhase phase,UINT firstProbeDelay=0){
    (void)firstProbeDelay;
    const ULONGLONG now=GetTickCount64();s.runPhase=phase;
    s.nextProbeTick=con_filter_nosleep_logic::NotBefore(now,s.lastActionTick,s.config.minFilterActionMs);
    s.phaseDeadlineTick=now+static_cast<ULONGLONG>(kStateTimeoutMs);
}

void FinishRun(State& s,const std::wstring& status){
    KillTimer(s.hwnd,kRunTimer);s.runningChain=false;s.runPhase=RunPhase::Idle;EnableWindow(GetDlgItem(s.hwnd,IDC_TEST),TRUE);SetStatus(s.hwnd,status);
}

bool ClickCurrentSlot(State& s,std::wstring& error){
    if(s.slotIndex>=s.config.steps.size()){error=L"slot index vượt cấu hình";return false;}
    int cw=0,ch=0,x=0,y=0;if(!CurrentClientSize(s.target.gameWindow,cw,ch)||!ResolveStepPoint(s.config.steps[s.slotIndex],cw,ch,x,y)){error=L"không resolve được tọa CLICK";return false;}
    return RawClick(s,x,y,cw,ch,error);
}

bool ProbeTimedOut(const State& s,ULONGLONG now){return now>=s.phaseDeadlineTick;}
void ScheduleNextProbe(State& s,ULONGLONG now){s.nextProbeTick=now+kProbeIntervalMs;}

void ProcessRunTick(State& s){
    if(!s.runningChain)return;
    if((GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0){FinishRun(s,L"ĐÃ DỪNG BẰNG ESC tại CLICK "+std::to_wstring(s.slotIndex+1));return;}
    if(!s.target.gameWindow||!IsWindow(s.target.gameWindow)){FinishRun(s,L"FAIL • cửa sổ game đã mất");return;}
    const ULONGLONG now=GetTickCount64();if(now<s.nextProbeTick)return;

    Image frame{};std::wstring backend,error;
    if(!CaptureClient(s.target.gameWindow,frame,backend,error)){FinishRun(s,L"CAPTURE FAIL • "+error);return;}

    if(s.runPhase==RunPhase::WaitItemReady){
        Match good{};if(!ScanGoodOnFrame(s,frame,good,error)){FinishRun(s,L"ROI ẢNH 1 FAIL • "+error);return;}
        if(good.found){
            Match close{};if(!ScanCloseOnFrame(s,frame,close,error)){FinishRun(s,L"ROI DẤU X FAIL • "+error);return;}
            if(close.found){
                if(!RawClick(s,close.x+s.closeTpl.width/2,close.y+s.closeTpl.height/2,frame.width,frame.height,error)){FinishRun(s,L"CLICK TÂM X FAIL • "+error);return;}
                SetStatus(s.hwnd,L"CLICK "+std::to_wstring(s.slotIndex+1)+L" • GOOD → X • no-sleep");ArmRunPhase(s,RunPhase::WaitCloseGone);return;
            }
            if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT • GOOD nhưng DẤU X chưa xuất hiện");return;}ScheduleNextProbe(s,now);return;
        }
        std::wstring sem;if(SemanticDiscard(s,true,sem)){
            if(!SemanticDiscard(s,false,sem)){FinishRun(s,L"SEMANTIC VỨT callback FAIL • "+sem);return;}
            SetStatus(s.hwnd,L"CLICK "+std::to_wstring(s.slotIndex+1)+L" • BAD → semantic VỨT • no-sleep");ArmRunPhase(s,RunPhase::WaitDiscardGone);return;
        }
        const auto failure=con_filter_nosleep_logic::ClassifySemanticFailure(sem);
        if(failure==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){FinishRun(s,L"SEMANTIC VỨT AMBIGUOUS • fail-closed");return;}
        if(failure==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){s.runPhase=RunPhase::WaitEmptyRetry;s.nextProbeTick=now+kEmptyRetryMs;s.phaseDeadlineTick=0;SetStatus(s.hwnd,L"NO GOOD + NO semantic VỨT = EMPTY • chờ 10s rồi click lại cùng ô");return;}
        if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT semantic VỨT • "+sem);return;}ScheduleNextProbe(s,now);return;
    }
    if(s.runPhase==RunPhase::WaitEmptyRetry){if(!ClickCurrentSlot(s,error)){FinishRun(s,L"EMPTY RETRY FAIL • "+error);return;}ArmRunPhase(s,RunPhase::WaitItemReady);return;}
    if(s.runPhase==RunPhase::WaitDiscardGone){
        std::wstring sem;if(SemanticDiscard(s,true,sem)){if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT • semantic VỨT chưa biến mất");return;}ScheduleNextProbe(s,now);return;}
        const auto failure=con_filter_nosleep_logic::ClassifySemanticFailure(sem);
        if(failure==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){FinishRun(s,L"SEMANTIC VỨT AMBIGUOUS sau callback");return;}
        if(failure==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){
            const ULONGLONG due=s.lastActionTick+static_cast<ULONGLONG>(std::max(0,s.config.minDiscardConfirmMs));
            if(s.phaseDeadlineTick<due+kStateTimeoutMs)s.phaseDeadlineTick=due+kStateTimeoutMs;
            if(now<due){s.nextProbeTick=due;return;}
            std::wstring confirm;if(SemanticDiscardConfirm(s,true,confirm)){
                if(!SemanticDiscardConfirm(s,false,confirm)){FinishRun(s,L"SEMANTIC XÁC NHẬN SAU VỨT FAIL • "+confirm);return;}
                ArmRunPhase(s,RunPhase::WaitPopupGoneAfterConfirm);return;
            }
            const auto cf=con_filter_nosleep_logic::ClassifySemanticFailure(confirm);
            if(cf==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){FinishRun(s,L"SEMANTIC XÁC NHẬN SAU VỨT AMBIGUOUS");return;}
            if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT semantic XÁC NHẬN SAU VỨT • "+confirm);return;}
            ScheduleNextProbe(s,now);return;
        }
        if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT probe VỨT • "+sem);return;}ScheduleNextProbe(s,now);return;
    }
    if(s.runPhase==RunPhase::WaitPopupGoneAfterConfirm){
        Match good{},close{};std::wstring e1,e2;if(!ScanGoodOnFrame(s,frame,good,e1)||!ScanCloseOnFrame(s,frame,close,e2)){FinishRun(s,L"SCAN sau VỨT FAIL • "+(!e1.empty()?e1:e2));return;}
        std::wstring sem;const bool discardStill=SemanticDiscard(s,true,sem);const auto failure=discardStill?con_filter_nosleep_logic::SemanticProbeFailure::Retry:con_filter_nosleep_logic::ClassifySemanticFailure(sem);
        if(!discardStill&&failure==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){FinishRun(s,L"SEMANTIC VỨT AMBIGUOUS sau xác nhận");return;}
        if(!good.found&&!close.found&&!discardStill&&failure==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){++s.discardCount;if(!ClickCurrentSlot(s,error)){FinishRun(s,L"LẶP SAME SLOT FAIL • "+error);return;}ArmRunPhase(s,RunPhase::WaitItemReady);return;}
        if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT • popup sau VỨT chưa đóng");return;}ScheduleNextProbe(s,now);return;
    }
    if(s.runPhase==RunPhase::WaitCloseGone){
        Match close{};if(!ScanCloseOnFrame(s,frame,close,error)){FinishRun(s,L"ROI DẤU X FAIL • "+error);return;}
        if(!close.found){++s.slotIndex;s.discardCount=0;if(s.slotIndex>=s.config.steps.size()){FinishRun(s,L"LỌC HOÀN TẤT • semantic VỨT • no fixed Sleep");return;}if(!ClickCurrentSlot(s,error)){FinishRun(s,L"CLICK NEXT FAIL • "+error);return;}ArmRunPhase(s,RunPhase::WaitItemReady);return;}
        if(ProbeTimedOut(s,now)){FinishRun(s,L"TIMEOUT • DẤU X không biến mất");return;}ScheduleNextProbe(s,now);return;
    }
}

void RunTest(State& s) {
    if(s.runningChain)return;SyncConfig(s);
    if(!s.target.gameWindow||!IsWindow(s.target.gameWindow)){SetStatus(s.hwnd,L"FAIL • cửa sổ game đã mất");return;}
    if(!s.target.semanticDiscard){SetStatus(s.hwnd,L"FAIL • semantic VỨT callback chưa sẵn sàng");return;}
    std::wstring error;if(!ValidateFilter(s,error)){SetStatus(s.hwnd,L"CHƯA THỂ CHẠY • "+error);return;}
    if(!LoadImageWic(s.config.templatePath,s.goodTpl,error)){SetStatus(s.hwnd,L"ẢNH 1 FAIL • "+error);return;}
    if(!LoadImageWic(s.config.closeTemplatePath,s.closeTpl,error)){SetStatus(s.hwnd,L"ẢNH X FAIL • "+error);return;}
    s.runningChain=true;s.slotIndex=0;s.discardCount=0;s.lastActionTick=0;s.runPhase=RunPhase::Idle;EnableWindow(GetDlgItem(s.hwnd,IDC_TEST),FALSE);
    if(!ClickCurrentSlot(s,error)){FinishRun(s,L"CLICK 1 FAIL • "+error);return;}
    ArmRunPhase(s,RunPhase::WaitItemReady);SetTimer(s.hwnd,kRunTimer,10,nullptr);
    SetStatus(s.hwnd,L"V4 NO-SLEEP START • GOOD/X image giữ nguyên • VỨT = semantic callback • MIN LỌC CON gate");
}

void AddRoiEditors(State& s,int y,int xId,int yId,int wId,int hId,int pickId,int previewId,int fullId,
                   int roiX,int roiY,int roiW,int roiH,const wchar_t* label){
    const int left=18;
    Add(s.hwnd,L"STATIC",label,SS_LEFT|SS_CENTERIMAGE,left,y,80,24,0);
    Add(s.hwnd,L"STATIC",L"X",SS_CENTER|SS_CENTERIMAGE,left+82,y,16,24,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(roiX).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,left+98,y,54,24,xId);
    Add(s.hwnd,L"STATIC",L"Y",SS_CENTER|SS_CENTERIMAGE,left+154,y,16,24,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(roiY).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,left+170,y,54,24,yId);
    Add(s.hwnd,L"STATIC",L"W",SS_CENTER|SS_CENTERIMAGE,left+226,y,16,24,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(roiW).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,left+242,y,58,24,wId);
    Add(s.hwnd,L"STATIC",L"H",SS_CENTER|SS_CENTERIMAGE,left+302,y,16,24,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(roiH).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,left+318,y,58,24,hId);
    Add(s.hwnd,L"BUTTON",L"CHỌN VÙNG",BS_PUSHBUTTON,left+386,y-2,112,28,pickId);
    Add(s.hwnd,L"BUTTON",L"XEM",BS_PUSHBUTTON,left+504,y-2,68,28,previewId);
    Add(s.hwnd,L"BUTTON",L"FULL",BS_PUSHBUTTON,left+578,y-2,68,28,fullId);
}

void BuildControls(State& s) {
    Add(s.hwnd,L"STATIC",(L"ACC TEST: "+s.target.accountLabel).c_str(),SS_LEFT|SS_CENTERIMAGE,18,9,990,23,0);

    Add(s.hwnd,L"STATIC",L"ẢNH 1 • ĐỒ ĐÚNG (chỉ scan):",SS_LEFT|SS_CENTERIMAGE,18,38,205,25,0);
    Add(s.hwnd,L"EDIT",s.config.templatePath.c_str(),WS_BORDER|ES_AUTOHSCROLL,225,38,555,25,IDC_TEMPLATE);
    Add(s.hwnd,L"BUTTON",L"CHỌN ẢNH 1",BS_PUSHBUTTON,790,37,110,27,IDC_PICK);
    AddRoiEditors(s,69,IDC_X,IDC_Y,IDC_W,IDC_H,IDC_PICK_REGION,IDC_PREVIEW_REGION,IDC_FULL_REGION,s.config.x,s.config.y,s.config.w,s.config.h,L"ROI ẢNH 1");
    Add(s.hwnd,L"STATIC",L"Ngưỡng %",SS_LEFT|SS_CENTERIMAGE,835,69,65,24,0);Add(s.hwnd,L"EDIT",std::to_wstring(s.config.thresholdPercent).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,902,69,58,24,IDC_THRESHOLD);

    Add(s.hwnd,L"STATIC",L"NÚT VỨT • semantic callback nội bộ, không scan ảnh:",SS_LEFT|SS_CENTERIMAGE,18,105,480,25,0);
    Add(s.hwnd,L"STATIC",L"MIN LỌC CON (ms)",SS_LEFT|SS_CENTERIMAGE,610,105,150,25,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(s.config.minFilterActionMs).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,765,105,90,25,IDC_FILTER_MIN_ACTION);
    Add(s.hwnd,L"STATIC",L"0 = pure no-sleep",SS_LEFT|SS_CENTERIMAGE,862,105,130,25,0);
    Add(s.hwnd,L"STATIC",L"Exact semantic: VỨT / VỨT BỎ; nhiều candidate = fail-closed.",SS_LEFT|SS_CENTERIMAGE,18,136,760,25,0);

    Add(s.hwnd,L"STATIC",L"DẤU X • scan rồi click tâm:",SS_LEFT|SS_CENTERIMAGE,18,172,205,25,0);
    Add(s.hwnd,L"EDIT",s.config.closeTemplatePath.c_str(),WS_BORDER|ES_AUTOHSCROLL,225,172,555,25,IDC_TEMPLATE_CLOSE);
    Add(s.hwnd,L"BUTTON",L"CHỌN DẤU X",BS_PUSHBUTTON,790,171,110,27,IDC_PICK_CLOSE);
    AddRoiEditors(s,203,IDC_CLOSE_X,IDC_CLOSE_Y,IDC_CLOSE_W,IDC_CLOSE_H,IDC_CLOSE_REGION,IDC_CLOSE_PREVIEW,IDC_CLOSE_FULL,s.config.closeRoi.x,s.config.closeRoi.y,s.config.closeRoi.w,s.config.closeRoi.h,L"ROI DẤU X");
    Add(s.hwnd,L"STATIC",L"V4: 3 ROI khác nhau nhưng mọi scan trong cùng một probe dùng CHUNG 1 frame PrintWindow.",SS_LEFT|SS_CENTERIMAGE,675,203,330,24,0);

    Add(s.hwnd,L"BUTTON",L"GRID 0..41 • MAP POSITION 0..99 • tất cả click no-sleep, chỉ dùng MIN LỌC CON",BS_GROUPBOX,18,238,987,330,0);
    Add(s.hwnd,L"BUTTON",L"REC FULL 0–41",BS_PUSHBUTTON,350,236,145,26,IDC_STEP_REC42);
    Add(s.hwnd,L"BUTTON",L"REC TỪ GRID",BS_PUSHBUTTON,502,236,145,26,IDC_STEP_REC_FROM_SELECTED);
    Add(s.hwnd,L"BUTTON",L"DỪNG REC",BS_PUSHBUTTON,654,236,110,26,IDC_STEP_REC42_STOP);
    Add(s.hwnd,L"BUTTON",L"XÓA GRID",BS_PUSHBUTTON,771,236,105,26,IDC_STEP_DELETE_GRID);
    Add(s.hwnd,L"BUTTON",L"XÓA TẤT CẢ",BS_PUSHBUTTON,883,236,112,26,IDC_STEP_DELETE_ALL_GRID);
    s.stepList=Add(s.hwnd,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER,32,263,959,205,IDC_STEP_LIST);
    ListView_SetExtendedListViewStyle(s.stepList,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
    AddColumn(s.stepList,0,60,L"GRID");AddColumn(s.stepList,1,110,L"X");AddColumn(s.stepList,2,110,L"Y");AddColumn(s.stepList,3,150,L"Base size");AddColumn(s.stepList,4,500,L"MAP POSITION 0..99");

    Add(s.hwnd,L"STATIC",L"X:",SS_LEFT|SS_CENTERIMAGE,32,480,20,27,0);Add(s.hwnd,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,54,480,80,27,IDC_STEP_X);
    Add(s.hwnd,L"STATIC",L"Y:",SS_LEFT|SS_CENTERIMAGE,145,480,20,27,0);Add(s.hwnd,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,168,480,80,27,IDC_STEP_Y);
    Add(s.hwnd,L"BUTTON",L"LƯU TỌA",BS_PUSHBUTTON,270,480,115,27,IDC_STEP_SAVE);Add(s.hwnd,L"BUTTON",L"LẤY TỌA F8",BS_PUSHBUTTON,397,480,135,27,IDC_STEP_CAPTURE);Add(s.hwnd,L"BUTTON",L"TEST CLICK ẨN",BS_PUSHBUTTON,545,480,145,27,IDC_STEP_TEST);
    Add(s.hwnd,L"STATIC",L"TIME tối thiểu duy nhất ở trên • mặc định 0",SS_LEFT|SS_CENTERIMAGE,710,480,270,27,0);

    Add(s.hwnd,L"STATIC",L"TIME MIN XÁC NHẬN SAU VỨT (ms):",SS_LEFT|SS_CENTERIMAGE,32,522,260,27,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(s.config.minDiscardConfirmMs).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,300,522,90,27,IDC_DISCARD_CONFIRM_MIN);
    Add(s.hwnd,L"STATIC",L"0 = no-sleep • callback semantic XÁC NHẬN",SS_LEFT|SS_CENTERIMAGE,405,522,430,27,0);

    Add(s.hwnd,L"STATIC",L"STATE: CLICK N → GOOD? X : semantic VỨT; NO GOOD + NO VỨT = EMPTY.",SS_LEFT|SS_CENTERIMAGE,32,548,955,20,0);
    Add(s.hwnd,L"BUTTON",L"CHẠY TEST LỌC • SEMANTIC VỨT • NO-SLEEP",BS_DEFPUSHBUTTON,18,582,987,39,IDC_TEST);

    Add(s.hwnd,L"BUTTON",L"AUTO LỌC VK CHỈ CON • tick CON + 2 click mở tùy chọn + 1 click đóng",BS_GROUPBOX,18,628,987,190,0);
    Add(s.hwnd,L"STATIC",L"CON:",SS_LEFT|SS_CENTERIMAGE,32,648,40,24,0);
    Add(s.hwnd,L"STATIC",L"Tối đa CON scan cùng lúc (0 = TẮT)",SS_LEFT|SS_CENTERIMAGE,742,648,170,24,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(s.config.maxConcurrentScan).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,914,648,48,24,IDC_SCAN_MAX_CONCURRENT);
    for(int i=0;i<30;++i){
        const std::wstring label=L"C"+std::to_wstring(i+1);
        const int col=i%10,row=i/10;
        HWND cb=Add(s.hwnd,L"BUTTON",label.c_str(),BS_AUTOCHECKBOX,74+col*88,648+row*24,82,24,IDC_CHILD_SCAN_BASE+i);
        CheckDlgButton(s.hwnd,IDC_CHILD_SCAN_BASE+i,s.config.childEnabled[static_cast<std::size_t>(i)]?BST_CHECKED:BST_UNCHECKED);(void)cb;
    }

    Add(s.hwnd,L"STATIC",L"TAY NẢI UI DIRECT:",SS_LEFT|SS_CENTERIMAGE,32,724,150,25,0);
    Add(s.hwnd,L"STATIC",L"MỞ TAY NẢI = ButBag/ButBagClick • đóng bằng X Tay nải UI DIRECT • không dùng tọa mở/đóng túi",SS_LEFT|SS_CENTERIMAGE,185,724,790,25,0);
    Add(s.hwnd,L"BUTTON",L"XUẤT SCAN",BS_PUSHBUTTON,482,754,145,25,IDC_EXPORT_SCAN);Add(s.hwnd,L"BUTTON",L"NHẬP SCAN",BS_PUSHBUTTON,637,754,145,25,IDC_IMPORT_SCAN);
    Add(s.hwnd,L"STATIC",L"VUỐT:",SS_LEFT|SS_CENTERIMAGE,32,786,55,25,0);
    Add(s.hwnd,L"BUTTON",L"F8 ĐẦU",BS_PUSHBUTTON,88,786,88,25,IDC_SWIPE_START_CAPTURE);
    Add(s.hwnd,L"BUTTON",L"F8 CUỐI",BS_PUSHBUTTON,181,786,88,25,IDC_SWIPE_END_CAPTURE);
    Add(s.hwnd,L"BUTTON",L"TEST VUỐT",BS_PUSHBUTTON,274,786,100,25,IDC_SWIPE_TEST);
    Add(s.hwnd,L"STATIC",L"Số lần",SS_LEFT|SS_CENTERIMAGE,382,786,52,25,0);
    Add(s.hwnd,L"EDIT",std::to_wstring(s.config.maxSwipes).c_str(),WS_BORDER|ES_NUMBER|ES_CENTER,436,786,38,25,IDC_SCAN_MAX_SWIPES);
    Add(s.hwnd,L"STATIC",L"(1-2) • vuốt no-sleep + MIN",SS_LEFT|SS_CENTERIMAGE,478,786,220,25,0);

    Add(s.hwnd,L"BUTTON",L"PRECHECK TAY NẢI • UI DIRECT trước khi mở",BS_GROUPBOX,18,824,987,118,0);
    Add(s.hwnd,L"STATIC",L"Probe MỞ TAY NẢI trước. Nếu chưa thấy thì click tọa chuyển UI dưới đây và retry no-sleep đến TIME MAX.",SS_LEFT|SS_CENTERIMAGE,32,846,930,25,0);
    Add(s.hwnd,L"STATIC",L"Tọa chuyển UI X",SS_LEFT|SS_CENTERIMAGE,32,880,102,25,0);
    Add(s.hwnd,L"EDIT",s.config.bagUiSwitch.valid?std::to_wstring(s.config.bagUiSwitch.x).c_str():L"",WS_BORDER|ES_NUMBER|ES_CENTER,138,880,62,25,IDC_PRECHECK_X);
    Add(s.hwnd,L"STATIC",L"Y",SS_CENTER|SS_CENTERIMAGE,204,880,18,25,0);
    Add(s.hwnd,L"EDIT",s.config.bagUiSwitch.valid?std::to_wstring(s.config.bagUiSwitch.y).c_str():L"",WS_BORDER|ES_NUMBER|ES_CENTER,225,880,62,25,IDC_PRECHECK_Y);
    Add(s.hwnd,L"BUTTON",L"LẤY F8 PRECHECK",BS_PUSHBUTTON,305,880,160,25,IDC_PRECHECK_CAPTURE);
    Add(s.hwnd,L"STATIC",L"MỞ TAY NẢI X",SS_LEFT|SS_CENTERIMAGE,475,880,100,25,0);
    Add(s.hwnd,L"EDIT",s.config.bagOpenClick.valid?std::to_wstring(s.config.bagOpenClick.x).c_str():L"",WS_BORDER|ES_NUMBER|ES_CENTER,575,880,62,25,IDC_BAG_OPEN_X);
    Add(s.hwnd,L"STATIC",L"Y",SS_CENTER|SS_CENTERIMAGE,641,880,18,25,0);
    Add(s.hwnd,L"EDIT",s.config.bagOpenClick.valid?std::to_wstring(s.config.bagOpenClick.y).c_str():L"",WS_BORDER|ES_NUMBER|ES_CENTER,662,880,62,25,IDC_BAG_OPEN_Y);
    Add(s.hwnd,L"BUTTON",L"LẤY F8 MỞ BAG",BS_PUSHBUTTON,735,880,190,25,IDC_BAG_OPEN_CAPTURE);

    Add(s.hwnd,L"STATIC",L"Sẵn sàng. GRID 0..41 • P0..41 → vuốt1 → P42..83 → vuốt2 → P84..99 • mọi action no-sleep + MIN.",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,18,950,987,62,IDC_STATUS);
    Add(s.hwnd,L"BUTTON",L"ĐÓNG",BS_PUSHBUTTON,885,1020,120,30,IDCANCEL);
    RefreshStepList(s);RefreshAutoEditors(s);
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    State* s=reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);s=static_cast<State*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));if(s)s->hwnd=hwnd;}
    if(!s)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg){
        case WM_CREATE:BuildControls(*s);return 0;
        case WM_TIMER:if(wp==kF8PollTimer){PollF8(*s);return 0;}if(wp==kRec42PollTimer){PollRec42(*s);return 0;}if(wp==kRunTimer){ProcessRunTick(*s);return 0;}break;
        case WM_NOTIFY:{
            auto* hdr=reinterpret_cast<NMHDR*>(lp);
            if(hdr&&hdr->idFrom==IDC_STEP_LIST&&hdr->code==LVN_ITEMCHANGED){
                const auto* n=reinterpret_cast<NMLISTVIEW*>(hdr);
                if((n->uChanged&LVIF_STATE)!=0&&(n->uNewState&LVIS_SELECTED)!=0)LoadStepEditor(*s,n->iItem);
            }
            return 0;
        }
        case WM_COMMAND:
            if(s->runningChain && LOWORD(wp)!=IDCANCEL){SetStatus(s->hwnd,L"Đang chạy FILTER v4 • ESC để dừng trước khi sửa cấu hình");return 0;}
            switch(LOWORD(wp)){
                case IDC_PICK:PickTemplate(*s);return 0;
                case IDC_PICK_DISCARD:PickDiscardTemplate(*s);return 0;
                case IDC_PICK_CLOSE:PickCloseTemplate(*s);return 0;
                case IDC_PICK_REGION:SelectRegion(*s);return 0;
                case IDC_PREVIEW_REGION:PreviewRegion(*s);return 0;
                case IDC_FULL_REGION:ResetFullRegion(*s);return 0;
                case IDC_DISCARD_REGION:SelectDiscardRegion(*s);return 0;
                case IDC_DISCARD_PREVIEW:PreviewDiscardRegion(*s);return 0;
                case IDC_DISCARD_FULL:ResetDiscardRegion(*s);return 0;
                case IDC_CLOSE_REGION:SelectCloseRegion(*s);return 0;
                case IDC_CLOSE_PREVIEW:PreviewCloseRegion(*s);return 0;
                case IDC_CLOSE_FULL:ResetCloseRegion(*s);return 0;
                case IDC_STEP_ADD:AddStep(*s);return 0;
                case IDC_STEP_DELETE:DeleteStep(*s);return 0;
                case IDC_STEP_UP:MoveStep(*s,-1);return 0;
                case IDC_STEP_DOWN:MoveStep(*s,1);return 0;
                case IDC_STEP_SAVE:SaveSelectedStep(*s);return 0;
                case IDC_STEP_CAPTURE:ArmF8(*s);return 0;
                case IDC_STEP_TEST:TestSelectedStep(*s);return 0;
                case IDC_STEP_REC42:StartRec42(*s);return 0;
                case IDC_STEP_REC_FROM_SELECTED:StartRec42FromSelected(*s);return 0;
                case IDC_STEP_REC42_STOP:StopRec42(*s,false);return 0;
                case IDC_STEP_DELETE_GRID:DeleteSelectedGrid(*s);return 0;
                case IDC_STEP_DELETE_ALL_GRID:DeleteAllGrid(*s);return 0;
                case IDC_PRECHECK_CAPTURE:ArmBagF8(*s,-3,L"PRECHECK TAY NẢI");return 0;
                case IDC_BAG_OPEN_CAPTURE:ArmBagF8(*s,-6,L"MỞ TAY NẢI");return 0;
                case IDC_SWIPE_START_CAPTURE:ArmBagF8(*s,-4,L"VUỐT START");return 0;
                case IDC_SWIPE_END_CAPTURE:ArmBagF8(*s,-5,L"VUỐT END");return 0;
                case IDC_SWIPE_TEST:TestSwipe(*s);return 0;
                case IDC_EXPORT_SCAN:ExportScanConfig(*s);return 0;
                case IDC_IMPORT_SCAN:ImportScanConfig(*s);return 0;
                case IDC_TEST:RunTest(*s);return 0;
                case IDCANCEL:if(!s->runningChain){SyncConfig(*s);DestroyWindow(hwnd);}return 0;
            }
            break;
        case WM_CLOSE:if(!s->runningChain){if(s->rec42Active)StopRec42(*s,true);SyncConfig(*s);DestroyWindow(hwnd);}return 0;
        case WM_NCDESTROY:KillTimer(hwnd,kF8PollTimer);KillTimer(hwnd,kRec42PollTimer);KillTimer(hwnd,kRunTimer);s->hwnd=nullptr;s->stepList=nullptr;return DefWindowProcW(hwnd,msg,wp,lp);
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

} // namespace

void RunDialog(const Target& target){
    if(!target.owner||!target.gameWindow)return;
    EnsurePersistentConfigLoaded();
    State state{};state.target=target;state.config=g_lastConfig;RebaseForCurrentClient(state.config,target.gameWindow);if(state.config.steps.size()!=kDefaultInitialSteps)state.config.steps.resize(kDefaultInitialSteps);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=WndProc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=kClassName;
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
    EnableWindow(target.owner,FALSE);
    HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,kClassName,L"TÙY CHỈNH LỌC ĐỒ • GRID 0..41 ↔ POSITION 0..99 • NO-SLEEP",
                              WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                              CW_USEDEFAULT,CW_USEDEFAULT,1000,1090,target.owner,nullptr,GetModuleHandleW(nullptr),&state);
    if(!hwnd){EnableWindow(target.owner,TRUE);return;}
    ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
    MSG msg{};bool sawQuit=false;int quitCode=0;
    while(IsWindow(hwnd)){
        const BOOL gm=GetMessageW(&msg,nullptr,0,0);
        if(gm<=0){if(gm==0){sawQuit=true;quitCode=static_cast<int>(msg.wParam);}break;}
        if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    g_lastConfig=state.config;SavePersistentConfig();EnableWindow(target.owner,TRUE);SetActiveWindow(target.owner);if(sawQuit)PostQuitMessage(quitCode);
}

} // namespace image_scan_test

#include "image_scan_auto_ext.inl"
