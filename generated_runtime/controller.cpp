// Test Don Vang account manager, specialized to one independent target loop per ACC.
// Reuses the original FindClients() and BridgeClient (SetWindowsHookEx + shared block).
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "protocol.h"
#include "target_loop_logic.h"
using namespace cleanroute;
extern "C" long long ThanLongLicenseRemainingSeconds();
extern "C" int ThanLongLicenseActionAllowed();
extern "C" unsigned long long ThanLongLicenseSessionToken();
namespace {
constexpr wchar_t kTitle[] = L"Test Đơn Vàng • Quản lý ACC • AUTO TARGET ID";
constexpr wchar_t kGameModule[] = L"GameAssembly.dll";
constexpr DWORD kBridgeNudgeMs = 750;
constexpr UINT_PTR kTimer = 1;
constexpr UINT kUiPulse = WM_APP + 0x610;
constexpr int IDC_CLIENT_LIST = 100, IDC_SCAN = 101, IDC_START_CHECKED = 102,
    IDC_STOP_CHECKED = 103, IDC_MAIN_TAB = 207, IDC_LOG = 160,
    IDC_CLEAR_LOG = 237, IDC_EXPORT_LOG = 161, IDC_NEARBY_LIST = 3200,
    IDC_NEARBY_REFRESH = 3201, IDC_CAPTURE_CLICK2 = 3202,
    IDC_CLICK2_LABEL = 3203, IDC_SELECTION = 3204, IDC_HELP_TEXT = 3205,
    IDC_CAPTURE_CLICK1 = 3206, IDC_CLICK1_LABEL = 3207,
    IDC_SAVE_TIMING = 3208, IDC_REPEAT = 3209,
    IDC_DELAY_TARGET = 3210, IDC_DELAY_CLICK1 = 3211,
    IDC_DELAY_TRADE = 3212, IDC_DELAY_CLICK2 = 3213,
    IDC_DELAY_CYCLE = 3214, IDC_LOG_TOGGLE = 3215,
    IDC_TEST_CLICK1 = 3216, IDC_TEST_CLICK2 = 3217;
struct GameClient { DWORD pid=0; DWORD threadId=0; HWND window=nullptr; std::wstring title; };
template <typename T> bool ResolveProc(HMODULE module, const char* name, T& out) {
    out = nullptr;
    FARPROC raw = GetProcAddress(module, name);
    if (!raw) return false;
    static_assert(sizeof(raw) == sizeof(out), "pointer size mismatch");
    std::memcpy(&out, &raw, sizeof(out));
    return out != nullptr;
}
std::wstring ExeDir() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, _countof(path));
    if (wchar_t* slash = wcsrchr(path, L'\\')) *slash = 0;
    return path;
}
std::wstring ConfigDir() {
    wchar_t local[MAX_PATH]{};
    const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,_countof(local));
    if (n && n < _countof(local)) {
        std::wstring dir=std::wstring(local)+L"\\ThanLongCleanRoute";
        CreateDirectoryW(dir.c_str(),nullptr);
        return dir;
    }
    return ExeDir();
}
std::wstring ConfigPath() { return ConfigDir()+L"\\target_id_accounts.ini"; }
std::wstring AccountSection(int roleID) { return L"Role_"+std::to_wstring(roleID); }
int ReadIni(int id,const wchar_t* key,int fallback=0) {
    return static_cast<int>(GetPrivateProfileIntW(AccountSection(id).c_str(),key,fallback,ConfigPath().c_str()));
}
void WriteIni(int id,const wchar_t* key,int value) {
    const auto section=AccountSection(id);
    const auto data=std::to_wstring(value);
    WritePrivateProfileStringW(section.c_str(),key,data.c_str(),ConfigPath().c_str());
}
std::wstring GetText(HWND h) {
    if (!h) return {};
    const int len=GetWindowTextLengthW(h);
    std::wstring out(static_cast<std::size_t>(len)+1,L'\0');
    GetWindowTextW(h,out.data(),len+1);
    out.resize(static_cast<std::size_t>(len));
    return out;
}

bool HasModule(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W e{};
    e.dwSize = sizeof(e);
    bool found = false;
    if (Module32FirstW(snap, &e)) {
        do {
            if (_wcsicmp(e.szModule, name) == 0) { found = true; break; }
        } while (Module32NextW(snap, &e));
    }
    CloseHandle(snap);
    return found;
}

BOOL CALLBACK EnumGameWindows(HWND hwnd, LPARAM param) {
    if (!IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) <= 0) return TRUE;
    DWORD pid = 0;
    const DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || !tid || !HasModule(pid, kGameModule)) return TRUE;
    auto* out = reinterpret_cast<std::vector<GameClient>*>(param);
    for (const auto& g : *out) if (g.pid == pid) return TRUE;
    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, _countof(title));
    out->push_back({pid, tid, hwnd, title});
    return TRUE;
}

std::vector<GameClient> FindClients() {
    std::vector<GameClient> out;
    EnumWindows(EnumGameWindows, reinterpret_cast<LPARAM>(&out));
    std::sort(out.begin(), out.end(), [](const GameClient& a, const GameClient& b){ return a.pid < b.pid; });
    return out;
}

class BridgeClient {
public:
    BridgeClient() = default;
    BridgeClient(const BridgeClient&) = delete;
    BridgeClient& operator=(const BridgeClient&) = delete;
    ~BridgeClient() { Close(); }

    bool Attach(const GameClient& game, std::wstring& error) {
        Close();
        game_ = game;
        wchar_t mappingName[96]{};
        MappingName(game.pid, mappingName, _countof(mappingName));
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                      sizeof(SharedBlock), mappingName);
        if (!mapping_) { error = L"Không tạo được shared memory"; return false; }
        shared_ = reinterpret_cast<SharedBlock*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock)));
        if (!shared_) { error = L"Không map được shared memory"; Close(); return false; }
        ZeroMemory(shared_, sizeof(*shared_));
        shared_->magic = kMagic;
        shared_->protocolVersion = kProtocolVersion;
        shared_->targetPid = game.pid;
        shared_->targetWindowThreadId = game.threadId;
        shared_->licenseSessionToken = ThanLongLicenseSessionToken();
        InterlockedExchange(&shared_->licenseGate, ThanLongLicenseActionAllowed() ? 1 : 0);

        const std::wstring path = ExeDir() + L"\\ThanLongCleanRouteBridge.dll";
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            error = L"Thiếu ThanLongCleanRouteBridge.dll cạnh EXE";
            Close();
            return false;
        }
        SetLastError(ERROR_SUCCESS);
        localDll_ = LoadLibraryW(path.c_str());
        const DWORD loadError = GetLastError();
        if (!localDll_) {
            error = L"Có Bridge DLL nhưng LoadLibrary thất bại Win32=" + std::to_wstring(loadError);
            Close();
            return false;
        }
        HOOKPROC proc = nullptr;
        if (!ResolveProc(localDll_, "TlcGetMessageHook", proc)) {
            error = L"Bridge DLL thiếu TlcGetMessageHook";
            Close();
            return false;
        }
        hook_ = SetWindowsHookExW(WH_GETMESSAGE, proc, localDll_, game.threadId);
        if (!hook_) {
            error = L"Không hook được game; hãy chạy tool cùng quyền với game";
            Close();
            return false;
        }
        if (!PostThreadMessageW(game.threadId, kWakeMessage, 0, 0)) {
            error = L"Không đánh thức được message thread game";
            Close();
            return false;
        }
        attached_ = true;
        return true;
    }

    void Close() {
        if (hook_) UnhookWindowsHookEx(hook_);
        if (localDll_) FreeLibrary(localDll_);
        if (shared_) UnmapViewOfFile(shared_);
        if (mapping_) CloseHandle(mapping_);
        hook_ = nullptr;
        localDll_ = nullptr;
        shared_ = nullptr;
        mapping_ = nullptr;
        attached_ = false;
        pendingSeq_ = 0;
        pendingWakeTick_ = 0;
        pendingCommandRaw_ = 0;
        pendingArg0_ = pendingArg1_ = pendingArg2_ = 0;
    }

    bool AttachedTo(DWORD pid) const { return attached_ && game_.pid == pid; }
    bool Attached() const { return attached_; }

    bool Call(Command command, int a0, int a1, int a2, Response& out,
              std::wstring& error, DWORD timeoutMs = 1000, bool noSleep = false) {
        if (!attached_ || !shared_) { error = L"Bridge chưa attach"; return false; }

        const bool licenseProtected = IsLicenseProtectedCommand(command);
        const bool licenseAllowed = ThanLongLicenseActionAllowed() != 0;
        InterlockedExchange(&shared_->licenseGate, licenseAllowed ? 1 : 0);
        if (licenseProtected && !licenseAllowed) {
            error = L"LICENSE CORE GUARD: action nội bộ bị khóa";
            return false;
        }
        if (licenseProtected) {
            const auto token = static_cast<std::uint64_t>(ThanLongLicenseSessionToken());
            if (token == 0) {
                InterlockedExchange(&shared_->licenseGate, 0);
                error = L"LICENSE CORE GUARD: thiếu session proof";
                return false;
            }
            shared_->licenseSessionToken = token;
        }

        // V20: never overwrite an in-flight request. If a timed-out request finishes
        // late and the caller is retrying the SAME logical command/arguments, harvest
        // that exact response instead of discarding it and accidentally dispatching a
        // duplicate semantic action. This is shared by VỨT / ĐẶT LÊN / BÁN CÁCH 2.
        if (pendingSeq_ > 0) {
            if (shared_->completedSeq == pendingSeq_) {
                MemoryBarrier();
                const bool samePendingRequest =
                    pendingCommandRaw_ == static_cast<std::uint32_t>(command) &&
                    pendingArg0_ == a0 && pendingArg1_ == a1 && pendingArg2_ == a2;
                const Response lateResponse = shared_->response;
                pendingSeq_ = 0;
                pendingWakeTick_ = 0;
                pendingCommandRaw_ = 0;
                pendingArg0_ = pendingArg1_ = pendingArg2_ = 0;
                if (samePendingRequest) {
                    out = lateResponse;
                    if (!out.ok) {
                        error = out.detail[0] ? out.detail : L"Bridge trả lỗi muộn";
                        return false;
                    }
                    return true;
                }
                // A different logical operation arrived after the old request completed.
                // The old response must not be mis-attributed; it is drained, then the
                // current operation may be dispatched below because the Bridge is free.
            } else {
                const DWORD now = GetTickCount();
                if (shared_->bridgeBusy == 0 &&
                    (pendingWakeTick_ == 0 || now - pendingWakeTick_ >= kBridgeNudgeMs)) {
                    (void)PostThreadMessageW(game_.threadId, kWakeMessage, 0, 0);
                    pendingWakeTick_ = now;
                }
                error = L"Bridge còn bận sau timeout; không gửi chồng request";
                return false;
            }
        }
        if (shared_->bridgeBusy != 0) {
            error = L"Bridge busy; không gửi chồng request";
            return false;
        }

        const LONG next = shared_->requestSeq + 1;
        shared_->request = {};
        shared_->request.command = static_cast<std::uint32_t>(command);
        shared_->request.arg0 = a0;
        shared_->request.arg1 = a1;
        shared_->request.arg2 = a2;
        shared_->requestLicenseProof = licenseProtected
            ? LicenseRequestProof(shared_->licenseSessionToken, next, shared_->request)
            : 0ull;
        MemoryBarrier();
        InterlockedExchange(&shared_->requestSeq, next);
        if (!PostThreadMessageW(game_.threadId, kWakeMessage, 0, 0)) {
            error = L"Không đánh thức được game thread";
            return false;
        }
        const DWORD begin = GetTickCount();
        while (GetTickCount() - begin < timeoutMs) {
            if (shared_->completedSeq == next) {
                MemoryBarrier();
                pendingSeq_ = 0;
                pendingWakeTick_ = 0;
                out = shared_->response;
                if (!out.ok) {
                    error = out.detail[0] ? out.detail : L"Bridge trả lỗi";
                    return false;
                }
                return true;
            }
            if (noSleep) { if (!SwitchToThread()) YieldProcessor(); }
            else Sleep(2);
        }
        pendingSeq_ = next;
        pendingWakeTick_ = GetTickCount();
        pendingCommandRaw_ = static_cast<std::uint32_t>(command);
        pendingArg0_ = a0; pendingArg1_ = a1; pendingArg2_ = a2;
        error = L"Bridge timeout; fail-closed";
        return false;
    }

private:
    GameClient game_{};
    HANDLE mapping_ = nullptr;
    SharedBlock* shared_ = nullptr;
    HMODULE localDll_ = nullptr;
    HHOOK hook_ = nullptr;
    bool attached_ = false;
    LONG pendingSeq_ = 0;
    DWORD pendingWakeTick_ = 0;
    std::uint32_t pendingCommandRaw_ = 0;
    int pendingArg0_ = 0;
    int pendingArg1_ = 0;
    int pendingArg2_ = 0;
};


struct NearbyEntry { int roleID=0; std::wstring name; };
// Same as v10.6: capture client pixels together with the reference client size.
struct SavedClickPoint {
    int x=-1,y=-1,baseW=0,baseH=0;
    bool Valid() const {return x>=0&&y>=0&&baseW>0&&baseH>0&&x<baseW&&y<baseH;}
};
bool NormalizeSavedClick(HWND window,const SavedClickPoint& p,int& nx,int& ny,std::wstring& error) {
    nx=ny=-1;
    if(!p.Valid()){error=L"CLICK ẨN: chưa gán tọa độ";return false;}
    if(!IsWindow(window)){error=L"CLICK ẨN: cửa sổ game đã đóng";return false;}
    RECT rc{};
    if(!GetClientRect(window,&rc)||rc.right<=0||rc.bottom<=0){error=L"CLICK ẨN: không lấy được client size";return false;}
    // 10.6 scales stored point by current base size BEFORE normalizing.
    int x=MulDiv(p.x,rc.right,p.baseW);
    int y=MulDiv(p.y,rc.bottom,p.baseH);
    if(x<0||y<0||x>=rc.right||y>=rc.bottom){error=L"CLICK ẨN: tọa sau scale nằm ngoài game";return false;}
    nx=static_cast<int>(static_cast<long long>(x)*10000/rc.right);
    ny=static_cast<int>(static_cast<long long>(y)*10000/rc.bottom);
    return nx>=0&&nx<10000&&ny>=0&&ny<10000;
}

struct Account {
    GameClient game{};
    BridgeClient bridge{};
    std::mutex io{};                     // exactly one Bridge request per PID
    std::mutex data{};
    std::atomic<bool> running{false};
    std::thread worker{};
    int selfRoleID = 0;
    int targetRoleID = 0;
    SavedClickPoint click1{};
    SavedClickPoint click2{};
    target_loop::Settings timing{};
    std::vector<NearbyEntry> nearby{};
    target_loop::State loop{};
    std::wstring displayName;
    std::wstring status = L"Đã dừng";
    DWORD lastErrorTick = 0;
    std::wstring lastError;
    ~Account() { running=false; if(worker.joinable()) worker.join(); }
};
constexpr int kScale=10000;
// Exactly one InputSync click route for both worker and TEST, same as 10.6.
// Called under the per-PID Bridge mutex.
bool DispatchHiddenClick(Account& a,const SavedClickPoint& point,
                         Response& response,std::wstring& error,int& nx,int& ny) {
    if(!NormalizeSavedClick(a.game.window,point,nx,ny,error))return false;
    return a.bridge.Call(Command::ClickInternalPoint,nx,ny,0,response,error,2200);
}
void InterruptibleDelay(const std::atomic<bool>& running, int milliseconds) {
    while (milliseconds>0 && running.load()) {
        const int fragment=std::min(milliseconds,50);
        Sleep(static_cast<DWORD>(fragment));
        milliseconds-=fragment;
    }
}
std::mutex g_logMutex;
std::deque<std::wstring> g_pendingLogs;
// Global ON/OFF switch controls log collection, not account automation.
std::atomic<bool> g_logEnabled{true};
void PushLog(DWORD pid,const std::wstring& text) {
    SYSTEMTIME t{}; GetLocalTime(&t);
    wchar_t stamp[60]{};
    swprintf_s(stamp,L"[%02u:%02u:%02u | PID %lu] ",t.wHour,t.wMinute,t.wSecond,static_cast<unsigned long>(pid));
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logEnabled.load(std::memory_order_relaxed)) return;
    g_pendingLogs.push_back(std::wstring(stamp)+text);
    if (g_pendingLogs.size()>1000) g_pendingLogs.pop_front();
}
void SetStatus(Account& account,const std::wstring& status) {
    std::lock_guard<std::mutex> lock(account.data);
    account.status=status;
}
void ReportStep(Account& account, target_loop::Step step,bool ok,const std::wstring& info) {
    static const wchar_t* names[]={L"TARGET",L"CLICK 1 MẶT",L"CALLBACK GIAO DỊCH",L"CLICK 2"};
    const DWORD tick=GetTickCount();
    std::wstring msg=std::wstring(names[static_cast<int>(step)])+L" • "+(ok?L"OK":L"LỖI")+L" • "+info;
    bool logIt=ok;
    {
        std::lock_guard<std::mutex> lock(account.data);
        account.loop.Finish(ok);
        account.status=msg;
        if (!ok) {
            logIt=msg!=account.lastError || tick-account.lastErrorTick>5000;
            if (logIt) {account.lastError=msg;account.lastErrorTick=tick;}
        } else logIt=(account.loop.cycles<=2 || account.loop.cycles%10==0);
    }
    if(logIt) PushLog(account.game.pid,msg);
}
void Worker(Account* account) {
    if (!account) return;
    PushLog(account->game.pid,L"Khởi động vòng độc lập");
    bool completed=false;
    while (account->running.load()) {
        int target=0;
        SavedClickPoint clickPoint{};
        target_loop::Step step;
        target_loop::Settings settings;
        {
            std::lock_guard<std::mutex> lock(account->data);
            target=account->targetRoleID;
            step=account->loop.step;
            settings=account->timing;
            if (step==target_loop::Step::Face) {
                clickPoint=account->click1;
            } else if (step==target_loop::Step::Click2) {
                clickPoint=account->click2;
            }
        }
        if(target<=0){ SetStatus(*account,L"Chưa tick RoleID trong Scan người xung quanh"); InterruptibleDelay(account->running,400); continue; }
        Response response{}; std::wstring error;
        bool ok=false;int nx=-1,ny=-1;
        {
            std::lock_guard<std::mutex> lock(account->io);
            if (!account->bridge.AttachedTo(account->game.pid))
                ok=account->bridge.Attach(account->game,error);
            else ok=true;
            if(ok) {
                switch(step) {
                    case target_loop::Step::Target:
                        ok=account->bridge.Call(Command::SelectTargetByRoleID,target,1,0,response,error,2200);
                        break;
                    case target_loop::Step::Face:
                        ok=DispatchHiddenClick(*account,clickPoint,response,error,nx,ny);
                        break;
                    case target_loop::Step::Trade:
                        ok=account->bridge.Call(Command::ClickTravelSemantic,static_cast<int>(TravelSemantic::Trade),0,0,response,error,2200);
                        break;
                    case target_loop::Step::Click2:
                        ok=DispatchHiddenClick(*account,clickPoint,response,error,nx,ny);
                        break;
                }
            }
        }
        std::wstring info=ok?std::wstring(response.detail):error;
        if(nx>=0&&ny>=0)info+=L" | norm="+std::to_wstring(nx)+L","+std::to_wstring(ny);
        ReportStep(*account,step,ok,info);
        // Every step gets its own delay. A completed chain additionally gets cycle delay.
        // After a finite repeat count, stop without starting one extra chain.
        if (step==target_loop::Step::Click2) {
            std::lock_guard<std::mutex> lock(account->data);
            completed=settings.Finished(account->loop.cycles);
        }
        if(completed) break;
        InterruptibleDelay(account->running,settings.AfterStep(step));
        if(step==target_loop::Step::Click2)
            InterruptibleDelay(account->running,settings.delayCycleMs);
    }
    account->running=false;
    if(completed) {
        std::uint64_t cycles=0;
        {std::lock_guard<std::mutex> lock(account->data);cycles=account->loop.cycles;}
        SetStatus(*account,L"Hoàn tất "+std::to_wstring(cycles)+L" chuỗi");
        PushLog(account->game.pid,L"Đã chạy đủ số chuỗi: "+std::to_wstring(cycles));
    } else {
        SetStatus(*account,L"Đã dừng");
        PushLog(account->game.pid,L"Dừng ACC");
    }
}
class App {
public:
    HWND hwnd_=nullptr, accountsView_=nullptr, nearbyView_=nullptr, tabs_=nullptr, log_=nullptr;
    HWND lblSelection_=nullptr, lblClick1_=nullptr, lblClick2_=nullptr, help_=nullptr;
    HWND scanButton_=nullptr, capture1_=nullptr, capture2_=nullptr, test1_=nullptr, test2_=nullptr, saveTiming_=nullptr, logToggle_=nullptr;
    HWND repeatEdit_=nullptr, delayTargetEdit_=nullptr, delayClick1Edit_=nullptr;
    HWND delayTradeEdit_=nullptr, delayClick2Edit_=nullptr, delayCycleEdit_=nullptr;
    std::vector<HWND> autoControls_{};
    HFONT font_=nullptr;
    std::vector<std::unique_ptr<Account>> accounts_{};
    DWORD selectedPid_=0;
    bool updatingAccounts_=false, updatingNearby_=false;
    int selectedTab_=0;
    int armedClick_=0; DWORD armedPid_=0;
    bool wasF7_=false,wasF8_=false;
    DWORD lastUiRefresh_=0;
    void Font(HWND control){if(control && font_)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);}
    HWND Control(const wchar_t* clazz,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id,HWND parent=nullptr) {
        HWND c=CreateWindowExW(0,clazz,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent?parent:hwnd_,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        Font(c); return c;
    }
    Account* Get(DWORD pid) {
        for(auto& a:accounts_) if(a->game.pid==pid) return a.get();
        return nullptr;
    }
    Account* Selected(){return Get(selectedPid_);}
    void SetLogToggleCaption() {
        if (logToggle_) SetWindowTextW(logToggle_,g_logEnabled.load()?L"LOG: ON":L"LOG: OFF");
    }
    void ToggleLog() {
        const bool on=!g_logEnabled.load();
        if (!on) PushLog(0,L"LOG OFF — ngừng ghi dòng mới (giữ log đã có)");
        g_logEnabled.store(on);
        WritePrivateProfileStringW(L"Interface",L"LogEnabled",on?L"1":L"0",ConfigPath().c_str());
        SetLogToggleCaption();
        if (on) PushLog(0,L"LOG ON — tiếp tục ghi log");
    }
    void Init() {
        INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};InitCommonControlsEx(&icc);
        // Condense vertical sizing by approximately half; retain original 1060px window width.
        // Keep text at 13px for legibility rather than physically scaling glyphs to 8px.
        LOGFONTW lf{};lf.lfHeight=-13;wcscpy_s(lf.lfFaceName,L"Segoe UI");
        font_=CreateFontIndirectW(&lf);
        g_logEnabled.store(GetPrivateProfileIntW(L"Interface",L"LogEnabled",1,ConfigPath().c_str())!=0);
        Control(L"STATIC",L"DANH SÁCH ACC — Tick ACC rồi Bắt đầu / Dừng",0,14,5,740,15,-1);
        accountsView_=Control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER,14,22,1012,110,IDC_CLIENT_LIST);
        ListView_SetExtendedListViewStyle(accountsView_,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);
        const wchar_t* cols[]={L"Tên nhân vật / Client",L"PID",L"RoleID ACC",L"RoleID mục tiêu",L"Trạng thái"};
        const int widths[]={320,95,130,145,280};
        for(int i=0;i<5;++i){LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=const_cast<LPWSTR>(cols[i]);col.cx=widths[i];ListView_InsertColumn(accountsView_,i,&col);}
        Control(L"BUTTON",L"QUÉT CLIENT",BS_PUSHBUTTON,14,137,153,19,IDC_SCAN);
        Control(L"BUTTON",L"BẮT ĐẦU ACC TICK",BS_PUSHBUTTON,178,137,182,19,IDC_START_CHECKED);
        Control(L"BUTTON",L"DỪNG ACC TICK",BS_PUSHBUTTON,370,137,168,19,IDC_STOP_CHECKED);
        // Global toggle is visible on both tabs. It only switches log collection.
        logToggle_=Control(L"BUTTON",L"LOG: ON",BS_PUSHBUTTON,550,137,140,19,IDC_LOG_TOGGLE);
        SetLogToggleCaption();
        tabs_=Control(WC_TABCONTROLW,L"",WS_CLIPSIBLINGS|TCS_FIXEDWIDTH,14,160,1012,26,IDC_MAIN_TAB);
        TCITEMW tab{}; tab.mask=TCIF_TEXT;tab.pszText=const_cast<LPWSTR>(L"AUTO TARGET ID");TabCtrl_InsertItem(tabs_,0,&tab);
        tab.pszText=const_cast<LPWSTR>(L"LOG");TabCtrl_InsertItem(tabs_,1,&tab);
        lblSelection_=Control(L"STATIC",L"Chọn một ACC để quét người xung quanh",0,14,190,880,14,IDC_SELECTION);
        scanButton_=Control(L"BUTTON",L"QUÉT NGƯỜI XUNG QUANH",BS_PUSHBUTTON,14,207,245,20,IDC_NEARBY_REFRESH);
        nearbyView_=Control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER,14,230,1012,85,IDC_NEARBY_LIST);
        ListView_SetExtendedListViewStyle(nearbyView_,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER);
        const wchar_t* ncols[]={L"Tên nhân vật xung quanh",L"RoleID"};
        const int nwidths[]={650,320};
        for(int i=0;i<2;++i){LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=const_cast<LPWSTR>(ncols[i]);col.cx=nwidths[i];ListView_InsertColumn(nearbyView_,i,&col);}
        capture1_=Control(L"BUTTON",L"CHỌN CLICK 1 → F7",BS_PUSHBUTTON,14,319,178,19,IDC_CAPTURE_CLICK1);
        test1_=Control(L"BUTTON",L"TEST CLICK 1",BS_PUSHBUTTON,198,319,121,19,IDC_TEST_CLICK1);
        capture2_=Control(L"BUTTON",L"CHỌN CLICK 2 → F8",BS_PUSHBUTTON,325,319,178,19,IDC_CAPTURE_CLICK2);
        test2_=Control(L"BUTTON",L"TEST CLICK 2",BS_PUSHBUTTON,509,319,121,19,IDC_TEST_CLICK2);
        lblClick1_=Control(L"STATIC",L"C1: chưa gán",0,640,319,182,15,IDC_CLICK1_LABEL);
        lblClick2_=Control(L"STATIC",L"C2: chưa gán",0,828,319,198,15,IDC_CLICK2_LABEL);
        auto makeSetting=[&](const wchar_t* label,HWND& edit,int x,int y,int id) {
            HWND title=Control(L"STATIC",label,0,x,y+2,194,15,-1);
            edit=Control(L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,x+195,y,85,19,id);
            autoControls_.push_back(title);
            autoControls_.push_back(edit);
        };
        makeSetting(L"Số chuỗi (0 = vô hạn)",repeatEdit_,14,348,IDC_REPEAT);
        makeSetting(L"Delay sau Target (ms)",delayTargetEdit_,360,348,IDC_DELAY_TARGET);
        makeSetting(L"Delay sau Click 1 (ms)",delayClick1Edit_,706,348,IDC_DELAY_CLICK1);
        makeSetting(L"Delay sau Callback (ms)",delayTradeEdit_,14,371,IDC_DELAY_TRADE);
        makeSetting(L"Delay sau Click 2 (ms)",delayClick2Edit_,360,371,IDC_DELAY_CLICK2);
        makeSetting(L"Delay giữa chuỗi (ms)",delayCycleEdit_,706,371,IDC_DELAY_CYCLE);
        saveTiming_=Control(L"BUTTON",L"LƯU REPEAT / DELAY CHO ACC",BS_PUSHBUTTON,14,394,300,19,IDC_SAVE_TIMING);
        help_=Control(L"STATIC",L"CHỌN CLICK trên tool trước, đặt chuột vào đúng game và nhấn F7/F8. TEST CLICK để chẩn đoán.",0,
                      14,416,1012,15,IDC_HELP_TEXT);
        log_=Control(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER,14,190,1012,200,IDC_LOG);
        Control(L"BUTTON",L"XÓA LOG",BS_PUSHBUTTON,14,394,140,19,IDC_CLEAR_LOG);
        Control(L"BUTTON",L"XUẤT LOG",BS_PUSHBUTTON,166,394,140,19,IDC_EXPORT_LOG);
        SwitchTab(0);
        // Same as source 10.6: arm and poll while cursor is in game; no global hotkeys.
        SetTimer(hwnd_,kTimer,30,nullptr);
        ScanClients();
        PushLog(0,L"Đã khởi tạo quản lý ACC và tab LOG; bỏ toàn bộ workflow cũ");
    }
    void SwitchTab(int i) {
        selectedTab_=i;
        const bool autoTab=i==0;
        for(HWND c:{lblSelection_,scanButton_,capture1_,capture2_,test1_,test2_,lblClick1_,lblClick2_,nearbyView_,help_,saveTiming_})
            ShowWindow(c,autoTab?SW_SHOW:SW_HIDE);
        for(HWND c:autoControls_)ShowWindow(c,autoTab?SW_SHOW:SW_HIDE);
        ShowWindow(log_,autoTab?SW_HIDE:SW_SHOW);
        for(int id:{IDC_CLEAR_LOG,IDC_EXPORT_LOG}) {
            HWND h=GetDlgItem(hwnd_,id);ShowWindow(h,autoTab?SW_HIDE:SW_SHOW);
        }
    }
    void ScanClients() {
        const auto found=FindClients();
        std::vector<DWORD> seen;
        for(const auto& g:found){
            seen.push_back(g.pid);
            Account* a=Get(g.pid);
            if(!a){
                auto created=std::make_unique<Account>();
                created->game=g;created->displayName=g.title;
                accounts_.push_back(std::move(created));
                PushLog(g.pid,L"Nhận diện game client GameAssembly.dll");
            } else a->game=g;
        }
        // Retire disappeared processes only after stopping their own worker.
        for(auto it=accounts_.begin();it!=accounts_.end();) {
            if(std::find(seen.begin(),seen.end(),(*it)->game.pid)==seen.end()) {
                (*it)->running=false;
                if((*it)->worker.joinable())(*it)->worker.join();
                PushLog((*it)->game.pid,L"Client đã đóng / mất khỏi danh sách");
                if(selectedPid_==(*it)->game.pid) selectedPid_=0;
                it=accounts_.erase(it);
            } else ++it;
        }
        RefreshAccounts();
        if(!selectedPid_ && !accounts_.empty()) selectedPid_=accounts_.front()->game.pid;
        SelectAccountRow();RefreshNearby();
    }
    void SelectAccountRow() {
        for(int i=0;i<ListView_GetItemCount(accountsView_);++i) {
            LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=i;
            if(ListView_GetItem(accountsView_,&item) && static_cast<DWORD>(item.lParam)==selectedPid_) {
                updatingAccounts_=true;
                ListView_SetItemState(accountsView_,i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
                updatingAccounts_=false;
                break;
            }
        }
    }
    void RefreshAccounts() {
        std::vector<DWORD> checked;
        for(int i=0;i<ListView_GetItemCount(accountsView_);++i) {
            if(!ListView_GetCheckState(accountsView_,i)) continue;
            LVITEMW li{};li.mask=LVIF_PARAM;li.iItem=i;
            if(ListView_GetItem(accountsView_,&li))checked.push_back(static_cast<DWORD>(li.lParam));
        }
        updatingAccounts_=true;
        ListView_DeleteAllItems(accountsView_);
        for(std::size_t i=0;i<accounts_.size();++i) {
            Account& a=*accounts_[i];
            std::wstring name,status;int own=0,target=0;
            {std::lock_guard<std::mutex> lock(a.data);name=a.displayName;status=a.status;own=a.selfRoleID;target=a.targetRoleID;}
            LVITEMW it{};it.mask=LVIF_TEXT|LVIF_PARAM;it.iItem=static_cast<int>(i);it.pszText=name.data();it.lParam=a.game.pid;
            int row=ListView_InsertItem(accountsView_,&it);
            const std::wstring pid=std::to_wstring(a.game.pid),ownStr=own?std::to_wstring(own):L"—",targetStr=target?std::to_wstring(target):L"—";
            ListView_SetItemText(accountsView_,row,1,const_cast<LPWSTR>(pid.c_str()));
            ListView_SetItemText(accountsView_,row,2,const_cast<LPWSTR>(ownStr.c_str()));
            ListView_SetItemText(accountsView_,row,3,const_cast<LPWSTR>(targetStr.c_str()));
            ListView_SetItemText(accountsView_,row,4,status.data());
            ListView_SetCheckState(accountsView_,row,std::find(checked.begin(),checked.end(),a.game.pid)!=checked.end());
        }
        updatingAccounts_=false;
        SelectAccountRow();
    }
    void RefreshNearby() {
        Account* a=Selected();
        updatingNearby_=true;
        ListView_DeleteAllItems(nearbyView_);
        if(!a) {
            SetWindowTextW(lblSelection_,L"Chưa chọn ACC");
            SetWindowTextW(lblClick1_,L"C1: —");SetWindowTextW(lblClick2_,L"C2: —");
            for(HWND h:{repeatEdit_,delayTargetEdit_,delayClick1Edit_,delayTradeEdit_,delayClick2Edit_,delayCycleEdit_})SetWindowTextW(h,L"");
            updatingNearby_=false;return;
        }
        std::vector<NearbyEntry> players;
        std::wstring name;int target,own,x1,y1,x2,y2;
        target_loop::Settings settings;
        {
            std::lock_guard<std::mutex> lock(a->data);
            players=a->nearby;name=a->displayName;target=a->targetRoleID;own=a->selfRoleID;
            x1=a->click1X;y1=a->click1Y;x2=a->click2X;y2=a->click2Y;settings=a->timing;
        }
        const auto selection=L"ACC: "+name+L"   |   PID "+std::to_wstring(a->game.pid)+L"   |   RoleID được tick: "+(target>0?std::to_wstring(target):L"chưa chọn");
        SetWindowTextW(lblSelection_,selection.c_str());
        const auto label1=(x1>=0&&y1>=0)?L"C1: "+std::to_wstring(x1)+L","+std::to_wstring(y1):L"C1: chưa gán";
        const auto label2=(x2>=0&&y2>=0)?L"C2: "+std::to_wstring(x2)+L","+std::to_wstring(y2):L"C2: chưa gán";
        SetWindowTextW(lblClick1_,label1.c_str());SetWindowTextW(lblClick2_,label2.c_str());
        auto setNumber=[](HWND ctrl,int number) {const auto value=std::to_wstring(number);SetWindowTextW(ctrl,value.c_str());};
        setNumber(repeatEdit_,settings.repeatCycles);
        setNumber(delayTargetEdit_,settings.delayTargetMs);
        setNumber(delayClick1Edit_,settings.delayClick1Ms);
        setNumber(delayTradeEdit_,settings.delayTradeMs);
        setNumber(delayClick2Edit_,settings.delayClick2Ms);
        setNumber(delayCycleEdit_,settings.delayCycleMs);
        bool foundTarget=false;
        for(std::size_t i=0;i<players.size();++i){
            const auto& p=players[i];
            std::wstring role=std::to_wstring(p.roleID);
            LVITEMW li{};li.mask=LVIF_TEXT|LVIF_PARAM;li.iItem=static_cast<int>(i);li.pszText=const_cast<LPWSTR>(p.name.c_str());li.lParam=p.roleID;
            int row=ListView_InsertItem(nearbyView_,&li);
            ListView_SetItemText(nearbyView_,row,1,role.data());
            const bool checked=target==p.roleID;
            ListView_SetCheckState(nearbyView_,row,checked);
            foundTarget|=checked;
        }
        // A checked target is retained even when the player goes out of range.
        if(target>0&&!foundTarget){
            std::wstring cachedName=L"RoleID đã lưu (ngoài vùng quét)";
            LVITEMW li{};li.mask=LVIF_TEXT|LVIF_PARAM;li.iItem=ListView_GetItemCount(nearbyView_);li.pszText=cachedName.data();li.lParam=target;
            int row=ListView_InsertItem(nearbyView_,&li);auto id=std::to_wstring(target);
            ListView_SetItemText(nearbyView_,row,1,id.data());ListView_SetCheckState(nearbyView_,row,TRUE);
        }
        (void)own;
        updatingNearby_=false;
    }
    void ScanNearby() {
        Account* a=Selected();if(!a)return;
        Response resp{};std::wstring error;bool ok=false;
        {
            std::lock_guard<std::mutex> lock(a->io);
            if(!a->bridge.AttachedTo(a->game.pid))ok=a->bridge.Attach(a->game,error);
            else ok=true;
            if(ok) {
                Response state{};
                ok=a->bridge.Call(Command::ReadState,0,0,0,state,error,1200);
                if(ok && (state.snapshot.validMask&ValidIdentity) && state.snapshot.roleID>0) {
                    std::lock_guard<std::mutex> dataLock(a->data);
                    if(a->selfRoleID!=state.snapshot.roleID) {
                        a->selfRoleID=state.snapshot.roleID;
                        a->targetRoleID=ReadIni(a->selfRoleID,L"TargetRoleID",0);
                        a->click1X=ReadIni(a->selfRoleID,L"Click1X",-1);
                        a->click1Y=ReadIni(a->selfRoleID,L"Click1Y",-1);
                        a->click2X=ReadIni(a->selfRoleID,L"Click2X",-1);
                        a->click2Y=ReadIni(a->selfRoleID,L"Click2Y",-1);
                        auto& t=a->timing;
                        t.repeatCycles=ReadIni(a->selfRoleID,L"RepeatCycles",t.repeatCycles);
                        t.delayTargetMs=ReadIni(a->selfRoleID,L"DelayTargetMs",t.delayTargetMs);
                        t.delayClick1Ms=ReadIni(a->selfRoleID,L"DelayClick1Ms",t.delayClick1Ms);
                        t.delayTradeMs=ReadIni(a->selfRoleID,L"DelayTradeMs",t.delayTradeMs);
                        t.delayClick2Ms=ReadIni(a->selfRoleID,L"DelayClick2Ms",t.delayClick2Ms);
                        t.delayCycleMs=ReadIni(a->selfRoleID,L"DelayCycleMs",t.delayCycleMs);
                        if(!t.Valid()) {t={};PushLog(a->game.pid,L"Cấu hình delay không hợp lệ; sử dụng mặc định");}
                        a->loop.targetRoleID=a->targetRoleID;
                    }
                    if(state.snapshot.characterName[0]) a->displayName=state.snapshot.characterName;
                }
                if(ok)ok=a->bridge.Call(Command::ScanNearbyPlayers,0,0,0,resp,error,2000);
            }
        }
        if(ok) {
            std::lock_guard<std::mutex> lock(a->data);
            a->nearby.clear();
            for(int i=0;i<resp.nearbyCount && i<static_cast<int>(kNearbyCapacity);++i)
                if(resp.nearby[i].roleID>0)a->nearby.push_back({resp.nearby[i].roleID,resp.nearby[i].name});
        }
        PushLog(a->game.pid,ok?std::wstring(resp.detail):L"Quét xung quanh lỗi: "+error);
        RefreshNearby();RefreshAccounts();
    }
    void SetTargetFromCheck(int row,bool checked) {
        Account* a=Selected();if(!a)return;
        LVITEMW li{};li.mask=LVIF_PARAM;li.iItem=row;
        if(!ListView_GetItem(nearbyView_,&li))return;
        const int target=checked?static_cast<int>(li.lParam):0;
        int self;
        {std::lock_guard<std::mutex> lock(a->data);
            a->targetRoleID=target;
            a->loop.targetRoleID=target;
            a->loop.ResetStep();
            self=a->selfRoleID;
        }
        if(self>0)WriteIni(self,L"TargetRoleID",target);
        PushLog(a->game.pid,target>0?L"Đã tick RoleID "+std::to_wstring(target):L"Đã bỏ tick target");
        RefreshNearby();RefreshAccounts();
    }
    void StartChecked(bool start){
        for(int i=0;i<ListView_GetItemCount(accountsView_);++i) {
            if(!ListView_GetCheckState(accountsView_,i))continue;
            LVITEMW li{};li.mask=LVIF_PARAM;li.iItem=i;
            if(!ListView_GetItem(accountsView_,&li))continue;
            Account* a=Get(static_cast<DWORD>(li.lParam));if(!a)continue;
            if(start){
                if(a->running)continue;
                {std::lock_guard<std::mutex> lock(a->data);
                    if(a->targetRoleID<=0){a->status=L"Chưa tick RoleID mục tiêu";PushLog(a->game.pid,a->status);continue;}
                    a->loop.ResetStep();
                    a->loop.cycles=0;
                    a->loop.errors=0;
                }
                if(a->worker.joinable())a->worker.join();
                a->running=true;a->worker=std::thread(Worker,a);
            } else {
                a->running=false;
                if(a->worker.joinable())a->worker.join();
            }
        }
        RefreshAccounts();
    }
    void ArmClick(int point) {
        Account* a=Selected();if(!a){PushLog(0,L"Chưa chọn ACC để gán CLICK");return;}
        armedClick_=point;armedPid_=a->game.pid;
        wasF7_=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
        wasF8_=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
        std::wstring msg=L"CHỜ F"+std::to_wstring(point==1?7:8)+L" | PID "+
            std::to_wstring(armedPid_)+L" | đặt chuột trong GAME rồi bấm phím";
        SetStatus(*a,msg);PushLog(a->game.pid,msg);
    }
    void PollCaptureHotkeys() {
        const bool f7=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
        const bool f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
        if(armedClick_==1&&f7&&!wasF7_)CaptureClick(1);
        if(armedClick_==2&&f8&&!wasF8_)CaptureClick(2);
        wasF7_=f7;wasF8_=f8;
    }
    void CaptureClick(int point) {
        Account* a=Get(armedPid_);if(!a)return;
        POINT p{};RECT rc{};
        if(!GetCursorPos(&p)||!ScreenToClient(a->game.window,&p)||!GetClientRect(a->game.window,&rc)||
            rc.right<=0||rc.bottom<=0||p.x<0||p.y<0||p.x>=rc.right||p.y>=rc.bottom){
            PushLog(a->game.pid,(point==1?L"F7: ":L"F8: ")+std::wstring(L"chuột chưa ở trong cửa sổ game cần cấu hình"));return;
        }
        int nx=static_cast<int>(static_cast<long long>(p.x)*kScale/rc.right);
        int ny=static_cast<int>(static_cast<long long>(p.y)*kScale/rc.bottom);
        int self;
        {std::lock_guard<std::mutex> lock(a->data);
            if(point==1){a->click1X=nx;a->click1Y=ny;}
            else {a->click2X=nx;a->click2Y=ny;}
            self=a->selfRoleID;
        }
        if(self>0){
            WriteIni(self,point==1?L"Click1X":L"Click2X",nx);
            WriteIni(self,point==1?L"Click1Y":L"Click2Y",ny);
        }
        armedClick_=0;armedPid_=0;
        PushLog(a->game.pid,L"F7/F8 PASS: Đã lưu Click "+std::to_wstring(point)+L" normalized "+std::to_wstring(nx)+L","+std::to_wstring(ny));
        RefreshNearby();
    }
    void TestHiddenClick(int which) {
        Account* a=Selected();if(!a)return;
        int x=-1,y=-1;
        {std::lock_guard<std::mutex> lock(a->data);
            if(which==1){x=a->click1X;y=a->click1Y;}
            else{x=a->click2X;y=a->click2Y;}
        }
        Response response{};std::wstring error;bool ok=false;
        if(x<0||x>=10000||y<0||y>=10000)error=L"Chưa gán điểm CLICK bằng F7/F8";
        else {
            std::lock_guard<std::mutex> lock(a->io);
            if(!a->bridge.AttachedTo(a->game.pid))ok=a->bridge.Attach(a->game,error);
            else ok=true;
            if(ok)ok=a->bridge.Call(Command::ClickInternalPoint,x,y,0,response,error,2200);
        }
        std::wstring msg=L"TEST CLICK "+std::to_wstring(which)+(ok?L" PASS | ":L" FAIL | ")+
            (ok?std::wstring(response.detail):error)+L" | norm="+std::to_wstring(x)+L","+std::to_wstring(y);
        SetStatus(*a,msg);PushLog(a->game.pid,msg);RefreshAccounts();
    }
    void SaveTiming() {
        Account* a=Selected();if(!a)return;
        target_loop::Settings values;
        auto parse=[](HWND h,int max,int& out)->bool {
            const std::wstring text=GetText(h);
            if(text.empty() || text.size()>9)return false;
            for(wchar_t c:text)if(c<L'0'||c>L'9')return false;
            const unsigned long long number=std::wcstoull(text.c_str(),nullptr,10);
            if(number>static_cast<unsigned long long>(max))return false;
            out=static_cast<int>(number);return true;
        };
        if(!parse(repeatEdit_,target_loop::Settings::kMaxRepeats,values.repeatCycles)||
           !parse(delayTargetEdit_,target_loop::Settings::kMaxDelayMs,values.delayTargetMs)||
           !parse(delayClick1Edit_,target_loop::Settings::kMaxDelayMs,values.delayClick1Ms)||
           !parse(delayTradeEdit_,target_loop::Settings::kMaxDelayMs,values.delayTradeMs)||
           !parse(delayClick2Edit_,target_loop::Settings::kMaxDelayMs,values.delayClick2Ms)||
           !parse(delayCycleEdit_,target_loop::Settings::kMaxDelayMs,values.delayCycleMs)||!values.Valid()) {
            PushLog(a->game.pid,L"Repeat phải 0..1000000; mỗi delay phải 0..60000 ms; nhập số nguyên không âm");return;
        }
        int self=0;
        {std::lock_guard<std::mutex> lock(a->data);a->timing=values;self=a->selfRoleID;}
        if(self>0){
            WriteIni(self,L"RepeatCycles",values.repeatCycles);
            WriteIni(self,L"DelayTargetMs",values.delayTargetMs);
            WriteIni(self,L"DelayClick1Ms",values.delayClick1Ms);
            WriteIni(self,L"DelayTradeMs",values.delayTradeMs);
            WriteIni(self,L"DelayClick2Ms",values.delayClick2Ms);
            WriteIni(self,L"DelayCycleMs",values.delayCycleMs);
        }
        if(self<=0)PushLog(a->game.pid,L"Chưa nhận diện RoleID ACC: thiết lập delay chỉ có hiệu lực trong phiên; hãy quét người xung quanh trước khi lưu");
        PushLog(a->game.pid,L"Đã lưu Repeat="+std::to_wstring(values.repeatCycles)+
            L"; delays(ms)="+std::to_wstring(values.delayTargetMs)+L","+
            std::to_wstring(values.delayClick1Ms)+L","+std::to_wstring(values.delayTradeMs)+L","+
            std::to_wstring(values.delayClick2Ms)+L"; cycle="+std::to_wstring(values.delayCycleMs));
    }
    void FlushLog(){
        std::deque<std::wstring> messages;
        {std::lock_guard<std::mutex> lock(g_logMutex);messages.swap(g_pendingLogs);}
        if(messages.empty()||!log_)return;
        int length=GetWindowTextLengthW(log_);
        if(length>120000){SetWindowTextW(log_,L"[LOG: tự dọn nội dung cũ]\r\n");}
        for(const auto& msg:messages){
            const auto line=msg+L"\r\n";
            const int end=GetWindowTextLengthW(log_);
            SendMessageW(log_,EM_SETSEL,end,end);
            SendMessageW(log_,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(line.c_str()));
        }
        SendMessageW(log_,EM_SCROLLCARET,0,0);
    }
    void ExportLog() {
        const std::wstring path=ConfigDir()+L"\\target_id_log.txt";
        const std::wstring content=GetText(log_);
        HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){PushLog(0,L"Không ghi được file log");return;}
        const wchar_t bom=0xFEFF;DWORD done;
        WriteFile(file,&bom,sizeof(bom),&done,nullptr);
        WriteFile(file,content.data(),static_cast<DWORD>(content.size()*sizeof(wchar_t)),&done,nullptr);
        CloseHandle(file);PushLog(0,L"Đã xuất LOG: "+path);
    }
    void ListNotification(NMHDR* hdr) {
        if(!hdr)return;
        if(hdr->idFrom==IDC_MAIN_TAB && hdr->code==TCN_SELCHANGE){SwitchTab(TabCtrl_GetCurSel(tabs_));return;}
        if(hdr->idFrom==IDC_CLIENT_LIST && hdr->code==LVN_ITEMCHANGED && !updatingAccounts_){
            const auto* item=reinterpret_cast<NMLISTVIEW*>(hdr);
            if(item->iItem<0 || !(item->uChanged&LVIF_STATE) || !(item->uNewState&LVIS_SELECTED))return;
            LVITEMW li{};li.mask=LVIF_PARAM;li.iItem=item->iItem;
            if(ListView_GetItem(accountsView_,&li)){selectedPid_=static_cast<DWORD>(li.lParam);RefreshNearby();}
            return;
        }
        if(hdr->idFrom==IDC_NEARBY_LIST && hdr->code==LVN_ITEMCHANGED && !updatingNearby_) {
            const auto* item=reinterpret_cast<NMLISTVIEW*>(hdr);
            if(item->iItem<0||!(item->uChanged&LVIF_STATE))return;
            const UINT oldCheck=item->uOldState&LVIS_STATEIMAGEMASK;
            const UINT newCheck=item->uNewState&LVIS_STATEIMAGEMASK;
            if(oldCheck!=newCheck&&newCheck)SetTargetFromCheck(item->iItem,ListView_GetCheckState(nearbyView_,item->iItem)!=FALSE);
        }
    }
    LRESULT Handle(UINT msg,WPARAM wp,LPARAM lp){
        switch(msg){
            case WM_CREATE:Init();return 0;
            case WM_NOTIFY:ListNotification(reinterpret_cast<NMHDR*>(lp));return 0;
            case WM_COMMAND:
                switch(LOWORD(wp)){
                    case IDC_SCAN:ScanClients();return 0;
                    case IDC_START_CHECKED:StartChecked(true);return 0;
                    case IDC_STOP_CHECKED:StartChecked(false);return 0;
                    case IDC_NEARBY_REFRESH:ScanNearby();return 0;
                    case IDC_CAPTURE_CLICK1:ArmClick(1);return 0;
                    case IDC_CAPTURE_CLICK2:ArmClick(2);return 0;
                    case IDC_TEST_CLICK1:TestHiddenClick(1);return 0;
                    case IDC_TEST_CLICK2:TestHiddenClick(2);return 0;
                    case IDC_SAVE_TIMING:SaveTiming();return 0;
                    case IDC_LOG_TOGGLE:ToggleLog();return 0;
                    case IDC_CLEAR_LOG:SetWindowTextW(log_,L"");return 0;
                    case IDC_EXPORT_LOG:ExportLog();return 0;
                }break;
            case WM_TIMER:if(wp==kTimer){
                PollCaptureHotkeys();
                const DWORD now=GetTickCount();
                if(now-lastUiRefresh_>=500){FlushLog();RefreshAccounts();lastUiRefresh_=now;}
            }return 0;
            case WM_CLOSE:DestroyWindow(hwnd_);return 0;
            case WM_DESTROY:
                KillTimer(hwnd_,kTimer);
                for(auto& a:accounts_)a->running=false;
                for(auto& a:accounts_)if(a->worker.joinable())a->worker.join();
                accounts_.clear();
                if(font_){DeleteObject(font_);font_=nullptr;}
                PostQuitMessage(0);return 0;
        }
        return DefWindowProcW(hwnd_,msg,wp,lp);
    }
};
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){
        auto* create=reinterpret_cast<CREATESTRUCTW*>(lp);
        app=reinterpret_cast<App*>(create->lpCreateParams);
        app->hwnd_=hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
    }
    return app?app->Handle(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance=instance;wc.lpfnWndProc=WindowProc;
    wc.lpszClassName=L"ThanLongTargetIdManager";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassExW(&wc))return 1;
    App app;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,kTitle,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,1060,475,nullptr,nullptr,instance,&app);
    if(!hwnd)return 2;
    ShowWindow(hwnd,show);UpdateWindow(hwnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return static_cast<int>(msg.wParam);
}
