#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cerrno>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>
#include <functional>
#include <deque>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "protocol.h"
#include "route_logic.h"
#include "trade_coordinator_logic.h"
#include "telegram_notifier.h"
#include "telegram_logic.h"
#include "gold_history_store.h"
#include "fixed_slot_sell_logic.h"
#include "internal_ui_click_logic.h"
#include "travel_fight_guard_logic.h"
#include "auto_fight_retry_logic.h"
#include "image_scan_test.h"
#include "automation_bulk_logic.h"
#include "party_build_logic.h"
#include "equip_point_db.h"
#include "bag_filter_v2_logic.h"
#include "main_macro_sell_logic.h"
#include "trade_quota_v18_logic.h"
#include "auto_loot_logic.h"
#include "auto_role_logic.h"

using namespace cleanroute;
using namespace cleanroute_logic;
using namespace itemtrade_coordinator;
using namespace trade_quota_v18_logic;

extern "C" long long ThanLongLicenseRemainingSeconds();
extern "C" int ThanLongLicenseActionAllowed();
extern "C" unsigned long long ThanLongLicenseSessionToken();

namespace {

constexpr wchar_t kTitle[] = L"Công cụ hỗ trợ game rảnh tay • 10.6";
constexpr int kMainTabX = 18;
constexpr int kMainTabY = 4;
constexpr int kMainTabWidth = 870;
constexpr int kMainTabHeight = 32;
constexpr int kCoordinatorStatusX = 18;
constexpr int kCoordinatorStatusY = 934;
constexpr int kCoordinatorStatusWidth = 1005;
constexpr int kCoordinatorStatusHeight = 27;
constexpr int kCompactCoordinatorStatusY = 243;
constexpr wchar_t kGameModule[] = L"GameAssembly.dll";
constexpr UINT_PTR kTimer = 1;
constexpr UINT_PTR kMainMacroSellTimer = 4;
constexpr UINT kMainMacroSellTimerMs = 10;
constexpr int IDC_MAIN_MACRO_SELL_X = 5901;
constexpr int IDC_MAIN_MACRO_SELL_Y = 5902;
constexpr int IDC_MAIN_MACRO_SELL_CAPTURE = 5903;
constexpr int IDC_MAIN_MACRO_SELL_TEST = 5904;
constexpr int IDC_MAIN_MACRO_SELL_CLEAR = 5905;
constexpr int IDC_MAIN_MACRO_SELL_DELAY = 5906;
constexpr int IDC_MAIN_MACRO_SELL_SAVE = 5907;
constexpr int IDC_MAIN_MACRO_SELL_STATUS = 5908;
constexpr int IDC_MAIN_MACRO_SELL_CLOSE = 5909;
constexpr UINT_PTR kRecordTimer = 2;
constexpr int kCaptureHotkeyId = 9001;
constexpr int kPauseHotkeyId = 9002;
constexpr DWORD kClientStableResumeMs = 2000;
constexpr DWORD kTradePostPutUpRecoveryMaxMs = 8000;
constexpr DWORD kTradePutUpCallbackWaitMaxMs = 8000;
constexpr DWORD kBridgeNudgeMs = 750;
constexpr DWORD kReadFailLogIntervalMs = 2000;
constexpr UINT kWindowResponsiveProbeMs = 120;
constexpr DWORD kTrainPositionCheckMs = 60000;
constexpr DWORD kAutoFightRecheckMs = 60000;
constexpr DWORD kMountRetryWaitMs = 5000;
constexpr DWORD kMountFightBoostMs = 10000;
constexpr DWORD kPriorityAutoVerifyMs = 1300;
constexpr int kUnderworldMapId = 87;
constexpr int kLauLanMapId = 5;
constexpr int kNhanNamMapId = 24;
constexpr int kThuyKinhHoMapId = 44;
constexpr int kLieuTayMapId = 45;
constexpr int kTruongBachSonMapId = 48;
constexpr int kXaTruyenBinhNpcId = 387; // DATA verified: M5 Lâu Lan
constexpr int kNgaiNiNgoaNhiNpcId = 913; // DATA verified: M5 Lâu Lan
constexpr DWORD kLauLanGateStallMs = 3000;
constexpr DWORD kLauLanConfirmRetryMs = 3000;
constexpr DWORD kAutoPathFightConflictRetryMs = 1000;
constexpr DWORD kRouteOwnershipStopRetryMs = 1200;
constexpr int kRouteOwnershipStopMaxAttempts = 3;
constexpr DWORD kTradeBagStableMs = 1000;      // v9.9: require 1s stable MAIN bag snapshot before deciding the completed pass.
constexpr DWORD kTradeBagVerifyMaxMs = 2000;    // v9.9: bounded fail-closed window until post-pass is decided.
constexpr DWORD kTradeTargetTimeoutMs = 4500;
constexpr DWORD kTradeTargetRetryMs = 500;
constexpr int kPreciseWorldTolerance = 20; // v1.6: GD/NPC gần như tuyệt đối; train vẫn dùng profile tolerance.
constexpr DWORD kShortcutPathAcceptMs = 5000; // Cho snapshot đủ thời gian chứng minh AutoPath/movement thực.
constexpr int kShortcutPathMaxDispatch = 5;
// The game creates/rebuilds the NPC GameDialog on a later UI frame. The
// successful v0.1.8 probe starts with an already-open dialog; automated
// ClickNPC/TryClickUI needs a slower controller-side handoff. Keep all waits
// outside the game UI thread and give every semantic stage 2 seconds to settle.
constexpr DWORD kShortcutNpcUiReadyMs = 2000;
constexpr DWORD kShortcutSemanticRetryMs = 2000;
constexpr DWORD kShortcutConfirmUiReadyMs = 2000;
constexpr DWORD kShortcutConfirmRetryMs = 2000;
constexpr int kShortcutSemanticMaxAttempts = 12;
constexpr int kShortcutConfirmMaxAttempts = 20;
constexpr int kMainTradeRole = 1;
constexpr int kFirstChildTradeRole = 2;
constexpr int kChildTradeCount = 30;
constexpr int kLastChildTradeRole = kFirstChildTradeRole + kChildTradeCount - 1;
constexpr int kOverflowChildTradeRole = kLastChildTradeRole + 1; // CON beyond scheduler capacity.






constexpr int IDC_CLIENT_LIST = 100;
constexpr int IDC_SCAN = 101;
constexpr int IDC_TEST_IMAGE_SCAN = 106;
constexpr int IDC_FILTER_MODE2 = 5002;
constexpr int IDC_SCAN_BAG_SEMANTIC = 5003;
constexpr int IDC_AUTO_LOOT_TOGGLE = 5004;
constexpr int IDC_AUTO_LOOT_INTERVAL = 5005;
constexpr int IDC_LOOT_SCAN = 5006;
constexpr int IDC_LOOT_PICK = 5007;
constexpr int IDC_LOOT_OUTPUT = 5008;
constexpr int IDC_BAG_SCAN_REFRESH = 5101;
constexpr int IDC_BAG_SCAN_ADD_SELECTED = 5102;
constexpr int IDC_BAG_SCAN_NAME_EDIT = 5103;
constexpr int IDC_BAG_SCAN_ADD_NAME = 5104;
constexpr int IDC_BAG_SCAN_REMOVE_NAME = 5105;
constexpr int IDC_BAG_SCAN_CLEAR_NAMES = 5106;
constexpr int IDC_BAG_SCAN_ITEMS = 5107;
constexpr int IDC_BAG_SCAN_NAMES = 5108;
constexpr int IDC_BAG_SCAN_STATUS = 5109;
constexpr int IDC_START_CHECKED = 102;
constexpr int IDC_STOP_CHECKED = 103;
constexpr int IDC_SELECTED = 104;
constexpr int IDC_LIVE = 105;
constexpr int IDC_TARGET_NAME = 110;
constexpr int IDC_SAVE_TARGET = 111;
constexpr int IDC_TARGET_TEXT = 112;
constexpr int IDC_TOLERANCE = 113;
constexpr int IDC_SPOT_COMBO = 114;
constexpr int IDC_DELETE_SPOT = 115;
constexpr int IDC_ENABLE_REVIVE = 120;
constexpr int IDC_ENABLE_CONFIRM = 121;
constexpr int IDC_ENABLE_FIGHT = 122;
constexpr int IDC_CAPTURE_AUTO = 132;
constexpr int IDC_CAPTURE_ATTACK = 133;
constexpr int IDC_CAPTURE_STOP_AUTO_2 = 135;
constexpr int IDC_POINT_AUTO = 142;
constexpr int IDC_POINT_ATTACK = 143;
constexpr int IDC_POINT_STOP_AUTO_2 = 145;
constexpr int IDC_TEST_AUTO = 152;
constexpr int IDC_TEST_ATTACK = 153;
constexpr int IDC_TEST_STOP_AUTO_2 = 155;
constexpr int IDC_LOG = 160;
constexpr int IDC_EXPORT_LOG = 161;
constexpr int IDC_LOG_ENABLED = 162;
constexpr int IDC_TRADE_ROLE = 190;
constexpr int IDC_TRADE_RENDEZVOUS_CAPTURE = 195;
constexpr int IDC_SELL_SEQUENCE = 197;
constexpr int IDC_MAIN_TRADE_SEQUENCE = 198;
constexpr int IDC_CHILD_TRADE_SEQUENCE = 199;
constexpr int IDC_COPY_CLICKS = 200;
constexpr int IDC_CONSOLIDATE_TOGGLE = 205;
constexpr int IDC_MAIN_TAB = 207;
constexpr int IDC_COMPACT_TOGGLE = 209;
constexpr int IDC_ENABLE_SHORTCUT = 214;
constexpr int IDC_SHORTCUT_SETTINGS = 215;
constexpr int IDC_SELECT_ALL_ACCOUNTS = 216;
constexpr int IDC_CLEAR_ALL_ACCOUNTS = 217;
constexpr int IDC_PARTY_COMBO = 218;
constexpr int IDC_ASSIGN_PARTY = 219;
constexpr int IDC_APPLY_SPOT_PARTY = 220;
constexpr int IDC_APPLY_SPOT_ALL_CON = 221;
constexpr int IDC_GATHER_CAPTURE = 222;
constexpr int IDC_GATHER_TOGGLE = 223;
constexpr int IDC_SET_PARTY_KEY = 224;
constexpr int IDC_PARTY_BUILD_TOGGLE = 225;
constexpr int IDC_PB_CAPTURE_CLICK1 = 226;
constexpr int IDC_PB_CAPTURE_CLICK2 = 227;
constexpr int IDC_PB_CAPTURE_FACE = 228;
constexpr int IDC_PB_TEST_CLICK1 = 229;
constexpr int IDC_PB_TEST_CLICK2 = 230;
constexpr int IDC_PB_TEST_FACE = 231;
constexpr int IDC_PB_DELAY_CLICK1 = 232;
constexpr int IDC_PB_DELAY_CLICK2 = 233;
constexpr int IDC_PB_DELAY_FACE = 234;
constexpr int IDC_PB_TARGET_RETRY = 235;
constexpr int IDC_PB_INVITE_RETRY = 236;
constexpr int IDC_CLEAR_LOG = 237;
// Shortcut settings secondary window. 630-699 is isolated from inventory/Telegram IDs.
constexpr int IDC_SC_THEME = 630;
constexpr int IDC_SC_KUNLUN_X = 631;
constexpr int IDC_SC_KUNLUN_Y = 632;
constexpr int IDC_SC_XA_X = 633;
constexpr int IDC_SC_XA_Y = 634;
constexpr int IDC_SC_NGAI_X = 635;
constexpr int IDC_SC_NGAI_Y = 636;
constexpr int IDC_SC_TINHTUC_X = 637;
constexpr int IDC_SC_TINHTUC_Y = 638;
constexpr int IDC_SC_SAVE = 646;
constexpr int IDC_SC_CLOSE = 647;
constexpr int IDC_SC_CAPTURE_COORD_0 = 651;
constexpr int IDC_SC_CAPTURE_COORD_1 = 652;
constexpr int IDC_SC_CAPTURE_COORD_2 = 653;
constexpr int IDC_SC_CAPTURE_COORD_3 = 654;
constexpr int IDC_SC_SELLER_COMBO = 658;
constexpr int IDC_SC_SELLER_CAPTURE = 659;
constexpr int IDC_SC_SELLER_LABEL = 660;
constexpr int IDC_SC_CAPTURE_KUNLUN_CLICK_0 = 682;
constexpr int IDC_SC_CAPTURE_KUNLUN_CLICK_1 = 683;
constexpr int IDC_SC_CAPTURE_KUNLUN_CLICK_2 = 684;
constexpr int IDC_SC_KUNLUN_TIME_0 = 685;
constexpr int IDC_SC_KUNLUN_TIME_1 = 686;
constexpr int IDC_SC_KUNLUN_TIME_2 = 687;
constexpr int IDC_SC_KUNLUN_DELAY_0 = 688;
constexpr int IDC_SC_KUNLUN_DELAY_1 = 689;
constexpr int IDC_SC_KUNLUN_DELAY_2 = 690;
constexpr int IDC_SC_POST_TRADE_ENABLED = 691;
constexpr int IDC_SC_POST_TRADE_DELAY = 692;
constexpr int IDC_SC_POST_TRADE_REPEAT = 693;
constexpr int IDC_SC_POST_TRADE_CAPTURE = 694;
// Telegram controls use a dedicated range to stay isolated from the main editor.
constexpr int IDC_TG_ENABLED = 500;
constexpr int IDC_TG_TOKEN = 501;
constexpr int IDC_TG_SHOW_TOKEN = 502;
constexpr int IDC_TG_CHAT_ID = 503;
constexpr int IDC_TG_SAVE = 504;
constexpr int IDC_TG_TEST_BOT = 505;
constexpr int IDC_TG_DISCOVER_CHAT = 506;
constexpr int IDC_TG_SEND_TEST = 507;
constexpr int IDC_TG_SEND_SUMMARY = 508;
constexpr int IDC_TG_LOG = 509;
constexpr int IDC_TG_CLEAR_LOG = 510;
constexpr int IDC_TG_COPY_LOG = 511;
constexpr int IDC_TG_EXPORT_LOG = 512;
constexpr int IDC_TG_LOG_ENABLED = 513;
constexpr int IDC_TG_NOTIFY_DEATH = 520;
constexpr int IDC_TG_NOTIFY_REVIVE = 521;
constexpr int IDC_TG_NOTIFY_SELL_COMPLETE = 522;
constexpr int IDC_TG_NOTIFY_SELL_SUMMARY = 523;
constexpr int IDC_TG_NOTIFY_TRADE = 524;
constexpr int IDC_TG_NOTIFY_FREEZE = 525;
constexpr int IDC_TG_NOTIFY_FIFO = 526;
constexpr int IDC_TG_NOTIFY_LAULAN = 527;
constexpr int IDC_TG_NOTIFY_WORLDFLOW_TIMEOUT = 528;
constexpr int IDC_TG_NOTIFY_TOOL_STATE = 529;
constexpr int IDC_TG_NOTIFY_SESSION_SUMMARY = 530;
constexpr int IDC_TG_INTERVAL_ENABLED = 540;
constexpr int IDC_TG_INTERVAL_MINUTES = 541;
constexpr int IDC_TG_DAILY_ENABLED = 542;
constexpr int IDC_TG_DAILY_TIME1 = 543;
constexpr int IDC_TG_DAILY_TIME2 = 544;
constexpr int IDC_TG_DAILY_TIME3 = 545;
constexpr int IDC_TG_DAILY_TIME4 = 546;
constexpr int IDC_TG_WORLDFLOW_TIMEOUT_SEC = 547;
constexpr int IDC_TG_STATUS = 548;
constexpr int IDC_TG_NOTIFY_FUN_ALERTS = 549;
constexpr int IDC_TG_MONEY_1M = 550;
constexpr int IDC_TG_MONEY_5M = 551;
constexpr int IDC_TG_MONEY_60M = 552;
constexpr int IDC_TG_MONEY_6H = 553;
constexpr int IDC_TG_MONEY_24H = 554;
constexpr UINT kTelegramResultMessage = WM_APP + 0x61;
constexpr int IDC_SEQ_LIST = 300;
constexpr int IDC_SEQ_TARGET = 301;
constexpr int IDC_SEQ_DESC = 303;
constexpr int IDC_SEQ_DELAY = 304;
constexpr int IDC_SEQ_REPEAT = 305;
constexpr int IDC_SEQ_ADD = 306;
constexpr int IDC_SEQ_DELETE = 307;
constexpr int IDC_SEQ_UP = 308;
constexpr int IDC_SEQ_DOWN = 309;
constexpr int IDC_SEQ_SAVE = 310;
constexpr int IDC_SEQ_CAPTURE = 311;
constexpr int IDC_SEQ_TEST = 312;
constexpr int IDC_SEQ_CLOSE = 313;
constexpr int IDC_SEQ_REC = 314;
constexpr int IDC_SEQ_COPY = 315;
constexpr int IDC_SEQ_PASTE = 316;
constexpr int IDC_SEQ_GROUP_REPEAT = 317;
constexpr int IDC_SEQ_GROUP_SELECTED = 318;
constexpr int IDC_SEQ_UNGROUP = 319;
constexpr int IDC_SEQ_AFTER_PUTUP = 320;
constexpr int IDC_SEQ_ACTION = 321;
constexpr int IDC_SEQ_MIN_TIME = 322;
constexpr int IDC_SEQ_POST_CLEANUP = 323;

constexpr std::array<const wchar_t*, 5> kClickKeys = {
    L"Confirm", L"Revive", L"AutoMenu", L"Attack", L"StopAuto2"
};
constexpr std::array<const wchar_t*, 5> kClickLabels = {
    L"XÁC NHẬN RA MAP", L"ĐẦU THAI", L"AUTO", L"ĐÁNH QUÁI", L"DỪNG AUTO 2"
};

enum class ClickSlot : int {
    None = -1,
    Confirm = 0,
    Revive = 1,
    AutoMenu = 2, // one saved UI point named AUTO replaces old DỪNG AUTO 1
    Attack = 3,
    StopAuto2 = 4,
};

enum class PriorityAutoOwner : int {
    None = 0,
    TravelGuardStop,
    TravelGuardReset,
    Train,
    MountRecovery,
};

struct ClickPoint {
    int x = 0;
    int y = 0;
    int baseW = 0;
    int baseH = 0;
    bool valid = false;
};

struct TimedClickPoint {
    ClickPoint point{};
    // Controller-side wait before dispatching TryClickUI. Keeping this wait out
    // of the game thread prevents a configurable timing value from freezing Unity.
    int timeMs = 0;
    // Wait after this completed TryClickUI before the next step/map check.
    int delayMs = 2000;
};

struct PartyBuildSettings {
    std::array<ClickPoint, 3> clicks{}; // 0=KEY click1, 1=KEY click2, 2=member face
    std::array<int, 3> delaysMs{{300, 300, 700}};
    int targetRetry = 6;
    int inviteRetry = 8;
};

struct ShortcutSettings {
    bool enabled = false;
    int theme = 0; // 0=system/light, 1=dark for the shortcut settings panel.
    // Legacy shortcut points remain user-supplied.
    int kunlunNpcX = 0, kunlunNpcY = 0;
    int xaTruyenX = 0, xaTruyenY = 0; // legacy v3 fields: v3.2 runtime NEVER reads these; ID387 uses sellNpcPositions_.
    int ngaiX = 0, ngaiY = 0;
    int tinhTucX = 0, tinhTucY = 0;
    std::array<TimedClickPoint, 3> kunlunExitClicks{};

    // Independent best-effort click that runs after the saved trade sequence finishes.
    // It is deliberately NOT a TradeSequenceStep and uses one shared coordinate for
    // MAIN first, then the active CON that just traded with MAIN.
    bool postTradeClickEnabled = false;
    ClickPoint postTradeClick{};
    int postTradeClickDelayMs = 200;
    int postTradeClickRepeat = 1; // 0 = enabled but intentionally do no click.

};

enum class ShortcutKind : int {
    None = 0,
    KunLunExit = 1,
    KunLunEnter = 2,
    FireEnter = 3,
    FireExit = 4,
};

inline bool IsPrimaryShortcutOriginMap(int mapID) {
    // Verified MAPS.csv: Đại Lý=2, Tô Châu=4, Lâu Lan=5, Võ Đang=14, Mộ Dung=15.
    return mapID==2||mapID==4||mapID==5||mapID==14||mapID==15;
}
constexpr int kThienSonMapId = 13;
constexpr int kThienSonTransitX = 3073;
constexpr int kThienSonTransitY = 2338;

struct TradeSequenceStep {
    // v0.2.7 child workflow semantics:
    // target=0 => active CON uses this row's own point.
    // target=1 => MAIN executes shared step mainRef from the MAIN common sequence.
    int target = 0;
    int mainRef = -1;
    std::wstring description;
    ClickPoint point{};
    int delayMs = 500;
    int repeat = 1;       // repeat this individual row
    int groupId = 0;      // 0=not grouped; >0=contiguous mini-sequence
    int groupRepeat = 1;  // repeat the whole mini-sequence before continuing
    int afterAction = 0;   // 0=none; 1=semantic PUT UP after each raw click occurrence
    // V23 UI DIRECT: raw coordinate and semantic UI callbacks share the same reorderable row model.
    // actionKind=0 keeps the proven coordinate macro; actionKind=1 invokes uiDirectTarget with
    // state-driven retry and the same 2000ms timeout contract as the existing ĐẶT LÊN callback.
    int actionKind = 0;    // 0=raw coordinate, 1=UI DIRECT
    int uiDirectTarget = static_cast<int>(UiDirectTarget::None);
    int minTimeMs = 0;    // non-blocking not-before time for UI DIRECT rows, default 0ms
};

enum class RecorderMode : int { None = 0, TradeMain = 1, TradeChild = 2 };

struct RecordedClick {
    DWORD pid = 0;
    ClickPoint point{};
    DWORD tick = 0;
};

struct SellNpcPreset {
    const wchar_t* name;
    int mapID;
    int npcID;
};

constexpr std::array<SellNpcPreset, 6> kSellNpcs = {{
    {L"Mã Kiêu Minh • M5 • ID 373", 5, 373},
    {L"Dược Đại Phu • Hỏa Diệm Sơn M55 • ID 279", 55, 279},
    {L"Ba Nhĩ • Lâu Lan M5 • ID 328", 5, 328},
    {L"Xa Truyền Bình • Lâu Lan M5 • ID 387", 5, 387},
    {L"Uông Diên • Đôn Hoàng M22 • ID 159", 22, 159},
    {L"A Lạp Bá Nhân • Hỏa Diệm Sơn M55 • ID 275", 55, 275},
}};

struct SellNpcPosition {
    int x = 0;
    int y = 0;
    bool valid = false;
};

struct TargetProfile {
    std::wstring name;
    int mapID = 0;
    int x = 0;
    int y = 0;
    bool valid = false;
};

struct TelegramSettings {
    bool enabled = false;
    std::wstring botToken{};
    std::wstring chatId{};
    bool notifyDeath = true;
    bool notifyRevive = true;
    bool notifySellComplete = false; // intentionally OFF by default to avoid spam.
    bool notifySellSummary = true;
    bool notifyTradeComplete = true;
    bool notifyClientFreeze = true;
    bool notifyFifoEnter = false;
    bool notifyLauLanConfirm = false;
    bool notifyWorldFlowTimeout = true;
    bool notifyToolState = false;
    bool notifySessionSummary = true;
    bool notifyFunAlerts = false; // legacy field ignored by CP9
    bool reportDeathCount = true;
    bool reportReceiveCount = true;
    bool reportSellCount = true;
    bool notifyMoneyMilestones = true;
    bool notifyDeathBurst = true;
    // Currency milestones are independently selectable. 1m/5m are test-friendly and OFF by default.
    std::array<bool, 5> currencyMilestones{false, false, true, true, true};
    bool intervalEnabled = true;
    int intervalMinutes = 60;
    bool dailyEnabled = true;
    std::array<std::wstring, 4> dailyTimes{L"08:00", L"12:00", L"18:00", L"23:00"};
    int worldFlowTimeoutSec = 120;
};

struct TelegramStats {
    bool active = false;
    SYSTEMTIME startedLocal{};
    ULONGLONG startedTick = 0;
    int sellTotal = 0;
    int tradeTotal = 0;
    int deathTotal = 0;
    int reviveTotal = 0;
    int fifoTotal = 0;
    int lauLanConfirmTotal = 0;
    int clientFreezeTotal = 0;
    int worldFlowTimeoutTotal = 0;
    std::map<DWORD, int> sellsByPid{};
    std::map<DWORD, int> tradesByChildPid{};
};

struct TelegramReportBaseline {
    int sellTotal = 0;
    int tradeTotal = 0;
    int deathTotal = 0;
    int reviveTotal = 0;
    int fifoTotal = 0;
    int lauLanConfirmTotal = 0;
    int clientFreezeTotal = 0;
    int worldFlowTimeoutTotal = 0;
    std::map<DWORD, int> sellsByPid{};
    std::map<DWORD, int> tradesByChildPid{};
};

struct TelegramAccountWatch {
    bool lifeKnown = false;
    bool lastDead = false;
    DWORD deathStartedTick = 0;
    std::deque<DWORD> recentDeathTicks{};
    DWORD deathBurstLastAlertTick = 0;
    DWORD autoTrainOffStartedTick = 0;
    bool autoTrainOffAlertSent = false;
    std::uint64_t lastWorkflowTicket = 0;
    DWORD worldFlowTravelStartedTick = 0;
    bool worldFlowTimeoutSent = false;
    bool criticalFreezeNotified = false;
};

struct AccountProfile {
    std::wstring section;
    // Runtime-only role: 1=MAIN, 2..31=CON1..CON30, 32=CON overflow.
    // CON numbering is regenerated on every scan and is never persisted.
    int tradeRole = 0;
    // Chỉ là nhãn UI để gom/nhìn/chọn nhanh; tuyệt đối không tham gia workflow NORMAL.
    int displayParty = 0;
    // CP4: chỉ dùng bởi chế độ độc quyền TỰ TẠO PT. Mỗi PT được phép có đúng một KEY.
    bool partyKey = false;
    std::wstring selectedSpot;
    int tolerance = 120;
    bool enableRevive = true;
    bool enableConfirm = true;
    bool enableFight = true;
    bool enableSell = false; // MAIN count-driven seller only; NONE disabled.
    TargetProfile target{};
    std::array<ClickPoint, 5> points{};
    // Legacy v0.2.3-v0.2.6 per-CON workflow kept only for one-time v0.2.7 migration.
    // Active v0.2.7 uses one global childTradeSequence_ shared by every CON.
    std::vector<TradeSequenceStep> childTradeSequence{};
};

struct GameClient {
    DWORD pid = 0;
    DWORD threadId = 0;
    HWND window = nullptr;
    std::wstring title;
};


enum class AccountUiRecoveryPlan : int { None=0, GenericFailClosed=1, PostRevive=2 };

struct RuntimeState {
    bool running = false;
    std::wstring status = L"Đã dừng";
    int qualifiedMap = 0;
    int candidateMap = 0;
    int candidateCount = 0;
    DWORD lastActionTick = 0;
    Action lastAction = Action::Wait;
    DWORD lastStartPathPassTick = 0;

    DWORD deadSinceTick = 0;
    int revivePhase = 0;
    DWORD revivePhaseTick = 0;
    DWORD lastReviveClickTick = 0;

    int lastObservedMap = 0;
    int lastObservedX = 0;
    int lastObservedY = 0;
    DWORD lastMovementTick = 0;
    bool crossMapSeenAutoPath = false;
    DWORD stallSinceTick = 0;
    int confirmAttempts = 0;
    DWORD lastLauLanConfirmTick = 0;

    int fightPhase = 0;
    DWORD fightPhaseTick = 0;
    int fightAttempts = 0;
    DWORD fightRetryWaitTick = 0;
    bool wasAtTarget = false;

    // Once AUTO fight is confirmed at the training spot, position is intentionally
    // checked every 1 minute. Death/bag state are still observed every tick.
    bool trainPositionMonitorArmed = false;
    DWORD lastTrainPositionCheckTick = 0;
    DWORD lastAutoFightCheckTick = 0;
    int trainRecoveryPhase = 0;

    // Dedicated trade-rendezvous state. AutoFight stopping is no longer duplicated here;
    // every StartPath is protected by the shared v0.3 AutoFight Travel Guard.
    int tradeTravelPhase = 0;
    DWORD tradeTravelTick = 0;
    bool tradeTravelReady = false;
    std::uint64_t tradeWorkflowEntrySeq = 0; // R7: immutable FIFO ticket while staged in workflow.
    std::wstring tradeAdmissionReport{}; // De-duplicates FULL-at-bãi admission reports.
    bool mainParkReportSent = false;

    // Priority-AUTO request/result mailbox. v0.6.1.6 dispatches configured points
    // through InputSyncManager. An Attack request owns both AUTO then ĐÁNH QUÁI
    // phases and publishes one result only after click 2 finishes.
    ClickSlot priorityAutoRequestSlot = ClickSlot::None;
    PriorityAutoOwner priorityAutoRequestOwner = PriorityAutoOwner::None;
    ClickSlot priorityAutoCompletedSlot = ClickSlot::None;
    PriorityAutoOwner priorityAutoCompletedOwner = PriorityAutoOwner::None;
    bool priorityAutoCompletedOk = false;
    DWORD priorityAutoCompletedTick = 0;
    int priorityAutoPointPhase = 0;
    DWORD priorityAutoPointTick = 0;

    // v0.3 shared AutoFight Travel Guard. Any StartPath must prove authoritative
    // AutoFight OFF. Two failed stop cycles trigger AUTO->Attack reset, then stop retries.
    int travelFightGuardPhase = 0;
    DWORD travelFightGuardTick = 0;
    int travelFightStopAttempts = 0;

    // Hard runtime invariant. If a snapshot ever exposes AutoPath=ON together
    // with AutoFight=ON, stop the path first, finish the normal two-stop/reset
    // Travel Guard, and require both states OFF before any route may resume.
    bool autoPathFightConflictLatched = false;
    DWORD autoPathFightConflictTick = 0;
    int autoPathFightConflictStopAttempts = 0;

    // Shared robust-travel helper: mount x2 -> fight 10s -> stop fight -> mount x2.
    // If still not mounted, reset and repeat; StartPath on foot is forbidden.
    int travelMountAttempts = 0;
    DWORD travelMountTick = 0;
    int travelMountCycle = 0; // 0=before fight boost, 1=after 10s fight boost
    int travelFightBoostPhase = 0;
    DWORD travelFightBoostTick = 0;
    bool travelFootFallback = false;
    DWORD travelFootTick = 0;

    bool crossMapRouteArmed = false;
    bool crossMapRouteMoved = false;

    // Optional user-enabled shortcut router. It wraps the existing robust travel helper;
    // unrelated travel remains unchanged when shortcutKind=None or the global checkbox is off.
    ShortcutKind shortcutKind = ShortcutKind::None;
    int shortcutPhase = 0;
    int shortcutFinalMap = 0;
    int shortcutExpectedMap = 0;
    int shortcutSourceMap = 0;
    int shortcutClickIndex = 0;
    DWORD shortcutTick = 0;
    int shortcutAttempts = 0;

    // T17 inactive NONE FSM compatibility; removed by T18.
    int retiredNoneSellPreset = 0;
    int sellPhase = 0;
    DWORD sellPhaseTick = 0;
    int sellOpenAttempts = 0;
    int sellMacroIndex = 0;
    int sellMacroRepeatDone = 0;
    DWORD sellMacroNextTick = 0;
    DWORD sellMacroCompletionDueTick = 0;
    int sellMacroPass = 0;
    int sellLastFreeBag = -1;
    DWORD sellBagStableSince = 0;
    int sellBlockReportCode = 0;

    // Global per-PID transition/unresponsive safety gate. While active, no mutable
    // gameplay/window action may be dispatched. Read-only state polling continues
    // until the client is continuously healthy for kClientStableResumeMs.
    bool clientFreezeActive = false;
    DWORD clientFreezeSinceTick = 0;
    DWORD clientStableSinceTick = 0;
    int readStateFailStreak = 0;
    DWORD lastReadFailureLogTick = 0;

    // Map 87 = Địa Phủ uses the same v0.3 AutoFight Travel Guard as every other
    // movement flow. This flag is log-only; there is no separate M87 stop state machine.
    bool underworldGuardLogged = false;

    // A tool-runtime reset does not stop the game client's real AutoPath. After a
    // fresh Start or revive cold-start, reacquire route ownership by forcing any
    // stale AutoPath OFF and verifying it before a new StartPath may arm Confirm.
    bool routeOwnershipResetPending = false;
    DWORD routeOwnershipStopTick = 0;
    int routeOwnershipStopAttempts = 0;
    bool routeOwnershipResetLogged = false;

    // High-priority UI recovery. It blocks normal gameplay mutation while active.
    AccountUiRecoveryPlan uiRecoveryPlan = AccountUiRecoveryPlan::None;
    int uiRecoveryStage = 0;
    DWORD uiRecoveryStartedTick = 0;
    DWORD uiRecoveryNextTick = 0;
    bool uiRecoverySweepChanged = false;
    bool uiRecoverySweepUnresolved = false;
    int uiRecoverySweepCount = 0;
    int uiRecoveryCleanSweepStreak = 0;
    bool postReviveRecoveryPending = false;
    std::wstring uiRecoveryReason{};

    // 10.2 lowest-priority semantic ItemPack scheduler. User enable is global/persistent;
    // these fields are runtime-only and never change the user's ON/OFF setting.
    DWORD autoLootNextTick = 0;
    DWORD autoLootLastErrorLogTick = 0;
    bool autoLootErrorLatched = false;
    bool autoLootBlockedThisTick = false;
};

template <typename T>
bool ResolveProc(HMODULE module, const char* name, T& out) {
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

std::wstring LegacyConfigPath() { return ExeDir() + L"\\ThanLongCleanRoute.accounts.ini"; }

std::wstring ConfigDir() {
    wchar_t localAppData[4096]{};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
    if (n > 0 && n < _countof(localAppData)) {
        std::wstring dir = std::wstring(localAppData) + L"\\ThanLongCleanRoute";
        (void)CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return ExeDir();
}

std::wstring ConfigPath() {
    static const std::wstring path = ConfigDir() + L"\\ThanLongCleanRoute.accounts.ini";
    return path;
}

void MigrateLegacyConfigIfNeeded() {
    const std::wstring current = ConfigPath();
    const std::wstring legacy = LegacyConfigPath();
    if (current == legacy) return;
    if (GetFileAttributesW(current.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    if (GetFileAttributesW(legacy.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    (void)CopyFileW(legacy.c_str(), current.c_str(), TRUE);
}

void FlushIni() {
    (void)WritePrivateProfileStringW(nullptr, nullptr, nullptr, ConfigPath().c_str());
}

void EnsureUnicodeIni() {
    const std::wstring path = ConfigPath();
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    const BYTE bom[2] = {0xFF, 0xFE};
    DWORD done = 0;
    (void)WriteFile(h, bom, 2, &done, nullptr);
    CloseHandle(h);
}

int ReadIniInt(const std::wstring& section, const std::wstring& key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(section.c_str(), key.c_str(), fallback, ConfigPath().c_str()));
}

void WriteIniInt(const std::wstring& section, const std::wstring& key, int value) {
    wchar_t text[32]{};
    wsprintfW(text, L"%d", value);
    WritePrivateProfileStringW(section.c_str(), key.c_str(), text, ConfigPath().c_str());
}

std::wstring ReadIniText(const std::wstring& section, const std::wstring& key) {
    wchar_t text[512]{};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), L"", text, _countof(text), ConfigPath().c_str());
    return text;
}

void WriteIniText(const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), ConfigPath().c_str());
}

std::wstring Utf8ToWide(const std::string& input);
std::string WideToUtf8(const std::wstring& input);

TelegramSettings LoadTelegramSettings(std::wstring& warning) {
    TelegramSettings t{};
    const std::wstring section = L"Telegram";
    t.enabled = ReadIniInt(section, L"Enabled", 0) != 0;
    t.chatId = ReadIniText(section, L"ChatId");
    t.notifyDeath = false; // CP9 legacy TELE event removed
    t.notifyRevive = false; // CP9 legacy TELE event removed
    t.notifySellComplete = false; // CP9 legacy TELE event removed
    t.notifySellSummary = false; // CP9 legacy TELE event removed
    t.notifyTradeComplete = false; // CP9 legacy TELE event removed
    t.notifyClientFreeze = false; // CP9 legacy TELE event removed
    t.notifyFifoEnter = false; // CP9 legacy TELE event removed
    t.notifyLauLanConfirm = false; // CP9 legacy TELE event removed
    t.notifyWorldFlowTimeout = false; // CP9 legacy TELE event removed
    t.notifyToolState = false; // CP9 legacy TELE event removed
    t.notifySessionSummary = false; // CP9 legacy TELE event removed
    t.notifyFunAlerts = false; // CP9 legacy TELE event removed
    t.reportDeathCount = ReadIniInt(section,L"ReportDeathCount",1)!=0;
    t.reportReceiveCount = ReadIniInt(section,L"ReportReceiveCount",1)!=0;
    t.reportSellCount = ReadIniInt(section,L"ReportSellCount",1)!=0;
    t.notifyMoneyMilestones = ReadIniInt(section,L"NotifyMoneyMilestones",1)!=0;
    t.notifyDeathBurst = ReadIniInt(section,L"NotifyDeathBurst",1)!=0;
    t.currencyMilestones[0] = ReadIniInt(section, L"MoneyMilestone1m", 0) != 0;
    t.currencyMilestones[1] = ReadIniInt(section, L"MoneyMilestone5m", 0) != 0;
    t.currencyMilestones[2] = ReadIniInt(section, L"MoneyMilestone60m", 1) != 0;
    t.currencyMilestones[3] = ReadIniInt(section, L"MoneyMilestone6h", 1) != 0;
    t.currencyMilestones[4] = ReadIniInt(section, L"MoneyMilestone24h", 1) != 0;
    t.intervalEnabled = ReadIniInt(section, L"IntervalEnabled", 1) != 0;
    t.intervalMinutes = telegram_logic::ClampSummaryIntervalMinutes(ReadIniInt(section, L"IntervalMinutes", 60));
    t.dailyEnabled = ReadIniInt(section, L"DailyEnabled", 1) != 0;
    const std::array<std::wstring, 4> defaults{L"08:00", L"12:00", L"18:00", L"23:00"};
    for (int i = 0; i < 4; ++i) {
        const std::wstring raw = ReadIniText(section, L"DailyTime" + std::to_wstring(i + 1));
        t.dailyTimes[static_cast<std::size_t>(i)] = telegram_logic::NormalizeDailyTime(raw.empty() ? defaults[static_cast<std::size_t>(i)] : raw,
                                                                                       defaults[static_cast<std::size_t>(i)]);
    }
    t.worldFlowTimeoutSec = telegram_logic::ClampWorldFlowTimeoutSeconds(ReadIniInt(section, L"WorldFlowTimeoutSec", 120));

    const std::wstring protectedToken = ReadIniText(section, L"BotTokenProtected");
    if (!protectedToken.empty()) {
        std::wstring error;
        if (!telegram_notify::UnprotectTokenForCurrentUser(protectedToken, t.botToken, error)) {
            warning = L"Telegram Bot Token đã lưu không giải mã được bằng Windows DPAPI; Telegram tạm tắt. " + error;
            t.enabled = false;
            t.botToken.clear();
        }
    }
    return t;
}

bool SaveTelegramSettings(const TelegramSettings& t, std::wstring& error) {
    EnsureUnicodeIni();
    const std::wstring section = L"Telegram";
    std::wstring protectedToken;
    if (!telegram_notify::ProtectTokenForCurrentUser(t.botToken, protectedToken, error)) return false;
    WriteIniInt(section, L"Enabled", t.enabled ? 1 : 0);
    WriteIniText(section, L"BotTokenProtected", protectedToken);
    // Clean any accidental/plain legacy key if it ever existed during development.
    WritePrivateProfileStringW(section.c_str(), L"BotToken", nullptr, ConfigPath().c_str());
    WriteIniText(section, L"ChatId", t.chatId);
    WriteIniInt(section, L"NotifyDeath", t.notifyDeath ? 1 : 0);
    WriteIniInt(section, L"NotifyRevive", t.notifyRevive ? 1 : 0);
    WriteIniInt(section, L"NotifySellComplete", t.notifySellComplete ? 1 : 0);
    WriteIniInt(section, L"NotifySellSummary", t.notifySellSummary ? 1 : 0);
    WriteIniInt(section, L"NotifyTradeComplete", t.notifyTradeComplete ? 1 : 0);
    WriteIniInt(section, L"NotifyClientFreeze", t.notifyClientFreeze ? 1 : 0);
    WriteIniInt(section, L"NotifyFifoEnter", t.notifyFifoEnter ? 1 : 0);
    WriteIniInt(section, L"NotifyLauLanConfirm", t.notifyLauLanConfirm ? 1 : 0);
    WriteIniInt(section, L"NotifyWorldFlowTimeout", t.notifyWorldFlowTimeout ? 1 : 0);
    WriteIniInt(section, L"NotifyToolState", t.notifyToolState ? 1 : 0);
    WriteIniInt(section, L"NotifySessionSummary", t.notifySessionSummary ? 1 : 0);
    WriteIniInt(section, L"NotifyFunAlerts", t.notifyFunAlerts ? 1 : 0);
    WriteIniInt(section,L"ReportDeathCount",t.reportDeathCount?1:0);
    WriteIniInt(section,L"ReportReceiveCount",t.reportReceiveCount?1:0);
    WriteIniInt(section,L"ReportSellCount",t.reportSellCount?1:0);
    WriteIniInt(section,L"NotifyMoneyMilestones",t.notifyMoneyMilestones?1:0);
    WriteIniInt(section,L"NotifyDeathBurst",t.notifyDeathBurst?1:0);
    WriteIniInt(section, L"MoneyMilestone1m", t.currencyMilestones[0] ? 1 : 0);
    WriteIniInt(section, L"MoneyMilestone5m", t.currencyMilestones[1] ? 1 : 0);
    WriteIniInt(section, L"MoneyMilestone60m", t.currencyMilestones[2] ? 1 : 0);
    WriteIniInt(section, L"MoneyMilestone6h", t.currencyMilestones[3] ? 1 : 0);
    WriteIniInt(section, L"MoneyMilestone24h", t.currencyMilestones[4] ? 1 : 0);
    WriteIniInt(section, L"IntervalEnabled", t.intervalEnabled ? 1 : 0);
    WriteIniInt(section, L"IntervalMinutes", telegram_logic::ClampSummaryIntervalMinutes(t.intervalMinutes));
    WriteIniInt(section, L"DailyEnabled", t.dailyEnabled ? 1 : 0);
    for (int i = 0; i < 4; ++i) WriteIniText(section, L"DailyTime" + std::to_wstring(i + 1), t.dailyTimes[static_cast<std::size_t>(i)]);
    WriteIniInt(section, L"WorldFlowTimeoutSec", telegram_logic::ClampWorldFlowTimeoutSeconds(t.worldFlowTimeoutSec));
    FlushIni();
    return true;
}

ShortcutSettings LoadShortcutSettings() {
    ShortcutSettings sc{};
    const std::wstring section = L"Shortcut";
    sc.enabled = ReadIniInt(section, L"Enabled", 0) != 0;
    sc.theme = std::clamp(ReadIniInt(section, L"Theme", 0), 0, 1);
    sc.postTradeClickEnabled = ReadIniInt(section, L"PostTradeClickEnabled", 0) != 0;
    sc.postTradeClick.x = ReadIniInt(section, L"PostTradeClickX", -1);
    sc.postTradeClick.y = ReadIniInt(section, L"PostTradeClickY", -1);
    sc.postTradeClick.baseW = ReadIniInt(section, L"PostTradeClickW", 0);
    sc.postTradeClick.baseH = ReadIniInt(section, L"PostTradeClickH", 0);
    sc.postTradeClick.valid = ReadIniInt(section, L"PostTradeClickValid", 0) != 0 &&
                              sc.postTradeClick.x >= 0 && sc.postTradeClick.y >= 0 &&
                              sc.postTradeClick.baseW > 0 && sc.postTradeClick.baseH > 0;
    sc.postTradeClickDelayMs = std::clamp(ReadIniInt(section, L"PostTradeClickDelayMs", 200), 0, 60000);
    sc.postTradeClickRepeat = std::clamp(ReadIniInt(section, L"PostTradeClickRepeat", 1), 0, 999);
    const int coordinateVersion = ReadIniInt(section, L"CoordinateVersion", 0);
    if (coordinateVersion >= 3) {
        sc.kunlunNpcX = ReadIniInt(section, L"KunLunNpcX", 0); sc.kunlunNpcY = ReadIniInt(section, L"KunLunNpcY", 0);
        sc.xaTruyenX = ReadIniInt(section, L"XaTruyenX", 0); sc.xaTruyenY = ReadIniInt(section, L"XaTruyenY", 0);
        sc.ngaiX = ReadIniInt(section, L"NgaiNiNgoaNhiX", 0); sc.ngaiY = ReadIniInt(section, L"NgaiNiNgoaNhiY", 0);
        sc.tinhTucX = ReadIniInt(section, L"TinhTucX", 0); sc.tinhTucY = ReadIniInt(section, L"TinhTucY", 0);
    }
    if (coordinateVersion >= 4) {
        for (std::size_t i = 0; i < sc.kunlunExitClicks.size(); ++i) {
            const std::wstring prefix = L"KunLunExitClick" + std::to_wstring(i) + L"_";
            TimedClickPoint& click = sc.kunlunExitClicks[i];
            click.point.x = ReadIniInt(section, prefix + L"X", -1);
            click.point.y = ReadIniInt(section, prefix + L"Y", -1);
            click.point.baseW = ReadIniInt(section, prefix + L"W", 0);
            click.point.baseH = ReadIniInt(section, prefix + L"H", 0);
            click.point.valid = ReadIniInt(section, prefix + L"Valid", 0) != 0 &&
                                click.point.x >= 0 && click.point.y >= 0 &&
                                click.point.baseW > 0 && click.point.baseH > 0;
            click.timeMs = std::clamp(ReadIniInt(section, prefix + L"TimeMs", 0), 0, 60000);
            click.delayMs = std::clamp(ReadIniInt(section, prefix + L"DelayMs", 2000), 0, 60000);
        }
    } else if (coordinateVersion >= 3) {
        // One-time migration: old opener -> click #1; #2/#3 stay invalid.
        TimedClickPoint& first = sc.kunlunExitClicks[0];
        first.point.x = ReadIniInt(section, L"KunLunOpenClickX", -1);
        first.point.y = ReadIniInt(section, L"KunLunOpenClickY", -1);
        first.point.baseW = ReadIniInt(section, L"KunLunOpenClickW", 0);
        first.point.baseH = ReadIniInt(section, L"KunLunOpenClickH", 0);
        first.point.valid = first.point.x >= 0 && first.point.y >= 0 &&
                            first.point.baseW > 0 && first.point.baseH > 0;
    }
    // CoordinateVersion < 3 is intentionally discarded: incompatible coordinate system.
    return sc;
}

void SaveShortcutSettings(const ShortcutSettings& sc) {
    EnsureUnicodeIni();
    const std::wstring section = L"Shortcut";
    WriteIniInt(section, L"Enabled", sc.enabled ? 1 : 0); WriteIniInt(section, L"Theme", sc.theme);
    WriteIniInt(section, L"PostTradeClickEnabled", sc.postTradeClickEnabled ? 1 : 0);
    WriteIniInt(section, L"PostTradeClickValid", sc.postTradeClick.valid ? 1 : 0);
    WriteIniInt(section, L"PostTradeClickX", sc.postTradeClick.valid ? sc.postTradeClick.x : -1);
    WriteIniInt(section, L"PostTradeClickY", sc.postTradeClick.valid ? sc.postTradeClick.y : -1);
    WriteIniInt(section, L"PostTradeClickW", sc.postTradeClick.valid ? sc.postTradeClick.baseW : 0);
    WriteIniInt(section, L"PostTradeClickH", sc.postTradeClick.valid ? sc.postTradeClick.baseH : 0);
    WriteIniInt(section, L"PostTradeClickDelayMs", std::clamp(sc.postTradeClickDelayMs, 0, 60000));
    WriteIniInt(section, L"PostTradeClickRepeat", std::clamp(sc.postTradeClickRepeat, 0, 999));
    WriteIniInt(section, L"CoordinateVersion", 5);
    WriteIniInt(section, L"KunLunNpcX", sc.kunlunNpcX); WriteIniInt(section, L"KunLunNpcY", sc.kunlunNpcY);
    WriteIniInt(section, L"XaTruyenX", sc.xaTruyenX); WriteIniInt(section, L"XaTruyenY", sc.xaTruyenY);
    WriteIniInt(section, L"NgaiNiNgoaNhiX", sc.ngaiX); WriteIniInt(section, L"NgaiNiNgoaNhiY", sc.ngaiY);
    WriteIniInt(section, L"TinhTucX", sc.tinhTucX); WriteIniInt(section, L"TinhTucY", sc.tinhTucY);
    for (std::size_t i = 0; i < sc.kunlunExitClicks.size(); ++i) {
        const std::wstring prefix = L"KunLunExitClick" + std::to_wstring(i) + L"_";
        const TimedClickPoint& click = sc.kunlunExitClicks[i];
        WriteIniInt(section, prefix + L"Valid", click.point.valid ? 1 : 0);
        WriteIniInt(section, prefix + L"X", click.point.valid ? click.point.x : -1);
        WriteIniInt(section, prefix + L"Y", click.point.valid ? click.point.y : -1);
        WriteIniInt(section, prefix + L"W", click.point.valid ? click.point.baseW : 0);
        WriteIniInt(section, prefix + L"H", click.point.valid ? click.point.baseH : 0);
        WriteIniInt(section, prefix + L"TimeMs", std::clamp(click.timeMs, 0, 60000));
        WriteIniInt(section, prefix + L"DelayMs", std::clamp(click.delayMs, 0, 60000));
    }
    for (const wchar_t* key : {L"KunLunOpenClickX", L"KunLunOpenClickY", L"KunLunOpenClickW", L"KunLunOpenClickH"})
        WritePrivateProfileStringW(section.c_str(), key, nullptr, ConfigPath().c_str());
    FlushIni();
}

PartyBuildSettings LoadPartyBuildSettings() {
    PartyBuildSettings s{};
    const std::wstring section = L"PartyBuilder";
    const std::array<std::wstring, 3> prefix{{L"KeyClick1", L"KeyClick2", L"MemberFace"}};
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        ClickPoint& p = s.clicks[i];
        p.x = ReadIniInt(section, prefix[i] + L"X", -1);
        p.y = ReadIniInt(section, prefix[i] + L"Y", -1);
        p.baseW = ReadIniInt(section, prefix[i] + L"W", 0);
        p.baseH = ReadIniInt(section, prefix[i] + L"H", 0);
        p.valid = ReadIniInt(section, prefix[i] + L"Valid", 0) != 0 &&
                  p.x >= 0 && p.y >= 0 && p.baseW > 0 && p.baseH > 0;
        const int fallback = i == 2 ? 700 : 300;
        s.delaysMs[i] = std::clamp(ReadIniInt(section, prefix[i] + L"DelayMs", fallback), 0, 60000);
    }
    s.targetRetry = party_build_logic::ClampRetry(ReadIniInt(section, L"TargetRetry", 6));
    s.inviteRetry = party_build_logic::ClampRetry(ReadIniInt(section, L"InviteRetry", 8));
    return s;
}

void SavePartyBuildSettings(const PartyBuildSettings& s) {
    EnsureUnicodeIni();
    const std::wstring section = L"PartyBuilder";
    const std::array<std::wstring, 3> prefix{{L"KeyClick1", L"KeyClick2", L"MemberFace"}};
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const ClickPoint& p = s.clicks[i];
        WriteIniInt(section, prefix[i] + L"Valid", p.valid ? 1 : 0);
        WriteIniInt(section, prefix[i] + L"X", p.valid ? p.x : -1);
        WriteIniInt(section, prefix[i] + L"Y", p.valid ? p.y : -1);
        WriteIniInt(section, prefix[i] + L"W", p.valid ? p.baseW : 0);
        WriteIniInt(section, prefix[i] + L"H", p.valid ? p.baseH : 0);
        WriteIniInt(section, prefix[i] + L"DelayMs", std::clamp(s.delaysMs[i], 0, 60000));
    }
    WriteIniInt(section, L"TargetRetry", party_build_logic::ClampRetry(s.targetRetry));
    WriteIniInt(section, L"InviteRetry", party_build_logic::ClampRetry(s.inviteRetry));
    FlushIni();
}

std::array<SellNpcPosition, kSellNpcs.size()> LoadSharedSellNpcPositions() {
    std::array<SellNpcPosition, kSellNpcs.size()> positions{};
    const std::wstring section = L"SellNpcPositions";
    const int coordinateVersion = ReadIniInt(section, L"CoordinateVersion", 0);
    if (coordinateVersion < 2) return positions; // discard every legacy/default coordinate
    for (std::size_t i = 0; i < kSellNpcs.size(); ++i) {
        const std::wstring prefix = L"SellNpcPos_" + std::to_wstring(i) + L"_";
        SellNpcPosition& pos = positions[i];
        pos.x = ReadIniInt(section, prefix + L"X", -1);
        pos.y = ReadIniInt(section, prefix + L"Y", -1);
        pos.valid = pos.x >= 0 && pos.y >= 0 && ReadIniInt(section, prefix + L"Valid", 0) != 0;
    }
    return positions;
}

void SaveSharedSellNpcPositions(const std::array<SellNpcPosition, kSellNpcs.size()>& positions) {
    EnsureUnicodeIni();
    const std::wstring section = L"SellNpcPositions";
    WriteIniInt(section, L"CoordinateVersion", 2);
    for (std::size_t i = 0; i < kSellNpcs.size(); ++i) {
        const std::wstring prefix = L"SellNpcPos_" + std::to_wstring(i) + L"_";
        const SellNpcPosition& pos = positions[i];
        WriteIniInt(section, prefix + L"X", pos.valid ? pos.x : -1);
        WriteIniInt(section, prefix + L"Y", pos.valid ? pos.y : -1);
        WriteIniInt(section, prefix + L"Valid", pos.valid ? 1 : 0);
    }
    FlushIni();
}

AccountProfile LoadProfile(const std::wstring& section) {
    AccountProfile p{};
    p.section = section;
    // TradeRole is intentionally not loaded: MAIN identity is global; CON slots are runtime-only.
    p.tradeRole = 0;
    p.displayParty = std::clamp(ReadIniInt(section, L"DisplayParty", 0), 0, kChildTradeCount);
    p.partyKey = ReadIniInt(section, L"PartyKey", 0) != 0;
    if (p.tradeRole == kMainTradeRole) { p.displayParty = 0; p.partyKey = false; }
    if (p.displayParty <= 0) p.partyKey = false;
    p.tolerance = ReadIniInt(section, L"Tolerance", 120);
    if (p.tolerance < 20) p.tolerance = 20;
    if (p.tolerance > 2000) p.tolerance = 2000;
    p.enableRevive = ReadIniInt(section, L"EnableRevive", 1) != 0;
    p.enableConfirm = ReadIniInt(section, L"EnableConfirm", 1) != 0;
    p.enableFight = ReadIniInt(section, L"EnableFight", 1) != 0;
    // Legacy NONE seller is ignored; runtime role assignment enables MAIN only.
    p.enableSell = false;
    p.selectedSpot = ReadIniText(section, L"SelectedSpot");
    p.target.name = ReadIniText(section, L"TargetName");
    p.target.mapID = ReadIniInt(section, L"TargetMap", 0);
    p.target.x = ReadIniInt(section, L"TargetX", 0);
    p.target.y = ReadIniInt(section, L"TargetY", 0);
    p.target.valid = p.target.mapID > 0 && ReadIniInt(section, L"TargetValid", 0) != 0;
    if (p.selectedSpot.empty() && p.target.valid) p.selectedSpot = p.target.name;
    for (int i : {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)}) {
        const std::wstring prefix = kClickKeys[static_cast<std::size_t>(i)];
        ClickPoint& c = p.points[static_cast<std::size_t>(i)];
        c.x = ReadIniInt(section, prefix + L"X", -1);
        c.y = ReadIniInt(section, prefix + L"Y", -1);
        c.baseW = ReadIniInt(section, prefix + L"W", 0);
        c.baseH = ReadIniInt(section, prefix + L"H", 0);
        c.valid = c.x >= 0 && c.y >= 0 && c.baseW > 0 && c.baseH > 0;
    }
    int childTradeCount = ReadIniInt(section, L"ChildTradeCount", 0);
    childTradeCount = std::clamp(childTradeCount, 0, 64);
    for (int i = 0; i < childTradeCount; ++i) {
        TradeSequenceStep step{};
        const std::wstring prefix = L"ChildTrade_" + std::to_wstring(i) + L"_";
        step.target = std::clamp(ReadIniInt(section, prefix + L"Target", 0), 0, 1);
        step.mainRef = ReadIniInt(section, prefix + L"MainRef", -1);
        step.description = ReadIniText(section, prefix + L"Desc");
        step.point.x = ReadIniInt(section, prefix + L"X", -1);
        step.point.y = ReadIniInt(section, prefix + L"Y", -1);
        step.point.baseW = ReadIniInt(section, prefix + L"W", 0);
        step.point.baseH = ReadIniInt(section, prefix + L"H", 0);
        step.point.valid = step.point.x >= 0 && step.point.y >= 0 && step.point.baseW > 0 && step.point.baseH > 0;
        step.delayMs = std::clamp(ReadIniInt(section, prefix + L"Delay", 500), 50, 60000);
        step.repeat = std::clamp(ReadIniInt(section, prefix + L"Repeat", 1), 1, 999);
        step.groupId = std::max(0, ReadIniInt(section, prefix + L"GroupId", 0));
        step.groupRepeat = std::clamp(ReadIniInt(section, prefix + L"GroupRepeat", 1), 1, 999);
        step.afterAction = std::clamp(ReadIniInt(section, prefix + L"AfterAction", 0), 0, 1);
        if (step.target != 0) step.afterAction = 0;
        p.childTradeSequence.push_back(step);
    }
    return p;
}

void SaveProfile(const AccountProfile& p) {
    EnsureUnicodeIni();
    WriteIniInt(p.section, L"DisplayParty", p.displayParty);
    WriteIniInt(p.section, L"PartyKey", p.partyKey && p.tradeRole != kMainTradeRole && p.displayParty > 0 ? 1 : 0);
    WriteIniInt(p.section, L"Tolerance", p.tolerance);
    WriteIniInt(p.section, L"EnableRevive", p.enableRevive ? 1 : 0);
    WriteIniInt(p.section, L"EnableConfirm", p.enableConfirm ? 1 : 0);
    WriteIniInt(p.section, L"EnableFight", p.enableFight ? 1 : 0);
    WriteIniInt(p.section, L"EnableSell", (p.tradeRole == kMainTradeRole && p.enableSell) ? 1 : 0);
    WriteIniText(p.section, L"SelectedSpot", p.selectedSpot);
    WriteIniText(p.section, L"TargetName", p.target.name);
    WriteIniInt(p.section, L"TargetMap", p.target.mapID);
    WriteIniInt(p.section, L"TargetX", p.target.x);
    WriteIniInt(p.section, L"TargetY", p.target.y);
    WriteIniInt(p.section, L"TargetValid", p.target.valid ? 1 : 0);
    for (int i : {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)}) {
        const std::wstring prefix = kClickKeys[static_cast<std::size_t>(i)];
        const ClickPoint& c = p.points[static_cast<std::size_t>(i)];
        WriteIniInt(p.section, prefix + L"X", c.valid ? c.x : -1);
        WriteIniInt(p.section, prefix + L"Y", c.valid ? c.y : -1);
        WriteIniInt(p.section, prefix + L"W", c.valid ? c.baseW : 0);
        WriteIniInt(p.section, prefix + L"H", c.valid ? c.baseH : 0);
    }
    WriteIniInt(p.section, L"ChildTradeCount", static_cast<int>(p.childTradeSequence.size()));
    for (std::size_t i = 0; i < p.childTradeSequence.size(); ++i) {
        const TradeSequenceStep& step = p.childTradeSequence[i];
        const std::wstring prefix = L"ChildTrade_" + std::to_wstring(i) + L"_";
        WriteIniInt(p.section, prefix + L"Target", step.target);
        WriteIniInt(p.section, prefix + L"MainRef", step.mainRef);
        WriteIniText(p.section, prefix + L"Desc", step.description);
        WriteIniInt(p.section, prefix + L"X", step.point.valid ? step.point.x : -1);
        WriteIniInt(p.section, prefix + L"Y", step.point.valid ? step.point.y : -1);
        WriteIniInt(p.section, prefix + L"W", step.point.valid ? step.point.baseW : 0);
        WriteIniInt(p.section, prefix + L"H", step.point.valid ? step.point.baseH : 0);
        WriteIniInt(p.section, prefix + L"Delay", step.delayMs);
        WriteIniInt(p.section, prefix + L"Repeat", step.repeat);
        WriteIniInt(p.section, prefix + L"GroupId", step.groupId);
        WriteIniInt(p.section, prefix + L"GroupRepeat", step.groupRepeat);
        WriteIniInt(p.section, prefix + L"AfterAction", step.target == 0 ? std::clamp(step.afterAction, 0, 1) : 0);
    }
    FlushIni();
}


std::wstring SpotsPath() { return ExeDir() + L"\\ThanLongCleanRoute.spots.tsv"; }

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                           static_cast<int>(input.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
                        out.data(), needed);
    return out;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), out.data(), needed,
                        nullptr, nullptr);
    return out;
}

std::wstring SanitizeSpotName(std::wstring name) {
    for (wchar_t& c : name) {
        if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
    }
    while (!name.empty() && name.front() == L' ') name.erase(name.begin());
    while (!name.empty() && name.back() == L' ') name.pop_back();
    return name;
}

std::vector<std::wstring> SplitSpotLine(const std::wstring& line) {
    wchar_t separator = L'\t';
    if (line.find(L'\t') == std::wstring::npos) {
        if (line.find(L'|') != std::wstring::npos) separator = L'|';
        else if (line.find(L';') != std::wstring::npos) separator = L';';
        else return {};
    }
    std::vector<std::wstring> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t pos = line.find(separator, start);
        if (pos == std::wstring::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return fields;
}

int FindSpotIndex(const std::vector<TargetProfile>& spots, const std::wstring& name);

std::vector<TargetProfile> LoadSharedSpots() {
    std::vector<TargetProfile> out;
    const std::wstring path = SpotsPath();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) {
        CloseHandle(h);
        return out;
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr)) {
        CloseHandle(h);
        return out;
    }
    CloseHandle(h);
    bytes.resize(read);

    std::wstring text;
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const std::size_t wcharCount = (bytes.size() - 2) / 2;
        text.resize(wcharCount);
        for (std::size_t i = 0; i < wcharCount; ++i) {
            const unsigned char lo = static_cast<unsigned char>(bytes[2 + i * 2]);
            const unsigned char hi = static_cast<unsigned char>(bytes[3 + i * 2]);
            text[i] = static_cast<wchar_t>(lo | (static_cast<unsigned int>(hi) << 8));
        }
    } else {
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
            bytes.erase(0, 3);
        }
        text = Utf8ToWide(bytes);
    }
    std::size_t lineStart = 0;
    while (lineStart <= text.size()) {
        std::size_t lineEnd = text.find(L'\n', lineStart);
        if (lineEnd == std::wstring::npos) lineEnd = text.size();
        std::wstring line = text.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty() && line[0] != L'#') {
            const auto f = SplitSpotLine(line);
            if (f.size() >= 4) {
                TargetProfile t{};
                const bool firstNumeric = !f[0].empty() && (f[0][0] >= L'0' && f[0][0] <= L'9');
                if (firstNumeric) {
                    t.mapID = _wtoi(f[0].c_str());
                    t.x = _wtoi(f[1].c_str());
                    t.y = _wtoi(f[2].c_str());
                    t.name = SanitizeSpotName(f[3]);
                } else {
                    t.name = SanitizeSpotName(f[0]);
                    t.mapID = _wtoi(f[1].c_str());
                    t.x = _wtoi(f[2].c_str());
                    t.y = _wtoi(f[3].c_str());
                }
                t.valid = !t.name.empty() && t.mapID > 0;
                if (t.valid && FindSpotIndex(out, t.name) < 0) out.push_back(std::move(t));
            }
        }
        if (lineEnd == text.size()) break;
        lineStart = lineEnd + 1;
    }
    return out;
}

TargetProfile LoadGatherTarget() {
    const std::wstring section = L"GatherTarget";
    TargetProfile t{};
    t.name = L"TẬP TRUNG";
    t.mapID = ReadIniInt(section, L"MapID", 0);
    t.x = ReadIniInt(section, L"X", 0);
    t.y = ReadIniInt(section, L"Y", 0);
    t.valid = ReadIniInt(section, L"Valid", 0) != 0 && t.mapID > 0;
    return t;
}

void SaveGatherTarget(const TargetProfile& t) {
    EnsureUnicodeIni();
    const std::wstring section = L"GatherTarget";
    WriteIniInt(section, L"Valid", t.valid ? 1 : 0);
    WriteIniInt(section, L"MapID", t.valid ? t.mapID : 0);
    WriteIniInt(section, L"X", t.valid ? t.x : 0);
    WriteIniInt(section, L"Y", t.valid ? t.y : 0);
    FlushIni();
}

void SaveSharedSpots(const std::vector<TargetProfile>& spots) {
    std::wstring wide;
    for (const auto& spot : spots) {
        if (!spot.valid || spot.mapID <= 0 || spot.name.empty()) continue;
        wide += SanitizeSpotName(spot.name) + L"\t" + std::to_wstring(spot.mapID) + L"\t" +
                std::to_wstring(spot.x) + L"\t" + std::to_wstring(spot.y) + L"\r\n";
    }
    const std::string bytes = WideToUtf8(wide);
    const std::wstring path = SpotsPath();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    if (!bytes.empty()) (void)WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(h);
}

int FindSpotIndex(const std::vector<TargetProfile>& spots, const std::wstring& name) {
    if (name.empty()) return -1;
    for (std::size_t i = 0; i < spots.size(); ++i) {
        if (_wcsicmp(spots[i].name.c_str(), name.c_str()) == 0) return static_cast<int>(i);
    }
    return -1;
}

std::wstring GetText(HWND h) {
    const int n = GetWindowTextLengthW(h);
    std::wstring out(static_cast<std::size_t>(n) + 1, L'\0');
    if (n > 0) GetWindowTextW(h, out.data(), n + 1);
    out.resize(static_cast<std::size_t>(n));
    return out;
}

void SetText(HWND h, const std::wstring& s) { SetWindowTextW(h, s.c_str()); }

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


struct MainMacroSellConfig {
    ClickPoint point{};
    int delayMs = 600;
};

struct MainMacroSellRuntime {
    enum class Phase : int { Idle=0, Scan=1, Clicking=2, RefreshCapacity=3, Blocked=4 };
    Phase phase = Phase::Idle;
    int targetClicks = 0;
    int doneClicks = 0;
    DWORD nextTick = 0;
    bool idleSweep = false;
    int blockedFree = -1;
};

struct Account {
    GameClient game{};
    BridgeClient bridge{};
    Snapshot snapshot{};
    bool snapshotValid = false;
    std::wstring displayName;
    AccountProfile profile{};
    RuntimeState runtime{};
    // Lifecycle latch intentionally lives OUTSIDE RuntimeState. ResetRuntime() may wipe
    // every automation phase at death/alive boundaries without forgetting that both
    // snapshots still belong to the same death session.
    bool deathSessionLatched = false;

    // Trade coordinator owns only the paired MAIN/CON while a transaction is active.
    // Snapshot polling continues; normal route/death FSM resumes immediately after abort/release.
    bool tradeHeld = false;
    MainMacroSellRuntime macroSell{};
    // 10.2 independent NONE seller: after the first historical 90-click ceiling,
    // reuse the stable FreeBagSpace learned from the previous successful sale.
    int sellStep5LearnedRepeat = 0;

};


enum class PartyBuildPhase : int {
    KeyClick1 = 0,
    KeyClick2,
    MemberTarget,
    MemberFace,
    MemberInvite,
    Done,
};

struct PartyBuildSession {
    int party = 0;
    DWORD keyPid = 0;
    std::vector<DWORD> memberPids{};
    std::size_t memberIndex = 0;
    PartyBuildPhase phase = PartyBuildPhase::KeyClick1;
    DWORD nextTick = 0;
    DWORD phaseStartedTick = 0;
    int targetAttempts = 0;
    int inviteAttempts = 0;
    int invited = 0;
    int failed = 0;
    std::vector<std::wstring> failures{};
    bool summaryEmitted = false;
};

std::wstring TradeRoleLabel(int role) {
    if (role == kMainTradeRole) return L"MAIN";
    if (role >= kFirstChildTradeRole && role <= kLastChildTradeRole)
        return L"CON " + std::to_wstring(role - 1);
    if (role == kOverflowChildTradeRole) return L"CON >30";
    return L"CON AUTO";
}

std::wstring PointDescription(const ClickPoint& p) {
    if (!p.valid) return L"CHƯA LẤY";
    return std::to_wstring(p.x) + L"," + std::to_wstring(p.y) + L" @ " +
           std::to_wstring(p.baseW) + L"x" + std::to_wstring(p.baseH);
}

bool ScaleClickPoint(const GameClient& game, const ClickPoint& saved, POINT& point, std::wstring& error) {
    if (!saved.valid) { error = L"Chưa lấy tọa độ click"; return false; }
    if (!game.window || !IsWindow(game.window)) { error = L"Cửa sổ game không còn tồn tại"; return false; }
    RECT rc{};
    if (!GetClientRect(game.window, &rc)) { error = L"Không đọc được client rect"; return false; }
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0 || saved.baseW <= 0 || saved.baseH <= 0) {
        error = L"Kích thước cửa sổ không hợp lệ";
        return false;
    }
    point.x = MulDiv(saved.x, width, saved.baseW);
    point.y = MulDiv(saved.y, height, saved.baseH);
    if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height) {
        error = L"Tọa độ sau scale nằm ngoài cửa sổ";
        return false;
    }
    return true;
}

bool NormalizeClickPointForBridge(const GameClient& game, const ClickPoint& saved,
                                  int& normalizedX, int& normalizedY,
                                  std::wstring& error) {
    POINT point{};
    if (!ScaleClickPoint(game, saved, point, error)) return false;
    RECT rc{};
    if (!GetClientRect(game.window, &rc)) {
        error = L"Không đọc được client rect để chuẩn hóa tọa độ";
        return false;
    }
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    normalizedX = fixed_slot_sell_logic::NormalizeClientCoordinate(point.x, width);
    normalizedY = fixed_slot_sell_logic::NormalizeClientCoordinate(point.y, height);
    if (normalizedX < 0 || normalizedY < 0) {
        error = L"Không chuẩn hóa được tọa độ UI nội bộ";
        return false;
    }
    return true;
}

bool Elapsed(DWORD now, DWORD since, DWORD delay) {
    return since != 0 && now - since >= delay;
}

std::wstring FormatLicenseRemaining(long long seconds) {
    if (seconds < 0) return L"Đang đồng bộ key";
    const long long days = seconds / 86400;
    const long long hours = (seconds % 86400) / 3600;
    const long long minutes = (seconds % 3600) / 60;
    if (days > 0) return L"Còn " + std::to_wstring(days) + L" ngày " + std::to_wstring(hours) + L" giờ";
    if (hours > 0) return L"Còn " + std::to_wstring(hours) + L" giờ " + std::to_wstring(minutes) + L" phút";
    return L"Còn " + std::to_wstring(std::max(0LL, minutes)) + L" phút";
}

void ResetRuntime(RuntimeState& r) {
    const bool running = r.running;
    r = RuntimeState{};
    r.running = running;
    r.status = running ? L"Đang giám sát" : L"Đã dừng";
}

enum class TradePhase { Idle, Rendezvous, TargetMain, Sequence, Cleanup, SellPause };

class App {
public:
    bool Create(HINSTANCE instance) {
        instance_ = instance;
        MigrateLegacyConfigIfNeeded();
        EnsureUnicodeIni();
        autoLootEnabled_ = ReadIniInt(L"AutoLoot", L"AutoLootEnabled", 0) != 0;
        autoLootIntervalMs_ = auto_loot_logic::NormalizeIntervalMs(
            ReadIniInt(L"AutoLoot", L"AutoLootIntervalMs", auto_loot_logic::kDefaultIntervalMs));
        goldHistoryReady_ = goldHistory_.Open(std::filesystem::path(ConfigDir()) / L"GoldHistory.v1.tsv",
                                              gold_history::UnixNow(), goldHistoryWarning_);
        if (!goldHistoryReady_) goldHistoryRetryDueTick_ = GetTickCount() + 30000u;
        LoadEquipPointDb();
        LoadAutoDropNames();
        image_scan_test::SetAutoFilterMode(ReadIniInt(L"WeaponFilterV2",L"Mode",1)==2?image_scan_test::FilterMode::SemanticBag:image_scan_test::FilterMode::V4);
        LoadTradeSettings();
        telegramSettings_ = LoadTelegramSettings(telegramLoadWarning_);
        LoadTradeSequence();
        spots_ = LoadSharedSpots();
        INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&ic);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ThanLongCleanRouteMultiWindow";
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        hwnd_ = CreateWindowExW(0, wc.lpszClassName, kTitle,
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1060, 1030,
                                nullptr, nullptr, instance, this);
        return hwnd_ != nullptr;
    }

    void Show(int cmd) {
        ShowWindow(hwnd_, cmd);
        UpdateWindow(hwnd_);
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = reinterpret_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        }
        return self ? self->Handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK TradeEditorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = reinterpret_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (self) self->tradeEditor_ = hwnd;
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        return self->HandleTradeEditor(hwnd, msg, wp, lp);
    }


    static LRESULT CALLBACK ShortcutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = reinterpret_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (self) self->shortcutWindow_ = hwnd;
        }
        return self ? self->HandleShortcutWindow(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK TradeSequenceListSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                         UINT_PTR, DWORD_PTR refData) {
        App* self = reinterpret_cast<App*>(refData);
        if (!self) return DefSubclassProc(hwnd, msg, wp, lp);
        switch (msg) {
            case WM_LBUTTONDOWN: {
                const LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
                LVHITTESTINFO hit{};
                hit.pt.x = GET_X_LPARAM(lp);
                hit.pt.y = GET_Y_LPARAM(lp);
                const int row = ListView_HitTest(hwnd, &hit);
                if (row >= 0) {
                    self->tradeSeqDragSelecting_ = true;
                    self->tradeSeqDragStartRow_ = row;
                    self->SelectTradeSequenceDragRange(row, row);
                    SetCapture(hwnd);
                }
                return result;
            }
            case WM_MOUSEMOVE:
                if (self->tradeSeqDragSelecting_ && (wp & MK_LBUTTON)) {
                    LVHITTESTINFO hit{};
                    hit.pt.x = GET_X_LPARAM(lp);
                    hit.pt.y = GET_Y_LPARAM(lp);
                    int row = ListView_HitTest(hwnd, &hit);
                    if (row < 0) {
                        const int count = ListView_GetItemCount(hwnd);
                        RECT rc{}; GetClientRect(hwnd, &rc);
                        if (count > 0 && hit.pt.y < rc.top) row = 0;
                        else if (count > 0 && hit.pt.y >= rc.bottom) row = count - 1;
                    }
                    if (row >= 0) self->SelectTradeSequenceDragRange(self->tradeSeqDragStartRow_, row);
                    return 0;
                }
                break;
            case WM_LBUTTONUP:
                if (self->tradeSeqDragSelecting_) {
                    self->tradeSeqDragSelecting_ = false;
                    self->tradeSeqDragStartRow_ = -1;
                    if (GetCapture() == hwnd) ReleaseCapture();
                }
                break;
            case WM_CAPTURECHANGED:
                self->tradeSeqDragSelecting_ = false;
                self->tradeSeqDragStartRow_ = -1;
                break;
            case WM_NCDESTROY:
                RemoveWindowSubclass(hwnd, TradeSequenceListSubclassProc, 1);
                break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    HWND MakeIn(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
        HWND hWnd = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                    x, y, w, h, parent,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        if (hWnd) SendMessageW(hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return hWnd;
    }

    HWND Make(const wchar_t* cls, const wchar_t* text, DWORD style,
              int x, int y, int w, int h, int id) {
        return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                               x, y, w, h, hwnd_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    }

    void AddListColumn(int index, int width, const wchar_t* text) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = const_cast<wchar_t*>(text);
        c.cx = width;
        c.iSubItem = index;
        ListView_InsertColumn(clientList_, index, &c);
    }


    int ChildScanSlot(const Account& account) const {
        return account.profile.tradeRole >= kFirstChildTradeRole && account.profile.tradeRole <= kLastChildTradeRole
            ? account.profile.tradeRole - 1 : 0;
    }

    image_scan_test::Target MakeImageScanTarget(Account& account) {
        image_scan_test::Target target{};
        target.owner = hwnd_;
        target.gameWindow = account.game.window;
        target.accountLabel = AccountTag(account);
        target.context = &account;
        target.bagContext = this;
        target.readDropCandidates = [](void* accountContext, void* bagContext, std::vector<int>& positions, int& freeBagSpace, std::wstring& detail) -> bool {
            Account* acc=static_cast<Account*>(accountContext);App* app=static_cast<App*>(bagContext);
            return acc&&app?app->BuildSemanticDropCandidates(*acc,positions,freeBagSpace,detail):false;
        };
        target.semanticDiscard = [](void* accountContext, bool probeOnly, std::wstring& detail) -> bool {
            Account* acc=static_cast<Account*>(accountContext);
            if(!acc||!acc->bridge.Attached()){detail=L"bridge chưa attach";return false;}
            Response response{};std::wstring error;
            if(!acc->bridge.Call(Command::ClickTravelSemantic,(int)TravelSemantic::DiscardPopup,probeOnly?1:0,0,response,error,250,true)){detail=error;return false;}
            detail=response.detail[0]?response.detail:L"SEMANTIC VỨT PASS";return true;
        };
        target.semanticDiscardConfirm = [](void* accountContext, bool probeOnly, std::wstring& detail) -> bool {
            Account* acc=static_cast<Account*>(accountContext);
            if(!acc||!acc->bridge.Attached()){detail=L"bridge chưa attach";return false;}
            Response response{};std::wstring error;
            if(!acc->bridge.Call(Command::ClickTravelSemantic,(int)TravelSemantic::DiscardConfirm,probeOnly?1:0,0,response,error,250,true)){detail=error;return false;}
            detail=response.detail[0]?response.detail:L"SEMANTIC XÁC NHẬN SAU VỨT PASS";return true;
        };
        target.uiDirect = [](void* accountContext, UiDirectTarget uiTarget, bool invoke, std::wstring& detail) -> int {
            Account* acc=static_cast<Account*>(accountContext);
            if(!acc||!acc->bridge.Attached()){detail=L"bridge chưa attach";return -1;}
            Response response{};std::wstring error;
            const Command command=invoke?Command::InvokeUiDirect:Command::ProbeUiDirect;
            if(!acc->bridge.Call(command,static_cast<int>(uiTarget),0,0,response,error,250,true)){detail=error;return -1;}
            detail=response.detail[0]?response.detail:L"UI DIRECT";
            if(invoke)return response.resultCode==static_cast<std::int32_t>(ActionResult::ActionInvoked)?1:0;
            return response.value0!=0?1:0;
        };
        target.hiddenDrag = [](void* context, int sx, int sy, int ex, int ey, int clientW, int clientH, std::wstring& detail) -> bool {
            Account* acc = static_cast<Account*>(context);
            if (!acc || !acc->bridge.Attached()) { detail = L"bridge chưa attach"; return false; }
            const int nsx=fixed_slot_sell_logic::NormalizeClientCoordinate(sx,clientW);
            const int nsy=fixed_slot_sell_logic::NormalizeClientCoordinate(sy,clientH);
            const int nex=fixed_slot_sell_logic::NormalizeClientCoordinate(ex,clientW);
            const int ney=fixed_slot_sell_logic::NormalizeClientCoordinate(ey,clientH);
            if(nsx<0||nsy<0||nex<0||ney<0){detail=L"không chuẩn hóa được tọa vuốt";return false;}
            const auto q16=[](int v){return static_cast<int>((static_cast<long long>(std::clamp(v,0,fixed_slot_sell_logic::kCoordinateScale))*65535LL+fixed_slot_sell_logic::kCoordinateScale/2)/fixed_slot_sell_logic::kCoordinateScale);};
            const int packed=((q16(nex)&0xffff)<<16)|(q16(ney)&0xffff);Response response{};std::wstring error;
            if(!acc->bridge.Call(Command::DragInternalPoint,nsx,nsy,packed,response,error,2600,true)){detail=error;return false;}
            detail=response.detail[0]?response.detail:L"TryClickUI → UpdateUIDrag x12 → EndUIDrag";return true;
        };
        target.hiddenClick = [](void* context, int x, int y, int clientW, int clientH, std::wstring& detail) -> bool {
            Account* acc = static_cast<Account*>(context);
            if (!acc || !acc->bridge.Attached()) { detail = L"bridge chưa attach"; return false; }
            const int nx = fixed_slot_sell_logic::NormalizeClientCoordinate(x, clientW);
            const int ny = fixed_slot_sell_logic::NormalizeClientCoordinate(y, clientH);
            if (nx < 0 || ny < 0) { detail = L"không chuẩn hóa được tọa độ match"; return false; }
            Response response{}; std::wstring error;
            if (!acc->bridge.Call(Command::ClickInternalPointRawTest, nx, ny, 0, response, error, 1800, true)) {
                detail = error; return false;
            }
            detail = response.detail[0] ? response.detail : L"TryClickUI → EndUIDrag PASS";
            return true;
        };
        return target;
    }

    static std::wstring TrimName(std::wstring value) {
        auto ws=[](wchar_t c){return c==L' '||c==L'\t'||c==L'\r'||c==L'\n';};
        while(!value.empty()&&ws(value.front()))value.erase(value.begin());
        while(!value.empty()&&ws(value.back()))value.pop_back();
        return value;
    }
    static std::wstring FoldName(std::wstring value) {
        value=TrimName(std::move(value));
        for(wchar_t& c:value)c=static_cast<wchar_t>(towlower(c));
        return value;
    }
    bool LoadEquipPointDb() {
        equipPointDbReady_=false;equipPointDbError_.clear();
        HRSRC resource=FindResourceW(instance_,MAKEINTRESOURCEW(202),RT_RCDATA);
        if(!resource){equipPointDbError_=L"RCDATA EquipPoint 202 không tồn tại";return false;}
        HGLOBAL loaded=LoadResource(instance_,resource);if(!loaded){equipPointDbError_=L"LoadResource EquipPoint fail";return false;}
        const DWORD size=SizeofResource(instance_,resource);const void* data=LockResource(loaded);
        if(!data||size==0){equipPointDbError_=L"EquipPoint resource rỗng";return false;}
        std::string error;
        if(!equipPointDb_.LoadCsv(std::string_view(static_cast<const char*>(data),size),&error)||equipPointDb_.Size()!=22763){equipPointDbError_=Utf8ToWide(error.empty()?"EquipPoint count != 22763":error);return false;}
        equipPointDbReady_=true;return true;
    }
    void LoadAutoDropNames() {
        autoDropNames_.clear();const int count=std::clamp(ReadIniInt(L"WeaponFilterV2",L"DropNameCount",0),0,512);
        for(int i=0;i<count;++i){const std::wstring n=TrimName(ReadIniText(L"WeaponFilterV2",L"DropName"+std::to_wstring(i)));if(!n.empty())autoDropNames_.push_back(n);}
    }
    void SaveAutoDropNames() {
        const int old=std::clamp(ReadIniInt(L"WeaponFilterV2",L"DropNameCount",0),0,512);
        WriteIniInt(L"WeaponFilterV2",L"DropNameCount",static_cast<int>(autoDropNames_.size()));
        for(std::size_t i=0;i<autoDropNames_.size();++i)WriteIniText(L"WeaponFilterV2",L"DropName"+std::to_wstring(i),autoDropNames_[i]);
        for(int i=static_cast<int>(autoDropNames_.size());i<old;++i)WritePrivateProfileStringW(L"WeaponFilterV2",(L"DropName"+std::to_wstring(i)).c_str(),nullptr,ConfigPath().c_str());
        FlushIni();RefreshAutoDropNameList();RefreshBagScanRows();
    }
    bool ExactAutoDropName(const std::wstring& name) const {
        const std::wstring key=FoldName(name);if(key.empty())return false;
        for(const auto& entry:autoDropNames_)if(FoldName(entry)==key)return true;return false;
    }
    bool ReadAllBagSemantic(Account& a,std::vector<BagItemSnapshot>& rows,int& freeSpace,std::wstring& error,bool noSleepBridge=false) {
        std::wstring attachError;if(!EnsureAttach(a,attachError)){error=L"attach bridge • "+attachError;return false;}
        for(int restart=0;restart<2;++restart){rows.clear();int start=0,expected=-1;freeSpace=-1;bool changed=false;
            for(int page=0;page<64;++page){Response r{};if(!a.bridge.Call(Command::ReadBagPage,start,0,0,r,error,2200,noSleepBridge))return false;const auto& b=r.bagPage;
                if(b.totalCount<0||b.totalCount>1000||b.pageCount<0||b.pageCount>static_cast<int>(kBagPageCapacity)||b.pageStart!=start){error=L"BagPage contract không hợp lệ";return false;}
                if(expected<0)expected=b.totalCount;if(b.totalCount!=expected){changed=true;break;}freeSpace=b.freeBagSpace;
                for(int i=0;i<b.pageCount;++i)rows.push_back(b.items[i]);start+=b.pageCount;if(start>=expected)break;if(b.pageCount==0){error=L"BagPage dừng sớm";return false;}
            }
            if(changed)continue;if(expected>=0&&static_cast<int>(rows.size())!=expected){error=L"Tay nải đổi trong lúc quét";continue;}
            std::sort(rows.begin(),rows.end(),[](const BagItemSnapshot& x,const BagItemSnapshot& y){return x.position<y.position;});return true;
        }
        error=L"Tay nải thay đổi liên tục khi scan";return false;
    }
    bool BuildSemanticDropCandidates(Account& a,std::vector<int>& positions,int& freeBagSpace,std::wstring& detail) {
        positions.clear();if(!equipPointDbReady_){detail=L"EquipPoint DB chưa sẵn sàng • "+equipPointDbError_;return false;}
        std::vector<BagItemSnapshot> items;if(!ReadAllBagSemantic(a,items,freeBagSpace,detail))return false;
        for(const auto& item:items){if(!bag_filter_v2_logic::ValidPosition(item.position))continue;const auto ep=equipPointDb_.Find(item.itemID);
            bag_filter_v2_logic::ItemDecisionInput d{};d.position=item.position;d.isEquip=item.isEquip!=0;d.bound=item.bound!=0;d.equipPointKnown=ep.has_value();d.equipPoint=ep.value_or(-1);d.exactNameOverride=ExactAutoDropName(item.name);
            if(bag_filter_v2_logic::ShouldDrop(d))positions.push_back(item.position);
        }
        positions=bag_filter_v2_logic::NormalizeCandidates(std::move(positions));
        detail=L"semantic 0–99 PASS • candidates="+std::to_wstring(positions.size())+L" • free="+std::to_wstring(freeBagSpace);return true;
    }
    void UpdateFilterModeButtons() {
        const bool c2=image_scan_test::GetAutoFilterMode()==image_scan_test::FilterMode::SemanticBag;
        if(imageScanButton_)SetWindowTextW(imageScanButton_,c2?L"LỌC VK V4 [C1: OFF]":L"LỌC VK V4 [C1: ON]");
        if(filterMode2Button_)SetWindowTextW(filterMode2Button_,c2?L"LỌC VK CÁCH 2: ON":L"LỌC VK CÁCH 2: OFF");
    }
    void ToggleFilterMode2() {
        const bool enable=image_scan_test::GetAutoFilterMode()!=image_scan_test::FilterMode::SemanticBag;
        image_scan_test::SetAutoFilterMode(enable?image_scan_test::FilterMode::SemanticBag:image_scan_test::FilterMode::V4);
        WriteIniInt(L"WeaponFilterV2",L"Mode",enable?2:1);FlushIni();UpdateFilterModeButtons();
        Log(enable?L"LỌC VK: chuyển sang CÁCH 2 • 1 semantic scan 0–99 / lượt CON • V4 OFF":L"LỌC VK: CÁCH 2 OFF • quay lại V4 CÁCH 1");
    }
    std::wstring BagDecisionText(const BagItemSnapshot& item) const {
        if(item.bound)return L"GIỮ • KHÓA";if(ExactAutoDropName(item.name))return L"AUTO VỨT • THEO TÊN";
        if(!item.isEquip)return L"GIỮ • KHÔNG EQUIP";const auto ep=equipPointDb_.Find(item.itemID);if(!ep)return L"GIỮ • CHƯA CÓ EQUIPPOINT";if(*ep==0)return L"GIỮ • VŨ KHÍ";return L"AUTO VỨT • EQUIP KHÁC VK";
    }
    void RefreshAutoDropNameList() {
        if(!bagScanNames_||!IsWindow(bagScanNames_))return;SendMessageW(bagScanNames_,LB_RESETCONTENT,0,0);for(const auto& n:autoDropNames_)SendMessageW(bagScanNames_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(n.c_str()));
    }
    void RefreshBagScanRows() {
        if(!bagScanItems_||!IsWindow(bagScanItems_))return;ListView_DeleteAllItems(bagScanItems_);
        for(std::size_t i=0;i<lastBagScanItems_.size();++i){const auto& b=lastBagScanItems_[i];std::wstring pos=std::to_wstring(b.position),id=std::to_wstring(b.itemID),type=b.itemType,bound=b.bound?L"KHÓA":L"-";const auto ep=equipPointDb_.Find(b.itemID);std::wstring eps=ep?std::to_wstring(*ep):L"-",decision=BagDecisionText(b);
            LVITEMW row{};row.mask=LVIF_TEXT;row.iItem=static_cast<int>(i);row.pszText=pos.data();ListView_InsertItem(bagScanItems_,&row);
            std::wstring vals[]={b.name,id,type,bound,eps,decision};for(int c=0;c<6;++c)ListView_SetItemText(bagScanItems_,static_cast<int>(i),c+1,vals[c].data());}
    }
    void ScanBagIntoWindow() {
        if(!bagScanStatus_)return;Account* a=SelectedAccount();if(!a){SetWindowTextW(bagScanStatus_,L"Chọn acc trước");return;}std::wstring e;int freeSpace=-1;std::vector<BagItemSnapshot> items;
        if(!ReadAllBagSemantic(*a,items,freeSpace,e)){SetWindowTextW(bagScanStatus_,(L"SCAN FAIL • "+e).c_str());return;}lastBagScanItems_=std::move(items);RefreshBagScanRows();
        int candidates=0;std::vector<int> p;int f=-1;std::wstring d;if(BuildSemanticDropCandidates(*a,p,f,d))candidates=static_cast<int>(p.size());
        SetWindowTextW(bagScanStatus_,(L"SCAN 0–99 PASS • item="+std::to_wstring(lastBagScanItems_.size())+L" • auto vứt="+std::to_wstring(candidates)+L" • trống="+std::to_wstring(freeSpace)).c_str());
    }
    void AddAutoDropName(std::wstring name) {
        name=TrimName(std::move(name));if(name.empty())return;const std::wstring key=FoldName(name);for(const auto& n:autoDropNames_)if(FoldName(n)==key)return;autoDropNames_.push_back(std::move(name));SaveAutoDropNames();
    }
    static void AddBagColumn(HWND list,int index,int width,const wchar_t* text){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;c.iSubItem=index;c.cx=width;c.pszText=const_cast<wchar_t*>(text);ListView_InsertColumn(list,index,&c);}
    void BuildBagScanWindowUi() {
        HFONT font=reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));auto mk=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){HWND z=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,bagScanWindow_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);if(z)SendMessageW(z,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return z;};
        mk(L"BUTTON",L"QUÉT LẠI TAY NẢI",BS_PUSHBUTTON,12,10,160,30,IDC_BAG_SCAN_REFRESH);mk(L"BUTTON",L"+ AUTO VỨT ITEM ĐANG CHỌN",BS_PUSHBUTTON,182,10,230,30,IDC_BAG_SCAN_ADD_SELECTED);
        bagScanNameEdit_=mk(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,425,11,235,28,IDC_BAG_SCAN_NAME_EDIT);mk(L"BUTTON",L"THÊM TÊN",BS_PUSHBUTTON,670,10,110,30,IDC_BAG_SCAN_ADD_NAME);
        bagScanStatus_=mk(L"STATIC",L"Sẵn sàng",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,12,48,768,30,IDC_BAG_SCAN_STATUS);
        bagScanItems_=mk(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER,12,86,768,430,IDC_BAG_SCAN_ITEMS);ListView_SetExtendedListViewStyle(bagScanItems_,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
        const wchar_t* cols[]={L"Pos",L"Tên",L"ItemID",L"Loại",L"Khóa",L"EquipPoint",L"Kết luận"};const int widths[]={48,190,90,90,60,80,190};for(int i=0;i<7;++i)AddBagColumn(bagScanItems_,i,widths[i],cols[i]);
        mk(L"STATIC",L"AUTO VỨT THEO TÊN (exact-name, vẫn chặn item khóa)",SS_LEFT|SS_CENTERIMAGE,795,10,340,28,0);bagScanNames_=mk(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,795,45,240,390,IDC_BAG_SCAN_NAMES);
        mk(L"BUTTON",L"XÓA TÊN",BS_PUSHBUTTON,795,445,112,30,IDC_BAG_SCAN_REMOVE_NAME);mk(L"BUTTON",L"XÓA TẤT CẢ",BS_PUSHBUTTON,918,445,117,30,IDC_BAG_SCAN_CLEAR_NAMES);
        mk(L"STATIC",L"Cách 2: 1 lần scan semantic toàn bộ Position 0–99 → lập batch → click/vứt liên tục → ROI X/CLOSE/POSTCHECK → nhường CON. Không quét lại 10s.",SS_LEFT,795,490,240,90,0);
        RefreshAutoDropNameList();
    }
    static LRESULT CALLBACK BagScanWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){App* self=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);self=static_cast<App*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));if(self)self->bagScanWindow_=hwnd;}
        if(self&&msg==WM_COMMAND){switch(LOWORD(wp)){case IDC_BAG_SCAN_REFRESH:self->ScanBagIntoWindow();return 0;case IDC_BAG_SCAN_ADD_SELECTED:{const int row=ListView_GetNextItem(self->bagScanItems_,-1,LVNI_SELECTED);if(row>=0&&row<static_cast<int>(self->lastBagScanItems_.size()))self->AddAutoDropName(self->lastBagScanItems_[static_cast<std::size_t>(row)].name);return 0;}case IDC_BAG_SCAN_ADD_NAME:{wchar_t text[128]{};GetWindowTextW(self->bagScanNameEdit_,text,_countof(text));self->AddAutoDropName(text);SetWindowTextW(self->bagScanNameEdit_,L"");return 0;}case IDC_BAG_SCAN_REMOVE_NAME:{const int row=static_cast<int>(SendMessageW(self->bagScanNames_,LB_GETCURSEL,0,0));if(row>=0&&row<static_cast<int>(self->autoDropNames_.size())){self->autoDropNames_.erase(self->autoDropNames_.begin()+row);self->SaveAutoDropNames();}return 0;}case IDC_BAG_SCAN_CLEAR_NAMES:self->autoDropNames_.clear();self->SaveAutoDropNames();return 0;}}
        if(self&&msg==WM_CLOSE){ShowWindow(hwnd,SW_HIDE);return 0;}if(self&&msg==WM_NCDESTROY){self->bagScanWindow_=nullptr;self->bagScanItems_=nullptr;self->bagScanNames_=nullptr;self->bagScanNameEdit_=nullptr;self->bagScanStatus_=nullptr;}return DefWindowProcW(hwnd,msg,wp,lp);}
    void OpenBagScanWindow() {
        if(bagScanWindow_&&IsWindow(bagScanWindow_)){ShowWindow(bagScanWindow_,SW_SHOW);SetForegroundWindow(bagScanWindow_);ScanBagIntoWindow();return;}
        static bool registered=false;if(!registered){WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=BagScanWndProc;wc.hInstance=instance_;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"ThanLongBagScanCP11";if(RegisterClassExW(&wc)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS)registered=true;}
        if(!registered){Log(L"QUÉT TAY NẢI: không đăng ký được cửa sổ");return;}bagScanWindow_=CreateWindowExW(WS_EX_TOOLWINDOW,L"ThanLongBagScanCP11",L"QUÉT TAY NẢI • LỌC VK CÁCH 2",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,1065,625,hwnd_,nullptr,instance_,this);if(!bagScanWindow_)return;BuildBagScanWindowUi();ShowWindow(bagScanWindow_,SW_SHOW);UpdateWindow(bagScanWindow_);ScanBagIntoWindow();
    }

    void RequestImageScanTest() {
        OpenImageScanTest();
    }

    void OpenImageScanTest() {
        Account* account = SelectedAccount();
        if (!account) { Log(L"SCAN LỌC VK: hãy chọn 1 acc trong danh sách client trước."); return; }
        if (!account->game.window || !IsWindow(account->game.window)) {
            LogAccount(*account, L"SCAN LỌC VK: HWND game không còn tồn tại."); return;
        }
        std::wstring attachError;
        if (!EnsureAttach(*account, attachError)) {
            LogAccount(*account, L"SCAN LỌC VK: không attach được bridge • " + attachError); return;
        }
        LogAccount(*account, L"MỞ SCAN LỌC VK V4 • cấu hình CON/open/close/export-import + test FILTER V4.");
        image_scan_test::RunDialog(MakeImageScanTarget(*account));
    }

    void ResetAutoLootSchedules() {
        for (auto& item : accounts_) {
            if (!item) continue;
            item->runtime.autoLootNextTick = 0;
            item->runtime.autoLootErrorLatched = false;
        }
    }

    void RefreshAutoLootDeveloperUi() {
        if (autoLootToggleButton_) {
            SetWindowTextW(autoLootToggleButton_, autoLootEnabled_
                ? L"AUTO NHẶT KHÔNG CẦN BÌNH: ON"
                : L"AUTO NHẶT KHÔNG CẦN BÌNH: OFF");
        }
        if (autoLootIntervalEdit_) {
            const std::wstring value = std::to_wstring(autoLootIntervalMs_);
            SetWindowTextW(autoLootIntervalEdit_, value.c_str());
        }
    }

    void ToggleAutoLootDeveloper() {
        autoLootEnabled_ = !autoLootEnabled_;
        WriteIniInt(L"AutoLoot", L"AutoLootEnabled", autoLootEnabled_ ? 1 : 0);
        FlushIni();
        ResetAutoLootSchedules();
        RefreshAutoLootDeveloperUi();
        Log(std::wstring(L"AUTO NHẶT KHÔNG CẦN BÌNH: ") + (autoLootEnabled_ ? L"ON" : L"OFF"));
    }

    void PersistAutoLootIntervalFromUi() {
        if (!autoLootIntervalEdit_) return;
        wchar_t text[64]{};
        GetWindowTextW(autoLootIntervalEdit_, text, _countof(text));
        wchar_t* end = nullptr;
        errno = 0;
        const long parsed = wcstol(text, &end, 10);
        const bool valid = end != text && end && *end == L'\0' && errno != ERANGE &&
                           parsed > 0 && parsed <= INT_MAX;
        if (!valid) {
            Log(L"AUTO NHẶT: chu kỳ không hợp lệ • giữ " + std::to_wstring(autoLootIntervalMs_) + L" ms");
            RefreshAutoLootDeveloperUi();
            return;
        }
        const int next = auto_loot_logic::NormalizeIntervalMs(static_cast<int>(parsed));
        if (next != autoLootIntervalMs_) {
            autoLootIntervalMs_ = next;
            ResetAutoLootSchedules();
            Log(L"AUTO NHẶT: chu kỳ mới " + std::to_wstring(autoLootIntervalMs_) + L" ms");
        }
        WriteIniInt(L"AutoLoot", L"AutoLootIntervalMs", autoLootIntervalMs_);
        FlushIni();
        RefreshAutoLootDeveloperUi();
    }

    void SetLootProbeOutput(const std::wstring& text) {
        if (lootOutput_) SetWindowTextW(lootOutput_, text.c_str());
    }

    void RunLootProbe(bool pick) {
        Account* a = SelectedAccount();
        if (!a) {
            SetLootProbeOutput(L"Chưa chọn ACC. Chọn một client trong danh sách trước.");
            return;
        }
        std::wstring error;
        if (!a->bridge.Attached() && !EnsureAttach(*a, error)) {
            SetLootProbeOutput(L"Không attach được bridge: " + error);
            return;
        }
        if (pick && AutoLootCriticalBusy(*a)) {
            SetLootProbeOutput(L"NHẶT BUSY • account đang có exclusive gameplay owner; không cướp lượt.");
            LogAccount(*a, L"LOOT PICK MANUAL BUSY • nhường owner hiện tại.");
            return;
        }
        Response r{};
        const Command command = pick ? Command::PickNearestLoot : Command::ProbeNearbyLoot;
        if (!a->bridge.Call(command, 0, 0, 0, r, error, 1800)) {
            const std::wstring prefix = pick ? L"NHẶT FAIL • " : L"SCAN FAIL • ";
            SetLootProbeOutput(prefix + error);
            LogAccount(*a, prefix + error);
            return;
        }
        const auto result = static_cast<ActionResult>(r.resultCode);
        std::wstring text;
        if (result == ActionResult::NoCandidate) text = pick ? L"NHẶT: không có ItemPack gần nhất" : L"SCAN: không có ItemPack gần nhất";
        else text = pick ? L"NHẶT PASS • " : L"SCAN PASS • ";
        if (r.detail[0] != L'\0') text += r.detail;
        if (r.value64_0 > 0) text += L"\r\nNearest ID: " + std::to_wstring(r.value64_0);
        if (r.value0 != 0 || r.value1 != 0) text += L"\r\nWorld X,Y: " + std::to_wstring(r.value0) + L"," + std::to_wstring(r.value1);
        SetLootProbeOutput(text);
        LogAccount(*a, (pick ? L"LOOT PICK MANUAL • " : L"LOOT SCAN MANUAL • ") + std::wstring(r.detail));
    }

    void BuildUi() {
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto addFont = [font](HWND h){ if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); };

        mainTab_ = Make(WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | TCS_FIXEDWIDTH | TCS_FOCUSNEVER,
                        kMainTabX, kMainTabY, kMainTabWidth, kMainTabHeight, IDC_MAIN_TAB); addFont(mainTab_);
        if (mainTab_) {
            TCITEMW tab{}; tab.mask = TCIF_TEXT;
            tab.pszText = const_cast<wchar_t*>(L"DỒN ĐỒ"); TabCtrl_InsertItem(mainTab_, 0, &tab);
            tab.pszText = const_cast<wchar_t*>(L"AUTO"); TabCtrl_InsertItem(mainTab_, 1, &tab);
            tab.pszText = const_cast<wchar_t*>(L"LOG"); TabCtrl_InsertItem(mainTab_, 2, &tab);
            tab.pszText = const_cast<wchar_t*>(L"TELEGRAM"); TabCtrl_InsertItem(mainTab_, 3, &tab);
            tab.pszText = const_cast<wchar_t*>(L"DEVELOPER"); TabCtrl_InsertItem(mainTab_, 4, &tab);
            TabCtrl_SetItemSize(mainTab_, (kMainTabWidth - 8) / 5, 28);
            TabCtrl_SetCurSel(mainTab_, 0);
        }
        tradeStatus_ = Make(L"STATIC", L"ĐIỀU PHỐI: khởi động...", SS_LEFT | SS_CENTERIMAGE | WS_BORDER,
                            kCoordinatorStatusX, kCoordinatorStatusY,
                            kCoordinatorStatusWidth, kCoordinatorStatusHeight, 0); addFont(tradeStatus_);
        if (tradeStatus_) ShowWindow(tradeStatus_, SW_HIDE); // giữ runtime status nhưng ẩn khỏi UI khách hàng
        compactButton_ = Make(L"BUTTON", L"THU NHỎ", BS_PUSHBUTTON, 896, 6, 127, 27, IDC_COMPACT_TOGGLE); addFont(compactButton_);
        selectAllButton_ = Make(L"BUTTON", L"CHỌN TẤT CẢ", BS_PUSHBUTTON, 18, 40, 105, 22, IDC_SELECT_ALL_ACCOUNTS); addFont(selectAllButton_);
        clearAllButton_ = Make(L"BUTTON", L"BỎ TẤT CẢ", BS_PUSHBUTTON, 128, 40, 95, 22, IDC_CLEAR_ALL_ACCOUNTS); addFont(clearAllButton_);
        addFont(Make(L"STATIC", L"PT:", SS_LEFT | SS_CENTERIMAGE, 235, 40, 25, 22, 0));
        partyCombo_ = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 262, 39, 80, 350, IDC_PARTY_COMBO); addFont(partyCombo_);
        SendMessageW(partyCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"KHÔNG PT"));
        for (int party = 1; party <= kChildTradeCount; ++party) {
            const std::wstring label = L"PT" + std::to_wstring(party);
            SendMessageW(partyCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(partyCombo_, CB_SETCURSEL, 0, 0);
        assignPartyButton_ = Make(L"BUTTON", L"GÁN PT", BS_PUSHBUTTON, 348, 40, 78, 22, IDC_ASSIGN_PARTY); addFont(assignPartyButton_);

        clientList_ = Make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER,
                           18, 66, 1005, 220, IDC_CLIENT_LIST);
        addFont(clientList_);
        ListView_SetExtendedListViewStyle(clientList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
        ListView_EnableGroupView(clientList_, TRUE);
        AddListColumn(0, 170, L"Nhân vật / RoleID");
        AddListColumn(1, 72, L"Vai trò");
        AddListColumn(2, 48, L"PT");
        AddListColumn(3, 24, L"▶");
        AddListColumn(4, 220, L"Trạng thái");
        AddListColumn(5, 190, L"Map / X,Y / Túi");
        AddListColumn(6, 280, L"Bãi train");

        scanButton_ = Make(L"BUTTON", L"QUÉT CLIENT", BS_PUSHBUTTON, 18, 294, 120, 30, IDC_SCAN); addFont(scanButton_);
        startCheckedButton_ = Make(L"BUTTON", L"BẮT ĐẦU ACC TICK", BS_DEFPUSHBUTTON, 148, 294, 175, 30, IDC_START_CHECKED); addFont(startCheckedButton_);
        stopCheckedButton_ = Make(L"BUTTON", L"DỪNG ACC TICK", BS_PUSHBUTTON, 333, 294, 155, 30, IDC_STOP_CHECKED); addFont(stopCheckedButton_);
        addFont(Make(L"STATIC", L"Vai trò:", SS_LEFT | SS_CENTERIMAGE, 500, 294, 55, 30, 0));
        tradeRoleCombo_ = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 558, 294, 105, 90, IDC_TRADE_ROLE); addFont(tradeRoleCombo_);
        for (const wchar_t* r : {L"CON AUTO", L"MAIN"})
            SendMessageW(tradeRoleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(r));
        selected_ = Make(L"STATIC", L"ACC ĐANG CHỈNH: chưa chọn", SS_LEFT | SS_CENTERIMAGE | WS_BORDER,
                         675, 294, 348, 30, IDC_SELECTED); addFont(selected_);

        live_ = Make(L"STATIC", L"STATE: chưa có", SS_LEFT | SS_CENTERIMAGE | WS_BORDER,
                     18, 332, 510, 38, IDC_LIVE); addFont(live_);
        tradeEnable_ = Make(L"BUTTON", L"DỒN ĐỒ: BẮT BUỘC",
                            BS_PUSHBUTTON, 538, 336, 120, 27, IDC_CONSOLIDATE_TOGGLE); addFont(tradeEnable_);
        addFont(Make(L"STATIC", L"CON FULL = 0 ô", 0, 663, 341, 92, 22, 0));
        addFont(Make(L"STATIC", L"MAIN QUOTA=0 → MACRO BÁN", 0, 758, 341, 120, 22, 0));
        sellSequenceButton_ = Make(L"BUTTON", L"TÙY CHỈNH BÁN", BS_PUSHBUTTON, 878, 336, 145, 27, IDC_SELL_SEQUENCE); addFont(sellSequenceButton_);
        tradeRendezvousCaptureButton_ = Make(L"BUTTON", L"TỌA GD • LẤY", BS_PUSHBUTTON, 538, 362, 110, 24, IDC_TRADE_RENDEZVOUS_CAPTURE); addFont(tradeRendezvousCaptureButton_);
        tradeRendezvousLabel_ = Make(L"STATIC", L"CHƯA LẤY TỌA GD", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 655, 362, 110, 24, 0); addFont(tradeRendezvousLabel_);
        mainTradeSequenceButton_ = Make(L"BUTTON", L"CHUỖI GD MAIN", BS_PUSHBUTTON, 772, 362, 120, 24, IDC_MAIN_TRADE_SEQUENCE); addFont(mainTradeSequenceButton_);
        childTradeSequenceButton_ = Make(L"BUTTON", L"CHUỖI GD ACC CON", BS_PUSHBUTTON, 900, 362, 123, 24, IDC_CHILD_TRADE_SEQUENCE); addFont(childTradeSequenceButton_);

        addFont(Make(L"STATIC", L"SETTING RIÊNG ACC", 0, 18, 379, 150, 20, 0));
        addFont(Make(L"STATIC", L"GD CON: tự đổi acc khi pass cuối làm MAIN nhận ≤8 slot", 0, 560, 377, 463, 20, 0));
        addFont(Make(L"STATIC", L"Bãi:", 0, 18, 405, 45, 22, 0));
        spotCombo_ = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 63, 401, 220, 240, IDC_SPOT_COMBO); addFont(spotCombo_);
        addFont(Make(L"STATIC", L"Tên lưu:", 0, 292, 405, 60, 22, 0));
        targetName_ = Make(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 352, 401, 135, 27, IDC_TARGET_NAME); addFont(targetName_);
        addFont(Make(L"BUTTON", L"LƯU/CẬP NHẬT", BS_PUSHBUTTON, 497, 401, 150, 28, IDC_SAVE_TARGET));
        addFont(Make(L"BUTTON", L"XÓA BÃI", BS_PUSHBUTTON, 657, 401, 90, 28, IDC_DELETE_SPOT));
        targetText_ = Make(L"STATIC", L"CHƯA CHỌN", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 757, 401, 266, 28, IDC_TARGET_TEXT); addFont(targetText_);

        addFont(Make(L"STATIC", L"Sai số:", 0, 18, 439, 55, 22, 0));
        tolerance_ = Make(L"EDIT", L"120", WS_BORDER | ES_NUMBER | ES_CENTER, 73, 435, 70, 27, IDC_TOLERANCE); addFont(tolerance_);
        enableRevive_ = Make(L"BUTTON", L"Tự Đầu thai", BS_AUTOCHECKBOX, 160, 436, 125, 24, IDC_ENABLE_REVIVE); addFont(enableRevive_);
        enableConfirm_ = Make(L"BUTTON", L"XN Lâu Lan mắc cổng", BS_AUTOCHECKBOX, 300, 436, 175, 24, IDC_ENABLE_CONFIRM); addFont(enableConfirm_);
        enableShortcut_ = Make(L"BUTTON", L"ĐƯỜNG TẮT", BS_AUTOCHECKBOX, 480, 436, 112, 24, IDC_ENABLE_SHORTCUT); addFont(enableShortcut_);
        SendMessageW(enableShortcut_, BM_SETCHECK, shortcutSettings_.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        shortcutSettingsButton_ = Make(L"BUTTON", L"TÙY CHỈNH", BS_PUSHBUTTON, 320, 112, 124, 30, IDC_SHORTCUT_SETTINGS); addFont(shortcutSettingsButton_);
        enableFight_ = Make(L"BUTTON", L"AUTO → Đánh quái", BS_AUTOCHECKBOX, 600, 436, 145, 24, IDC_ENABLE_FIGHT); addFont(enableFight_);
        // Dòng mô tả kỹ thuật InputSync/callback được ẩn khỏi giao diện khách hàng.
        addFont(Make(L"BUTTON", L"LẤY 3 CLICK CỦA ACC...", BS_PUSHBUTTON, 755, 526, 268, 27, IDC_COPY_CLICKS));
        const int visibleSlots[3] = {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)};
        const int rowY[3] = {552, 578, 604};
        const int pointIds[3] = {IDC_POINT_AUTO, IDC_POINT_ATTACK, IDC_POINT_STOP_AUTO_2};
        const int captureIds[3] = {IDC_CAPTURE_AUTO, IDC_CAPTURE_ATTACK, IDC_CAPTURE_STOP_AUTO_2};
        const int testIds[3] = {IDC_TEST_AUTO, IDC_TEST_ATTACK, IDC_TEST_STOP_AUTO_2};
        for (int row = 0; row < 3; ++row) {
            const int i = visibleSlots[row];
            addFont(Make(L"STATIC", kClickLabels[static_cast<std::size_t>(i)], SS_LEFT | SS_CENTERIMAGE, 18, rowY[row], 150, 24, 0));
            pointLabels_[static_cast<std::size_t>(i)] = Make(L"STATIC", L"CHƯA LẤY", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 172, rowY[row], 430, 24, pointIds[row]);
            addFont(pointLabels_[static_cast<std::size_t>(i)]);
            addFont(Make(L"BUTTON", L"LẤY F8", BS_PUSHBUTTON, 612, rowY[row], 115, 24, captureIds[row]));
            addFont(Make(L"BUTTON", L"TEST", BS_PUSHBUTTON, 737, rowY[row], 90, 24, testIds[row]));
        }
        // Dòng mô tả nội bộ MAIN/FIFO/batch được ẩn khỏi giao diện khách hàng.
        addFont(Make(L"BUTTON", L"QUẢN LÝ NHANH • BÃI TRAIN / TẬP TRUNG / PT", BS_GROUPBOX, 18, 662, 1005, 70, 0));
        addFont(Make(L"BUTTON", L"ÁP BÃI PT", BS_PUSHBUTTON, 32, 686, 100, 28, IDC_APPLY_SPOT_PARTY));
        addFont(Make(L"BUTTON", L"ÁP ALL CON", BS_PUSHBUTTON, 140, 686, 112, 28, IDC_APPLY_SPOT_ALL_CON));
        gatherToggleButton_ = Make(L"BUTTON", L"TẬP TRUNG: OFF", BS_PUSHBUTTON, 260, 686, 132, 28, IDC_GATHER_TOGGLE); addFont(gatherToggleButton_);
        addFont(Make(L"BUTTON", L"LẤY TỌA TẬP TRUNG", BS_PUSHBUTTON, 400, 686, 155, 28, IDC_GATHER_CAPTURE));
        gatherLabel_ = Make(L"STATIC", L"CHƯA TỌA", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 563, 686, 150, 28, 0); addFont(gatherLabel_);
        setPartyKeyButton_ = Make(L"BUTTON", L"ĐẶT KEY", BS_PUSHBUTTON, 721, 686, 92, 28, IDC_SET_PARTY_KEY); addFont(setPartyKeyButton_);
        partyBuildToggleButton_ = Make(L"BUTTON", L"TỰ TẠO PT: OFF", BS_PUSHBUTTON, 821, 686, 182, 28, IDC_PARTY_BUILD_TOGGLE); addFont(partyBuildToggleButton_);

        imageScanButton_ = Make(L"BUTTON", L"LỌC VK V4 [C1: ON]", BS_PUSHBUTTON, 18, 62, 198, 30, IDC_TEST_IMAGE_SCAN); addFont(imageScanButton_);
        filterMode2Button_ = Make(L"BUTTON", L"LỌC VK CÁCH 2: OFF", BS_PUSHBUTTON, 224, 62, 210, 30, IDC_FILTER_MODE2); addFont(filterMode2Button_);
        bagScanButton_ = Make(L"BUTTON", L"QUÉT TAY NẢI", BS_PUSHBUTTON, 452, 112, 160, 30, IDC_SCAN_BAG_SEMANTIC); addFont(bagScanButton_);
        if (imageScanButton_) ShowWindow(imageScanButton_, SW_HIDE);
        if (filterMode2Button_) ShowWindow(filterMode2Button_, SW_HIDE);
        if (bagScanButton_) ShowWindow(bagScanButton_, SW_HIDE);
        UpdateFilterModeButtons();

        auto pb = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
            HWND control = Make(cls, text, style, x, y, w, h, id);
            addFont(control);
            partyBuildDevControls_.push_back(control);
            if (control) ShowWindow(control, SW_HIDE);
            return control;
        };
        pb(L"BUTTON", L"AUTO TẠO PT • DEVELOPER", BS_GROUPBOX, 18, 150, 1005, 160, 0);
        const std::array<const wchar_t*, 3> pbNames{{L"KEY CLICK 1", L"KEY CLICK 2", L"MẶT MEMBER"}};
        const std::array<int, 3> pbCaptureIds{{IDC_PB_CAPTURE_CLICK1, IDC_PB_CAPTURE_CLICK2, IDC_PB_CAPTURE_FACE}};
        const std::array<int, 3> pbTestIds{{IDC_PB_TEST_CLICK1, IDC_PB_TEST_CLICK2, IDC_PB_TEST_FACE}};
        const std::array<int, 3> pbDelayIds{{IDC_PB_DELAY_CLICK1, IDC_PB_DELAY_CLICK2, IDC_PB_DELAY_FACE}};
        for (int i = 0; i < 3; ++i) {
            const int y = 176 + i * 34;
            pb(L"STATIC", pbNames[static_cast<std::size_t>(i)], SS_LEFT | SS_CENTERIMAGE, 32, y, 105, 25, 0);
            partyBuildPointLabels_[static_cast<std::size_t>(i)] = pb(L"STATIC", L"CHƯA LẤY", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 140, y, 315, 25, 0);
            const wchar_t* capText = i == 0 ? L"LẤY F8 CLICK 1" : (i == 1 ? L"LẤY F8 CLICK 2" : L"LẤY F8 MẶT");
            pb(L"BUTTON", capText, BS_PUSHBUTTON, 463, y, 135, 25, pbCaptureIds[static_cast<std::size_t>(i)]);
            pb(L"BUTTON", L"TEST", BS_PUSHBUTTON, 606, y, 70, 25, pbTestIds[static_cast<std::size_t>(i)]);
            pb(L"STATIC", L"Delay", SS_LEFT | SS_CENTERIMAGE, 686, y, 42, 25, 0);
            partyBuildDelayEdits_[static_cast<std::size_t>(i)] = pb(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER, 730, y, 70, 25, pbDelayIds[static_cast<std::size_t>(i)]);
            pb(L"STATIC", L"ms", SS_LEFT | SS_CENTERIMAGE, 804, y, 25, 25, 0);
        }
        pb(L"STATIC", L"Retry target", SS_LEFT | SS_CENTERIMAGE, 840, 176, 75, 25, 0);
        partyBuildTargetRetryEdit_ = pb(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER, 920, 176, 55, 25, IDC_PB_TARGET_RETRY);
        pb(L"STATIC", L"Retry invite", SS_LEFT | SS_CENTERIMAGE, 840, 210, 75, 25, 0);
        partyBuildInviteRetryEdit_ = pb(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER, 920, 210, 55, 25, IDC_PB_INVITE_RETRY);
        pb(L"STATIC", L"KEY: click1 → click2 → target từng member → click mặt → callback 'Mời vào nhóm'.",
           SS_LEFT | SS_CENTERIMAGE, 840, 244, 145, 52, 0);

        logCaption_ = Make(L"STATIC", L"LOG CHẨN ĐOÁN / BỘ ĐIỀU PHỐI • có thể TẮT để giảm tải UI", SS_LEFT | SS_CENTERIMAGE,
                           18, 54, 640, 28, 0); addFont(logCaption_);
        mainLogToggleButton_ = Make(L"BUTTON", mainLogEnabled_ ? L"LOG: ON" : L"LOG: OFF", BS_PUSHBUTTON, 670, 54, 105, 28, IDC_LOG_ENABLED); addFont(mainLogToggleButton_);
        clearLogButton_ = Make(L"BUTTON", L"XÓA LOG", BS_PUSHBUTTON, 900, 54, 123, 28, IDC_CLEAR_LOG); addFont(clearLogButton_);
        log_ = Make(L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL,
                    18, 88, 1005, 826, IDC_LOG); addFont(log_);
        if (log_) SendMessageW(log_, EM_SETLIMITTEXT, 4 * 1024 * 1024, 0);
        runtimeLogControls_ = {logCaption_, mainLogToggleButton_, clearLogButton_, log_};
        for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);

        exportLogButton_ = Make(L"BUTTON", L"XUẤT FILE LOG", BS_PUSHBUTTON, 850, 151, 173, 28, IDC_EXPORT_LOG); addFont(exportLogButton_);

        autoLootGroup_ = Make(L"BUTTON", L"AUTO NHẶT ITEMPACK • 10.6", BS_GROUPBOX, 18, 322, 1005, 86, 0); addFont(autoLootGroup_);
        autoLootToggleButton_ = Make(L"BUTTON", L"AUTO NHẶT KHÔNG CẦN BÌNH: OFF", BS_PUSHBUTTON, 32, 347, 285, 30, IDC_AUTO_LOOT_TOGGLE); addFont(autoLootToggleButton_);
        autoLootIntervalLabel_ = Make(L"STATIC", L"Chu kỳ nhặt (ms):", SS_LEFT | SS_CENTERIMAGE, 338, 347, 225, 30, 0); addFont(autoLootIntervalLabel_);
        autoLootIntervalEdit_ = Make(L"EDIT", L"1000", WS_BORDER | ES_NUMBER | ES_CENTER, 568, 348, 110, 28, IDC_AUTO_LOOT_INTERVAL); addFont(autoLootIntervalEdit_);

        lootProbeGroup_ = Make(L"BUTTON", L"ITEMPACK TEST • V99 CP19", BS_GROUPBOX, 18, 414, 1005, 184, 0); addFont(lootProbeGroup_);
        lootScanButton_ = Make(L"BUTTON", L"SCAN QUANH ACC", BS_PUSHBUTTON, 32, 441, 205, 32, IDC_LOOT_SCAN); addFont(lootScanButton_);
        lootPickButton_ = Make(L"BUTTON", L"NHẶT GẦN NHẤT", BS_PUSHBUTTON, 248, 441, 205, 32, IDC_LOOT_PICK); addFont(lootPickButton_);
        lootOutput_ = Make(L"EDIT", L"Chọn ACC rồi bấm SCAN QUANH ACC.", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                           32, 482, 975, 100, IDC_LOOT_OUTPUT); addFont(lootOutput_);

        developerControls_ = {shortcutSettingsButton_, bagScanButton_, exportLogButton_,
                              autoLootGroup_, autoLootToggleButton_, autoLootIntervalLabel_, autoLootIntervalEdit_,
                              lootProbeGroup_, lootScanButton_, lootPickButton_, lootOutput_};
        developerControls_.insert(developerControls_.end(), partyBuildDevControls_.begin(), partyBuildDevControls_.end());
        LoadPartyBuildSettingsToUi();
        RefreshAutoLootDeveloperUi();
        for (HWND h : developerControls_) if (h) ShowWindow(h, SW_HIDE);

        // v0.6 TELEGRAM is a pure observer/output tab. None of these controls participate
        // in click lease, World Flow, scanner, P1/P2/P3, Sell or Trade state machines.
        auto tg = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
            HWND control = Make(cls, text, style, x, y, w, h, id);
            addFont(control);
            telegramControls_.push_back(control);
            if (control) ShowWindow(control, SW_HIDE);
            return control;
        };
        tg(L"BUTTON", L"CẤU HÌNH TELEGRAM BOT", BS_GROUPBOX, 18, 48, 1005, 190, 0);
        telegramEnabled_ = tg(L"BUTTON", L"BẬT THÔNG BÁO TELEGRAM", BS_AUTOCHECKBOX, 35, 70, 220, 24, IDC_TG_ENABLED);
        tg(L"STATIC", L"Bot Token:", SS_LEFT | SS_CENTERIMAGE, 35, 103, 75, 25, 0);
        telegramToken_ = tg(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD, 112, 101, 655, 28, IDC_TG_TOKEN);
        telegramShowToken_ = tg(L"BUTTON", L"HIỆN", BS_PUSHBUTTON, 775, 101, 72, 28, IDC_TG_SHOW_TOKEN);
        tg(L"STATIC", L"Chat ID:", SS_LEFT | SS_CENTERIMAGE, 35, 137, 75, 25, 0);
        telegramChatId_ = tg(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 112, 135, 350, 28, IDC_TG_CHAT_ID);
        tg(L"BUTTON", L"LƯU CẤU HÌNH", BS_DEFPUSHBUTTON, 475, 135, 128, 28, IDC_TG_SAVE);
        tg(L"BUTTON", L"TEST BOT", BS_PUSHBUTTON, 611, 135, 92, 28, IDC_TG_TEST_BOT);
        tg(L"BUTTON", L"LẤY CHAT ID", BS_PUSHBUTTON, 711, 135, 108, 28, IDC_TG_DISCOVER_CHAT);
        tg(L"BUTTON", L"GỬI TIN THỬ", BS_PUSHBUTTON, 827, 135, 110, 28, IDC_TG_SEND_TEST);
        tg(L"BUTTON", L"GỬI BÁO CÁO NGAY", BS_PUSHBUTTON, 827, 169, 160, 28, IDC_TG_SEND_SUMMARY);
        telegramStatus_ = tg(L"STATIC", L"Telegram worker: khởi động...", SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 35, 169, 775, 28, IDC_TG_STATUS);
        tg(L"STATIC", L"HƯỚNG DẪN: 1) Telegram → @BotFather → /newbot → dán Token.  2) Mở bot và bấm Start / gửi 1 tin.  3) Bấm TEST BOT → LẤY CHAT ID → GỬI TIN THỬ.  Group: thêm bot vào group rồi gửi /test trước khi LẤY CHAT ID. Nếu bot đang dùng webhook thì getUpdates không lấy được Chat ID; có thể nhập Chat ID thủ công.",
           SS_LEFT, 35, 203, 950, 30, 0);

        tg(L"BUTTON", L"SỰ KIỆN / BÁO CÁO", BS_GROUPBOX, 18, 245, 1005, 285, 0);
        telegramNotifyDeath_ = tg(L"BUTTON", L"Số lần chết", BS_AUTOCHECKBOX, 38, 272, 170, 24, IDC_TG_NOTIFY_DEATH);
        telegramNotifyTrade_ = tg(L"BUTTON", L"Số lần nhận đồ", BS_AUTOCHECKBOX, 230, 272, 180, 24, IDC_TG_NOTIFY_TRADE);
        telegramNotifySellComplete_ = tg(L"BUTTON", L"Số lần bán", BS_AUTOCHECKBOX, 430, 272, 160, 24, IDC_TG_NOTIFY_SELL_COMPLETE);
        telegramNotifyFunAlerts_ = tg(L"BUTTON", L"Cảnh báo mốc vàng", BS_AUTOCHECKBOX, 610, 272, 190, 24, IDC_TG_NOTIFY_FUN_ALERTS);
        telegramNotifyFifo_ = tg(L"BUTTON", L"Chết >10 lần / 10 phút", BS_AUTOCHECKBOX, 38, 306, 230, 24, IDC_TG_NOTIFY_FIFO);

        telegramIntervalEnabled_ = tg(L"BUTTON", L"Báo cáo mỗi", BS_AUTOCHECKBOX, 38, 402, 112, 24, IDC_TG_INTERVAL_ENABLED);
        telegramIntervalMinutes_ = tg(L"EDIT", L"60", WS_BORDER | ES_NUMBER | ES_CENTER, 150, 400, 55, 27, IDC_TG_INTERVAL_MINUTES);
        tg(L"STATIC", L"phút (5–1440)", SS_LEFT | SS_CENTERIMAGE, 212, 402, 105, 24, 0);
        telegramDailyEnabled_ = tg(L"BUTTON", L"Mốc cố định:", BS_AUTOCHECKBOX, 330, 402, 110, 24, IDC_TG_DAILY_ENABLED);
        const int timeIds[4] = {IDC_TG_DAILY_TIME1, IDC_TG_DAILY_TIME2, IDC_TG_DAILY_TIME3, IDC_TG_DAILY_TIME4};
        for (int i = 0; i < 4; ++i) telegramDailyTime_[static_cast<std::size_t>(i)] = tg(L"EDIT", L"", WS_BORDER | ES_CENTER, 445 + i * 70, 400, 62, 27, timeIds[i]);
        tg(L"STATIC", L"Báo cáo vàng: 60 phút gần nhất + tổng từ lúc bật tool.", SS_LEFT | SS_CENTERIMAGE, 38, 482, 620, 24, 0);

        tg(L"BUTTON", L"LOCAL / TELE LOG — có thể TẮT ghi UI để giảm tải; Telegram/report vẫn chạy", BS_GROUPBOX, 18, 540, 1005, 386, 0);
        telegramLog_ = tg(L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 35, 568, 970, 306, IDC_TG_LOG);
        if (telegramLog_) SendMessageW(telegramLog_, EM_SETLIMITTEXT, 1024 * 1024, 0);
        tg(L"BUTTON", L"XÓA LOG", BS_PUSHBUTTON, 35, 884, 105, 28, IDC_TG_CLEAR_LOG);
        tg(L"BUTTON", L"SAO CHÉP LOG", BS_PUSHBUTTON, 150, 884, 130, 28, IDC_TG_COPY_LOG);
        tg(L"BUTTON", L"XUẤT FILE TELE", BS_PUSHBUTTON, 290, 884, 145, 28, IDC_TG_EXPORT_LOG);
        telegramLogToggleButton_ = tg(L"BUTTON", telegramLogEnabled_ ? L"TELE LOG: ON" : L"TELE LOG: OFF", BS_PUSHBUTTON, 445, 884, 145, 28, IDC_TG_LOG_ENABLED);
        // Cảnh báo kỹ thuật BotTokenProtected/DPAPI được ẩn khỏi giao diện; cơ chế bảo vệ vẫn giữ nguyên.

        LoadTelegramSettingsToUi();
        if (!telegramWorker_.Start(hwnd_, kTelegramResultMessage)) SetTelegramStatus(L"Telegram worker START FAIL");
        else SetTelegramStatus(L"Telegram worker READY • network tách khỏi toàn bộ auto");
        if (!telegramLoadWarning_.empty()) AddTelegramLog(L"CONFIG", L"-", L"WARN", telegramLoadWarning_);


        if (!RegisterHotKey(hwnd_, kCaptureHotkeyId, MOD_NOREPEAT, VK_F8)) {
            Log(L"CẢNH BÁO: không đăng ký được F8 global.");
        }
        if (!RegisterHotKey(hwnd_, kPauseHotkeyId, MOD_NOREPEAT, VK_F4)) {
            Log(L"CẢNH BÁO: không đăng ký được F4 global.");
        }
        Log(L"HIDDEN ACTION ENGINE ON • auto-click dùng InputSync nội bộ; không chiếm chuột Windows.");
        SetTimer(hwnd_, kTimer, 250, nullptr);
        RefreshLicenseTitle();
        UpdateTradeRendezvousLabel();
        UpdateRoleActionButtons();
        UpdateGatherLabel();
        UpdatePartyBuildToggleLabel();
        ScanClients();
    }

    bool IsTelegramControl(HWND h) const {
        return std::find(telegramControls_.begin(), telegramControls_.end(), h) != telegramControls_.end();
    }

    bool IsRuntimeLogControl(HWND h) const {
        return std::find(runtimeLogControls_.begin(), runtimeLogControls_.end(), h) != runtimeLogControls_.end();
    }

    bool IsDeveloperControl(HWND h) const {
        return std::find(developerControls_.begin(), developerControls_.end(), h) != developerControls_.end();
    }

    void ShowDeveloperControls(bool showPage) {
        for (HWND h : developerControls_) if (h) ShowWindow(h, showPage ? SW_SHOW : SW_HIDE);
    }

    void SwitchMainTab(int index) {
        if (!mainTab_) return;
        index = std::clamp(index, 0, 4);
        if (index == mainTabIndex_) return;

        if (mainTabIndex_ == 0) {
            autoTabVisibility_.clear();
            for (HWND child = GetWindow(hwnd_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                if (child == mainTab_ || IsTelegramControl(child) || IsRuntimeLogControl(child) || IsDeveloperControl(child)) continue;
                autoTabVisibility_.push_back({child, IsWindowVisible(child) != FALSE});
                ShowWindow(child, SW_HIDE);
            }
        } else if (mainTabIndex_ == 1) {
            if (imageScanButton_) ShowWindow(imageScanButton_, SW_HIDE);
            if (filterMode2Button_) ShowWindow(filterMode2Button_, SW_HIDE);
            if (bagScanButton_) ShowWindow(bagScanButton_, SW_HIDE);
        } else if (mainTabIndex_ == 2) {
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);
        } else if (mainTabIndex_ == 3) {
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_HIDE);
        } else if (mainTabIndex_ == 4) {
            ShowDeveloperControls(false);
        }

        if (index == 0) {
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_HIDE);
            ShowDeveloperControls(false);
            for (const auto& saved : autoTabVisibility_) {
                if (saved.first && IsWindow(saved.first)) ShowWindow(saved.first, saved.second ? SW_SHOW : SW_HIDE);
            }
            autoTabVisibility_.clear();
        } else if (index == 1) {
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_HIDE);
            ShowDeveloperControls(false);
            if (imageScanButton_) ShowWindow(imageScanButton_, SW_SHOW);
            if (filterMode2Button_) ShowWindow(filterMode2Button_, SW_SHOW);
            // QUÉT TAY NẢI is DEVELOPER-only from CP12; ShowDeveloperControls owns visibility.
            UpdateFilterModeButtons();
        } else if (index == 2) {
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_HIDE);
            ShowDeveloperControls(false);
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_SHOW);
            if (log_) SendMessageW(log_, EM_SCROLLCARET, 0, 0);
        } else if (index == 3) {
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);
            ShowDeveloperControls(false);
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_SHOW);
        } else {
            for (HWND h : runtimeLogControls_) if (h) ShowWindow(h, SW_HIDE);
            for (HWND h : telegramControls_) if (h) ShowWindow(h, SW_HIDE);
            ShowDeveloperControls(true);
        }
        mainTabIndex_ = index;
    }


    bool IsCompactKeepControl(HWND h) const {
        return h == clientList_ || h == selectAllButton_ || h == clearAllButton_ || h == scanButton_ ||
               h == startCheckedButton_ || h == stopCheckedButton_ || h == compactButton_;
    }

    void ToggleCompactMode() {
        if (!compactMode_) {
            // Compact mode chỉ giữ danh sách acc và các nút scan/start/pause;
            // coordinator status vẫn ẩn theo UI khách hàng.
            if (mainTabIndex_ != 0) SwitchMainTab(0);
            compactVisibility_.clear();
            for (HWND child = GetWindow(hwnd_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                compactVisibility_.push_back({child, IsWindowVisible(child) != FALSE});
                if (!IsCompactKeepControl(child)) ShowWindow(child, SW_HIDE);
            }
            if (tradeStatus_) SetWindowPos(tradeStatus_, nullptr,
                                           kCoordinatorStatusX, kCompactCoordinatorStatusY,
                                           kCoordinatorStatusWidth, kCoordinatorStatusHeight,
                                           SWP_NOZORDER | SWP_NOACTIVATE);
            if (compactButton_) {
                SetWindowTextW(compactButton_, L"MỞ RỘNG");
                ShowWindow(compactButton_, SW_SHOW);
            }
            if (clientList_) ShowWindow(clientList_, SW_SHOW);
            if (scanButton_) ShowWindow(scanButton_, SW_SHOW);
            if (startCheckedButton_) ShowWindow(startCheckedButton_, SW_SHOW);
            if (stopCheckedButton_) ShowWindow(stopCheckedButton_, SW_SHOW);
            if (tradeStatus_) ShowWindow(tradeStatus_, SW_HIDE);
            SetWindowPos(hwnd_, nullptr, 0, 0, 1060, 350, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            compactMode_ = true;
            return;
        }

        for (const auto& saved : compactVisibility_) {
            if (saved.first && IsWindow(saved.first)) ShowWindow(saved.first, saved.second ? SW_SHOW : SW_HIDE);
        }
        compactVisibility_.clear();
        if (tradeStatus_) SetWindowPos(tradeStatus_, nullptr,
                                       kCoordinatorStatusX, kCoordinatorStatusY,
                                       kCoordinatorStatusWidth, kCoordinatorStatusHeight,
                                       SWP_NOZORDER | SWP_NOACTIVATE);
        if (compactButton_) {
            SetWindowTextW(compactButton_, L"THU NHỎ");
            ShowWindow(compactButton_, SW_SHOW);
        }
        SetWindowPos(hwnd_, nullptr, 0, 0, 1060, 1030, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        compactMode_ = false;
    }


    bool ExportTextFile(const wchar_t* defaultName, const std::wstring& text) {
        wchar_t fileName[MAX_PATH]{};
        wcsncpy_s(fileName, defaultName ? defaultName : L"ThanLong_Log.txt", _TRUNCATE);
        const wchar_t filter[] = L"Text file (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = _countof(fileName);
        ofn.lpstrDefExt = L"txt";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return false;

        HANDLE h = CreateFileW(fileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        const BYTE bom[2] = {0xFF, 0xFE};
        DWORD written = 0;
        bool ok = WriteFile(h, bom, sizeof(bom), &written, nullptr) != FALSE;
        const DWORD bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
        if (ok && bytes > 0)
            ok = WriteFile(h, text.data(), bytes, &written, nullptr) != FALSE && written == bytes;
        CloseHandle(h);
        return ok;
    }

    void ExportMainLog() {
        if (!log_) return;
        const int len = GetWindowTextLengthW(log_);
        std::wstring text(static_cast<std::size_t>(len) + 1u, L'\0');
        if (len > 0) GetWindowTextW(log_, text.data(), len + 1);
        text.resize(static_cast<std::size_t>(len));
        if (ExportTextFile(L"ThanLong_LOG.txt", text)) Log(L"Đã xuất file LOG.");
        else Log(L"Xuất file LOG bị hủy hoặc thất bại.");
    }

    void UpdateLogToggleLabels() {
        if (mainLogToggleButton_) SetWindowTextW(mainLogToggleButton_, mainLogEnabled_ ? L"LOG: ON" : L"LOG: OFF");
        if (telegramLogToggleButton_) SetWindowTextW(telegramLogToggleButton_, telegramLogEnabled_ ? L"TELE LOG: ON" : L"TELE LOG: OFF");
    }

    void ToggleMainLogEnabled() {
        mainLogEnabled_ = !mainLogEnabled_;
        WriteIniInt(L"UiPerformance", L"MainLogEnabled", mainLogEnabled_ ? 1 : 0);
        FlushIni();
        UpdateLogToggleLabels();
        if (mainLogEnabled_) Log(L"LOG UI đã BẬT • thao tác ghi log có thể tăng tải khi log dày.");
    }

    void ToggleTelegramLogEnabled() {
        telegramLogEnabled_ = !telegramLogEnabled_;
        WriteIniInt(L"UiPerformance", L"TelegramLogEnabled", telegramLogEnabled_ ? 1 : 0);
        FlushIni();
        UpdateLogToggleLabels();
        SetTelegramStatus(telegramLogEnabled_ ? L"TELE LOG UI: ON" : L"TELE LOG UI: OFF • Telegram/report vẫn hoạt động");
    }

    void Log(const std::wstring& text) {
        if (!mainLogEnabled_ || !log_) return;
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t prefix[32]{};
        wsprintfW(prefix, L"[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);
        std::wstring line = prefix + text + L"\r\n";
        const int len = GetWindowTextLengthW(log_);
        SendMessageW(log_, EM_SETSEL, len, len);
        SendMessageW(log_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
        SendMessageW(log_, EM_SCROLLCARET, 0, 0);
    }

    std::wstring AccountTag(const Account& a) const {
        if (!a.displayName.empty()) return a.displayName + L"/PID " + std::to_wstring(a.game.pid);
        return L"PID " + std::to_wstring(a.game.pid);
    }

    void LogAccount(const Account& a, const std::wstring& text) {
        Log(L"[" + AccountTag(a) + L"] " + text);
    }

    int SelectedIndex() const {
        if (!clientList_) return -1;
        return ListView_GetNextItem(clientList_, -1, LVNI_SELECTED);
    }

    Account* SelectedAccount() {
        const int i = SelectedIndex();
        if (i < 0 || i >= static_cast<int>(accounts_.size())) return nullptr;
        return accounts_[static_cast<std::size_t>(i)].get();
    }

    Account* AccountByPid(DWORD pid) {
        for (auto& a : accounts_) if (a->game.pid == pid) return a.get();
        return nullptr;
    }

    static std::wstring TrimWs(std::wstring text) {
        auto ws = [](wchar_t c){ return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
        while (!text.empty() && ws(text.front())) text.erase(text.begin());
        while (!text.empty() && ws(text.back())) text.pop_back();
        return text;
    }

    static std::wstring LocalDateTimeText() {
        SYSTEMTIME st{}; GetLocalTime(&st);
        wchar_t buf[64]{};
        wsprintfW(buf, L"%02u/%02u/%04u %02u:%02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
        return buf;
    }

    static std::wstring LocalTimeText() {
        SYSTEMTIME st{}; GetLocalTime(&st);
        wchar_t buf[32]{};
        wsprintfW(buf, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        return buf;
    }

    static std::wstring LocalHourMinuteText() {
        SYSTEMTIME st{}; GetLocalTime(&st);
        wchar_t buf[16]{};
        wsprintfW(buf, L"%02u:%02u", st.wHour, st.wMinute);
        return buf;
    }

    static std::wstring FormatDurationSeconds(ULONGLONG ms) {
        const ULONGLONG sec = ms / 1000ULL;
        const ULONGLONG h = sec / 3600ULL;
        const ULONGLONG m = (sec % 3600ULL) / 60ULL;
        const ULONGLONG s = sec % 60ULL;
        if (h > 0) return std::to_wstring(h) + L"h " + std::to_wstring(m) + L"m " + std::to_wstring(s) + L"s";
        if (m > 0) return std::to_wstring(m) + L"m " + std::to_wstring(s) + L"s";
        return std::to_wstring(s) + L"s";
    }

    std::wstring TelegramAccountLabel(const Account& a) const {
        std::wstring role = TradeRoleLabel(a.profile.tradeRole);
        if (role == L"-") role = L"ACC";
        // Telegram v1.3 is intentionally human-readable only: never append RoleID/PID.
        // displayName includes numeric identity in the main UI, so read the plain runtime name instead.
        if ((a.snapshot.validMask & ValidIdentity) && a.snapshot.characterName[0] != 0)
            return role + L" • " + std::wstring(a.snapshot.characterName);
        return role;
    }

    bool TelegramCheck(HWND h) const { return h && SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED; }
    void SetTelegramCheck(HWND h, bool value) { if (h) SendMessageW(h, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0); }

    void SetTelegramStatus(const std::wstring& text) {
        if (telegramStatus_) SetText(telegramStatus_, text);
    }

    void AddTelegramLog(const std::wstring&, const std::wstring&,
                        const std::wstring&, const std::wstring& detail) {
        if (!telegramLogEnabled_ || !telegramLog_ || detail.empty()) return;
        const int len = GetWindowTextLengthW(telegramLog_);
        SendMessageW(telegramLog_, EM_SETSEL, len, len);
        const std::wstring append = (len > 0 ? L"\r\n\r\n" : L"") + detail;
        SendMessageW(telegramLog_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(append.c_str()));
        SendMessageW(telegramLog_, EM_SCROLLCARET, 0, 0);
    }

    void AddLocalReport(const std::wstring& eventType, const std::wstring& account,
                        const std::wstring& detail) {
        AddTelegramLog(eventType, account, L"LOCAL ONLY", detail);
    }

    void ClearTelegramLog() {
        if (telegramLog_) SetText(telegramLog_, L"");
        SetTelegramStatus(L"Telegram log đã xóa");
    }

    void CopyTelegramLog() {
        if (!telegramLog_) return;
        const std::wstring text = GetText(telegramLog_);
        if (!OpenClipboard(hwnd_)) { SetTelegramStatus(L"Không mở được Clipboard"); return; }
        EmptyClipboard();
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem) {
            void* dst = GlobalLock(mem);
            if (dst) { std::memcpy(dst, text.c_str(), bytes); GlobalUnlock(mem); SetClipboardData(CF_UNICODETEXT, mem); mem = nullptr; }
        }
        if (mem) GlobalFree(mem);
        CloseClipboard();
        SetTelegramStatus(L"Đã sao chép LOCAL / TELE LOG");
    }


    std::wstring BuildTelegramLogText() const {
        return telegramLog_ ? GetText(telegramLog_) : std::wstring{};
    }

    void ExportTelegramLog() {
        if (ExportTextFile(L"ThanLong_TELE_LOG.txt", BuildTelegramLogText()))
            SetTelegramStatus(L"Đã xuất file TELE log");
        else
            SetTelegramStatus(L"Xuất file TELE log bị hủy hoặc thất bại");
    }

    void LoadTelegramSettingsToUi() {
        SetTelegramCheck(telegramEnabled_, telegramSettings_.enabled);
        SetText(telegramToken_, telegramSettings_.botToken);
        if (telegramToken_) { SendMessageW(telegramToken_, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'●'), 0); InvalidateRect(telegramToken_, nullptr, TRUE); }
        telegramTokenVisible_ = false;
        SetText(telegramChatId_, telegramSettings_.chatId);
        SetTelegramCheck(telegramNotifyDeath_, telegramSettings_.reportDeathCount);
        SetTelegramCheck(telegramNotifySellComplete_, telegramSettings_.reportSellCount);
        SetTelegramCheck(telegramNotifyTrade_, telegramSettings_.reportReceiveCount);
        SetTelegramCheck(telegramNotifyFifo_, telegramSettings_.notifyDeathBurst);
        SetTelegramCheck(telegramNotifyFunAlerts_, telegramSettings_.notifyMoneyMilestones);
        for (std::size_t i = 0; i < telegramMoneyMilestone_.size(); ++i)
            SetTelegramCheck(telegramMoneyMilestone_[i], telegramSettings_.currencyMilestones[i]);
        SetTelegramCheck(telegramIntervalEnabled_, telegramSettings_.intervalEnabled);
        SetText(telegramIntervalMinutes_, std::to_wstring(telegramSettings_.intervalMinutes));
        SetTelegramCheck(telegramDailyEnabled_, telegramSettings_.dailyEnabled);
        for (std::size_t i = 0; i < telegramDailyTime_.size(); ++i) if (telegramDailyTime_[i]) SetText(telegramDailyTime_[i], telegramSettings_.dailyTimes[i]);
    }

    bool PersistTelegramSettingsFromUi(bool feedback) {
        if (!telegramToken_) return false;
        TelegramSettings next = telegramSettings_;
        next.enabled = TelegramCheck(telegramEnabled_);
        next.botToken = TrimWs(GetText(telegramToken_));
        next.chatId = TrimWs(GetText(telegramChatId_));
        next.reportDeathCount = TelegramCheck(telegramNotifyDeath_);
        next.reportSellCount = TelegramCheck(telegramNotifySellComplete_);
        next.reportReceiveCount = TelegramCheck(telegramNotifyTrade_);
        next.notifyDeathBurst = TelegramCheck(telegramNotifyFifo_);
        next.notifyMoneyMilestones = TelegramCheck(telegramNotifyFunAlerts_);
        for (std::size_t i = 0; i < next.currencyMilestones.size(); ++i)
            next.currencyMilestones[i] = TelegramCheck(telegramMoneyMilestone_[i]);
        next.intervalEnabled = TelegramCheck(telegramIntervalEnabled_);
        next.intervalMinutes = telegram_logic::ClampSummaryIntervalMinutes(_wtoi(GetText(telegramIntervalMinutes_).c_str()));
        next.dailyEnabled = TelegramCheck(telegramDailyEnabled_);
        const std::array<std::wstring, 4> defaults{L"08:00", L"12:00", L"18:00", L"23:00"};
        for (std::size_t i = 0; i < next.dailyTimes.size(); ++i) {
            next.dailyTimes[i] = telegram_logic::NormalizeDailyTime(TrimWs(GetText(telegramDailyTime_[i])), defaults[i]);
            SetText(telegramDailyTime_[i], next.dailyTimes[i]);
        }
        SetText(telegramIntervalMinutes_, std::to_wstring(next.intervalMinutes));

        std::wstring error;
        if (!SaveTelegramSettings(next, error)) {
            AddTelegramLog(L"CONFIG", L"-", L"FAIL", error);
            SetTelegramStatus(L"Lưu cấu hình Telegram thất bại");
            return false;
        }
        telegramSettings_ = std::move(next);
        if (feedback) {
            AddTelegramLog(L"CONFIG", L"-", L"OK", L"Đã lưu • Bot Token được mã hóa bằng Windows DPAPI");
            SetTelegramStatus(L"Đã lưu cấu hình Telegram bằng DPAPI");
        }
        return true;
    }

    void ToggleTelegramTokenVisible() {
        if (!telegramToken_) return;
        telegramTokenVisible_ = !telegramTokenVisible_;
        SendMessageW(telegramToken_, EM_SETPASSWORDCHAR, telegramTokenVisible_ ? 0 : static_cast<WPARAM>(L'●'), 0);
        InvalidateRect(telegramToken_, nullptr, TRUE);
        if (telegramShowToken_) SetText(telegramShowToken_, telegramTokenVisible_ ? L"ẨN" : L"HIỆN");
    }

    bool QueueTelegramRequest(telegram_notify::TaskKind kind, const std::wstring& message,
                              const std::wstring& eventType, const std::wstring& account,
                              bool forceManual = false) {
        if (!forceManual && !telegramSettings_.enabled) return false;
        if (!telegram_logic::LooksLikeBotToken(telegramSettings_.botToken)) {
            if (forceManual) AddTelegramLog(eventType, account, L"FAIL", L"Bot Token chưa hợp lệ");
            SetTelegramStatus(L"Telegram: Bot Token chưa hợp lệ");
            return false;
        }
        if (kind == telegram_notify::TaskKind::SendMessage && !telegram_logic::LooksLikeTelegramChatId(telegramSettings_.chatId)) {
            if (forceManual) AddTelegramLog(eventType, account, L"FAIL", L"Chat ID chưa hợp lệ");
            SetTelegramStatus(L"Telegram: Chat ID chưa hợp lệ");
            return false;
        }
        telegram_notify::Request req{};
        req.id = ++telegramRequestCounter_;
        req.kind = kind;
        req.botToken = telegramSettings_.botToken;
        req.chatId = telegramSettings_.chatId;
        req.message = message;
        req.eventType = eventType;
        req.account = account;
        if (!telegramWorker_.Enqueue(std::move(req))) {
            AddTelegramLog(eventType, account, L"DROP", L"Worker dừng hoặc queue đã đầy 200 event");
            SetTelegramStatus(L"Telegram queue đầy/dừng • event bị DROP");
            return false;
        }
        SetTelegramStatus(L"Telegram queue: " + std::to_wstring(telegramWorker_.Pending()) + L" pending");
        return true;
    }

    void TelegramTestBot() {
        if (!PersistTelegramSettingsFromUi(false)) return;
        (void)QueueTelegramRequest(telegram_notify::TaskKind::TestBot, L"", L"TEST BOT", L"-", true);
    }

    void TelegramDiscoverChatId() {
        if (!PersistTelegramSettingsFromUi(false)) return;
        (void)QueueTelegramRequest(telegram_notify::TaskKind::DiscoverChatId, L"", L"LẤY CHAT ID", L"-", true);
    }

    void TelegramSendTest() {
        if (!PersistTelegramSettingsFromUi(false)) return;
        const std::wstring msg = L"✅ Công cụ hỗ trợ game rảnh tay • 10.6\nTelegram kết nối thành công.\nThời gian: " + LocalDateTimeText();
        (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"TIN THỬ", L"-", true);
    }

    void HandleTelegramWorkerResult(LPARAM lp) {
        std::unique_ptr<telegram_notify::Result> result(reinterpret_cast<telegram_notify::Result*>(lp));
        if (!result) return;
        if (result->kind == telegram_notify::TaskKind::DiscoverChatId && result->ok && !result->discoveredChatId.empty()) {
            telegramSettings_.chatId = result->discoveredChatId;
            SetText(telegramChatId_, telegramSettings_.chatId);
            std::wstring saveError;
            if (!SaveTelegramSettings(telegramSettings_, saveError)) result->detail += L" • lấy được Chat ID nhưng lưu config FAIL: " + saveError;
        }
        SetTelegramStatus(std::wstring(result->ok ? L"Telegram OK • " : L"Telegram FAIL • ") + result->detail);
    }

    bool AnyRunningAccount() const {
        return std::any_of(accounts_.begin(), accounts_.end(), [](const std::unique_ptr<Account>& a){ return a && a->runtime.running; });
    }

    static std::wstring GoldIdentityFor(const Account& a) {
        if (!a.profile.section.empty()) return a.profile.section;
        if (a.snapshotValid && (a.snapshot.validMask & ValidIdentity) && a.snapshot.roleID > 0)
            return L"Role_" + std::to_wstring(a.snapshot.roleID);
        return L"PID_" + std::to_wstring(a.game.pid);
    }

    static std::wstring GoldCharacterFor(const Account& a) {
        if (a.snapshotValid && (a.snapshot.validMask & ValidIdentity) && a.snapshot.characterName[0])
            return a.snapshot.characterName;
        if (!a.displayName.empty()) return a.displayName;
        return L"PID " + std::to_wstring(a.game.pid);
    }

    void SetGoldHistoryWarning(const std::wstring& warning) {
        if (warning == goldHistoryWarning_) return;
        goldHistoryWarning_ = warning;
        goldHistoryWarningLogged_ = false;
    }

    void TryReopenGoldHistory(DWORD now) {
        if (goldHistoryReady_) return;
        if (goldHistoryRetryDueTick_ != 0 && static_cast<LONG>(now - goldHistoryRetryDueTick_) < 0) return;
        std::wstring error;
        goldHistoryReady_ = goldHistory_.Open(std::filesystem::path(ConfigDir()) / L"GoldHistory.v1.tsv",
                                              gold_history::UnixNow(), error);
        if (goldHistoryReady_) {
            goldHistoryRetryDueTick_ = 0;
            SetGoldHistoryWarning(L"");
        } else {
            goldHistoryRetryDueTick_ = now + 30000u;
            SetGoldHistoryWarning(error.empty() ? L"GOLD HISTORY: mở store thất bại" : error);
        }
    }

    bool ReadMainGoldSample(bool force) {
        Account* main = AccountByTradeRole(kMainTradeRole);
        if (!main || !main->bridge.Attached() || main->runtime.clientFreezeActive) return false;

        const DWORD now = GetTickCount();
        constexpr DWORD kGoldSampleMs = 30000u;
        constexpr DWORD kGoldRetryMs = 1000u;
        if (!force) {
            if (goldReadRetryDueTick_ != 0) {
                if (static_cast<LONG>(now - goldReadRetryDueTick_) < 0) return false;
            } else if (goldLastReadTick_ != 0 && !Elapsed(now, goldLastReadTick_, kGoldSampleMs)) {
                return false;
            }
        }

        Response r{};
        std::wstring ignored;
        if (!main->bridge.Call(Command::ReadCurrency, 0, 0, 0, r, ignored, 700) || (r.value0 & 2) == 0) {
            goldReadRetryDueTick_ = now + kGoldRetryMs;
            return false;
        }

        goldReadRetryDueTick_ = 0;
        goldLastReadTick_ = now;
        gold_history::Sample sample{};
        sample.unixSeconds = gold_history::UnixNow();
        sample.identity = GoldIdentityFor(*main);
        sample.characterName = GoldCharacterFor(*main);
        sample.boundMoneyRaw = r.value64_1;
        currentMainGoldSample_ = sample;
        currentMainGoldKnown_ = true;

        if (telegramStats_.active && !sessionMainGoldBaselineKnown_) {
            sessionMainGoldBaselineKnown_ = true;
            sessionMainGoldIdentity_ = sample.identity;
            sessionMainGoldBaselineRaw_ = sample.boundMoneyRaw;
            sessionMainGoldBaselineUnix_ = sample.unixSeconds;
        }

        TryReopenGoldHistory(now);
        if (goldHistoryReady_) {
            std::wstring error;
            if (!goldHistory_.Append(sample, sample.unixSeconds, error)) {
                goldHistoryReady_ = false;
                goldHistoryRetryDueTick_ = now + 30000u;
                SetGoldHistoryWarning(error.empty() ? L"GOLD HISTORY: ghi sample thất bại" : error);
            }
        }
        return true;
    }

    void TickGoldHistory(DWORD now) {
        TryReopenGoldHistory(now);
        (void)ReadMainGoldSample(false);

        constexpr DWORD kGoldMaintenanceMs = 10u * 60u * 1000u;
        if (goldHistoryReady_ && (goldMaintenanceTick_ == 0 || Elapsed(now, goldMaintenanceTick_, kGoldMaintenanceMs))) {
            goldMaintenanceTick_ = now;
            std::wstring error;
            if (!goldHistory_.Prune(gold_history::UnixNow(), error)) {
                goldHistoryReady_ = false;
                goldHistoryRetryDueTick_ = now + 30000u;
                SetGoldHistoryWarning(error.empty() ? L"GOLD HISTORY: dọn dữ liệu 3 ngày thất bại" : error);
            }
        }
        if (!goldHistoryWarning_.empty() && !goldHistoryWarningLogged_) {
            Log(goldHistoryWarning_);
            goldHistoryWarningLogged_ = true;
        }
    }

    void ResetTelegramReportBaseline() {
        telegramReportBaselineTime_ = LocalDateTimeText();
        telegramReportBaseline_.sellTotal = telegramStats_.sellTotal;
        telegramReportBaseline_.tradeTotal = telegramStats_.tradeTotal;
        telegramReportBaseline_.deathTotal = telegramStats_.deathTotal;
        telegramReportBaseline_.reviveTotal = telegramStats_.reviveTotal;
        telegramReportBaseline_.fifoTotal = telegramStats_.fifoTotal;
        telegramReportBaseline_.lauLanConfirmTotal = telegramStats_.lauLanConfirmTotal;
        telegramReportBaseline_.clientFreezeTotal = telegramStats_.clientFreezeTotal;
        telegramReportBaseline_.worldFlowTimeoutTotal = telegramStats_.worldFlowTimeoutTotal;
        telegramReportBaseline_.sellsByPid = telegramStats_.sellsByPid;
        telegramReportBaseline_.tradesByChildPid = telegramStats_.tradesByChildPid;
    }

    void BeginTelegramSession() {
        telegramStats_ = TelegramStats{};
        telegramStats_.active = true;
        telegramStats_.startedTick = GetTickCount64();
        GetLocalTime(&telegramStats_.startedLocal);
        telegramWatch_.clear();
        sessionMainGoldBaselineKnown_ = false;
        sessionMainGoldIdentity_.clear();
        sessionMainGoldBaselineRaw_ = 0;
        sessionMainGoldBaselineUnix_ = 0;
        telegramLastIntervalSummaryTick_ = GetTickCount();
        telegramLastDailyKeys_.fill(0);
        ResetTelegramReportBaseline();
        (void)ReadMainGoldSample(true);
        AddLocalReport(L"SESSION START", L"-",
                       L"Bắt đầu tính giờ từ nút START • " + LocalDateTimeText());
        if (false) {
            (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage,
                L"▶️ AUTO SESSION START\nThời gian: " + LocalDateTimeText(), L"SESSION START", L"-");
        }
    }

    static std::wstring FormatInt64Grouped(std::int64_t value) {
        std::wstring raw = std::to_wstring(value);
        const std::size_t begin = (!raw.empty() && raw[0] == L'-') ? 1u : 0u;
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(raw.size()) - 3;
             i > static_cast<std::ptrdiff_t>(begin); i -= 3) {
            raw.insert(static_cast<std::size_t>(i), 1, L'.');
        }
        return raw;
    }

    static std::wstring SignedMoneyDelta(std::int64_t value) {
        return (value >= 0 ? L"+" : L"") + FormatInt64Grouped(value);
    }

    static std::wstring FunnySellText(const std::wstring& label, int count, std::uint64_t seed) {
        switch (telegram_logic::PhraseIndex(seed, 6)) {
            case 0: return L"Hôm nay " + label + L" chăm chỉ phết: đã bán " + std::to_wstring(count) + L" lượt đồ rồi 💰";
            case 1: return label + L" vừa được phong danh hiệu đồng nát: " + std::to_wstring(count) + L" lượt bán từ lúc bật tool 😆";
            case 2: return L"Kho ve chai của " + label + L" chạy tốt: " + std::to_wstring(count) + L" lượt bán rồi anh.";
            case 3: return label + L" dọn túi như dọn nhà: tổng " + std::to_wstring(count) + L" lượt bán hôm nay (từ lúc bật tool).";
            case 4: return L"Báo cáo tài chính kiểu Thần Long: " + label + L" đã quăng hàng ra shop " + std::to_wstring(count) + L" lượt.";
            default:return label + L" đang làm giàu cho NPC: " + std::to_wstring(count) + L" lượt bán rồi, cái túi chắc nhẹ hẳn 😄";
        }
    }

    static std::wstring FunnyTradeText(int count, std::uint64_t seed) {
        switch (telegram_logic::PhraseIndex(seed, 5)) {
            case 0: return L"Đàn em đã mớm cho acc chính " + std::to_wstring(count) + L" lần rồi anh 🍚";
            case 1: return L"Acc chính hôm nay được các em bón tận miệng " + std::to_wstring(count) + L" lần 😆";
            case 2: return L"Hệ thống tiếp tế hoạt động ngon: CON → MAIN " + std::to_wstring(count) + L" chuyến.";
            case 3: return L"Các acc con chăm MAIN như chăm em bé: mớm đủ " + std::to_wstring(count) + L" lần rồi.";
            default:return L"MAIN lại há mồm nhận hàng: tổng cộng " + std::to_wstring(count) + L" lần tiếp tế.";
        }
    }

    static std::wstring FunnyDeathBurstText(const std::wstring& label, std::size_t deaths, std::uint64_t seed) {
        const std::wstring n = std::to_wstring(deaths);
        switch (telegram_logic::PhraseIndex(seed, 6)) {
            case 0: return L"Acc " + label + L" đang bị thằng mả mẹ nào hành chết " + n + L" lần trong 10 phút kìa anh, vào bem lại nó 😤";
            case 1: return label + L" vừa nằm sàn " + n + L" lần/10 phút. Thằng nào đang lấy nó làm bao cát vậy anh?";
            case 2: return label + L" chết " + n + L" mạng trong 10 phút rồi. Có mùi bị hội đồng, vào đòi công đạo thôi.";
            case 3: return L"Báo động nghĩa địa: " + label + L" ghé thăm Địa Phủ " + n + L" lần trong 10 phút 😭";
            case 4: return label + L" đang phát vé free kill à anh? " + n + L" lần chết/10 phút rồi.";
            default:return L"Thằng nào thương " + label + L" quá mà tiễn nó về làng " + n + L" lần/10 phút vậy?";
        }
    }

    static std::wstring FunnyAutoTrainOffText(const std::wstring& label, int minutes, std::uint64_t seed) {
        const std::wstring m = std::to_wstring(minutes);
        switch (telegram_logic::PhraseIndex(seed, 6)) {
            case 0: return L"Acc " + label + L" nằm chơi hơn " + m + L" phút rồi kìa anh, vào đá đít nó bật train lại đi 😆";
            case 1: return L"Báo động lười biếng: " + label + L" tắt Auto Train " + m + L" phút. Nó đang lĩnh lương mà ngồi uống trà rồi.";
            case 2: return label + L" đình công " + m + L" phút rồi anh ơi. Vào vặn tai nó cho đánh quái tiếp thôi.";
            case 3: return L"Con hàng " + label + L" đứng ngắm cảnh " + m + L" phút rồi. Auto Train vẫn OFF, xử lý nó cái anh 😅";
            case 4: return label + L" hình như xin nghỉ phép không lương: Auto Train OFF " + m + L" phút liên tục.";
            default:return L"Anh ơi, " + label + L" trốn việc " + m + L" phút rồi. Vào đạp nhẹ một phát cho nó train lại.";
        }
    }

    std::wstring BuildTelegramSummary(const std::wstring& reason, bool finalSession) const {
        const std::wstring nowText = LocalDateTimeText();
        const ULONGLONG now64 = GetTickCount64();
        const std::int64_t nowUnix = gold_history::UnixNow();
        constexpr std::int64_t kFreshToleranceSeconds = 45;
        constexpr std::int64_t kCoverageMaxGapSeconds = 90;

        const Account* main = nullptr;
        for (const auto& item : accounts_) {
            if (item && item->profile.tradeRole == kMainTradeRole) { main = item.get(); break; }
        }
        const std::wstring mainName = main && !main->displayName.empty() ? main->displayName : L"-";
        const std::wstring identity = main ? GoldIdentityFor(*main) : L"";

        bool currentKnown = false;
        std::int64_t currentRaw = 0;
        gold_history::Sample currentSample{};
        if (!identity.empty() && currentMainGoldKnown_ && currentMainGoldSample_.identity == identity &&
            std::llabs(nowUnix - currentMainGoldSample_.unixSeconds) <= kFreshToleranceSeconds) {
            currentSample = currentMainGoldSample_;
            currentKnown = true;
        } else if (!identity.empty() && goldHistoryReady_) {
            const auto latest = goldHistory_.Latest(identity);
            if (latest && std::llabs(nowUnix - latest->unixSeconds) <= kFreshToleranceSeconds) {
                currentSample = *latest;
                currentKnown = true;
            }
        }
        if (currentKnown) currentRaw = currentSample.boundMoneyRaw;

        const bool baselineKnown = currentKnown && sessionMainGoldBaselineKnown_ &&
            sessionMainGoldIdentity_ == identity;
        const std::int64_t baselineRaw = baselineKnown ? sessionMainGoldBaselineRaw_ : 0;

        bool hourKnown = false;
        std::int64_t hourOldRaw = 0;
        if (currentKnown && goldHistoryReady_ && nowUnix >= 3600) {
            const std::int64_t target60 = nowUnix - 3600;
            const auto old = goldHistory_.Nearest(identity, target60, kFreshToleranceSeconds);
            if (old && goldHistory_.HasContinuousCoverage(identity, target60, nowUnix,
                                                          kFreshToleranceSeconds, kCoverageMaxGapSeconds)) {
                hourOldRaw = old->boundMoneyRaw;
                hourKnown = true;
            }
        }

        const auto signedGold = [](std::int64_t value) {
            if (value >= 0) return L"+" + FormatInt64Grouped(value) + L"v";
            return L"-" + FormatInt64Grouped(-value) + L"v";
        };
        const auto durationVi = [](ULONGLONG ms) {
            const ULONGLONG totalMinutes = ms / 60000ULL;
            const ULONGLONG hours = totalMinutes / 60ULL;
            const ULONGLONG minutes = totalMinutes % 60ULL;
            if (hours > 0) return std::to_wstring(hours) + L" tiếng " + std::to_wstring(minutes) + L" phút";
            return std::to_wstring(totalMinutes) + L" phút";
        };
        const ULONGLONG sessionMs = telegramStats_.active && now64 >= telegramStats_.startedTick
            ? now64 - telegramStats_.startedTick : 0;

        std::wstring msg = finalSession ? L"📊 TỔNG KẾT SESSION" : L"📦 BÁO CÁO ĐỊNH KỲ";
        msg += L"\nMốc: " + reason + L"\nThời gian: " + nowText + L"\nMAIN: " + mainName;
        msg += L"\n\nSố vàng thay đổi trong 1 tiếng: ";
        if (hourKnown) {
            const std::int64_t hourDeltaGold = telegram_logic::WholeGoldDeltaFromRaw(currentRaw, hourOldRaw);
            msg += signedGold(hourDeltaGold) + L" (trong 60p vừa qua)";
        } else {
            msg += L"N/A (chưa đủ 60p dữ liệu)";
        }
        msg += L"\nTổng vàng tăng: ";
        if (baselineKnown) {
            const std::int64_t totalDeltaGold = telegram_logic::WholeGoldDeltaFromRaw(currentRaw, baselineRaw);
            msg += signedGold(totalDeltaGold);
        } else {
            msg += L"N/A";
        }
        msg += L" (trong " + durationVi(sessionMs) + L" vừa rồi, kể từ khi bật tool)";
        msg += L"\nMốc vàng cũ: ";
        if (baselineKnown) msg += FormatInt64Grouped(telegram_logic::WholeGoldFromRaw(baselineRaw)) + L"v";
        else msg += L"N/A";
        msg += L"\nMốc vàng hiện tại: ";
        if (currentKnown) msg += FormatInt64Grouped(telegram_logic::WholeGoldFromRaw(currentRaw)) + L"v";
        else msg += L"N/A";
        return msg;
    }

    bool SendTelegramSummary(const std::wstring& reason, bool finalSession, bool forceManual = false) {
        if (!telegramStats_.active && !forceManual) return false;
        if (forceManual) (void)PersistTelegramSettingsFromUi(false);
        (void)ReadMainGoldSample(true);
        const std::wstring msg = BuildTelegramSummary(reason, finalSession);
        AddLocalReport(finalSession ? L"SESSION SUMMARY" : L"PERIODIC SUMMARY", L"-", msg);
        const bool networkWanted = telegramSettings_.enabled && (forceManual || telegramSettings_.reportDeathCount || telegramSettings_.reportReceiveCount || telegramSettings_.reportSellCount);
        if (networkWanted) {
            (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg,
                                       finalSession ? L"SESSION SUMMARY" : L"SELL SUMMARY",
                                       L"-", forceManual);
        }
        ResetTelegramReportBaseline();
        return true;
    }

    void TelegramSendSummaryNow() {
        if (!telegramStats_.active) {
            if (!PersistTelegramSettingsFromUi(false)) return;
            const std::wstring msg = L"📦 BÁO CÁO THỦ CÔNG\nChưa có AUTO session đang chạy.\nThời gian: " + LocalDateTimeText();
            AddLocalReport(L"MANUAL SUMMARY", L"-", msg);
            (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"SELL SUMMARY", L"-", true);
            return;
        }
        (void)SendTelegramSummary(L"GỬI THỦ CÔNG", false, true);
    }

    void TelegramRecordSellEpisode(Account& a) {
        if(!telegramStats_.active)return;
        ++telegramStats_.sellTotal;++telegramStats_.sellsByPid[a.game.pid];
    }

    void TelegramRecordTradeCompleted(Account& main, Account& child, int receivedSlots, int passCount) {
        (void)main;(void)child;(void)receivedSlots;(void)passCount;
    }

    void TelegramRecordLauLanConfirm(Account& a, int attempt) {
        /* CP9 TELE clean: Lâu Lan confirm not counted */
        const std::wstring account = TelegramAccountLabel(a);
        const std::wstring msg = L"🚪 LÂU LAN XÁC NHẬN CỔNG\nAcc: " + account +
            L"\nMapID: 5 • AutoPath ON nhưng đứng >=3s\nLần XN watchdog: " + std::to_wstring(attempt) +
            L"\nThời gian: " + LocalDateTimeText();
        AddLocalReport(L"LÂU LAN XN", account, msg);
        return;
        (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"LÂU LAN XN", account);
    }

    static bool CriticalFreezeReason(const wchar_t* reason) {
        if (!reason) return false;
        const std::wstring r(reason);
        return r.find(L"timeout") != std::wstring::npos || r.find(L"Timeout") != std::wstring::npos ||
               r.find(L"ReadState") != std::wstring::npos || r.find(L"không phản hồi") != std::wstring::npos ||
               r.find(L"busy") != std::wstring::npos || r.find(L"Bridge") != std::wstring::npos;
    }

    void TelegramRecordCriticalFreeze(Account& a, const wchar_t* reason) {
        TelegramAccountWatch& watch = telegramWatch_[a.game.pid];
        if (watch.criticalFreezeNotified || !CriticalFreezeReason(reason)) return;
        watch.criticalFreezeNotified = true;
        /* CP9 TELE clean: freeze not counted */
        const std::wstring account = TelegramAccountLabel(a);
        const std::wstring msg = L"⚠️ CLIENT FREEZE\nAcc: " + account +
            L"\nLý do: " + std::wstring(reason ? reason : L"không rõ") +
            L"\nAutomation: fail-closed, không gửi action mới\nThời gian: " + LocalDateTimeText();
        AddLocalReport(L"CLIENT FREEZE", account, msg);
        return;
        (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"CLIENT FREEZE", account);
    }

    void TelegramRecordFreezeRecovered(Account& a) {
        TelegramAccountWatch& watch = telegramWatch_[a.game.pid];
        if (!watch.criticalFreezeNotified) return;
        watch.criticalFreezeNotified = false;
        const std::wstring account = TelegramAccountLabel(a);
        const std::wstring msg = L"✅ CLIENT RECOVERED\nAcc: " + account +
            L"\nClient đã ổn định lại >=2s\nThời gian: " + LocalDateTimeText();
        AddLocalReport(L"CLIENT RECOVER", account, msg);
        return;
        (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"CLIENT RECOVER", account);
    }

    void ObserveTelegramAccountState(Account& a, DWORD now) {
        if (!a.runtime.running || !a.snapshotValid) return;
        TelegramAccountWatch& watch = telegramWatch_[a.game.pid];
        const Snapshot& st = a.snapshot;
        if (st.validMask & ValidLifeState) {
            auto emitDeath = [&]() {
                watch.deathStartedTick = now;
                if (telegramStats_.active) ++telegramStats_.deathTotal;
                while (!watch.recentDeathTicks.empty() && Elapsed(now, watch.recentDeathTicks.front(), 10u * 60u * 1000u))
                    watch.recentDeathTicks.pop_front();
                watch.recentDeathTicks.push_back(now);
                const bool deathBurst = telegram_logic::IsDeathBurst(watch.recentDeathTicks.size());
                const std::wstring account = TelegramAccountLabel(a);
                if (!deathBurst) watch.deathBurstLastAlertTick = 0;
                if (deathBurst && watch.deathBurstLastAlertTick == 0) {
                    watch.deathBurstLastAlertTick = now;
                    const std::wstring msg = L"⚠️ CHẾT QUÁ 10 LẦN / 10 PHÚT\nAcc: " + account +
                        L"\nSố lần chết trong 10 phút: " + std::to_wstring(watch.recentDeathTicks.size()) +
                        L"\nThời gian: " + LocalDateTimeText();
                    AddLocalReport(L"DEATH BURST", account, msg);
                    if (telegramSettings_.enabled && telegramSettings_.notifyDeathBurst)
                        (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage,msg,L"DEATH BURST",account);
                }
            };
            auto emitRevived = [&]() {
                watch.deathStartedTick = 0;
            };

            const bool deadNow = st.dead != 0;
            if (!watch.lifeKnown) {
                watch.lifeKnown = true;
                watch.lastDead = deadNow;
                // Starting monitoring while the character is already dead is still a real
                // actionable condition; report it once instead of waiting for another death edge.
                if (deadNow) emitDeath();
            } else if (deadNow != watch.lastDead) {
                watch.lastDead = deadNow;
                if (deadNow) emitDeath();
                else emitRevived();
            }
        }

        const bool autoTrainJudgable =
            (st.validMask & ValidMapTransition) && (st.validMask & ValidLifeState) &&
            (st.validMask & ValidAutoFight) && (st.validMask & ValidAutoPath) &&
            st.mapReady && !st.waitingChangeMap && !st.dead && !st.autoPathing &&
            a.runtime.trainPositionMonitorArmed && !a.tradeHeld && !a.runtime.clientFreezeActive &&
            a.runtime.sellPhase == 0 && a.runtime.revivePhase == 0 && a.runtime.trainRecoveryPhase == 0;
        if (!autoTrainJudgable || st.autoFight) {
            watch.autoTrainOffStartedTick = 0;
            watch.autoTrainOffAlertSent = false;
        } else {
            if (watch.autoTrainOffStartedTick == 0) watch.autoTrainOffStartedTick = now;
            constexpr DWORD kAutoTrainOffAlertMs = 20u * 60u * 1000u;
            if (!watch.autoTrainOffAlertSent && Elapsed(now, watch.autoTrainOffStartedTick, kAutoTrainOffAlertMs)) {
                watch.autoTrainOffAlertSent = true;
                const int minutes = static_cast<int>((now - watch.autoTrainOffStartedTick) / 60000u);
                const std::wstring account = TelegramAccountLabel(a);
                const std::wstring msg = L"🛑 " + FunnyAutoTrainOffText(account, minutes,
                    GetTickCount64() ^ (static_cast<std::uint64_t>(a.game.pid) << 12));
                AddLocalReport(L"AUTO TRAIN OFF 20M", account, msg);
                if (telegramSettings_.enabled && telegramSettings_.notifyFunAlerts)
                    (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"AUTO TRAIN OFF 20M", account);
            }
        }

        if (a.runtime.tradeWorkflowEntrySeq != 0 && a.runtime.tradeWorkflowEntrySeq != watch.lastWorkflowTicket) {
            watch.lastWorkflowTicket = a.runtime.tradeWorkflowEntrySeq;
            /* CP9 TELE clean: FIFO not counted */
            std::size_t localPos = 0;
            for (std::size_t i = 0; i < tradeQueuePids_.size(); ++i)
                if (tradeQueuePids_[i] == a.game.pid) { localPos = i + 1; break; }
            AddLocalReport(L"FIFO ARRIVAL", TelegramAccountLabel(a),
                           L"Đã tới TỌA GD • vé #" +
                           std::to_wstring(a.runtime.tradeWorkflowEntrySeq) + L" • vị trí " +
                           std::to_wstring(localPos) + L"/4 • " + LocalDateTimeText());
            if (false) {
                std::size_t pos = 0;
                for (std::size_t i = 0; i < tradeQueuePids_.size(); ++i) if (tradeQueuePids_[i] == a.game.pid) { pos = i + 1; break; }
                const std::wstring posText = pos > 0 ? (L"#" + std::to_wstring(pos) + L"/4") : L"đã nhận workflow ticket";
                const std::wstring msg = L"🧳 CON ĐÃ TỚI TỌA GD → VÀO FIFO\nAcc: " + TelegramAccountLabel(a) +
                    L"\nVị trí FIFO: " + posText + L"\nVé workflow: #" + std::to_wstring(a.runtime.tradeWorkflowEntrySeq) +
                    L"\nThời gian: " + LocalDateTimeText();
                (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"FIFO ENTER", TelegramAccountLabel(a));
            }
        } else if (a.runtime.tradeWorkflowEntrySeq == 0) {
            watch.lastWorkflowTicket = 0;
        }

        const bool lifeOk = (st.validMask & ValidLifeState) && !st.dead;
        const bool worldFlowTravel = a.tradeHeld && !a.runtime.tradeTravelReady && lifeOk && st.mapReady && !st.waitingChangeMap;
        if (!worldFlowTravel) {
            watch.worldFlowTravelStartedTick = 0;
            watch.worldFlowTimeoutSent = false;
        } else {
            if (watch.worldFlowTravelStartedTick == 0) watch.worldFlowTravelStartedTick = now;
            const DWORD timeoutMs = static_cast<DWORD>(telegramSettings_.worldFlowTimeoutSec) * 1000u;
            if (!watch.worldFlowTimeoutSent && Elapsed(now, watch.worldFlowTravelStartedTick, timeoutMs)) {
                watch.worldFlowTimeoutSent = true;
                /* CP9 TELE clean: WorldFlow timeout not counted */
                const std::wstring localDetail = L"Đã đi TỌA GD >= " +
                    std::to_wstring(telegramSettings_.worldFlowTimeoutSec) + L" giây • M" +
                    std::to_wstring(st.mapID) + L" • " + std::to_wstring(st.x) + L"," +
                    std::to_wstring(st.y) + L" • workflow vẫn được giữ nguyên";
                AddLocalReport(L"WORLDFLOW TIMEOUT", TelegramAccountLabel(a), localDetail);
                if (false) {
                    const std::wstring msg = L"⚠️ WORLDFLOW TIMEOUT\nAcc: " + TelegramAccountLabel(a) +
                        L"\nĐã đi về TỌA GD >= " + std::to_wstring(telegramSettings_.worldFlowTimeoutSec) + L" giây nhưng chưa Ready\nMapID: " +
                        std::to_wstring(st.mapID) + L" • X,Y: " + std::to_wstring(st.x) + L"," + std::to_wstring(st.y) +
                        L"\nAutoPath: " + std::wstring((st.validMask & ValidAutoPath) && st.autoPathing ? L"ON" : L"OFF") +
                        L"\nChỉ cảnh báo Telegram; KHÔNG thay đổi workflow.\nThời gian: " + LocalDateTimeText();
                    (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, msg, L"WORLDFLOW TIMEOUT", TelegramAccountLabel(a));
                }
            }
        }
    }

    void TickTelegramSchedules(DWORD now) {
        if (!telegramStats_.active) return;
        // Fixed daily milestones win over interval when both land on the same minute,
        // preventing duplicate zero-delta reports (e.g. 12:00 and a 60-minute interval).
        if (telegramSettings_.dailyEnabled) {
            SYSTEMTIME st{}; GetLocalTime(&st);
            const int currentMinute = static_cast<int>(st.wHour) * 60 + st.wMinute;
            const std::uint64_t day = static_cast<std::uint64_t>(st.wYear) * 10000ULL + st.wMonth * 100ULL + st.wDay;
            std::set<int> seenTargetMinutes; // duplicate configured HH:MM values must never produce duplicate sends
            for (std::size_t i = 0; i < telegramSettings_.dailyTimes.size(); ++i) {
                int targetMinute = -1;
                if (!telegram_logic::ParseDailyTimeMinutes(telegramSettings_.dailyTimes[i], targetMinute) || currentMinute != targetMinute) continue;
                if (!seenTargetMinutes.insert(targetMinute).second) continue;
                const std::uint64_t key = day * 1440ULL + static_cast<std::uint64_t>(targetMinute + 1);
                if (telegramLastDailyKeys_[i] == key) continue;
                telegramLastDailyKeys_[i] = key;
                if (SendTelegramSummary(L"MỐC " + telegramSettings_.dailyTimes[i], false)) {
                    telegramLastIntervalSummaryTick_ = now;
                    return;
                }
            }
        }
        if (telegramSettings_.intervalEnabled) {
            const DWORD intervalMs = static_cast<DWORD>(telegramSettings_.intervalMinutes) * 60u * 1000u;
            if (telegramLastIntervalSummaryTick_ == 0) telegramLastIntervalSummaryTick_ = now;
            if (Elapsed(now, telegramLastIntervalSummaryTick_, intervalMs)) {
                telegramLastIntervalSummaryTick_ = now;
                (void)SendTelegramSummary(L"MỖI " + std::to_wstring(telegramSettings_.intervalMinutes) + L" PHÚT", false);
            }
        }
    }



    static std::wstring ProfileSection(const Snapshot& s, DWORD pid) {
        if ((s.validMask & ValidIdentity) && s.roleID > 0) return L"Role_" + std::to_wstring(s.roleID);
        return L"PID_" + std::to_wstring(pid);
    }

    static std::wstring PidProfileSection(DWORD pid) {
        return L"PID_" + std::to_wstring(pid);
    }

    static std::wstring LoadDesignatedMainIdentity() {
        return ReadIniText(L"Global", L"MainIdentity");
    }

    static void SaveDesignatedMainIdentity(const std::wstring& identity) {
        EnsureUnicodeIni();
        WriteIniText(L"Global", L"MainIdentity", identity);
        FlushIni();
    }

    void NormalizeAutomaticTradeRoles(const std::wstring& requestedMainIdentity) {
        std::vector<auto_role_logic::ClientKey> keys;
        keys.reserve(accounts_.size());
        std::wstring upgradedMainIdentity;
        int identityMatches = 0;
        for (const auto& item : accounts_) {
            if (!item) { keys.push_back({}); continue; }
            const Account& account = *item;
            const bool hasRoleId = account.snapshotValid &&
                (account.snapshot.validMask & ValidIdentity) && account.snapshot.roleID > 0;
            const std::wstring stable = ProfileSection(account.snapshot, account.game.pid);
            const std::wstring pidFallback = PidProfileSection(account.game.pid);
            const bool isMain = !requestedMainIdentity.empty() &&
                (requestedMainIdentity == stable || requestedMainIdentity == pidFallback);
            if (isMain) {
                ++identityMatches;
                if (hasRoleId && requestedMainIdentity != stable) upgradedMainIdentity = stable;
            }
            keys.push_back({hasRoleId ? static_cast<std::uint64_t>(account.snapshot.roleID) : 0u,
                            static_cast<std::uint32_t>(account.game.pid), hasRoleId, isMain});
        }
        if (identityMatches == 1 && !upgradedMainIdentity.empty())
            SaveDesignatedMainIdentity(upgradedMainIdentity);

        const std::vector<int> roles = auto_role_logic::AssignRuntimeRoles(
            keys, kMainTradeRole, kFirstChildTradeRole, kChildTradeCount);
        int overflow = 0;
        bool hasMain = false;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            if (!accounts_[i]) continue;
            Account& account = *accounts_[i];
            const int oldRole = account.profile.tradeRole;
            const bool oldSell = account.profile.enableSell;
            account.profile.tradeRole = roles[i];
            account.tradeHeld = false;
            if (account.profile.tradeRole == kMainTradeRole) {
                hasMain = true;
                account.profile.displayParty = 0;
                account.profile.partyKey = false;
                account.profile.enableSell = true;
            } else {
                account.profile.enableSell = false;
                account.runtime.sellPhase = 0;
                if (account.profile.tradeRole == kOverflowChildTradeRole) ++overflow;
            }
            if (oldRole != account.profile.tradeRole || oldSell != account.profile.enableSell)
                SaveProfile(account.profile);
        }
        if (!hasMain)
            Log(L"CHƯA CÓ MAIN online/chỉ định • không tự promote • workflow giao dịch bị chặn.");
        if (identityMatches > 1)
            Log(L"CẢNH BÁO MAIN: identity trùng nhiều client; chặn MAIN để fail-safe.");
        if (overflow > 0)
            Log(L"CẢNH BÁO: vượt 30 CON; " + std::to_wstring(overflow) +
                L" client dư vẫn là CON nhưng không được cấp child slot.");
    }

    static std::wstring DisplayName(const Snapshot& s, DWORD pid) {
        std::wstring name = s.characterName[0] ? s.characterName : L"?";
        if ((s.validMask & ValidIdentity) && s.roleID > 0) {
            return name + L" • " + std::to_wstring(s.roleID);
        }
        return name + L" • PID " + std::to_wstring(pid);
    }

    bool EnsureAttach(Account& a, std::wstring& error) {
        if (a.bridge.AttachedTo(a.game.pid)) return true;
        if (!IsWindow(a.game.window)) { error = L"Cửa sổ game đã mất"; return false; }
        return a.bridge.Attach(a.game, error);
    }

    bool ReadSnapshot(Account& a, std::wstring& error, DWORD timeout = 850) {
        if (!EnsureAttach(a, error)) return false;
        Response r{};
        if (!a.bridge.Call(Command::ReadState, 0, 0, 0, r, error, timeout)) return false;
        a.snapshot = r.snapshot;
        a.snapshotValid = true;
        return true;
    }

    void ScanClients() {
        ReleaseTradeHolds();
        tradeTxn_ = TradeTxn{};
        captureSlot_ = ClickSlot::None;
        capturePid_ = 0;
        for (auto& a : accounts_) a->bridge.Close();
        accounts_.clear();
        ListView_DeleteAllItems(clientList_);

        const auto found = FindClients();
        for (const auto& game : found) {
            auto a = std::make_unique<Account>();
            a->game = game;
            std::wstring error;
            if (a->bridge.Attach(game, error)) {
                Response r{};
                if (a->bridge.Call(Command::ReadState, 0, 0, 0, r, error, 1200)) {
                    a->snapshot = r.snapshot;
                    a->snapshotValid = true;
                }
            }
            if (!a->snapshotValid) {
                a->snapshot = {};
                a->displayName = L"? • PID " + std::to_wstring(game.pid);
                Log(L"PID " + std::to_wstring(game.pid) + L": chưa đọc được identity: " + error);
            } else {
                a->displayName = DisplayName(a->snapshot, game.pid);
            }
            a->profile = LoadProfile(ProfileSection(a->snapshot, game.pid));
            MigrateLegacySpot(a->profile);
            a->runtime.status = L"Đã dừng";
            accounts_.push_back(std::move(a));
        }

        std::wstring designatedMainIdentity = LoadDesignatedMainIdentity();
        if (designatedMainIdentity.empty()) {
            std::vector<std::wstring> legacyMainCandidates;
            for (const auto& item : accounts_) {
                if (item && ReadIniInt(item->profile.section, L"TradeRole", 0) == kMainTradeRole)
                    legacyMainCandidates.push_back(item->profile.section);
            }
            if (legacyMainCandidates.size() == 1) {
                designatedMainIdentity = legacyMainCandidates.front();
                SaveDesignatedMainIdentity(designatedMainIdentity);
                Log(L"MIGRATE MAIN: chuyển TradeRole=MAIN cũ sang Global/MainIdentity.");
            } else if (legacyMainCandidates.size() > 1) {
                Log(L"MIGRATE MAIN: phát hiện nhiều MAIN cũ; không tự chọn bừa.");
            }
        }
        NormalizeAutomaticTradeRoles(designatedMainIdentity);

        for (std::size_t i = 0; i < accounts_.size(); ++i) InsertAccountRow(static_cast<int>(i), *accounts_[i]);
        RefreshAccountGroups();
        RefreshSpotCombo();
        if (!accounts_.empty()) {
            ListView_SetItemState(clientList_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(clientList_, 0, FALSE);
            LoadSelectedProfileToUi();
        } else {
            ClearEditor();
        }
        Log(L"Quét thấy " + std::to_wstring(accounts_.size()) + L" client GameAssembly.dll.");
    }

    static constexpr int kClientGroupMain = 1000;
    static constexpr int kClientGroupPartyBase = 2000;
    static constexpr int kClientGroupUnassigned = 3000;

    int ClientGroupId(const Account& a) const {
        if (a.profile.tradeRole == kMainTradeRole) return kClientGroupMain;
        if (a.profile.displayParty >= 1 && a.profile.displayParty <= kChildTradeCount)
            return kClientGroupPartyBase + a.profile.displayParty;
        return kClientGroupUnassigned;
    }

    void InsertClientGroup(int groupId, const std::wstring& header, const std::wstring& task = L"") {
        LVGROUP group{}; group.cbSize = sizeof(group);
        group.mask = LVGF_GROUPID | LVGF_HEADER; group.iGroupId = groupId;
        group.pszHeader = const_cast<wchar_t*>(header.c_str());
        if (!task.empty()) { group.mask |= LVGF_TASK; group.pszTask = const_cast<wchar_t*>(task.c_str()); }
        ListView_InsertGroup(clientList_, -1, &group);
    }

    void RefreshAccountGroups() {
        if (!clientList_) return;
        ListView_EnableGroupView(clientList_, TRUE);
        ListView_RemoveAllGroups(clientList_);
        bool hasMain = false, hasUnassigned = false;
        std::array<bool, kChildTradeCount + 1> usedParty{};
        std::array<int, kChildTradeCount + 1> keyCount{};
        std::array<std::wstring, kChildTradeCount + 1> keyName{};
        for (const auto& item : accounts_) {
            const Account& a = *item;
            if (a.profile.tradeRole == kMainTradeRole) hasMain = true;
            else if (a.profile.displayParty >= 1 && a.profile.displayParty <= kChildTradeCount) {
                const std::size_t idx = static_cast<std::size_t>(a.profile.displayParty);
                usedParty[idx] = true;
                if (a.profile.partyKey) { ++keyCount[idx]; if (keyName[idx].empty()) keyName[idx] = a.displayName; }
            } else hasUnassigned = true;
        }
        if (hasMain) InsertClientGroup(kClientGroupMain, L"MAIN");
        for (int party = 1; party <= kChildTradeCount; ++party) if (usedParty[static_cast<std::size_t>(party)]) {
            const std::size_t idx = static_cast<std::size_t>(party);
            std::wstring header = L"PT" + std::to_wstring(party);
            if (keyCount[idx] == 1) header += L" • KEY: " + keyName[idx];
            else if (keyCount[idx] > 1) header += L" • KEY TRÙNG";
            InsertClientGroup(kClientGroupPartyBase + party, header, L"CHỌN PT" + std::to_wstring(party));
        }
        if (hasUnassigned) InsertClientGroup(kClientGroupUnassigned, L"CHƯA GÁN PT");
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            LVITEMW item{}; item.mask = LVIF_GROUPID; item.iItem = static_cast<int>(i); item.iGroupId = ClientGroupId(*accounts_[i]);
            ListView_SetItem(clientList_, &item);
        }
    }

    void SetAllAccountChecks(bool checked) {
        const int count = ListView_GetItemCount(clientList_);
        for (int i = 0; i < count; ++i) ListView_SetCheckState(clientList_, i, checked ? TRUE : FALSE);
    }

    void CheckPartyGroup(int groupId) {
        const int party = groupId - kClientGroupPartyBase;
        if (party < 1 || party > kChildTradeCount) return;
        for (std::size_t i = 0; i < accounts_.size(); ++i)
            if (accounts_[i]->profile.tradeRole != kMainTradeRole && accounts_[i]->profile.displayParty == party)
                ListView_SetCheckState(clientList_, static_cast<int>(i), TRUE);
    }

    void AssignPartyToChecked() {
        if (!partyCombo_) return;
        const LRESULT sel = SendMessageW(partyCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || sel > kChildTradeCount) return;
        const int party = static_cast<int>(sel);
        int changed = 0;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            if (!ListView_GetCheckState(clientList_, static_cast<int>(i))) continue;
            Account& a = *accounts_[i];
            if (a.profile.tradeRole == kMainTradeRole) continue;
            if (a.profile.displayParty == party) continue;
            a.profile.displayParty = party;
            a.profile.partyKey = false; // đổi PT thì bắt buộc chỉ định KEY lại, tránh KEY trùng ngầm.
            SaveProfile(a.profile); ++changed;
            UpdateAccountRow(static_cast<int>(i), a);
        }
        RefreshAccountGroups();
        Log(L"GÁN PT: " + std::to_wstring(changed) + L" acc → " + (party == 0 ? std::wstring(L"KHÔNG PT") : L"PT" + std::to_wstring(party)));
    }

    void TogglePidColumn() {
        pidExpanded_ = !pidExpanded_;
        ListView_SetColumnWidth(clientList_, 3, pidExpanded_ ? 62 : 24);
        std::wstring title = pidExpanded_ ? L"PID ◀" : L"▶";
        LVCOLUMNW column{}; column.mask = LVCF_TEXT; column.pszText = const_cast<wchar_t*>(title.c_str());
        ListView_SetColumn(clientList_, 3, &column);
    }

    void InsertAccountRow(int row, const Account& a) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(a.displayName.c_str());
        item.lParam = static_cast<LPARAM>(a.game.pid);
        ListView_InsertItem(clientList_, &item);
        UpdateAccountRow(row, a);
    }

    void SetRowText(int row, int sub, const std::wstring& text) {
        ListView_SetItemText(clientList_, row, sub, const_cast<wchar_t*>(text.c_str()));
    }

    std::wstring MainVisibleStatus(const Account& a) const {
        if (!a.runtime.running) return L"Đã dừng";
        const bool activeMain = tradeTxn_.mainPid == a.game.pid && tradeTxn_.phase != TradePhase::Idle;
        if (activeMain && tradeTxn_.phase == TradePhase::SellPause) {
            return L"Đang giao dịch với CON" + std::to_wstring(tradeTxn_.childSlot) +
                   L" • MAIN quota=0 • macro bán theo số item scan";
        }
        if (activeMain && (tradeTxn_.phase == TradePhase::Rendezvous ||
                           tradeTxn_.phase == TradePhase::TargetMain ||
                           tradeTxn_.phase == TradePhase::Sequence ||
                           tradeTxn_.phase == TradePhase::Cleanup)) {
            return L"Đang giao dịch với CON" + std::to_wstring(tradeTxn_.childSlot);
        }
        if (!tradeQueuePids_.empty()) {
            const bool needsCapacity = a.snapshotValid &&
                                       (a.snapshot.validMask & ValidBagSpace) &&
                                       MainNeedsCapacitySell(a.snapshot.freeBagSpace);
            if (a.runtime.sellPhase != 0 || needsCapacity)
                return L"CON đang đợi • MAIN QUOTA=0 • macro bán theo số item scan";
            return L"CON đã tới • chuẩn bị giao dịch";
        }
        return L"MAIN đứng chờ tại TỌA GD";
    }

    void UpdateAccountRow(int row, const Account& a) {
        SetRowText(row, 0, a.displayName);
        SetRowText(row, 1, TradeRoleLabel(a.profile.tradeRole));
        SetRowText(row, 2, a.profile.tradeRole == kMainTradeRole || a.profile.displayParty == 0 ? L"-" :
                   L"PT" + std::to_wstring(a.profile.displayParty) + (a.profile.partyKey ? L"★" : L""));
        SetRowText(row, 3, std::to_wstring(a.game.pid));
        if (a.profile.tradeRole == kMainTradeRole) {
            SetRowText(row, 4, (a.runtime.running ? L"RUN • " : L"STOP • ") + MainVisibleStatus(a));
        } else {
            SetRowText(row, 4, (a.runtime.running ? L"RUN • " : L"STOP • ") + a.runtime.status);
        }
        if (a.snapshotValid && (a.snapshot.validMask & (ValidMap | ValidPosition)) == (ValidMap | ValidPosition)) {
            std::wstring mapText = L"M" + std::to_wstring(a.snapshot.mapID) + L" • " +
                                   std::to_wstring(a.snapshot.x) + L"," + std::to_wstring(a.snapshot.y);
            if (a.snapshot.validMask & ValidBagSpace) mapText += L" • Trống " + std::to_wstring(a.snapshot.freeBagSpace);
            SetRowText(row, 5, mapText);
        } else {
            SetRowText(row, 5, L"?");
        }
        if (a.profile.tradeRole == kMainTradeRole) {
            SetRowText(row, 6, L"MAIN • đứng tại TỌA GD");
        } else if (gatherModeActive_ && IsGatherPid(a.game.pid) && gatherTarget_.valid) {
            SetRowText(row, 6, L"TẬP TRUNG • M" + std::to_wstring(gatherTarget_.mapID) + L" • " +
                             std::to_wstring(gatherTarget_.x) + L"," + std::to_wstring(gatherTarget_.y));
        } else if (a.profile.target.valid) {
            SetRowText(row, 6, a.profile.target.name + L" • M" + std::to_wstring(a.profile.target.mapID) +
                             L" • " + std::to_wstring(a.profile.target.x) + L"," + std::to_wstring(a.profile.target.y));
        } else {
            SetRowText(row, 6, L"CHƯA CHỌN BÃI");
        }
    }


    void ResolveProfileTarget(AccountProfile& p) {
        const int index = FindSpotIndex(spots_, p.selectedSpot);
        if (index >= 0) {
            p.target = spots_[static_cast<std::size_t>(index)];
            p.target.valid = true;
        } else {
            p.target = {};
        }
    }

    void MigrateLegacySpot(AccountProfile& p) {
        if (p.selectedSpot.empty() && p.target.valid) p.selectedSpot = p.target.name;
        if (p.target.valid && !p.selectedSpot.empty()) {
            int index = FindSpotIndex(spots_, p.selectedSpot);
            if (index >= 0) {
                const TargetProfile& existing = spots_[static_cast<std::size_t>(index)];
                if (existing.mapID != p.target.mapID || existing.x != p.target.x || existing.y != p.target.y) {
                    p.selectedSpot += L" [M" + std::to_wstring(p.target.mapID) + L" " +
                                      std::to_wstring(p.target.x) + L"," + std::to_wstring(p.target.y) + L"]";
                    index = FindSpotIndex(spots_, p.selectedSpot);
                }
            }
            if (index < 0) {
                TargetProfile migrated = p.target;
                migrated.name = p.selectedSpot;
                migrated.valid = true;
                spots_.push_back(std::move(migrated));
                SaveSharedSpots(spots_);
            }
        }
        ResolveProfileTarget(p);
    }

    void RefreshSpotCombo() {
        if (!spotCombo_) return;
        SendMessageW(spotCombo_, CB_RESETCONTENT, 0, 0);
        for (const auto& spot : spots_) {
            SendMessageW(spotCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(spot.name.c_str()));
        }
        Account* a = SelectedAccount();
        int select = -1;
        if (a) select = FindSpotIndex(spots_, a->profile.selectedSpot);
        SendMessageW(spotCombo_, CB_SETCURSEL, select, 0);
    }

    void SelectSharedSpotForAccount() {
        Account* a = SelectedAccount();
        if (!a) return;
        const LRESULT sel = SendMessageW(spotCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || static_cast<std::size_t>(sel) >= spots_.size()) return;
        const TargetProfile& spot = spots_[static_cast<std::size_t>(sel)];
        const std::wstring oldSpot = a->profile.selectedSpot;
        a->profile.selectedSpot = spot.name;
        a->profile.target = spot;
        SetText(targetName_, spot.name);
        SaveProfile(a->profile);
        if (_wcsicmp(oldSpot.c_str(), spot.name.c_str()) != 0) {
            if (a->runtime.running) BeginTrainRecovery(*a, GetTickCount());
        }
        LoadSelectedProfileToUi();
        const int row = SelectedIndex();
        if (row >= 0) UpdateAccountRow(row, *a);
        LogAccount(*a, L"Đã chọn bãi chung: " + spot.name + L" • M" + std::to_wstring(spot.mapID) + L" • " +
                       std::to_wstring(spot.x) + L"," + std::to_wstring(spot.y));
    }

    void LoadPartyBuildSettingsToUi() {
        for (std::size_t i = 0; i < partyBuildPointLabels_.size(); ++i) {
            if (partyBuildPointLabels_[i]) SetWindowTextW(partyBuildPointLabels_[i], PointDescription(partyBuildSettings_.clicks[i]).c_str());
            if (partyBuildDelayEdits_[i]) SetWindowTextW(partyBuildDelayEdits_[i], std::to_wstring(partyBuildSettings_.delaysMs[i]).c_str());
        }
        if (partyBuildTargetRetryEdit_) SetWindowTextW(partyBuildTargetRetryEdit_, std::to_wstring(partyBuildSettings_.targetRetry).c_str());
        if (partyBuildInviteRetryEdit_) SetWindowTextW(partyBuildInviteRetryEdit_, std::to_wstring(partyBuildSettings_.inviteRetry).c_str());
    }

    void PersistPartyBuildSettingsFromUi() {
        for (std::size_t i = 0; i < partyBuildDelayEdits_.size(); ++i) {
            if (partyBuildDelayEdits_[i]) partyBuildSettings_.delaysMs[i] = ParseEditInt(partyBuildDelayEdits_[i], partyBuildSettings_.delaysMs[i], 0, 60000);
        }
        if (partyBuildTargetRetryEdit_) partyBuildSettings_.targetRetry = party_build_logic::ClampRetry(ParseEditInt(partyBuildTargetRetryEdit_, partyBuildSettings_.targetRetry, 1, 20));
        if (partyBuildInviteRetryEdit_) partyBuildSettings_.inviteRetry = party_build_logic::ClampRetry(ParseEditInt(partyBuildInviteRetryEdit_, partyBuildSettings_.inviteRetry, 1, 20));
        SavePartyBuildSettings(partyBuildSettings_);
        LoadPartyBuildSettingsToUi();
    }

    void SetPartyKeyForSelected() {
        if (partyBuildModeActive_) { Log(L"TỰ TẠO PT đang chạy • không đổi KEY giữa workflow."); return; }
        Account* selected = SelectedAccount();
        if (!selected) { Log(L"ĐẶT KEY: hãy chọn một acc trong PT."); return; }
        if (selected->profile.tradeRole == kMainTradeRole || selected->profile.displayParty <= 0) {
            LogAccount(*selected, L"ĐẶT KEY: MAIN hoặc acc CHƯA GÁN PT không thể làm KEY.");
            return;
        }
        const int party = selected->profile.displayParty;
        int cleared = 0;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            if (a.profile.tradeRole == kMainTradeRole || a.profile.displayParty != party) continue;
            const bool newValue = (&a == selected);
            if (a.profile.partyKey != newValue) { a.profile.partyKey = newValue; SaveProfile(a.profile); ++cleared; }
            UpdateAccountRow(static_cast<int>(i), a);
        }
        RefreshAccountGroups();
        LogAccount(*selected, L"ĐẶT KEY PT" + std::to_wstring(party) + L" PASS • mỗi PT chỉ giữ đúng 1 KEY.");
        (void)cleared;
    }

    void BeginPartyBuildCapture(int index) {
        if (index < 0 || index >= 3) return;
        Account* a = SelectedAccount();
        if (!a) { Log(L"AUTO PT F8: hãy chọn một acc làm mẫu tọa client trước."); return; }
        shortcutPostTradeCapture_ = false; shortcutKunlunCaptureIndex_ = -1; 
        captureSlot_ = ClickSlot::None; captureTradeSequenceIndex_ = -1;
        captureTradeSequenceMode_ = 0; captureTradeSequenceMainRef_ = -1;
        partyBuildCaptureIndex_ = index; capturePid_ = a->game.pid;
        const wchar_t* label = index == 0 ? L"KEY CLICK 1" : (index == 1 ? L"KEY CLICK 2" : L"MẶT MEMBER");
        LogAccount(*a, std::wstring(L"AUTO PT: chờ F8 lấy ") + label + L".");
    }

    void TestPartyBuildClick(int index) {
        if (index < 0 || index >= 3) return;
        PersistPartyBuildSettingsFromUi();
        Account* a = SelectedAccount();
        if (!a) { Log(L"AUTO PT TEST: hãy chọn một acc."); return; }
        if (!partyBuildSettings_.clicks[static_cast<std::size_t>(index)].valid) { LogAccount(*a, L"AUTO PT TEST: tọa chưa được F8."); return; }
        std::wstring error;
        const wchar_t* label = index == 0 ? L"KEY CLICK 1" : (index == 1 ? L"KEY CLICK 2" : L"MEMBER FACE");
        if (!CoordinatorInternalPointAction(*a, partyBuildSettings_.clicks[static_cast<std::size_t>(index)], label, error))
            LogAccount(*a, std::wstring(L"AUTO PT TEST FAIL • ") + label + L" • " + error);
        else LogAccount(*a, std::wstring(L"AUTO PT TEST PASS • ") + label);
    }

    bool SelectedSharedSpot(TargetProfile& spot) const {
        if (!spotCombo_) return false;
        const LRESULT sel = SendMessageW(spotCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || static_cast<std::size_t>(sel) >= spots_.size()) return false;
        spot = spots_[static_cast<std::size_t>(sel)];
        return spot.valid && spot.mapID > 0;
    }

    void ApplySpotBulk(automation_bulk_logic::SpotScope scope) {
        TargetProfile spot{};
        if (!SelectedSharedSpot(spot)) {
            Log(L"ÁP BÃI: hãy chọn một Bãi trong combo trước.");
            return;
        }
        Account* selected = SelectedAccount();
        const int selectedParty = selected ? selected->profile.displayParty : 0;
        if (scope == automation_bulk_logic::SpotScope::Party && selectedParty <= 0) {
            Log(L"ÁP BÃI PT: hãy chọn một acc đã được gán PT trước.");
            return;
        }
        const DWORD now = GetTickCount();
        int changed = 0;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            const bool isMain = a.profile.tradeRole == kMainTradeRole;
            if (!automation_bulk_logic::EligibleForSpot(isMain, a.profile.displayParty, selectedParty, scope)) continue;
            const bool same = a.profile.target.valid &&
                              a.profile.target.mapID == spot.mapID && a.profile.target.x == spot.x && a.profile.target.y == spot.y &&
                              _wcsicmp(a.profile.selectedSpot.c_str(), spot.name.c_str()) == 0;
            a.profile.selectedSpot = spot.name;
            a.profile.target = spot;
            SaveProfile(a.profile);
            if (!same) {
                ++changed;
                if (a.runtime.running && !gatherModeActive_) BeginTrainRecovery(a, now);
            }
            UpdateAccountRow(static_cast<int>(i), a);
        }
        LoadSelectedProfileToUi();
        const std::wstring scopeText = scope == automation_bulk_logic::SpotScope::Party
            ? L"PT" + std::to_wstring(selectedParty) : L"ALL CON";
        Log(L"ÁP BÃI " + scopeText + L": " + std::to_wstring(changed) + L" acc → " + spot.name +
            L" • M" + std::to_wstring(spot.mapID) + L" • " + std::to_wstring(spot.x) + L"," + std::to_wstring(spot.y));
    }

    void ApplySpotToParty() { ApplySpotBulk(automation_bulk_logic::SpotScope::Party); }
    void ApplySpotToAllCon() { ApplySpotBulk(automation_bulk_logic::SpotScope::AllCon); }

    bool IsGatherPid(DWORD pid) const {
        return std::find(gatherPids_.begin(), gatherPids_.end(), pid) != gatherPids_.end();
    }

    void UpdateGatherLabel() {
        if (gatherToggleButton_) SetWindowTextW(gatherToggleButton_, gatherModeActive_ ? L"TẬP TRUNG: ON" : L"TẬP TRUNG: OFF");
        if (!gatherLabel_) return;
        if (!gatherTarget_.valid) {
            SetWindowTextW(gatherLabel_, L"CHƯA LẤY TỌA TẬP TRUNG");
            return;
        }
        const std::wstring text = L"M" + std::to_wstring(gatherTarget_.mapID) + L" • " +
                                  std::to_wstring(gatherTarget_.x) + L"," + std::to_wstring(gatherTarget_.y);
        SetWindowTextW(gatherLabel_, text.c_str());
    }

    void CaptureGatherTarget() {
        Account* a = SelectedAccount();
        if (!a) { Log(L"TỌA TẬP TRUNG: hãy chọn một acc đang đứng tại điểm cần lấy."); return; }
        std::wstring error;
        if (!EnsureAttach(*a, error)) { LogAccount(*a, L"TỌA TẬP TRUNG: attach fail • " + error); return; }
        if (!ReadSnapshot(*a, error, 1200)) { LogAccount(*a, L"TỌA TẬP TRUNG: không đọc được state • " + error); return; }
        const std::uint32_t need = ValidMap | ValidPosition;
        if ((a->snapshot.validMask & need) != need || !a->snapshot.mapReady || a->snapshot.waitingChangeMap) {
            LogAccount(*a, L"TỌA TẬP TRUNG: state Map/X/Y chưa ổn định.");
            return;
        }
        gatherTarget_ = TargetProfile{L"TẬP TRUNG", a->snapshot.mapID, a->snapshot.x, a->snapshot.y, true};
        SaveGatherTarget(gatherTarget_);
        UpdateGatherLabel();
        if (gatherModeActive_) {
            for (DWORD pid : gatherPids_) {
                if (Account* item = AccountByPid(pid)) {
                    ResetShortcutRoute(item->runtime);
                    item->runtime.routeOwnershipResetPending = true;
                }
            }
        }
        LogAccount(*a, L"ĐÃ LẤY TỌA TẬP TRUNG • M" + std::to_wstring(gatherTarget_.mapID) + L" • " +
                       std::to_wstring(gatherTarget_.x) + L"," + std::to_wstring(gatherTarget_.y));
    }

    void StopAllNormalAutomationForExclusiveMode(const wchar_t* reason) {
        const DWORD now = GetTickCount();
        const std::wstring why = reason ? reason : L"exclusive mode";
        if (tradeTxn_.phase != TradePhase::Idle) AbortTrade(why, now);
        else { ReleaseTradeHolds(); ResetTradeTxn(); }
        globalPaused_ = false;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            if (a.runtime.running || a.tradeHeld) StopAccount(a);
            a.tradeHeld = false;
            UpdateAccountRow(static_cast<int>(i), a);
        }
        Log(L"EXCLUSIVE MODE: STOP toàn bộ auto hiện tại trước khi chạy " + why + L".");
    }

    void StartGatherMode() {
        if (partyBuildModeActive_) StopPartyBuildMode(L"chuyển sang TẬP TRUNG");
        if (!gatherTarget_.valid) {
            MessageBoxW(hwnd_, L"Chưa có TỌA TẬP TRUNG. Chọn một acc đứng tại điểm cần tập trung rồi bấm LẤY TỌA TẬP TRUNG trước.",
                        kTitle, MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ThanLongLicenseActionAllowed()) {
            Log(L"LICENSE CORE GUARD • TẬP TRUNG bị chặn vì license không còn hợp lệ/fresh");
            return;
        }
        StopAllNormalAutomationForExclusiveMode(L"TẬP TRUNG");
        gatherPids_.clear();
        gatherModeActive_ = true;
        int started = 0;
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            const bool isMain = a.profile.tradeRole == kMainTradeRole;
            if (!automation_bulk_logic::EligibleForGather(isMain)) continue;
            std::wstring error;
            if (!EnsureAttach(a, error)) {
                LogAccount(a, L"TẬP TRUNG: bỏ qua vì attach fail • " + error);
                continue;
            }
            a.deathSessionLatched = false;
            a.tradeHeld = false;
            a.runtime.running = true;
            ResetRuntime(a.runtime);
            a.runtime.running = true;
            a.runtime.routeOwnershipResetPending = true;
            a.runtime.status = L"TẬP TRUNG • chuẩn hóa AutoPath rồi về điểm chung";
            gatherPids_.push_back(a.game.pid);
            ++started;
            UpdateAccountRow(static_cast<int>(i), a);
        }
        if (started == 0) {
            gatherModeActive_ = false;
            gatherPids_.clear();
            Log(L"TẬP TRUNG: không có acc hợp lệ ngoài MAIN để chạy.");
        } else {
            SetTradeStatus(L"TẬP TRUNG độc quyền • trade/scan/train thường đang STOP");
            Log(L"TẬP TRUNG ON: " + std::to_wstring(started) + L" acc ngoài MAIN → M" +
                std::to_wstring(gatherTarget_.mapID) + L" • " + std::to_wstring(gatherTarget_.x) + L"," + std::to_wstring(gatherTarget_.y));
        }
        UpdateGatherLabel();
    }

    void StopGatherMode() {
        if (!gatherModeActive_) return;
        gatherModeActive_ = false;
        const std::vector<DWORD> pids = gatherPids_;
        gatherPids_.clear();
        for (DWORD pid : pids) {
            Account* a = AccountByPid(pid);
            if (!a) continue;
            StopAccount(*a);
            for (std::size_t i = 0; i < accounts_.size(); ++i) if (accounts_[i].get() == a) { UpdateAccountRow(static_cast<int>(i), *a); break; }
        }
        SetTradeStatus(L"TẬP TRUNG OFF • tool IDLE • không tự START lại auto cũ");
        Log(L"TẬP TRUNG OFF • tất cả acc gather đã dừng; không tự START lại auto cũ.");
        UpdateGatherLabel();
    }

    void ToggleGatherMode() {
        if (gatherModeActive_) StopGatherMode();
        else StartGatherMode();
    }

    void TickGatherAccount(Account& a, DWORD now) {
        if (!gatherModeActive_ || !IsGatherPid(a.game.pid) || !a.runtime.running) return;
        if (!gatherTarget_.valid) { a.runtime.status = L"TẬP TRUNG • mất tọa đích • fail-closed"; return; }
        const Snapshot& s = a.snapshot;
        if (!s.mapReady || s.waitingChangeMap) { a.runtime.status = L"TẬP TRUNG • đang chuyển map / chờ xác nhận nếu route yêu cầu"; return; }
        const std::uint32_t need = ValidMap | ValidPosition | ValidRiding | ValidAutoPath | ValidAutoFight;
        if ((s.validMask & need) != need) { a.runtime.status = L"TẬP TRUNG • chờ Map/X/Y/Ngựa/Path/AutoFight authoritative"; return; }
        if (HandleDeath(a, now)) return;
        if (HandleRouteOwnershipReset(a, now)) return;
        if (HandleUnderworldAutoFightGuard(a, now)) return;

        State gatherLogic{};
        gatherLogic.valid = true; gatherLogic.mapReady = true; gatherLogic.waitingMap = false;
        gatherLogic.mapID = s.mapID; gatherLogic.x = s.x; gatherLogic.y = s.y;
        gatherLogic.riding = s.riding != 0; gatherLogic.autoPathing = s.autoPathing != 0;
        Target gatherWorld{gatherTarget_.mapID, gatherTarget_.x, gatherTarget_.y, a.profile.tolerance};
        const bool alreadyAtGather = AtTarget(gatherLogic, gatherWorld);

        // Gather is a temporary common training target, not a special movement shortcut.
        // Before leaving the current point use the same hard route contract as normal train:
        // AutoFight OFF -> mount -> AutoPath. HandleRobustTravel keeps shortcut/map-confirm logic intact.
        if (!alreadyAtGather && s.autoFight) {
            if (!EnsureAutoFightOffForTravel(a, now, L"TẬP TRUNG • tắt AutoFight trước khi rời điểm hiện tại")) {
                a.runtime.status = L"TẬP TRUNG • tắt AutoFight trước → sau đó mới lên ngựa/AutoPath";
                return;
            }
        }

        bool arrived = false;
        (void)HandleRobustTravel(a, now, gatherTarget_, L"bãi tập trung", arrived);
        if (!arrived) return;

        // At the shared target, behave like a normal training spot: enable AutoFight with
        // the existing P3 AUTO->Đánh quái sequence (when that account has enableFight).
        if (HandleFightClicks(a, now)) {
            if ((a.snapshot.validMask & ValidAutoFight) && a.snapshot.autoFight)
                a.runtime.status = L"TẬP TRUNG • ĐÚNG BÃI CHUNG • AutoFight ON • tiếp tục check tọa";
            return;
        }
        a.runtime.status = L"TẬP TRUNG • ĐÚNG BÃI CHUNG • tiếp tục check tọa";
    }

    void UpdatePartyBuildToggleLabel() {
        if (partyBuildToggleButton_) SetWindowTextW(partyBuildToggleButton_, partyBuildModeActive_ ? L"TỰ TẠO PT: ON" : L"TỰ TẠO PT: OFF");
    }

    void PartyAdvanceMember(PartyBuildSession& s, DWORD now) {
        ++s.memberIndex;
        s.targetAttempts = 0; s.inviteAttempts = 0;
        s.phase = PartyBuildPhase::MemberTarget;
        s.phaseStartedTick = now; s.nextTick = now + 250;
    }

    void PartyFailMember(PartyBuildSession& s, const std::wstring& name, const std::wstring& reason, DWORD now) {
        ++s.failed;
        s.failures.push_back(name + L": " + reason);
        PartyAdvanceMember(s, now);
    }

    void PartyFinishSession(PartyBuildSession& s, DWORD now, const std::wstring& fatal = L"") {
        if (!fatal.empty()) { ++s.failed; s.failures.push_back(L"KEY: " + fatal); }
        s.phase = PartyBuildPhase::Done; s.nextTick = 0; s.phaseStartedTick = now;
        if (Account* key = AccountByPid(s.keyPid)) {
            key->runtime.running = false;
            key->runtime.status = fatal.empty() ? L"AUTO PT • PT đã xử lý xong" : L"AUTO PT FAIL • " + fatal;
        }
    }

    std::wstring PartySessionSummary(const PartyBuildSession& s) const {
        Account* key = const_cast<App*>(this)->AccountByPid(s.keyPid);
        const std::wstring keyName = key ? key->displayName : L"KEY đã mất";
        const bool pass = s.failed == 0;
        std::wstring line = L"PT" + std::to_wstring(s.party) + (pass ? L": PASS" : L": FAIL") +
                            L" • KEY: " + keyName + L" • Đã mời " + std::to_wstring(s.invited) + L"/" +
                            std::to_wstring(s.memberPids.size());
        for (const auto& fail : s.failures) line += L"\r\n   - " + fail;
        return line;
    }

    void CompletePartyBuildIfDone() {
        if (!partyBuildModeActive_) return;
        for (const auto& s : partyBuildSessions_) if (s.phase != PartyBuildPhase::Done) return;
        std::wstring detail = L"AUTO PT CHI TIẾT";
        bool allPass = true;
        for (const auto& line : partyBuildPreflightSummary_) { detail += L"\r\n" + line; allPass = false; }
        for (const auto& s : partyBuildSessions_) {
            detail += L"\r\n" + PartySessionSummary(s);
            if (s.failed != 0) allPass = false;
        }
        Log(detail);
        partyBuildModeActive_ = false;
        partyBuildSessions_.clear();
        for (auto& item : accounts_) if (item->runtime.running) StopAccount(*item);
        SetTradeStatus(L"TỰ TẠO PT OFF • tool IDLE • không tự START lại auto cũ");
        Log(L"TỰ TẠO PT OFF • hoàn tất; tool IDLE; không tự START lại auto cũ.");
        UpdatePartyBuildToggleLabel();
        MessageBoxW(hwnd_, allPass ? L"XONG" : L"CHƯA XONG", L"TỰ TẠO PT",
                    MB_OK | (allPass ? MB_ICONINFORMATION : MB_ICONWARNING));
    }

    void StopPartyBuildMode(const std::wstring& reason, bool popup = false) {
        if (!partyBuildModeActive_) return;
        partyBuildModeActive_ = false;
        partyBuildSessions_.clear();
        for (auto& item : accounts_) if (item->runtime.running) StopAccount(*item);
        SetTradeStatus(L"TỰ TẠO PT OFF • tool IDLE");
        Log(L"TỰ TẠO PT DỪNG • " + reason + L" • không tự START auto cũ.");
        UpdatePartyBuildToggleLabel();
        if (popup) MessageBoxW(hwnd_, reason.c_str(), L"TỰ TẠO PT", MB_OK | MB_ICONWARNING);
    }

    void StartPartyBuildMode() {
        if (!ThanLongLicenseActionAllowed()) { Log(L"LICENSE CORE GUARD • TỰ TẠO PT bị chặn."); return; }
        PersistPartyBuildSettingsFromUi();
        for (std::size_t i = 0; i < partyBuildSettings_.clicks.size(); ++i) {
            if (!partyBuildSettings_.clicks[i].valid) {
                MessageBoxW(hwnd_, L"Thiếu tọa AUTO PT trong DEVELOPER. Cần F8 đủ KEY CLICK 1, KEY CLICK 2 và MẶT MEMBER trước.", kTitle, MB_OK | MB_ICONWARNING);
                return;
            }
        }
        if (gatherModeActive_) StopGatherMode();
        StopAllNormalAutomationForExclusiveMode(L"TỰ TẠO PT");
        partyBuildPreflightSummary_.clear();
        partyBuildSessions_.clear();

        for (int party = 1; party <= kChildTradeCount; ++party) {
            std::vector<Account*> members;
            std::vector<Account*> keys;
            for (auto& item : accounts_) {
                Account& a = *item;
                if (a.profile.tradeRole == kMainTradeRole || a.profile.displayParty != party) continue;
                members.push_back(&a);
                if (party_build_logic::CanBePartyKey(false, a.profile.displayParty, a.profile.partyKey)) keys.push_back(&a);
            }
            if (members.empty()) continue;
            if (keys.size() != 1) {
                partyBuildPreflightSummary_.push_back(L"PT" + std::to_wstring(party) + L": FAIL • cần đúng 1 KEY, hiện có " + std::to_wstring(keys.size()));
                continue;
            }
            Account& key = *keys.front();
            std::wstring error;
            if (!EnsureAttach(key, error)) {
                partyBuildPreflightSummary_.push_back(L"PT" + std::to_wstring(party) + L": FAIL • KEY attach: " + error);
                continue;
            }
            ResetRuntime(key.runtime);
            key.runtime.running = true;
            key.runtime.status = L"AUTO PT • KEY chuẩn bị CLICK 1";
            PartyBuildSession s{}; s.party = party; s.keyPid = key.game.pid; s.phaseStartedTick = GetTickCount(); s.nextTick = GetTickCount();
            for (Account* member : members) if (member != &key) s.memberPids.push_back(member->game.pid);
            partyBuildSessions_.push_back(std::move(s));
        }

        if (partyBuildSessions_.empty()) {
            Log(L"AUTO PT CHI TIẾT • không có PT hợp lệ để chạy.");
            for (const auto& line : partyBuildPreflightSummary_) Log(line);
            SetTradeStatus(L"TỰ TẠO PT OFF • tool IDLE");
            MessageBoxW(hwnd_, L"CHƯA XONG", L"TỰ TẠO PT", MB_OK | MB_ICONWARNING);
            UpdatePartyBuildToggleLabel();
            return;
        }
        partyBuildModeActive_ = true;
        UpdatePartyBuildToggleLabel();
        SetTradeStatus(L"TỰ TẠO PT độc quyền • mọi auto khác STOP");
        Log(L"TỰ TẠO PT ON • " + std::to_wstring(partyBuildSessions_.size()) + L" KEY chạy độc lập; các auto khác đã STOP.");
    }

    void TogglePartyBuildMode() {
        if (partyBuildModeActive_) StopPartyBuildMode(L"tắt bằng nút TỰ TẠO PT");
        else StartPartyBuildMode();
    }

    void TickPartyBuildSession(PartyBuildSession& s, DWORD now) {
        if (s.phase == PartyBuildPhase::Done || static_cast<LONG>(now - s.nextTick) < 0) return;
        Account* key = AccountByPid(s.keyPid);
        if (!key || !IsWindow(key->game.window)) { PartyFinishSession(s, now, L"KEY/cửa sổ đã mất"); return; }
        if (!key->snapshotValid || (key->snapshot.validMask & ValidIdentity) == 0 || key->snapshot.roleID <= 0) {
            if (Elapsed(now, s.phaseStartedTick, 6000)) PartyFinishSession(s, now, L"KEY không có RoleID/state sau 6s");
            return;
        }
        if ((key->snapshot.validMask & ValidLifeState) && key->snapshot.dead) { PartyFinishSession(s, now, L"KEY đang chết; Auto PT không tự chạy revive"); return; }

        auto click = [&](int index, const wchar_t* label) -> bool {
            std::wstring error;
            if (!CoordinatorInternalPointAction(*key, partyBuildSettings_.clicks[static_cast<std::size_t>(index)], label, error)) {
                PartyFinishSession(s, now, std::wstring(label) + L" FAIL • " + error);
                return false;
            }
            return true;
        };

        if (s.phase == PartyBuildPhase::KeyClick1) {
            if (!click(0, L"AUTO PT KEY CLICK 1")) return;
            key->runtime.status = L"AUTO PT • KEY CLICK 1 PASS";
            s.phase = PartyBuildPhase::KeyClick2; s.phaseStartedTick = now; s.nextTick = now + static_cast<DWORD>(partyBuildSettings_.delaysMs[0]);
            return;
        }
        if (s.phase == PartyBuildPhase::KeyClick2) {
            if (!click(1, L"AUTO PT KEY CLICK 2")) return;
            key->runtime.status = L"AUTO PT • KEY CLICK 2 PASS • bắt đầu target member";
            s.phase = PartyBuildPhase::MemberTarget; s.phaseStartedTick = now; s.nextTick = now + static_cast<DWORD>(partyBuildSettings_.delaysMs[1]);
            return;
        }
        if (s.memberIndex >= s.memberPids.size()) { PartyFinishSession(s, now); return; }

        Account* member = AccountByPid(s.memberPids[s.memberIndex]);
        const std::wstring memberName = member ? member->displayName : L"member đã mất";
        if (!member || !IsWindow(member->game.window)) { PartyFailMember(s, memberName, L"cửa sổ/member đã mất", now); return; }
        if (!member->snapshotValid || (member->snapshot.validMask & ValidIdentity) == 0 || member->snapshot.roleID <= 0) {
            std::wstring readError;
            if (!ReadSnapshot(*member, readError, 900)) { PartyFailMember(s, memberName, L"không đọc được RoleID • " + readError, now); return; }
        }
        const int memberRole = member->snapshot.roleID;
        const int keyRole = key->snapshot.roleID;
        if (!party_build_logic::IsInviteMember(member->profile.tradeRole == kMainTradeRole, member->profile.displayParty, s.party, memberRole, keyRole)) {
            PartyFailMember(s, memberName, L"RoleID/PT không hợp lệ hoặc trùng KEY", now); return;
        }

        if (s.phase == PartyBuildPhase::MemberTarget) {
            std::wstring error; Response r{};
            const bool ok = key->bridge.Call(Command::SelectTargetByRoleID, memberRole, 1, 0, r, error, 1800);
            if (ok && r.resultCode == static_cast<std::int32_t>(ActionResult::ActionInvoked) && r.value0 == memberRole) {
                key->runtime.status = L"AUTO PT • target PASS " + memberName + L" • chuẩn bị click mặt";
                s.phase = PartyBuildPhase::MemberFace; s.phaseStartedTick = now; s.nextTick = now + 150; s.targetAttempts = 0;
                return;
            }
            ++s.targetAttempts;
            if (s.targetAttempts >= partyBuildSettings_.targetRetry) {
                PartyFailMember(s, memberName, L"target RoleID fail sau " + std::to_wstring(s.targetAttempts) + L" lần • " + error, now);
                return;
            }
            key->runtime.status = L"AUTO PT • target " + memberName + L" • retry " + std::to_wstring(s.targetAttempts);
            s.nextTick = now + 650;
            return;
        }
        if (s.phase == PartyBuildPhase::MemberFace) {
            if (!click(2, L"AUTO PT MEMBER FACE")) return;
            key->runtime.status = L"AUTO PT • MEMBER FACE PASS • chờ menu Mời vào nhóm";
            s.phase = PartyBuildPhase::MemberInvite; s.phaseStartedTick = now; s.nextTick = now + static_cast<DWORD>(partyBuildSettings_.delaysMs[2]);
            // Do not reset inviteAttempts here: retry path deliberately re-enters MemberFace
            // so each semantic miss gets a fresh popup-open click without losing the retry budget.
            return;
        }
        if (s.phase == PartyBuildPhase::MemberInvite) {
            std::wstring error; Response r{};
            const bool ok = key->bridge.Call(Command::ClickTravelSemantic, static_cast<int>(TravelSemantic::InviteParty), 0, 0, r, error, 2200);
            if (ok && r.resultCode == static_cast<std::int32_t>(ActionResult::ActionInvoked)) {
                ++s.invited;
                LogAccount(*key, L"AUTO PT PASS • " + memberName + L" • callback Mời vào nhóm • " + r.detail);
                PartyAdvanceMember(s, now);
                return;
            }
            ++s.inviteAttempts;
            if (s.inviteAttempts >= partyBuildSettings_.inviteRetry) {
                PartyFailMember(s, memberName, L"không callback được 'Mời vào nhóm' sau " + std::to_wstring(s.inviteAttempts) + L" lần • " + error, now);
                return;
            }
            key->runtime.status = L"AUTO PT • retry mở lại menu member • lần " + std::to_wstring(s.inviteAttempts);
            s.phase = PartyBuildPhase::MemberFace;
            s.phaseStartedTick = now;
            s.nextTick = now + 350;
            return;
        }
    }

    void TickPartyBuild(DWORD now) {
        if (!partyBuildModeActive_) return;
        for (auto& s : partyBuildSessions_) TickPartyBuildSession(s, now);
        CompletePartyBuildIfDone();
    }

    void DeleteSelectedSharedSpot() {
        Account* a = SelectedAccount();
        if (!a) { Log(L"Chưa chọn acc"); return; }
        const LRESULT sel = SendMessageW(spotCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || static_cast<std::size_t>(sel) >= spots_.size()) {
            Log(L"Chưa chọn bãi chung để xóa");
            return;
        }
        const std::wstring name = spots_[static_cast<std::size_t>(sel)].name;
        spots_.erase(spots_.begin() + sel);
        SaveSharedSpots(spots_);
        for (auto& item : accounts_) {
            if (_wcsicmp(item->profile.selectedSpot.c_str(), name.c_str()) == 0)
                item->profile.selectedSpot.clear();
            ResolveProfileTarget(item->profile);
            SaveProfile(item->profile);
        }
        RefreshSpotCombo();
        LoadSelectedProfileToUi();
        for (std::size_t i = 0; i < accounts_.size(); ++i) UpdateAccountRow(static_cast<int>(i), *accounts_[i]);
        Log(L"Đã xóa bãi chung: " + name);
    }

    int FocusedSelectedRow(HWND list) const {
        if (!list) return -1;
        const int focused = ListView_GetNextItem(list, -1, LVNI_FOCUSED);
        if (focused >= 0 && (ListView_GetItemState(list, focused, LVIS_SELECTED) & LVIS_SELECTED) != 0) return focused;
        return ListView_GetNextItem(list, -1, LVNI_SELECTED);
    }

    std::vector<int> SelectedRows(HWND list) const {
        std::vector<int> rows;
        if (!list) return rows;
        int row = -1;
        while ((row = ListView_GetNextItem(list, row, LVNI_SELECTED)) >= 0) rows.push_back(row);
        return rows;
    }

    void CopyClicksFromAnotherAccount() {
        Account* target = SelectedAccount();
        if (!target) { Log(L"LẤY 3 CLICK: chưa chọn acc đích."); return; }
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        std::vector<Account*> sources;
        for (auto& item : accounts_) {
            Account* source = item.get();
            if (!source || source->game.pid == target->game.pid) continue;
            int valid = 0;
            for (int i : {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)})
                if (source->profile.points[static_cast<std::size_t>(i)].valid) ++valid;
            if (valid == 0) continue;
            sources.push_back(source);
            const std::wstring label = AccountTag(*source) + L" • có " + std::to_wstring(valid) + L"/3 điểm";
            AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(6000 + sources.size() - 1), label.c_str());
        }
        if (sources.empty()) {
            DestroyMenu(menu);
            LogAccount(*target, L"LẤY 3 CLICK: chưa có acc khác nào đã gán tọa độ.");
            return;
        }
        POINT screen{}; GetCursorPos(&screen);
        const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                       screen.x, screen.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        if (cmd < 6000 || static_cast<std::size_t>(cmd - 6000) >= sources.size()) return;
        Account* source = sources[static_cast<std::size_t>(cmd - 6000)];
        int copied = 0;
        for (int i : {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)}) {
            if (!source->profile.points[static_cast<std::size_t>(i)].valid) continue;
            target->profile.points[static_cast<std::size_t>(i)] = source->profile.points[static_cast<std::size_t>(i)];
            ++copied;
        }
        SaveProfile(target->profile);
        LoadSelectedProfileToUi();
        LogAccount(*target, L"Đã lấy " + std::to_wstring(copied) + L"/3 CLICK từ " + AccountTag(*source) +
                           L" • điểm nguồn chưa gán không ghi đè điểm hiện tại.");
    }

    bool RecorderModeIsTrade(RecorderMode mode) const {
        return mode == RecorderMode::TradeMain || mode == RecorderMode::TradeChild;
    }

    int RecordedDelay(std::size_t index, int lastDefault) const {
        if (index + 1 >= recorderClicks_.size()) return lastDefault;
        const DWORD delta = recorderClicks_[index + 1].tick - recorderClicks_[index].tick;
        return std::clamp(static_cast<int>(delta), 50, 60000);
    }

    void UpdateRecorderUi(const std::wstring& status = L"") {
        const bool active = recorderMode_ != RecorderMode::None;
        if (tradeRecordButton_) SetWindowTextW(tradeRecordButton_, active && RecorderModeIsTrade(recorderMode_) ? L"DỪNG REC" : L"REC");
        std::wstring text = status;
        if (text.empty()) text = active ? L"REC đang ghi thao tác tay..." : L"REC: sẵn sàng";
        if (tradeRecordStatus_) SetWindowTextW(tradeRecordStatus_, text.c_str());
    }

    Account* RecorderAccountAtPoint(const POINT& screen) {
        HWND hit = WindowFromPoint(screen);
        HWND root = hit ? GetAncestor(hit, GA_ROOT) : nullptr;
        for (auto& item : accounts_) if (item && item->game.window == root) return item.get();
        return nullptr;
    }

    bool RecorderAllowsAccount(const Account& account) const {
        if (recorderMode_ == RecorderMode::TradeMain) return account.profile.tradeRole == 1;
        if (recorderMode_ == RecorderMode::TradeChild) {
            return account.game.pid == recorderPrimaryPid_ || account.profile.tradeRole == 1;
        }
        return false;
    }

    void CaptureRecorderClick() {
        if (recorderMode_ == RecorderMode::None || recorderClicks_.size() >= 64) return;
        POINT screen{};
        if (!GetCursorPos(&screen)) return;
        Account* account = RecorderAccountAtPoint(screen);
        if (!account || !RecorderAllowsAccount(*account)) return; // Click on tool/other apps/other CON is intentionally ignored.
        POINT client = screen;
        if (!ScreenToClient(account->game.window, &client)) return;
        RECT rc{};
        if (!GetClientRect(account->game.window, &rc)) return;
        const int width = rc.right - rc.left, height = rc.bottom - rc.top;
        if (width <= 0 || height <= 0 || client.x < 0 || client.y < 0 || client.x >= width || client.y >= height) return;
        RecordedClick click{};
        click.pid = account->game.pid;
        click.point = ClickPoint{client.x, client.y, width, height, true};
        click.tick = GetTickCount();
        recorderClicks_.push_back(click);
        const std::wstring status = L"REC • " + std::to_wstring(recorderClicks_.size()) + L" click • vừa ghi " +
                                    AccountTag(*account) + L" @ " + PointDescription(click.point);
        UpdateRecorderUi(status);
        SetTradeStatus(L"RECORDING • FREEZE AUTO • " + status);
    }

    void PollRecorder() {
        if (recorderMode_ == RecorderMode::None) return;
        const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (down) {
            recorderMouseDown_ = true;
            return;
        }
        if (recorderMouseDown_) {
            recorderMouseDown_ = false;
            CaptureRecorderClick();
        }
    }

    int FindSharedMainStepByPoint(const ClickPoint& point) const {
        for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) {
            const ClickPoint& p = mainTradeSequence_[i].point;
            if (p.valid && p.x == point.x && p.y == point.y && p.baseW == point.baseW && p.baseH == point.baseH) return static_cast<int>(i);
        }
        return -1;
    }


    void CommitRecordedTradeMain() {
        Account* main = AccountByTradeRole(1);
        if (!main) return;
        const std::size_t room = mainTradeSequence_.size() < 64 ? 64 - mainTradeSequence_.size() : 0;
        const std::size_t count = std::min(room, recorderClicks_.size());
        const std::size_t first = mainTradeSequence_.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (recorderClicks_[i].pid != main->game.pid) continue;
            TradeSequenceStep step{};
            step.target = 1; step.mainRef = static_cast<int>(mainTradeSequence_.size());
            step.description = L"REC MAIN bước " + std::to_wstring(mainTradeSequence_.size() + 1);
            step.point = recorderClicks_[i].point;
            step.delayMs = RecordedDelay(i, 500); step.repeat = 1;
            mainTradeSequence_.push_back(step);
        }
        for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) mainTradeSequence_[i].mainRef = static_cast<int>(i);
        SaveMainTradeSequence();
        RefreshTradeSequenceList();
        PopulateTradeTargetCombo();
        if (mainTradeSequence_.size() > first && tradeSeqList_) {
            const int row = static_cast<int>(first);
            ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(tradeSeqList_, row, FALSE);
            LoadTradeSequenceRowToEditor(row);
        }
        LogAccount(*main, L"REC CHUỖI GD MAIN → click đã được chuyển thành thư viện MAIN editable dùng chung.");
    }

    void CommitRecordedTradeChild(DWORD childPid) {
        Account* child = AccountByPid(childPid);
        Account* main = AccountByTradeRole(1);
        if (!child || child->profile.tradeRole < 2 || !main) return;
        EnsureSharedChildTradeSequence();
        const std::size_t first = childTradeSequence_.size();
        for (std::size_t i = 0; i < recorderClicks_.size() && childTradeSequence_.size() < 64; ++i) {
            const RecordedClick& click = recorderClicks_[i];
            TradeSequenceStep row{};
            row.repeat = 1;
            if (click.pid == child->game.pid) {
                row.target = 0; row.mainRef = -1;
                row.description = L"REC ACC CON bước " + std::to_wstring(childTradeSequence_.size() + 1);
                row.point = click.point; row.delayMs = RecordedDelay(i, 500);
            } else if (click.pid == main->game.pid) {
                int ref = FindSharedMainStepByPoint(click.point);
                if (ref < 0) {
                    if (mainTradeSequence_.size() >= 64) {
                        LogAccount(*child, L"REC bỏ qua click MAIN mới vì CHUỖI GD MAIN đã đủ 64 dòng.");
                        continue;
                    }
                    TradeSequenceStep shared{};
                    shared.target = 1; shared.mainRef = static_cast<int>(mainTradeSequence_.size());
                    shared.description = L"REC MAIN bước " + std::to_wstring(mainTradeSequence_.size() + 1);
                    shared.point = click.point; shared.delayMs = RecordedDelay(i, 500); shared.repeat = 1;
                    mainTradeSequence_.push_back(shared);
                    ref = static_cast<int>(mainTradeSequence_.size() - 1);
                }
                row.target = 1; row.mainRef = ref;
            } else continue;
            childTradeSequence_.push_back(row);
        }
        for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) mainTradeSequence_[i].mainRef = static_cast<int>(i);
        SaveMainTradeSequence();
        SaveSharedChildTradeSequence();
        RefreshTradeSequenceList(); PopulateTradeTargetCombo();
        if (childTradeSequence_.size() > first && tradeSeqList_) {
            const int row = static_cast<int>(first);
            ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(tradeSeqList_, row, FALSE); LoadTradeSequenceRowToEditor(row);
        }
        LogAccount(*child, L"REC CHUỖI GD ACC CON dùng chung → click trên " + TradeRoleLabel(child->profile.tradeRole) +
                           L" được lưu cho mọi CON; click MAIN vẫn tham chiếu CHUỖI GD MAIN.");
    }

    bool RecorderBlocksAccount(const Account& a) const {
        if (recorderMode_ == RecorderMode::None) return false;
        if (recorderMode_ == RecorderMode::TradeMain) {
            const Account* main = const_cast<App*>(this)->AccountByTradeRole(1);
            return main && a.game.pid == main->game.pid;
        }
        if (recorderMode_ == RecorderMode::TradeChild) {
            const Account* main = const_cast<App*>(this)->AccountByTradeRole(1);
            return a.game.pid == recorderPrimaryPid_ || (main && a.game.pid == main->game.pid);
        }
        return false;
    }

    void StopRecorder(bool commit) {
        if (recorderMode_ == RecorderMode::None) return;
        const RecorderMode mode = recorderMode_;
        const DWORD primaryPid = recorderPrimaryPid_;
        KillTimer(hwnd_, kRecordTimer);
        recorderMode_ = RecorderMode::None;
        recorderMouseDown_ = false;
        if (commit && !recorderClicks_.empty()) {
            if (mode == RecorderMode::TradeMain) CommitRecordedTradeMain();
            else if (mode == RecorderMode::TradeChild) CommitRecordedTradeChild(primaryPid);
        }
        const std::size_t count = recorderClicks_.size();
        recorderClicks_.clear(); recorderPrimaryPid_ = 0;
        const std::wstring status = commit ? L"REC xong • đã chuyển " + std::to_wstring(count) + L" click thành dòng tọa độ" : L"REC đã hủy";
        UpdateRecorderUi(status);
        SetTradeStatus(L"BĐPT thoát RECORDING CỤC BỘ • acc bị giữ tiếp tục auto");
    }

    void StartRecorder(RecorderMode mode) {
        if (recorderMode_ != RecorderMode::None) { StopRecorder(true); return; }
        // REC is now scoped to the window(s) being captured. It must never freeze
        // unrelated accounts merely because point capture uses the physical mouse.
        Account* primary = nullptr;
        if (mode == RecorderMode::TradeMain) {
            primary = AccountByTradeRole(1);
            if (!primary || tradeEditorMode_ != 1) { Log(L"REC MAIN: không có MAIN/editor MAIN."); return; }
        } else if (mode == RecorderMode::TradeChild) {
            primary = TradeEditorChild();
            if (!primary || !AccountByTradeRole(1)) { Log(L"REC CON cần cả MAIN và CON đang mở editor."); return; }
        } else return;
        recorderClicks_.clear(); recorderMode_ = mode; recorderPrimaryPid_ = primary->game.pid;
        recorderMouseDown_ = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        SetTimer(hwnd_, kRecordTimer, 10, nullptr);
        UpdateRecorderUi(L"REC ĐANG GHI • chỉ khóa acc/cặp acc đang capture • các acc khác tiếp tục auto");
        SetTradeStatus(L"RECORDING CỤC BỘ • chỉ giữ cửa sổ liên quan; scheduler acc khác tiếp tục");
        LogAccount(*primary, L"BĐPT vào RECORDING CỤC BỘ • chỉ acc/cặp acc capture bị giữ; acc khác vẫn auto độc lập.");
    }

    void ToggleTradeRecorder() { StartRecorder(tradeEditorMode_ == 1 ? RecorderMode::TradeMain : RecorderMode::TradeChild); }



    void CopySelectedTradeRows() {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        const std::vector<int> rows = SelectedRows(tradeSeqList_);
        if (!seq || rows.empty()) { Log(L"SAO CHÉP GD: hãy chọn một hoặc nhiều dòng."); return; }
        tradeClipboard_.clear(); tradeClipboardMode_ = tradeEditorMode_;
        for (int row : rows) if (row >= 0 && row < static_cast<int>(seq->size())) tradeClipboard_.push_back((*seq)[static_cast<std::size_t>(row)]);
        UpdateRecorderUi(L"Đã sao chép " + std::to_wstring(tradeClipboard_.size()) + L" dòng GD • bấm DÁN để thêm vào cuối chuỗi");
    }

    void PasteTradeRows() {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        if (!seq || tradeClipboard_.empty() || tradeClipboardMode_ != tradeEditorMode_) {
            Log(L"DÁN GD: clipboard rỗng hoặc khác loại editor MAIN/CON."); return;
        }
        const std::size_t first = seq->size();
        int nextGroupId = MaxTradeGroupId(*seq) + 1;
        std::vector<std::pair<int, int>> groupMap;
        for (TradeSequenceStep step : tradeClipboard_) {
            if (seq->size() >= 64) break;
            if (tradeEditorMode_ == 1) {
                step.target = 1;
                step.mainRef = static_cast<int>(seq->size());
                step.groupId = 0;
                step.groupRepeat = 1;
            } else if (step.groupId > 0) {
                int mapped = 0;
                for (const auto& entry : groupMap) {
                    if (entry.first == step.groupId) { mapped = entry.second; break; }
                }
                if (mapped == 0) {
                    mapped = nextGroupId++;
                    groupMap.emplace_back(step.groupId, mapped);
                }
                step.groupId = mapped;
                step.groupRepeat = std::clamp(step.groupRepeat, 1, 999);
            }
            seq->push_back(step);
        }
        if (tradeEditorMode_ == 1) {
            for (std::size_t i = 0; i < seq->size(); ++i) (*seq)[i].mainRef = static_cast<int>(i);
        } else {
            NormalizeTradeGroups(*seq);
        }
        SaveEditorSequence(); RefreshTradeSequenceList(); PopulateTradeTargetCombo();
        if (seq->size() > first) {
            const int row = static_cast<int>(first);
            ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(tradeSeqList_, row, FALSE);
            LoadTradeSequenceRowToEditor(row);
        }
        UpdateRecorderUi(L"Đã DÁN " + std::to_wstring(seq->size() - first) + L" dòng GD vào cuối chuỗi");
    }

    void GroupSelectedTradeRows() {
        if (tradeEditorMode_ != 2) { Log(L"GOM NHÓM chỉ dùng trong CHUỖI GD ACC CON."); return; }
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        std::vector<int> rows = SelectedRows(tradeSeqList_);
        if (!seq || rows.empty()) { Log(L"GOM NHÓM: chọn một hoặc nhiều dòng liên tiếp."); return; }
        std::sort(rows.begin(), rows.end());
        for (std::size_t i = 1; i < rows.size(); ++i) {
            if (rows[i] != rows[i - 1] + 1) {
                Log(L"GOM NHÓM: các dòng phải liên tiếp nhau."); return;
            }
        }
        const int repeat = tradeSeqGroupRepeat_
            ? std::clamp(_wtoi(GetText(tradeSeqGroupRepeat_).c_str()), 1, 999)
            : 1;
        const int id = MaxTradeGroupId(*seq) + 1;
        for (int row : rows) {
            if (row < 0 || row >= static_cast<int>(seq->size())) continue;
            TradeSequenceStep& step = (*seq)[static_cast<std::size_t>(row)];
            step.groupId = id;
            step.groupRepeat = repeat;
        }
        NormalizeTradeGroups(*seq);
        SaveEditorSequence();
        RefreshTradeSequenceList();
        for (int row : rows) {
            ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED, LVIS_SELECTED);
        }
        if (!rows.empty()) {
            ListView_SetItemState(tradeSeqList_, rows.front(), LVIS_FOCUSED, LVIS_FOCUSED);
            LoadTradeSequenceRowToEditor(rows.front());
        }
        Log(L"Đã GOM " + std::to_wstring(rows.size()) + L" dòng thành mini-sequence • lặp nhóm " + std::to_wstring(repeat) + L" lần.");
    }

    void UngroupSelectedTradeRows() {
        if (tradeEditorMode_ != 2) { Log(L"BỎ NHÓM chỉ dùng trong CHUỖI GD ACC CON."); return; }
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        const std::vector<int> rows = SelectedRows(tradeSeqList_);
        if (!seq || rows.empty()) { Log(L"BỎ NHÓM: chọn ít nhất một dòng thuộc nhóm."); return; }
        std::vector<int> groupIds;
        for (int row : rows) {
            if (row < 0 || row >= static_cast<int>(seq->size())) continue;
            const int id = (*seq)[static_cast<std::size_t>(row)].groupId;
            if (id > 0 && std::find(groupIds.begin(), groupIds.end(), id) == groupIds.end()) groupIds.push_back(id);
        }
        if (groupIds.empty()) { Log(L"BỎ NHÓM: các dòng đã chọn không thuộc nhóm nào."); return; }
        for (TradeSequenceStep& step : *seq) {
            if (std::find(groupIds.begin(), groupIds.end(), step.groupId) != groupIds.end()) {
                step.groupId = 0;
                step.groupRepeat = 1;
            }
        }
        NormalizeTradeGroups(*seq);
        SaveEditorSequence();
        RefreshTradeSequenceList();
        Log(L"Đã BỎ " + std::to_wstring(groupIds.size()) + L" nhóm khỏi chuỗi GD.");
    }












    void LoadTradeSettings() {
        tradeEnabled_ = true;
        tradeRendezvous_.name = L"TỌA GD";
        tradeRendezvous_.mapID = ReadIniInt(L"Global", L"TradeRendezvousMap", 0);
        tradeRendezvous_.x = ReadIniInt(L"Global", L"TradeRendezvousX", 0);
        tradeRendezvous_.y = ReadIniInt(L"Global", L"TradeRendezvousY", 0);
        tradeRendezvous_.valid = tradeRendezvous_.mapID > 0 && ReadIniInt(L"Global", L"TradeRendezvousValid", 0) != 0;
        tradeRendezvousTolerance_ = kPreciseWorldTolerance; // v1.6 migration: do not reuse legacy 120 for GD.
    }

    void LoadTradeSequence() {
        // v0.2.7: exactly two reusable trade definitions:
        // 1) MAIN shared coordinate library; 2) one shared ordered ACC CON workflow.
        mainTradeSequence_.clear();
        childTradeSequence_.clear();
        legacyChildTradeTemplate_.clear();
        sharedChildTradeMigrationDone_ = false;
        postTradeCleanupDelayMs_ = std::clamp(ReadIniInt(L"TradeUiDirect", L"PostCleanupDelayMs", 1000), 0, 60000);

        // Prefer the MAIN-shared section.
        int mainCount = std::clamp(ReadIniInt(L"MainTradeSequence", L"Count", 0), 0, 64);
        for (int i = 0; i < mainCount; ++i) {
            TradeSequenceStep step{};
            const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
            step.target = 1;
            step.mainRef = i;
            step.description = ReadIniText(L"MainTradeSequence", prefix + L"Desc");
            step.point.x = ReadIniInt(L"MainTradeSequence", prefix + L"X", -1);
            step.point.y = ReadIniInt(L"MainTradeSequence", prefix + L"Y", -1);
            step.point.baseW = ReadIniInt(L"MainTradeSequence", prefix + L"W", 0);
            step.point.baseH = ReadIniInt(L"MainTradeSequence", prefix + L"H", 0);
            step.point.valid = step.point.x >= 0 && step.point.y >= 0 && step.point.baseW > 0 && step.point.baseH > 0;
            step.delayMs = std::clamp(ReadIniInt(L"MainTradeSequence", prefix + L"Delay", 500), 50, 60000);
            step.repeat = std::clamp(ReadIniInt(L"MainTradeSequence", prefix + L"Repeat", 1), 1, 999);
            step.actionKind = std::clamp(ReadIniInt(L"MainTradeSequence", prefix + L"ActionKind", 0), 0, 1);
            step.uiDirectTarget = ReadIniInt(L"MainTradeSequence", prefix + L"UiDirectTarget", static_cast<int>(UiDirectTarget::None));
            step.minTimeMs = std::clamp(ReadIniInt(L"MainTradeSequence", prefix + L"MinTimeMs", 0), 0, 60000);
            mainTradeSequence_.push_back(step);
        }

        // v0.2.7 shared ACC CON workflow. Count=-1 means the section does not exist yet,
        // allowing one-time migration from the old per-CON profiles.
        const int sharedChildCountRaw = ReadIniInt(L"ChildTradeSequence", L"Count", -1);
        if (sharedChildCountRaw >= 0) {
            sharedChildTradeMigrationDone_ = true;
            const int sharedChildCount = std::clamp(sharedChildCountRaw, 0, 64);
            for (int i = 0; i < sharedChildCount; ++i) {
                TradeSequenceStep step{};
                const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
                step.target = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"Target", 0), 0, 1);
                step.mainRef = ReadIniInt(L"ChildTradeSequence", prefix + L"MainRef", -1);
                step.description = ReadIniText(L"ChildTradeSequence", prefix + L"Desc");
                step.point.x = ReadIniInt(L"ChildTradeSequence", prefix + L"X", -1);
                step.point.y = ReadIniInt(L"ChildTradeSequence", prefix + L"Y", -1);
                step.point.baseW = ReadIniInt(L"ChildTradeSequence", prefix + L"W", 0);
                step.point.baseH = ReadIniInt(L"ChildTradeSequence", prefix + L"H", 0);
                step.point.valid = step.point.x >= 0 && step.point.y >= 0 && step.point.baseW > 0 && step.point.baseH > 0;
                step.delayMs = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"Delay", 500), 50, 60000);
                step.repeat = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"Repeat", 1), 1, 999);
                step.groupId = std::max(0, ReadIniInt(L"ChildTradeSequence", prefix + L"GroupId", 0));
                step.groupRepeat = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"GroupRepeat", 1), 1, 999);
                step.afterAction = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"AfterAction", 0), 0, 1);
                step.actionKind = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"ActionKind", 0), 0, 1);
                step.uiDirectTarget = ReadIniInt(L"ChildTradeSequence", prefix + L"UiDirectTarget", static_cast<int>(UiDirectTarget::None));
                step.minTimeMs = std::clamp(ReadIniInt(L"ChildTradeSequence", prefix + L"MinTimeMs", 0), 0, 60000);
                if (step.target != 0) step.afterAction = 0;
                childTradeSequence_.push_back(step);
            }
        }

        // One-time migration template from v0.2.3 combined global TradeSequence.
        int legacyCount = std::clamp(ReadIniInt(L"TradeSequence", L"Count", 0), 0, 64);
        std::vector<int> oldMainToNew(static_cast<std::size_t>(legacyCount), -1);
        if (mainTradeSequence_.empty()) {
            for (int i = 0; i < legacyCount; ++i) {
                const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
                if (std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Target", 0), 0, 1) != 0) continue;
                TradeSequenceStep mainStep{};
                mainStep.target = 1; mainStep.mainRef = static_cast<int>(mainTradeSequence_.size());
                mainStep.description = ReadIniText(L"TradeSequence", prefix + L"Desc");
                mainStep.point.x = ReadIniInt(L"TradeSequence", prefix + L"X", -1);
                mainStep.point.y = ReadIniInt(L"TradeSequence", prefix + L"Y", -1);
                mainStep.point.baseW = ReadIniInt(L"TradeSequence", prefix + L"W", 0);
                mainStep.point.baseH = ReadIniInt(L"TradeSequence", prefix + L"H", 0);
                mainStep.point.valid = mainStep.point.x >= 0 && mainStep.point.y >= 0 && mainStep.point.baseW > 0 && mainStep.point.baseH > 0;
                mainStep.delayMs = std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Delay", 500), 50, 60000);
                mainStep.repeat = std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Repeat", 1), 1, 999);
                oldMainToNew[static_cast<std::size_t>(i)] = mainStep.mainRef;
                mainTradeSequence_.push_back(mainStep);
            }
            if (!mainTradeSequence_.empty()) SaveMainTradeSequence();
        } else {
            // Map legacy MAIN rows to new MAIN rows in original MAIN-order for child migration.
            int ref = 0;
            for (int i = 0; i < legacyCount; ++i) {
                const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
                if (std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Target", 0), 0, 1) == 0 && ref < static_cast<int>(mainTradeSequence_.size())) {
                    oldMainToNew[static_cast<std::size_t>(i)] = ref++;
                }
            }
        }
        for (int i = 0; i < legacyCount; ++i) {
            TradeSequenceStep step{};
            const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
            const int oldTarget = std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Target", 0), 0, 1);
            step.description = ReadIniText(L"TradeSequence", prefix + L"Desc");
            step.delayMs = std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Delay", 500), 50, 60000);
            step.repeat = std::clamp(ReadIniInt(L"TradeSequence", prefix + L"Repeat", 1), 1, 999);
            if (oldTarget == 0) {
                step.target = 1;
                step.mainRef = oldMainToNew[static_cast<std::size_t>(i)];
            } else {
                step.target = 0;
                step.mainRef = -1;
                step.point.x = ReadIniInt(L"TradeSequence", prefix + L"X", -1);
                step.point.y = ReadIniInt(L"TradeSequence", prefix + L"Y", -1);
                step.point.baseW = ReadIniInt(L"TradeSequence", prefix + L"W", 0);
                step.point.baseH = ReadIniInt(L"TradeSequence", prefix + L"H", 0);
                step.point.valid = step.point.x >= 0 && step.point.y >= 0 && step.point.baseW > 0 && step.point.baseH > 0;
            }
            if (step.target == 0 || step.mainRef >= 0) legacyChildTradeTemplate_.push_back(step);
        }
        NormalizeTradeGroups(childTradeSequence_);
        NormalizeTradeGroups(legacyChildTradeTemplate_);
    }

    void SaveMainTradeSequence() {
        EnsureUnicodeIni();
        WriteIniInt(L"MainTradeSequence", L"Count", static_cast<int>(mainTradeSequence_.size()));
        for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) {
            const TradeSequenceStep& step = mainTradeSequence_[i];
            const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
            WriteIniText(L"MainTradeSequence", prefix + L"Desc", step.description);
            WriteIniInt(L"MainTradeSequence", prefix + L"X", step.point.valid ? step.point.x : -1);
            WriteIniInt(L"MainTradeSequence", prefix + L"Y", step.point.valid ? step.point.y : -1);
            WriteIniInt(L"MainTradeSequence", prefix + L"W", step.point.valid ? step.point.baseW : 0);
            WriteIniInt(L"MainTradeSequence", prefix + L"H", step.point.valid ? step.point.baseH : 0);
            WriteIniInt(L"MainTradeSequence", prefix + L"Delay", step.delayMs);
            WriteIniInt(L"MainTradeSequence", prefix + L"Repeat", step.repeat);
            WriteIniInt(L"MainTradeSequence", prefix + L"ActionKind", step.actionKind);
            WriteIniInt(L"MainTradeSequence", prefix + L"UiDirectTarget", step.uiDirectTarget);
            WriteIniInt(L"MainTradeSequence", prefix + L"MinTimeMs", std::clamp(step.minTimeMs, 0, 60000));
        }
        WriteIniInt(L"TradeUiDirect", L"PostCleanupDelayMs", std::clamp(postTradeCleanupDelayMs_, 0, 60000));
        FlushIni();
    }

    void SaveSharedChildTradeSequence() {
        EnsureUnicodeIni();
        NormalizeTradeGroups(childTradeSequence_);
        WriteIniInt(L"ChildTradeSequence", L"Count", static_cast<int>(childTradeSequence_.size()));
        for (std::size_t i = 0; i < childTradeSequence_.size(); ++i) {
            const TradeSequenceStep& step = childTradeSequence_[i];
            const std::wstring prefix = L"Step_" + std::to_wstring(i) + L"_";
            WriteIniInt(L"ChildTradeSequence", prefix + L"Target", step.target);
            WriteIniInt(L"ChildTradeSequence", prefix + L"MainRef", step.mainRef);
            WriteIniText(L"ChildTradeSequence", prefix + L"Desc", step.description);
            WriteIniInt(L"ChildTradeSequence", prefix + L"X", step.point.valid ? step.point.x : -1);
            WriteIniInt(L"ChildTradeSequence", prefix + L"Y", step.point.valid ? step.point.y : -1);
            WriteIniInt(L"ChildTradeSequence", prefix + L"W", step.point.valid ? step.point.baseW : 0);
            WriteIniInt(L"ChildTradeSequence", prefix + L"H", step.point.valid ? step.point.baseH : 0);
            WriteIniInt(L"ChildTradeSequence", prefix + L"Delay", step.delayMs);
            WriteIniInt(L"ChildTradeSequence", prefix + L"Repeat", step.repeat);
            WriteIniInt(L"ChildTradeSequence", prefix + L"GroupId", step.groupId);
            WriteIniInt(L"ChildTradeSequence", prefix + L"GroupRepeat", step.groupRepeat);
            WriteIniInt(L"ChildTradeSequence", prefix + L"AfterAction", step.target == 0 ? std::clamp(step.afterAction, 0, 1) : 0);
            WriteIniInt(L"ChildTradeSequence", prefix + L"ActionKind", step.actionKind);
            WriteIniInt(L"ChildTradeSequence", prefix + L"UiDirectTarget", step.uiDirectTarget);
            WriteIniInt(L"ChildTradeSequence", prefix + L"MinTimeMs", std::clamp(step.minTimeMs, 0, 60000));
        }
        sharedChildTradeMigrationDone_ = true;
        WriteIniInt(L"TradeUiDirect", L"PostCleanupDelayMs", std::clamp(postTradeCleanupDelayMs_, 0, 60000));
        FlushIni();
    }

    void EnsureSharedChildTradeSequence() {
        if (sharedChildTradeMigrationDone_) return;
        sharedChildTradeMigrationDone_ = true;

        // Prefer the old sequence of the lowest CON slot so existing setups migrate deterministically.
        for (int slot = 1; slot <= kChildTradeCount; ++slot) {
            Account* child = AccountByTradeRole(slot + 1);
            if (!child || child->profile.childTradeSequence.empty()) continue;
            childTradeSequence_ = child->profile.childTradeSequence;
            SaveSharedChildTradeSequence();
            LogAccount(*child, L"v0.2.7 MIGRATE: lấy chuỗi GD cũ của " + TradeRoleLabel(child->profile.tradeRole) +
                               L" làm CHUỖI GD ACC CON dùng chung cho CON1→CON30.");
            return;
        }

        if (!legacyChildTradeTemplate_.empty()) {
            childTradeSequence_ = legacyChildTradeTemplate_;
            SaveSharedChildTradeSequence();
            Log(L"v0.2.7 MIGRATE: lấy template GD legacy làm CHUỖI GD ACC CON dùng chung.");
            return;
        }

        // Persist an intentional empty shared section so we do not repeatedly scan legacy profiles.
        SaveSharedChildTradeSequence();
    }

    std::vector<TradeSequenceStep>* EditorSequence() {
        if (tradeEditorMode_ == 1) return &mainTradeSequence_;
        if (tradeEditorMode_ == 2) {
            Account* child = TradeEditorChild();
            if (!child || child->profile.tradeRole < 2) return nullptr; // selected CON is only the capture/test donor.
            EnsureSharedChildTradeSequence();
            return &childTradeSequence_;
        }
        return nullptr;
    }

    Account* TradeEditorChild() {
        Account* child = AccountByPid(tradeEditorChildPid_);
        return child && child->profile.tradeRole >= 2 ? child : nullptr;
    }

    const TradeSequenceStep* ResolveMainReference(const TradeSequenceStep& step) const {
        if (step.target != 1 || step.mainRef < 0 || step.mainRef >= static_cast<int>(mainTradeSequence_.size())) return nullptr;
        return &mainTradeSequence_[static_cast<std::size_t>(step.mainRef)];
    }

    void NormalizeTradeGroups(std::vector<TradeSequenceStep>& seq) {
        int nextId = 1;
        int previousOldId = 0;
        int currentNewId = 0;
        int currentRepeat = 1;
        for (std::size_t i = 0; i < seq.size(); ++i) {
            TradeSequenceStep& step = seq[i];
            const int oldId = step.groupId;
            if (oldId <= 0) {
                step.groupId = 0;
                step.groupRepeat = 1;
                previousOldId = 0;
                currentNewId = 0;
                continue;
            }
            if (i == 0 || oldId != previousOldId || currentNewId == 0) {
                currentNewId = nextId++;
                currentRepeat = std::clamp(step.groupRepeat, 1, 999);
            }
            step.groupId = currentNewId;
            step.groupRepeat = currentRepeat;
            previousOldId = oldId;
        }
    }

    int MaxTradeGroupId(const std::vector<TradeSequenceStep>& seq) const {
        int maxId = 0;
        for (const TradeSequenceStep& step : seq) maxId = std::max(maxId, step.groupId);
        return maxId;
    }

    std::size_t TradeGroupStart(const std::vector<TradeSequenceStep>& seq, std::size_t index) const {
        if (index >= seq.size() || seq[index].groupId <= 0) return index;
        const int id = seq[index].groupId;
        while (index > 0 && seq[index - 1].groupId == id) --index;
        return index;
    }

    std::size_t TradeGroupEnd(const std::vector<TradeSequenceStep>& seq, std::size_t index) const {
        if (index >= seq.size() || seq[index].groupId <= 0) return index;
        const int id = seq[index].groupId;
        while (index + 1 < seq.size() && seq[index + 1].groupId == id) ++index;
        return index;
    }

    std::wstring TradeGroupLabel(const TradeSequenceStep& step) const {
        if (step.groupId <= 0) return L"-";
        return L"G" + std::to_wstring(step.groupId) + L" ×" + std::to_wstring(step.groupRepeat);
    }

    static bool IsTradeUiDirectTarget(int rawTarget) {
        switch (static_cast<UiDirectTarget>(rawTarget)) {
            case UiDirectTarget::TradeConfirm:
            case UiDirectTarget::TradeTabEquip:
            case UiDirectTarget::TradeLock:
            case UiDirectTarget::TradeSubmit:
            case UiDirectTarget::TradeRequestCancel:
                return true;
            default:
                return false;
        }
    }

    bool TradeSequenceReady(std::wstring& reason) {
        EnsureSharedChildTradeSequence();
        if (childTradeSequence_.empty()) { reason = L"chưa có CHUỖI GD ACC CON dùng chung"; return false; }
        for (std::size_t i = 0; i < childTradeSequence_.size(); ++i) {
            const TradeSequenceStep& step = childTradeSequence_[i];
            if (step.target == 1) {
                const TradeSequenceStep* shared = ResolveMainReference(step);
                if (!shared) { reason = L"bước " + std::to_wstring(i + 1) + L" tham chiếu MAIN không tồn tại"; return false; }
                if (shared->actionKind == 0 && !shared->point.valid) { reason = L"MAIN bước " + std::to_wstring(step.mainRef + 1) + L" chưa lấy tọa độ"; return false; }
                if (shared->actionKind == 1 && !IsTradeUiDirectTarget(shared->uiDirectTarget)) { reason = L"MAIN bước " + std::to_wstring(step.mainRef + 1) + L" UI DIRECT chưa chọn action"; return false; }
            } else if (step.actionKind == 0 && !step.point.valid) {
                reason = L"CON bước " + std::to_wstring(i + 1) + L" chưa lấy tọa độ"; return false;
            } else if (step.actionKind == 1 && !IsTradeUiDirectTarget(step.uiDirectTarget)) {
                reason = L"CON bước " + std::to_wstring(i + 1) + L" UI DIRECT chưa chọn action"; return false;
            }
        }
        return true;
    }

    static const wchar_t* TradeUiDirectLabel(int rawTarget) {
        switch (static_cast<UiDirectTarget>(rawTarget)) {
            case UiDirectTarget::TradeConfirm: return L"XÁC NHẬN GIAO DỊCH";
            case UiDirectTarget::TradeTabEquip: return L"TAB TRANG BỊ";
            case UiDirectTarget::TradeLock: return L"KHÓA";
            case UiDirectTarget::TradeSubmit: return L"GIAO DỊCH";
            case UiDirectTarget::TradeRequestCancel: return L"HỦY BỎ YÊU CẦU GIAO DỊCH";
            default: return L"UI DIRECT ?";
        }
    }

    void PopulateTradeActionCombo(const TradeSequenceStep* effective = nullptr, bool sharedRef = false) {
        if (!tradeSeqAction_) return;
        SendMessageW(tradeSeqAction_, CB_RESETCONTENT, 0, 0);
        struct Entry { const wchar_t* label; UiDirectTarget target; };
        static constexpr Entry kEntries[] = {
            {L"RAW CLICK TỌA ĐỘ", UiDirectTarget::None},
            {L"UI • Xác nhận giao dịch", UiDirectTarget::TradeConfirm},
            {L"UI • Tab Trang bị", UiDirectTarget::TradeTabEquip},
            {L"UI • Khóa", UiDirectTarget::TradeLock},
            {L"UI • Giao dịch", UiDirectTarget::TradeSubmit},
            {L"UI • Hủy bỏ yêu cầu giao dịch", UiDirectTarget::TradeRequestCancel},
        };
        int selected = 0;
        for (int i = 0; i < static_cast<int>(_countof(kEntries)); ++i) {
            const LRESULT idx = SendMessageW(tradeSeqAction_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kEntries[i].label));
            SendMessageW(tradeSeqAction_, CB_SETITEMDATA, idx, static_cast<LPARAM>(kEntries[i].target));
            if (effective && effective->actionKind == 1 && effective->uiDirectTarget == static_cast<int>(kEntries[i].target)) selected = i;
        }
        SendMessageW(tradeSeqAction_, CB_SETCURSEL, selected, 0);
        EnableWindow(tradeSeqAction_, !sharedRef);
    }

    int SelectedTradeUiDirectTarget() const {
        if (!tradeSeqAction_) return static_cast<int>(UiDirectTarget::None);
        const LRESULT sel = SendMessageW(tradeSeqAction_, CB_GETCURSEL, 0, 0);
        if (sel < 0) return static_cast<int>(UiDirectTarget::None);
        const LRESULT data = SendMessageW(tradeSeqAction_, CB_GETITEMDATA, sel, 0);
        return data == CB_ERR ? static_cast<int>(UiDirectTarget::None) : static_cast<int>(data);
    }

    std::wstring TradeStepTargetLabel(const TradeSequenceStep& step) {
        if (tradeEditorMode_ == 1) return L"MAIN";
        if (step.target == 1) return L"MAIN #" + std::to_wstring(step.mainRef + 1);
        return L"ACC CON ĐANG GD";
    }

    const TradeSequenceStep* EffectiveEditorStep(const TradeSequenceStep& step) const {
        return tradeEditorMode_ == 2 && step.target == 1 ? ResolveMainReference(step) : &step;
    }

    void RefreshTradeSequenceList() {
        if (!tradeSeqList_) return;
        ListView_DeleteAllItems(tradeSeqList_);
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        if (!seq) return;
        for (std::size_t i = 0; i < seq->size(); ++i) {
            const TradeSequenceStep& step = (*seq)[i];
            const TradeSequenceStep* effective = EffectiveEditorStep(step);
            std::wstring idx = std::to_wstring(i + 1);
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i); item.pszText = idx.data();
            ListView_InsertItem(tradeSeqList_, &item);
            const std::array<std::wstring, 7> cols = {{
                TradeStepTargetLabel(step),
                (effective && !effective->description.empty()) ? effective->description : L"(không mô tả)",
                effective ? (effective->actionKind == 1 ? std::wstring(L"UI DIRECT • ") + TradeUiDirectLabel(effective->uiDirectTarget) : PointDescription(effective->point)) : L"MAIN REF LỖI",
                effective ? (effective->actionKind == 1 ? (L"MIN " + std::to_wstring(effective->minTimeMs)) : std::to_wstring(effective->delayMs)) : L"-",
                effective ? std::to_wstring(effective->repeat) : L"-",
                tradeEditorMode_ == 2 ? TradeGroupLabel(step) : L"-",
                (effective && effective->actionKind == 1) ? L"NO SLEEP" : ((tradeEditorMode_ == 2 && step.target == 0 && step.afterAction == 1) ? L"ĐẶT LÊN" : L"-")
            }};
            for (int col = 0; col < 7; ++col) ListView_SetItemText(tradeSeqList_, static_cast<int>(i), col + 1, const_cast<wchar_t*>(cols[static_cast<std::size_t>(col)].c_str()));
        }
    }

    void SelectTradeSequenceDragRange(int startRow, int endRow) {
        if (!tradeSeqList_) return;
        const int count = ListView_GetItemCount(tradeSeqList_);
        if (count <= 0) return;
        startRow = std::clamp(startRow, 0, count - 1);
        endRow = std::clamp(endRow, 0, count - 1);
        const int first = std::min(startRow, endRow);
        const int last = std::max(startRow, endRow);
        tradeSeqDragUpdating_ = true;
        for (int i = 0; i < count; ++i) {
            const UINT state = (i >= first && i <= last) ? LVIS_SELECTED : 0;
            ListView_SetItemState(tradeSeqList_, i, state, LVIS_SELECTED);
        }
        ListView_SetItemState(tradeSeqList_, endRow, LVIS_FOCUSED, LVIS_FOCUSED);
        ListView_EnsureVisible(tradeSeqList_, endRow, FALSE);
        tradeSeqDragUpdating_ = false;
        LoadTradeSequenceRowToEditor(endRow);
        UpdateRecorderUi(L"Đã kéo chọn " + std::to_wstring(last - first + 1) + L" dòng GD");
    }

    int SelectedTradeSequenceIndex() const {
        return FocusedSelectedRow(tradeSeqList_);
    }

    void PopulateTradeTargetCombo(const TradeSequenceStep* step = nullptr) {
        if (!tradeSeqTarget_) return;
        SendMessageW(tradeSeqTarget_, CB_RESETCONTENT, 0, 0);
        if (tradeEditorMode_ == 1) {
            SendMessageW(tradeSeqTarget_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"MAIN (DÙNG CHUNG)"));
            SendMessageW(tradeSeqTarget_, CB_SETCURSEL, 0, 0);
            EnableWindow(tradeSeqTarget_, FALSE);
            return;
        }
        EnableWindow(tradeSeqTarget_, TRUE);
        const std::wstring childName = L"ACC CON ĐANG GD (DÙNG CHUNG)";
        SendMessageW(tradeSeqTarget_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(childName.c_str()));
        for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) {
            std::wstring label = L"MAIN #" + std::to_wstring(i + 1) + L" • " + (mainTradeSequence_[i].description.empty() ? L"(không mô tả)" : mainTradeSequence_[i].description);
            SendMessageW(tradeSeqTarget_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        int sel = 0;
        if (step && step->target == 1 && step->mainRef >= 0 && step->mainRef < static_cast<int>(mainTradeSequence_.size())) sel = step->mainRef + 1;
        SendMessageW(tradeSeqTarget_, CB_SETCURSEL, sel, 0);
    }

    void LoadTradeSequenceRowToEditor(int index) {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        if (!seq || index < 0 || index >= static_cast<int>(seq->size())) return;
        TradeSequenceStep& step = (*seq)[static_cast<std::size_t>(index)];
        const TradeSequenceStep* effective = EffectiveEditorStep(step);
        PopulateTradeTargetCombo(&step);
        const bool sharedRef = tradeEditorMode_ == 2 && step.target == 1;
        if (effective) {
            SetText(tradeSeqDesc_, effective->description);
            SetText(tradeSeqDelay_, std::to_wstring(effective->delayMs));
            SetText(tradeSeqRepeat_, std::to_wstring(effective->repeat));
            if (tradeSeqMinTime_) SetText(tradeSeqMinTime_, std::to_wstring(effective->minTimeMs));
        }
        PopulateTradeActionCombo(effective, sharedRef);
        if (tradeSeqPostCleanup_) SetText(tradeSeqPostCleanup_, std::to_wstring(postTradeCleanupDelayMs_));
        if (tradeSeqGroupRepeat_) SetText(tradeSeqGroupRepeat_, std::to_wstring(step.groupId > 0 ? step.groupRepeat : 1));
        if (tradeSeqAfterPutUp_) SendMessageW(tradeSeqAfterPutUp_, BM_SETCHECK, (tradeEditorMode_ == 2 && step.target == 0 && step.afterAction == 1) ? BST_CHECKED : BST_UNCHECKED, 0);
        const bool direct = effective && effective->actionKind == 1;
        EnableWindow(tradeSeqDesc_, !sharedRef);
        EnableWindow(tradeSeqDelay_, !sharedRef && !direct);
        if (tradeSeqMinTime_) EnableWindow(tradeSeqMinTime_, !sharedRef && direct);
        EnableWindow(tradeSeqRepeat_, !sharedRef);
        if (tradeSeqGroupRepeat_) EnableWindow(tradeSeqGroupRepeat_, tradeEditorMode_ == 2);
        if (tradeSeqAfterPutUp_) EnableWindow(tradeSeqAfterPutUp_, tradeEditorMode_ == 2 && step.target == 0 && !direct);
    }

    void SaveEditorSequence() {
        if (tradeEditorMode_ == 1) SaveMainTradeSequence();
        else if (tradeEditorMode_ == 2) SaveSharedChildTradeSequence();
    }

    void AddTradeSequenceRow() {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        if (!seq || seq->size() >= 64) return;
        const int selected = SelectedTradeSequenceIndex();
        const std::size_t insertAt = selected >= 0 && selected < static_cast<int>(seq->size())
            ? static_cast<std::size_t>(selected + 1) : seq->size();
        TradeSequenceStep step{};
        if (tradeEditorMode_ == 1) {
            // Inserting a MAIN row shifts every later MAIN reference used by the shared CON sequence.
            EnsureSharedChildTradeSequence();
            for (TradeSequenceStep& cs : childTradeSequence_) {
                if (cs.target == 1 && cs.mainRef >= static_cast<int>(insertAt)) ++cs.mainRef;
            }
            step.target = 1; step.mainRef = static_cast<int>(insertAt);
            step.description = L"MAIN bước " + std::to_wstring(insertAt + 1);
            seq->insert(seq->begin() + static_cast<std::ptrdiff_t>(insertAt), step);
            for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) mainTradeSequence_[i].mainRef = static_cast<int>(i);
            SaveSharedChildTradeSequence();
        } else {
            step.target = 0; step.mainRef = -1;
            step.description = L"CON bước " + std::to_wstring(insertAt + 1);
            seq->insert(seq->begin() + static_cast<std::ptrdiff_t>(insertAt), step);
            NormalizeTradeGroups(*seq);
        }
        SaveEditorSequence(); RefreshTradeSequenceList(); PopulateTradeTargetCombo();
        const int row = static_cast<int>(insertAt);
        ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(tradeSeqList_, row, FALSE); LoadTradeSequenceRowToEditor(row);
    }

    void DeleteTradeSequenceRow() {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        std::vector<int> rows = SelectedRows(tradeSeqList_);
        if (!seq || rows.empty()) return;
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](int row) {
            return row < 0 || row >= static_cast<int>(seq->size());
        }), rows.end());
        if (rows.empty()) return;
        std::sort(rows.begin(), rows.end());

        if (tradeEditorMode_ == 1) {
            // Repair the one shared ACC CON workflow against all deleted MAIN refs before erasing rows.
            EnsureSharedChildTradeSequence();
            for (TradeSequenceStep& cs : childTradeSequence_) if (cs.target == 1) {
                if (std::binary_search(rows.begin(), rows.end(), cs.mainRef)) {
                    cs.mainRef = -1;
                } else {
                    cs.mainRef -= static_cast<int>(std::lower_bound(rows.begin(), rows.end(), cs.mainRef) - rows.begin());
                }
            }
            SaveSharedChildTradeSequence();
        }

        for (auto it = rows.rbegin(); it != rows.rend(); ++it) seq->erase(seq->begin() + *it);
        if (tradeEditorMode_ == 1) {
            for (std::size_t i = 0; i < mainTradeSequence_.size(); ++i) mainTradeSequence_[i].mainRef = static_cast<int>(i);
        } else if (tradeEditorMode_ == 2) {
            NormalizeTradeGroups(*seq);
        }
        SaveEditorSequence();
        RefreshTradeSequenceList();
    }

    void MoveTradeSequenceRow(int delta) {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        const int row = SelectedTradeSequenceIndex();
        if (!seq || row < 0) return;
        const int next = row + delta;
        if (next < 0 || next >= static_cast<int>(seq->size())) return;
        if (tradeEditorMode_ == 1) {
            // Preserve shared ACC CON references by swapping ref IDs with MAIN content.
            std::swap((*seq)[static_cast<std::size_t>(row)], (*seq)[static_cast<std::size_t>(next)]);
            EnsureSharedChildTradeSequence();
            for (TradeSequenceStep& cs : childTradeSequence_) if (cs.target == 1) {
                if (cs.mainRef == row) cs.mainRef = next; else if (cs.mainRef == next) cs.mainRef = row;
            }
            for (std::size_t i = 0; i < seq->size(); ++i) (*seq)[i].mainRef = static_cast<int>(i);
            SaveSharedChildTradeSequence();
        } else {
            std::swap((*seq)[static_cast<std::size_t>(row)], (*seq)[static_cast<std::size_t>(next)]);
            NormalizeTradeGroups(*seq);
        }
        SaveEditorSequence(); RefreshTradeSequenceList();
        ListView_SetItemState(tradeSeqList_, next, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        LoadTradeSequenceRowToEditor(next);
    }

    void SaveTradeSequenceRowFromEditor(bool refreshUi = true, int forcedRow = -1) {
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        const int row = forcedRow >= 0 ? forcedRow : SelectedTradeSequenceIndex();
        if (!seq || row < 0 || row >= static_cast<int>(seq->size())) return;
        TradeSequenceStep& step = (*seq)[static_cast<std::size_t>(row)];
        if (tradeEditorMode_ == 1) {
            step.target = 1; step.mainRef = row;
            step.description = GetText(tradeSeqDesc_);
            step.delayMs = std::clamp(_wtoi(GetText(tradeSeqDelay_).c_str()), 50, 60000);
            step.repeat = std::clamp(_wtoi(GetText(tradeSeqRepeat_).c_str()), 1, 999);
            step.uiDirectTarget = SelectedTradeUiDirectTarget();
            step.actionKind = step.uiDirectTarget == static_cast<int>(UiDirectTarget::None) ? 0 : 1;
            step.minTimeMs = tradeSeqMinTime_ ? std::clamp(_wtoi(GetText(tradeSeqMinTime_).c_str()), 0, 60000) : 0;
            if (step.actionKind == 1) step.afterAction = 0;
        } else {
            const LRESULT targetSel = SendMessageW(tradeSeqTarget_, CB_GETCURSEL, 0, 0);
            if (targetSel > 0) {
                step.target = 1; step.mainRef = static_cast<int>(targetSel - 1); step.afterAction = 0;
            } else {
                step.target = 0; step.mainRef = -1;
                step.description = GetText(tradeSeqDesc_);
                step.delayMs = std::clamp(_wtoi(GetText(tradeSeqDelay_).c_str()), 50, 60000);
                step.repeat = std::clamp(_wtoi(GetText(tradeSeqRepeat_).c_str()), 1, 999);
                step.uiDirectTarget = SelectedTradeUiDirectTarget();
                step.actionKind = step.uiDirectTarget == static_cast<int>(UiDirectTarget::None) ? 0 : 1;
                step.minTimeMs = tradeSeqMinTime_ ? std::clamp(_wtoi(GetText(tradeSeqMinTime_).c_str()), 0, 60000) : 0;
                step.afterAction = step.actionKind == 0 && tradeSeqAfterPutUp_ && SendMessageW(tradeSeqAfterPutUp_, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
            }
        }
        if (tradeSeqPostCleanup_) postTradeCleanupDelayMs_ = std::clamp(_wtoi(GetText(tradeSeqPostCleanup_).c_str()), 0, 60000);
        SaveEditorSequence();
        if (!refreshUi) return;
        RefreshTradeSequenceList();
        ListView_SetItemState(tradeSeqList_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        LoadTradeSequenceRowToEditor(row);
    }

    Account* TradeSequenceCaptureAccount(TradeSequenceStep& step, ClickPoint*& pointOut) {
        pointOut = nullptr;
        if (tradeEditorMode_ == 1) {
            Account* main = AccountByTradeRole(1); pointOut = &step.point; return main;
        }
        // A child-sequence row may reference MAIN. Do not require the donor CON to still
        // exist just to capture a MAIN coordinate. Resolve the actual target first.
        if (step.target == 1) {
            if (step.mainRef < 0 || step.mainRef >= static_cast<int>(mainTradeSequence_.size())) return nullptr;
            pointOut = &mainTradeSequence_[static_cast<std::size_t>(step.mainRef)].point;
            return AccountByTradeRole(1);
        }
        Account* child = TradeEditorChild();
        if (!child) return nullptr;
        pointOut = &step.point; return child;
    }

    void BeginTradeSequenceCapture() {
        // Freeze the exact row/target before arming F8. Saving the editor used to rebuild
        // the ListView first, so drag/multi-selection could change the row used by capture.
        const int row = SelectedTradeSequenceIndex();
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        if (!seq || row < 0 || row >= static_cast<int>(seq->size())) {
            Log(L"BĐPT: hãy chọn đúng một dòng đang focus để lấy tọa độ chuỗi GD.");
            return;
        }
        SaveTradeSequenceRowFromEditor(false, row);
        seq = EditorSequence();
        if (!seq || row >= static_cast<int>(seq->size())) return;

        TradeSequenceStep& step = (*seq)[static_cast<std::size_t>(row)];
        const TradeSequenceStep* effectiveForCapture = EffectiveEditorStep(step);
        if (effectiveForCapture && effectiveForCapture->actionKind == 1) { Log(L"UI DIRECT không cần lấy tọa F8."); return; }
        ClickPoint* point = nullptr;
        Account* target = TradeSequenceCaptureAccount(step, point);
        if (!target || !point) { Log(L"BĐPT: không xác định được cửa sổ để lấy tọa độ chuỗi GD."); return; }

        shortcutPostTradeCapture_ = false;
        captureSlot_ = ClickSlot::None;
        captureTradeSequenceIndex_ = row;
        captureTradeSequenceMode_ = tradeEditorMode_;
        captureTradeSequenceMainRef_ = (tradeEditorMode_ == 2 && step.target == 1) ? step.mainRef : -1;
        capturePid_ = target->game.pid;
        LogAccount(*target, L"BĐPT yêu cầu lấy tọa chuỗi GD dòng " + std::to_wstring(row + 1) + L" → đưa chuột vào đúng vị trí và F8.");
    }

    void TestTradeSequenceRow() {
        SaveTradeSequenceRowFromEditor();
        std::vector<TradeSequenceStep>* seq = EditorSequence();
        const int row = SelectedTradeSequenceIndex();
        if (!seq || row < 0 || row >= static_cast<int>(seq->size())) return;
        TradeSequenceStep& stored = (*seq)[static_cast<std::size_t>(row)];
        const TradeSequenceStep* effective = EffectiveEditorStep(stored);
        Account* target = nullptr;
        if (tradeEditorMode_ == 1 || stored.target == 1) target = AccountByTradeRole(1); else target = TradeEditorChild();
        if (!target || !effective) { Log(L"BĐPT: TEST dòng không xác định được acc/MAIN reference."); return; }
        std::wstring error;
        if (effective->actionKind == 1) {
            const int direct = InvokeUiDirectNow(*target, static_cast<UiDirectTarget>(effective->uiDirectTarget), error);
            if (direct <= 0) { LogAccount(*target, L"TEST UI DIRECT chưa PASS: " + error); return; }
            LogAccount(*target, L"TEST UI DIRECT dòng " + std::to_wstring(row + 1) + L" • " + TradeUiDirectLabel(effective->uiDirectTarget) + L" • PASS");
            return;
        }
        if (!CoordinatorRawMacroPointAction(
                *target, effective->point,
                L"TEST CHUỖI GD dòng " + std::to_wstring(row + 1), error)) {
            LogAccount(*target, L"TEST chuỗi GD FAIL: " + error); return;
        }
        LogAccount(*target, L"TEST chuỗi GD dòng " + std::to_wstring(row + 1) +
                           L" PASS qua HIDDEN ACTION BĐPT.");
    }

    void BuildTradeEditorUi(HWND parent) {
        auto addColumn = [&](int index, int width, const wchar_t* text) {
            LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; c.pszText = const_cast<wchar_t*>(text); c.cx = width; c.iSubItem = index;
            ListView_InsertColumn(tradeSeqList_, index, &c);
        };
        const wchar_t* heading = tradeEditorMode_ == 1
            ? L"CHUỖI GD MAIN — tọa MAIN dùng chung cho mọi giao dịch"
            : L"CHUỖI GD ACC CON (DÙNG CHUNG) — mọi CON1..CON30 dùng đúng một workflow này";
        MakeIn(parent, L"STATIC", heading, 0, 15, 10, 850, 23, 0);
        tradeSeqList_ = MakeIn(parent, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER, 15, 38, 850, 235, IDC_SEQ_LIST);
        ListView_SetExtendedListViewStyle(tradeSeqList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        SetWindowSubclass(tradeSeqList_, TradeSequenceListSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
        addColumn(0, 35, L"#"); addColumn(1, 120, L"ACC THỰC HIỆN");
        addColumn(2, 185, L"Mô tả"); addColumn(3, 160, L"Tọa độ"); addColumn(4, 55, L"Delay"); addColumn(5, 50, L"Lặp"); addColumn(6, 95, L"Nhóm lặp"); addColumn(7, 95, L"SAU CLICK");
        MakeIn(parent, L"STATIC", L"ACC:", 0, 15, 287, 38, 22, 0);
        tradeSeqTarget_ = MakeIn(parent, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 55, 282, 250, 220, IDC_SEQ_TARGET);
        MakeIn(parent, L"STATIC", L"Mô tả:", 0, 315, 287, 50, 22, 0);
        tradeSeqDesc_ = MakeIn(parent, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 367, 282, 220, 27, IDC_SEQ_DESC);
        MakeIn(parent, L"STATIC", L"Delay:", 0, 597, 287, 42, 22, 0);
        tradeSeqDelay_ = MakeIn(parent, L"EDIT", L"500", WS_BORDER | ES_NUMBER | ES_CENTER, 641, 282, 65, 27, IDC_SEQ_DELAY);
        MakeIn(parent, L"STATIC", L"Lặp:", 0, 716, 287, 32, 22, 0);
        tradeSeqRepeat_ = MakeIn(parent, L"EDIT", L"1", WS_BORDER | ES_NUMBER | ES_CENTER, 750, 282, 55, 27, IDC_SEQ_REPEAT);
        MakeIn(parent, L"BUTTON", L"+ THÊM", BS_PUSHBUTTON, 15, 323, 90, 30, IDC_SEQ_ADD);
        MakeIn(parent, L"BUTTON", L"- XÓA", BS_PUSHBUTTON, 112, 323, 80, 30, IDC_SEQ_DELETE);
        MakeIn(parent, L"BUTTON", L"LÊN", BS_PUSHBUTTON, 199, 323, 70, 30, IDC_SEQ_UP);
        MakeIn(parent, L"BUTTON", L"XUỐNG", BS_PUSHBUTTON, 276, 323, 75, 30, IDC_SEQ_DOWN);
        MakeIn(parent, L"BUTTON", L"LƯU DÒNG", BS_PUSHBUTTON, 360, 323, 105, 30, IDC_SEQ_SAVE);
        MakeIn(parent, L"BUTTON", L"LẤY TỌA (F8)", BS_PUSHBUTTON, 474, 323, 125, 30, IDC_SEQ_CAPTURE);
        MakeIn(parent, L"BUTTON", L"TEST DÒNG", BS_PUSHBUTTON, 608, 323, 105, 30, IDC_SEQ_TEST);
        MakeIn(parent, L"BUTTON", L"ĐÓNG", BS_PUSHBUTTON, 775, 323, 90, 30, IDC_SEQ_CLOSE);
        tradeRecordButton_ = MakeIn(parent, L"BUTTON", L"REC", BS_PUSHBUTTON, 15, 360, 92, 30, IDC_SEQ_REC);
        MakeIn(parent, L"BUTTON", L"SAO CHÉP", BS_PUSHBUTTON, 115, 360, 105, 30, IDC_SEQ_COPY);
        MakeIn(parent, L"BUTTON", L"DÁN", BS_PUSHBUTTON, 228, 360, 80, 30, IDC_SEQ_PASTE);
        if (tradeEditorMode_ == 2) {
            MakeIn(parent, L"STATIC", L"Lặp nhóm:", 0, 320, 365, 68, 22, 0);
            tradeSeqGroupRepeat_ = MakeIn(parent, L"EDIT", L"2", WS_BORDER | ES_NUMBER | ES_CENTER, 390, 360, 45, 30, IDC_SEQ_GROUP_REPEAT);
            MakeIn(parent, L"BUTTON", L"GOM DÒNG ĐÃ CHỌN", BS_PUSHBUTTON, 443, 360, 170, 30, IDC_SEQ_GROUP_SELECTED);
            MakeIn(parent, L"BUTTON", L"BỎ NHÓM", BS_PUSHBUTTON, 621, 360, 110, 30, IDC_SEQ_UNGROUP);
            tradeSeqAfterPutUp_ = MakeIn(parent, L"BUTTON", L"SAU CLICK: ĐẶT LÊN", BS_AUTOCHECKBOX, 738, 360, 127, 30, IDC_SEQ_AFTER_PUTUP);
        }
        tradeRecordStatus_ = MakeIn(parent, L"STATIC", L"REC: sẵn sàng • chọn nhiều dòng liên tiếp để GOM và lặp mini-sequence", SS_LEFT | SS_CENTERIMAGE, 15, 398, 850, 30, 0);
        MakeIn(parent, L"STATIC", L"Hành động:", 0, 15, 442, 65, 24, 0);
        tradeSeqAction_ = MakeIn(parent, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 82, 437, 285, 220, IDC_SEQ_ACTION);
        MakeIn(parent, L"STATIC", L"TIME MIN (ms):", 0, 380, 442, 92, 24, 0);
        tradeSeqMinTime_ = MakeIn(parent, L"EDIT", L"0", WS_BORDER | ES_NUMBER | ES_CENTER, 477, 437, 70, 27, IDC_SEQ_MIN_TIME);
        if (tradeEditorMode_ == 2) {
            MakeIn(parent, L"STATIC", L"POST CLEANUP (ms):", 0, 565, 442, 115, 24, 0);
            tradeSeqPostCleanup_ = MakeIn(parent, L"EDIT", std::to_wstring(postTradeCleanupDelayMs_).c_str(), WS_BORDER | ES_NUMBER | ES_CENTER, 685, 437, 72, 27, IDC_SEQ_POST_CLEANUP);
            MakeIn(parent, L"STATIC", L"mặc định 1000", 0, 765, 442, 100, 24, 0);
        }
        MakeIn(parent, L"STATIC", tradeEditorMode_ == 1
            ? L"MAIN: RAW hoặc UI DIRECT. UI DIRECT retry 50ms, TIME MAX 2000ms như ĐẶT LÊN; TIME MIN mặc định 0."
            : L"CON: có thể xếp RAW/UI DIRECT tùy ý. Sau pass: MAIN đóng X GD+popup; CON đóng X GD+popup+tay nải.", 0, 15, 480, 850, 23, 0);
        PopulateTradeTargetCombo(); PopulateTradeActionCombo(); RefreshTradeSequenceList();
    }

    void OpenTradeSequenceEditor(int mode) {
        Account* selected = SelectedAccount();
        if (mode == 1 && (!selected || selected->profile.tradeRole != 1)) { Log(L"Chỉ acc MAIN mới mở CHUỖI GD MAIN."); return; }
        if (mode == 2 && (!selected || selected->profile.tradeRole < 2)) { Log(L"Chọn một CON bất kỳ để mở CHUỖI GD ACC CON dùng chung."); return; }
        if (tradeEditor_ && IsWindow(tradeEditor_)) DestroyWindow(tradeEditor_);
        tradeEditorMode_ = mode;
        tradeEditorChildPid_ = mode == 2 ? selected->game.pid : 0;
        if (mode == 2) EnsureSharedChildTradeSequence();
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = TradeEditorWndProc; wc.hInstance = instance_; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); wc.lpszClassName = L"ThanLongTradeSequenceEditorV03";
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { Log(L"Không đăng ký được cửa sổ chuỗi GD."); return; }
        const wchar_t* title = mode == 1 ? L"Thần Long • CHUỖI GD MAIN (DÙNG CHUNG) v0.3 • REC"
                                         : L"Thần Long • CHUỖI GD ACC CON (DÙNG CHUNG) v0.3 • REC + NHÓM LẶP";
        tradeEditor_ = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 900, 590, hwnd_, nullptr, instance_, this);
        if (!tradeEditor_) { Log(L"Không mở được cửa sổ chuỗi GD."); return; }
        BuildTradeEditorUi(tradeEditor_); ShowWindow(tradeEditor_, SW_SHOW); UpdateWindow(tradeEditor_);
    }

    LRESULT HandleTradeEditor(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_NOTIFY: {
                auto* hdr = reinterpret_cast<NMHDR*>(lp);
                if (hdr && hdr->idFrom == IDC_SEQ_LIST && hdr->code == LVN_ITEMCHANGED) {
                    const auto* n = reinterpret_cast<const NMLISTVIEW*>(hdr);
                    if (!tradeSeqDragUpdating_ && (n->uChanged & LVIF_STATE) != 0 && (n->uNewState & LVIS_SELECTED) != 0)
                        LoadTradeSequenceRowToEditor(n->iItem);
                }
                return 0;
            }
            case WM_COMMAND:
                switch (LOWORD(wp)) {
                    case IDC_SEQ_ADD: AddTradeSequenceRow(); return 0;
                    case IDC_SEQ_DELETE: DeleteTradeSequenceRow(); return 0;
                    case IDC_SEQ_UP: MoveTradeSequenceRow(-1); return 0;
                    case IDC_SEQ_DOWN: MoveTradeSequenceRow(1); return 0;
                    case IDC_SEQ_SAVE: SaveTradeSequenceRowFromEditor(); return 0;
                    case IDC_SEQ_CAPTURE: BeginTradeSequenceCapture(); return 0;
                    case IDC_SEQ_TEST: TestTradeSequenceRow(); return 0;
                    case IDC_SEQ_REC: ToggleTradeRecorder(); return 0;
                    case IDC_SEQ_COPY: CopySelectedTradeRows(); return 0;
                    case IDC_SEQ_PASTE: PasteTradeRows(); return 0;
                    case IDC_SEQ_GROUP_SELECTED: GroupSelectedTradeRows(); return 0;
                    case IDC_SEQ_UNGROUP: UngroupSelectedTradeRows(); return 0;
                    case IDC_SEQ_CLOSE: if (RecorderModeIsTrade(recorderMode_)) StopRecorder(true); DestroyWindow(hwnd); return 0;
                    case IDC_SEQ_TARGET: if (HIWORD(wp) == CBN_SELCHANGE) SaveTradeSequenceRowFromEditor(); return 0;
                    case IDC_SEQ_ACTION: if (HIWORD(wp) == CBN_SELCHANGE) SaveTradeSequenceRowFromEditor(); return 0;
                    case IDC_SEQ_POST_CLEANUP: if (HIWORD(wp) == EN_KILLFOCUS) SaveTradeSequenceRowFromEditor(); return 0;
                    case IDC_SEQ_AFTER_PUTUP: if (HIWORD(wp) == BN_CLICKED) SaveTradeSequenceRowFromEditor(); return 0;
                }
                break;
            case WM_CLOSE: if (RecorderModeIsTrade(recorderMode_)) StopRecorder(true); DestroyWindow(hwnd); return 0;
            case WM_NCDESTROY:
                tradeEditor_ = nullptr; tradeSeqList_ = nullptr; tradeSeqTarget_ = nullptr; tradeSeqDesc_ = nullptr; tradeSeqDelay_ = nullptr; tradeSeqRepeat_ = nullptr; tradeSeqGroupRepeat_ = nullptr; tradeSeqAfterPutUp_ = nullptr; tradeSeqAction_ = nullptr; tradeSeqMinTime_ = nullptr; tradeSeqPostCleanup_ = nullptr; tradeRecordButton_ = nullptr; tradeRecordStatus_ = nullptr;
                captureTradeSequenceIndex_ = -1; captureTradeSequenceMode_ = 0; captureTradeSequenceMainRef_ = -1; tradeEditorMode_ = 0; tradeEditorChildPid_ = 0;
                return DefWindowProcW(hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static int ParseEditInt(HWND edit, int fallback, int lo, int hi) {
        if (!edit) return fallback;
        wchar_t buf[64]{};
        GetWindowTextW(edit, buf, _countof(buf));
        wchar_t* end = nullptr;
        long value = wcstol(buf, &end, 10);
        if (end == buf) value = fallback;
        return std::clamp(static_cast<int>(value), lo, hi);
    }

    void RefreshShortcutSellerUi() {
        if (!shortcutSellerCombo_) return;
        LRESULT sel = SendMessageW(shortcutSellerCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || sel >= static_cast<LRESULT>(kSellNpcs.size())) {
            sel = 0; SendMessageW(shortcutSellerCombo_, CB_SETCURSEL, 0, 0);
        }
        const std::size_t i = static_cast<std::size_t>(sel);
        const auto& pos = sellNpcPositions_[i];
        if (shortcutSellerCoordLabel_) {
            SetText(shortcutSellerCoordLabel_, pos.valid
                ? (L"ĐÃ GÁN: " + std::to_wstring(pos.x) + L"," + std::to_wstring(pos.y))
                : L"CHƯA GÁN • đứng sát NPC rồi bấm LẤY TỌA NPC BÁN");
        }
    }

    void LoadShortcutSettingsToUi() {
        if (!shortcutWindow_) return;
        if (shortcutTheme_) SendMessageW(shortcutTheme_, CB_SETCURSEL, shortcutSettings_.theme, 0);
        const int values[8] = {
            shortcutSettings_.kunlunNpcX, shortcutSettings_.kunlunNpcY,
            shortcutSettings_.xaTruyenX, shortcutSettings_.xaTruyenY,
            shortcutSettings_.ngaiX, shortcutSettings_.ngaiY,
            shortcutSettings_.tinhTucX, shortcutSettings_.tinhTucY,
        };
        for (std::size_t i = 0; i < shortcutCoordEdits_.size(); ++i) {
            if (shortcutCoordEdits_[i]) SetWindowTextW(shortcutCoordEdits_[i], std::to_wstring(values[i]).c_str());
        }
        for (std::size_t i = 0; i < shortcutSettings_.kunlunExitClicks.size(); ++i) {
            const TimedClickPoint& click = shortcutSettings_.kunlunExitClicks[i];
            if (shortcutClickLabels_[i]) {
                SetText(shortcutClickLabels_[i], click.point.valid
                    ? PointDescription(click.point)
                    : L"CHƯA GÁN • đưa chuột vào game rồi F8");
            }
            if (shortcutClickTimeEdits_[i]) SetText(shortcutClickTimeEdits_[i], std::to_wstring(click.timeMs));
            if (shortcutClickDelayEdits_[i]) SetText(shortcutClickDelayEdits_[i], std::to_wstring(click.delayMs));
        }
        if (shortcutPostTradeEnabled_)
            SendMessageW(shortcutPostTradeEnabled_, BM_SETCHECK, shortcutSettings_.postTradeClickEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
        if (shortcutPostTradePointLabel_)
            SetText(shortcutPostTradePointLabel_, shortcutSettings_.postTradeClick.valid
                ? PointDescription(shortcutSettings_.postTradeClick)
                : L"CHƯA GÁN • chọn 1 acc mẫu, đưa chuột vào game rồi F8");
        if (shortcutPostTradeDelay_) SetText(shortcutPostTradeDelay_, std::to_wstring(shortcutSettings_.postTradeClickDelayMs));
        if (shortcutPostTradeRepeat_) SetText(shortcutPostTradeRepeat_, std::to_wstring(shortcutSettings_.postTradeClickRepeat));
        RefreshShortcutSellerUi();
    }

    void PersistShortcutSettingsFromUi(bool logSaved = true) {
        if (shortcutTheme_) {
            const LRESULT sel = SendMessageW(shortcutTheme_, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) shortcutSettings_.theme = std::clamp(static_cast<int>(sel), 0, 1);
        }
        if (shortcutCoordEdits_[0]) {
            auto read = [&](std::size_t i, int fallback){ return shortcutCoordEdits_[i] ? ParseEditInt(shortcutCoordEdits_[i], fallback, 0, 1000000) : fallback; };
            shortcutSettings_.kunlunNpcX = read(0, shortcutSettings_.kunlunNpcX); shortcutSettings_.kunlunNpcY = read(1, shortcutSettings_.kunlunNpcY);
            shortcutSettings_.xaTruyenX = read(2, shortcutSettings_.xaTruyenX); shortcutSettings_.xaTruyenY = read(3, shortcutSettings_.xaTruyenY);
            shortcutSettings_.ngaiX = read(4, shortcutSettings_.ngaiX); shortcutSettings_.ngaiY = read(5, shortcutSettings_.ngaiY);
            shortcutSettings_.tinhTucX = read(6, shortcutSettings_.tinhTucX); shortcutSettings_.tinhTucY = read(7, shortcutSettings_.tinhTucY);
        }
        for (std::size_t i = 0; i < shortcutSettings_.kunlunExitClicks.size(); ++i) {
            TimedClickPoint& click = shortcutSettings_.kunlunExitClicks[i];
            if (shortcutClickTimeEdits_[i])
                click.timeMs = ParseEditInt(shortcutClickTimeEdits_[i], click.timeMs, 0, 60000);
            if (shortcutClickDelayEdits_[i])
                click.delayMs = ParseEditInt(shortcutClickDelayEdits_[i], click.delayMs, 0, 60000);
        }
        if (shortcutPostTradeEnabled_)
            shortcutSettings_.postTradeClickEnabled = SendMessageW(shortcutPostTradeEnabled_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (shortcutPostTradeDelay_)
            shortcutSettings_.postTradeClickDelayMs = ParseEditInt(shortcutPostTradeDelay_, shortcutSettings_.postTradeClickDelayMs, 0, 60000);
        if (shortcutPostTradeRepeat_)
            shortcutSettings_.postTradeClickRepeat = ParseEditInt(shortcutPostTradeRepeat_, shortcutSettings_.postTradeClickRepeat, 0, 999);
        SaveShortcutSettings(shortcutSettings_);
        if (logSaved) Log(L"TÙY CHỈNH 10.2: đã lưu tọa + 3 click CLS Time/Delay + Click Sau Target Main.");
    }

    void ApplyShortcutPanelTheme(HWND hwnd) {
        if (!hwnd) return;
        if (shortcutSettings_.theme == 1 && !shortcutDarkBrush_) shortcutDarkBrush_ = CreateSolidBrush(RGB(35, 35, 35));
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
    }

    void CaptureShortcutCoordinate(int index) {
        if (index < 0 || index >= 4) return;
        Account* a = SelectedAccount();
        if (!a) { Log(L"TÙY CHỈNH TỌA: chọn 1 acc đang đứng đúng điểm trước."); return; }
        std::wstring error;
        if (!ReadSnapshot(*a, error, 1200)) { LogAccount(*a, L"Không đọc được state để LẤY TỌA: " + error); return; }
        const Snapshot& snap = a->snapshot;
        if ((snap.validMask & (ValidMap | ValidPosition)) != (ValidMap | ValidPosition)) { LogAccount(*a, L"State chưa có Map/X/Y để LẤY TỌA"); return; }
        static constexpr int expectedMaps[4] = {75, 5, 5, 12};
        static constexpr const wchar_t* labels[4] = {
            L"NPC RỜI Côn Lôn Sơn", L"Xa Truyền Bình • ResID 387 • ĐI VÀO Côn Lôn",
            L"Ngải Ni Ngoã Nhĩ • ResID 913", L"Tinh Túc Hải điểm ra"
        };
        if (snap.mapID != expectedMaps[index]) {
            LogAccount(*a, std::wstring(L"KHÔNG LƯU ") + labels[index] + L": đang M" + std::to_wstring(snap.mapID) +
                           L" nhưng điểm này phải gán ở M" + std::to_wstring(expectedMaps[index]));
            return;
        }
        PersistShortcutSettingsFromUi(false);
        int* xs[4] = {&shortcutSettings_.kunlunNpcX,&shortcutSettings_.xaTruyenX,&shortcutSettings_.ngaiX,&shortcutSettings_.tinhTucX};
        int* ys[4] = {&shortcutSettings_.kunlunNpcY,&shortcutSettings_.xaTruyenY,&shortcutSettings_.ngaiY,&shortcutSettings_.tinhTucY};
        *xs[index]=snap.x; *ys[index]=snap.y;
        SaveShortcutSettings(shortcutSettings_); LoadShortcutSettingsToUi();
        for (auto& item : accounts_) if (item) ResetShortcutRoute(item->runtime);
        LogAccount(*a, std::wstring(L"COORD CAPTURE RAW SHORTCUT • ") + labels[index] + L" • M" + std::to_wstring(snap.mapID) + L" • " +
                       std::to_wstring(snap.x) + L"," + std::to_wstring(snap.y));
    }

    void CaptureShortcutSellerPosition() {
        Account* a=SelectedAccount();
        if(!a){Log(L"TÙY CHỈNH NPC BÁN: chọn 1 acc đứng sát NPC trước.");return;}
        if(!shortcutSellerCombo_) return;
        const LRESULT sel=SendMessageW(shortcutSellerCombo_,CB_GETCURSEL,0,0);
        if(sel==CB_ERR||sel<0||sel>=static_cast<LRESULT>(kSellNpcs.size())) return;
        std::wstring error;
        if(!ReadSnapshot(*a,error,1200)){LogAccount(*a,L"Không đọc state để lấy tọa NPC bán: "+error);return;}
        const auto& npc=kSellNpcs[static_cast<std::size_t>(sel)]; const auto& snap=a->snapshot;
        if((snap.validMask&(ValidMap|ValidPosition))!=(ValidMap|ValidPosition)){LogAccount(*a,L"State thiếu Map/X/Y");return;}
        if(snap.mapID!=npc.mapID){LogAccount(*a,L"KHÔNG LƯU: "+std::wstring(npc.name)+L" thuộc M"+std::to_wstring(npc.mapID)+L", hiện đang M"+std::to_wstring(snap.mapID));return;}
        auto& pos=sellNpcPositions_[static_cast<std::size_t>(sel)]; pos={snap.x,snap.y,true}; SaveSharedSellNpcPositions(sellNpcPositions_);
        RefreshShortcutSellerUi();
        LogAccount(*a,L"ĐÃ GÁN NPC BÁN • "+std::wstring(npc.name)+L" • ResID "+std::to_wstring(npc.npcID)+L" • "+std::to_wstring(pos.x)+L","+std::to_wstring(pos.y));
    }

    void BuildShortcutSettingsUi(HWND parent) {
        MakeIn(parent, L"STATIC", L"TÙY CHỈNH 10.2 • mọi tọa đường tắt đều sửa tay hoặc LẤY TỌA. 0,0 = chưa gán / fail-closed.",
               SS_LEFT | SS_CENTERIMAGE | WS_BORDER, 15, 12, 930, 32, 0);
        MakeIn(parent, L"STATIC", L"Theme:", SS_LEFT | SS_CENTERIMAGE, 15, 54, 60, 25, 0);
        shortcutTheme_ = MakeIn(parent, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST, 78, 52, 170, 160, IDC_SC_THEME);
        SendMessageW(shortcutTheme_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sáng / hệ thống"));
        SendMessageW(shortcutTheme_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Tối"));

        struct Row { const wchar_t* label; int idX; int idY; int captureId; };
        const Row rows[4] = {
            {L"NPC RỜI Côn Lôn • M75", IDC_SC_KUNLUN_X, IDC_SC_KUNLUN_Y, IDC_SC_CAPTURE_COORD_0},
            {L"Xa Truyền Bình • ID387 • ĐI VÀO Côn Lôn • M5", IDC_SC_XA_X, IDC_SC_XA_Y, IDC_SC_CAPTURE_COORD_1},
            {L"Ngải Ni Ngoã Nhĩ • ResID 913 • M5", IDC_SC_NGAI_X, IDC_SC_NGAI_Y, IDC_SC_CAPTURE_COORD_2},
            {L"Tinh Túc Hải điểm ra • M12", IDC_SC_TINHTUC_X, IDC_SC_TINHTUC_Y, IDC_SC_CAPTURE_COORD_3},
        };
        MakeIn(parent,L"STATIC",L"ĐƯỜNG TẮT HIỆN CÓ",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,15,86,450,25,0);
        auto drawCoordinateRow = [&](int x, int y, const Row& row, std::size_t editOffset) {
            MakeIn(parent,L"STATIC",row.label,SS_LEFT|SS_CENTERIMAGE,x,y,220,27,0);
            MakeIn(parent,L"STATIC",L"X",SS_CENTERIMAGE,x+220,y,14,27,0);
            shortcutCoordEdits_[editOffset]=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,x+234,y,58,27,row.idX);
            MakeIn(parent,L"STATIC",L"Y",SS_CENTERIMAGE,x+294,y,14,27,0);
            shortcutCoordEdits_[editOffset+1]=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,x+308,y,58,27,row.idY);
            MakeIn(parent,L"BUTTON",L"LẤY TỌA",BS_PUSHBUTTON,x+371,y,79,27,row.captureId);
        };
        for (int i=0;i<4;++i) {
            // Xa Truyền Bình ID387 uses ONLY the main Auto-Sell NPC coordinate source.
            if (i == 1) {
                shortcutCoordEdits_[2] = nullptr;
                shortcutCoordEdits_[3] = nullptr;
                continue;
            }
            const int visualIndex = i > 1 ? i - 1 : i;
            drawCoordinateRow(15, 116+visualIndex*36, rows[i], static_cast<std::size_t>(i*2));
        }
        MakeIn(parent,L"STATIC",L"RỜI CÔN LÔN • ĐÚNG 3 TRYCLICKUI • Time (ms)=chờ trước click, Delay (ms)=chờ sau click; không dùng callback Đại Lý/Xác nhận.",
               SS_LEFT|SS_CENTERIMAGE|WS_BORDER,15,380,930,30,0);
        static constexpr const wchar_t* clickNames[3] = {L"1. Mở NPC rời CLS", L"2. Chọn Đại Lý", L"3. Xác nhận"};
        static constexpr int captureIds[3] = {IDC_SC_CAPTURE_KUNLUN_CLICK_0,IDC_SC_CAPTURE_KUNLUN_CLICK_1,IDC_SC_CAPTURE_KUNLUN_CLICK_2};
        static constexpr int timeIds[3] = {IDC_SC_KUNLUN_TIME_0,IDC_SC_KUNLUN_TIME_1,IDC_SC_KUNLUN_TIME_2};
        static constexpr int delayIds[3] = {IDC_SC_KUNLUN_DELAY_0,IDC_SC_KUNLUN_DELAY_1,IDC_SC_KUNLUN_DELAY_2};
        for (int i=0;i<3;++i) {
            const int y=418+i*38;
            MakeIn(parent,L"STATIC",clickNames[i],SS_LEFT|SS_CENTERIMAGE,15,y,150,28,0);
            shortcutClickLabels_[static_cast<std::size_t>(i)]=MakeIn(parent,L"STATIC",L"",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,165,y,350,28,0);
            MakeIn(parent,L"STATIC",L"Time",SS_CENTERIMAGE,520,y,38,28,0);
            shortcutClickTimeEdits_[static_cast<std::size_t>(i)]=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,558,y,65,28,timeIds[i]);
            MakeIn(parent,L"STATIC",L"Delay",SS_CENTERIMAGE,628,y,45,28,0);
            shortcutClickDelayEdits_[static_cast<std::size_t>(i)]=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,673,y,65,28,delayIds[i]);
            MakeIn(parent,L"BUTTON",L"LẤY CLICK (F8)",BS_PUSHBUTTON,748,y,130,28,captureIds[i]);
        }
        shortcutSellerCombo_ = nullptr;
        shortcutSellerCoordLabel_ = nullptr;
        MakeIn(parent,L"STATIC",L"NPC BÁN / XA TRUYỀN BÌNH: chỉ dùng tọa ở màn hình chính → chọn NPC bán → LẤY VỊ TRÍ. Không còn nguồn tọa thứ hai trong TÙY CHỈNH.",
               SS_LEFT|SS_CENTERIMAGE|WS_BORDER,15,542,930,38,0);
        MakeIn(parent,L"STATIC",L"CLICK SAU TARGET MAIN • CHỈ CON • mở menu trước callback Giao dịch, sau đó mới chạy CHUỖI GD",
               SS_LEFT|SS_CENTERIMAGE|WS_BORDER,15,648,930,30,0);
        shortcutPostTradeEnabled_=MakeIn(parent,L"BUTTON",L"Bật",BS_AUTOCHECKBOX,15,686,72,28,IDC_SC_POST_TRADE_ENABLED);
        shortcutPostTradePointLabel_=MakeIn(parent,L"STATIC",L"",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,90,686,405,28,0);
        MakeIn(parent,L"STATIC",L"Delay",SS_CENTERIMAGE,502,686,42,28,0);
        shortcutPostTradeDelay_=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,545,686,64,28,IDC_SC_POST_TRADE_DELAY);
        MakeIn(parent,L"STATIC",L"Repeat",SS_CENTERIMAGE,640,686,48,28,0);
        shortcutPostTradeRepeat_=MakeIn(parent,L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,690,686,60,28,IDC_SC_POST_TRADE_REPEAT);
        MakeIn(parent,L"BUTTON",L"LẤY CLICK (F8)",BS_PUSHBUTTON,760,686,143,28,IDC_SC_POST_TRADE_CAPTURE);
        MakeIn(parent,L"STATIC",L"ms",SS_LEFT|SS_CENTERIMAGE,612,686,24,28,0);

        MakeIn(parent,L"BUTTON",L"LƯU",BS_DEFPUSHBUTTON,705,812,105,32,IDC_SC_SAVE);
        MakeIn(parent,L"BUTTON",L"ĐÓNG",BS_PUSHBUTTON,820,812,125,32,IDC_SC_CLOSE);
        LoadShortcutSettingsToUi(); ApplyShortcutPanelTheme(parent);
    }

    void OpenShortcutSettingsWindow() {
        if (shortcutWindow_ && IsWindow(shortcutWindow_)) { ShowWindow(shortcutWindow_,SW_SHOW); SetForegroundWindow(shortcutWindow_); return; }
        WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=ShortcutWndProc; wc.hInstance=instance_;
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); wc.lpszClassName=L"ThanLongShortcutSettingsV30";
        if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){Log(L"Không đăng ký được cửa sổ Đường tắt.");return;}
        shortcutWindow_=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Công cụ hỗ trợ game rảnh tay • 10.6 • TÙY CHỈNH",
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,980,900,hwnd_,nullptr,instance_,this);
        if(!shortcutWindow_){Log(L"Không mở được cửa sổ Đường tắt.");return;}
        BuildShortcutSettingsUi(shortcutWindow_); ShowWindow(shortcutWindow_,SW_SHOW); UpdateWindow(shortcutWindow_);
    }

    void BeginKunlunExitClickCapture(int index) {
        if (index < 0 || index >= 3) return;
        Account* a=SelectedAccount();
        if(!a){Log(L"ĐƯỜNG TẮT: chọn 1 acc mẫu trước khi lấy 3 click RỜI Côn Lôn.");return;}
        shortcutPostTradeCapture_=false;
        shortcutKunlunCaptureIndex_=index; captureSlot_=ClickSlot::None; captureTradeSequenceIndex_=-1;
        captureTradeSequenceMode_=0; captureTradeSequenceMainRef_=-1; capturePid_=a->game.pid;
        static constexpr const wchar_t* labels[3] = {L"mở NPC rời Côn Lôn",L"chọn dòng Đại Lý",L"bấm Xác nhận"};
        LogAccount(*a,L"ĐƯỜNG TẮT CLS: đưa chuột đúng điểm \""+std::wstring(labels[index])+L"\" rồi nhấn F8 • click "+
                      std::to_wstring(index+1)+L"/3 • dùng chung ALL ACC.");
    }

    void BeginPostTradeClickCapture() {
        Account* a = SelectedAccount();
        if (!a) {
            Log(L"CLICK SAU TARGET MAIN: chọn 1 acc mẫu trước khi LẤY CLICK F8.");
            return;
        }
        shortcutPostTradeCapture_ = true;
        shortcutKunlunCaptureIndex_ = -1;
        
        captureSlot_ = ClickSlot::None;
        captureTradeSequenceIndex_ = -1;
        captureTradeSequenceMode_ = 0;
        captureTradeSequenceMainRef_ = -1;
        capturePid_ = a->game.pid;
        LogAccount(*a, L"CLICK SAU TARGET MAIN: đưa chuột vào đúng điểm trong game rồi F8 • một tọa dùng cho MAIN + CON vừa giao dịch.");
    }

    LRESULT HandleShortcutWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch(msg){
            case WM_COMMAND:
                switch(LOWORD(wp)){
                    case IDC_SC_CAPTURE_KUNLUN_CLICK_0: case IDC_SC_CAPTURE_KUNLUN_CLICK_1: case IDC_SC_CAPTURE_KUNLUN_CLICK_2:
                        if(HIWORD(wp)==BN_CLICKED) BeginKunlunExitClickCapture(LOWORD(wp)-IDC_SC_CAPTURE_KUNLUN_CLICK_0); return 0;
                    case IDC_SC_CAPTURE_COORD_0: case IDC_SC_CAPTURE_COORD_2: case IDC_SC_CAPTURE_COORD_3:
                        if(HIWORD(wp)==BN_CLICKED) CaptureShortcutCoordinate(LOWORD(wp)-IDC_SC_CAPTURE_COORD_0); return 0;
                    case IDC_SC_SELLER_CAPTURE: if(HIWORD(wp)==BN_CLICKED) CaptureShortcutSellerPosition(); return 0;
                    case IDC_SC_SELLER_COMBO: if(HIWORD(wp)==CBN_SELCHANGE) RefreshShortcutSellerUi(); return 0;
                    case IDC_SC_POST_TRADE_CAPTURE: if(HIWORD(wp)==BN_CLICKED) BeginPostTradeClickCapture(); return 0;
                    case IDC_SC_POST_TRADE_ENABLED: if(HIWORD(wp)==BN_CLICKED) PersistShortcutSettingsFromUi(false); return 0;
                    case IDC_SC_POST_TRADE_DELAY: case IDC_SC_POST_TRADE_REPEAT:
                        if(HIWORD(wp)==EN_KILLFOCUS) PersistShortcutSettingsFromUi(false); return 0;
                    case IDC_SC_SAVE: if(HIWORD(wp)==BN_CLICKED){PersistShortcutSettingsFromUi();ApplyShortcutPanelTheme(hwnd);LoadShortcutSettingsToUi();} return 0;
                    case IDC_SC_CLOSE: if(HIWORD(wp)==BN_CLICKED){PersistShortcutSettingsFromUi(false);ShowWindow(hwnd,SW_HIDE);} return 0;
                    case IDC_SC_THEME: if(HIWORD(wp)==CBN_SELCHANGE){PersistShortcutSettingsFromUi(false);ApplyShortcutPanelTheme(hwnd);} return 0;
                } break;
            case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT:
                if(shortcutSettings_.theme==1){HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,RGB(235,235,235));SetBkColor(dc,RGB(35,35,35));if(!shortcutDarkBrush_)shortcutDarkBrush_=CreateSolidBrush(RGB(35,35,35));return reinterpret_cast<LRESULT>(shortcutDarkBrush_);} break;
            case WM_ERASEBKGND:
                if(shortcutSettings_.theme==1){RECT rc{};GetClientRect(hwnd,&rc);if(!shortcutDarkBrush_)shortcutDarkBrush_=CreateSolidBrush(RGB(35,35,35));FillRect(reinterpret_cast<HDC>(wp),&rc,shortcutDarkBrush_);return 1;} break;
            case WM_CLOSE: PersistShortcutSettingsFromUi(false); ShowWindow(hwnd,SW_HIDE); return 0;
            case WM_NCDESTROY:
                shortcutWindow_=nullptr;shortcutTheme_=nullptr;shortcutSellerCombo_=nullptr;shortcutSellerCoordLabel_=nullptr;
                shortcutPostTradeEnabled_=nullptr;shortcutPostTradePointLabel_=nullptr;shortcutPostTradeDelay_=nullptr;shortcutPostTradeRepeat_=nullptr;
                shortcutCoordEdits_.fill(nullptr);shortcutClickLabels_.fill(nullptr);shortcutClickTimeEdits_.fill(nullptr);
                shortcutClickDelayEdits_.fill(nullptr);return 0;
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }


    void UpdateRoleActionButtons() {
        Account* a = SelectedAccount();
        const bool hasAccount = a != nullptr;
        const int role = a ? a->profile.tradeRole : 0;
        if (sellSequenceButton_) ShowWindow(sellSequenceButton_, hasAccount && role == kMainTradeRole ? SW_SHOW : SW_HIDE);
        if (mainTradeSequenceButton_) ShowWindow(mainTradeSequenceButton_, role == 1 ? SW_SHOW : SW_HIDE);
        if (childTradeSequenceButton_) {
            ShowWindow(childTradeSequenceButton_, role >= 2 ? SW_SHOW : SW_HIDE);
            if (role >= 2) SetWindowTextW(childTradeSequenceButton_, L"CHUỖI GD ACC CON");
        }
        if (tradeRendezvousCaptureButton_) ShowWindow(tradeRendezvousCaptureButton_, SW_SHOW);
        if (tradeRendezvousLabel_) ShowWindow(tradeRendezvousLabel_, SW_SHOW);
    }

    void PersistGlobalTradeSettings() {
        WriteIniInt(L"Global", L"TradeEnabled", 1);
        WriteIniInt(L"Global", L"ChildTriggerFreeSlots", 0);
        WriteIniInt(L"Global", L"TradeRendezvousMap", tradeRendezvous_.mapID);
        WriteIniInt(L"Global", L"TradeRendezvousX", tradeRendezvous_.x);
        WriteIniInt(L"Global", L"TradeRendezvousY", tradeRendezvous_.y);
        WriteIniInt(L"Global", L"TradeRendezvousValid", tradeRendezvous_.valid ? 1 : 0);
        WriteIniInt(L"Global", L"TradeRendezvousTolerance", tradeRendezvousTolerance_);
        FlushIni();
    }

    void UpdateConsolidationButton() {
        if (!tradeEnable_) return;
        SetWindowTextW(tradeEnable_, L"DỒN ĐỒ: BẮT BUỘC");
    }

    void ToggleConsolidationMode() {
        tradeEnabled_ = true;
        MessageBoxW(hwnd_, L"Auto dồn đồ là tính năng bắt buộc của bản 10 và không thể tắt trong session.",
                    kTitle, MB_OK | MB_ICONINFORMATION);
        WriteIniInt(L"Global", L"TradeEnabled", 1);
        FlushIni();
        UpdateConsolidationButton();
    }


    void SetTradeStatus(const std::wstring& text) {
        if (!tradeStatus_) return;
        const std::wstring line = L"ĐIỀU PHỐI: " + text;
        SetWindowTextW(tradeStatus_, line.c_str());
    }

    void ApplySelectedTradeRole() {
        Account* selected = SelectedAccount();
        if (!selected || !tradeRoleCombo_) return;
        const LRESULT sel = SendMessageW(tradeRoleCombo_, CB_GETCURSEL, 0, 0);
        if (sel == CB_ERR || sel < 0 || sel > 1) return;

        std::wstring mainIdentity = LoadDesignatedMainIdentity();
        const std::wstring selectedIdentity = ProfileSection(selected->snapshot, selected->game.pid);
        if (sel == 1) {
            mainIdentity = selectedIdentity;
            SaveDesignatedMainIdentity(mainIdentity);
        } else if (selected->profile.tradeRole == kMainTradeRole ||
                   mainIdentity == selectedIdentity || mainIdentity == PidProfileSection(selected->game.pid)) {
            mainIdentity.clear();
            SaveDesignatedMainIdentity(mainIdentity);
        }

        ReleaseTradeHolds();
        ResetTradeTxn();
        NormalizeAutomaticTradeRoles(mainIdentity);
        for (std::size_t i = 0; i < accounts_.size(); ++i)
            UpdateAccountRow(static_cast<int>(i), *accounts_[i]);
        RefreshAccountGroups();
        LoadSelectedProfileToUi();
        if (mainIdentity.empty())
            Log(L"MAIN đã bỏ chỉ định; mọi client online là CON AUTO và trade bị chặn.");
        else
            Log(L"MAIN đã chỉ định theo identity; mọi client khác tự đánh CON1..CONN.");
    }

    Account* AccountByTradeRole(int role) {
        for (auto& a : accounts_) if (a->profile.tradeRole == role) return a.get();
        return nullptr;
    }

    bool TradeStateReady(const Account& a) const {
        if (!a.runtime.running || !a.snapshotValid || !IsWindow(a.game.window)) return false;
        const Snapshot& s = a.snapshot;
        const std::uint32_t need = ValidLifeState | ValidBagSpace | ValidMap | ValidPosition;
        if ((s.validMask & need) != need) return false;
        if (s.dead || !s.mapReady || s.waitingChangeMap) return false;
        if (a.runtime.clientFreezeActive || a.runtime.revivePhase != 0 || a.runtime.sellPhase != 0 ||
            a.runtime.trainRecoveryPhase != 0 || a.runtime.routeOwnershipResetPending) return false;
        return true;
    }

    bool TradePairReadyForPreparation(const Account& main, const Account& child) const {
        if (!main.runtime.running || !child.runtime.running) return false;
        if (AccountUiRecoveryActive(main) || AccountUiRecoveryActive(child)) return false;
        if (!main.snapshotValid || !child.snapshotValid) return false;
        const Snapshot& ms = main.snapshot;
        const Snapshot& cs = child.snapshot;
        const std::uint32_t need = ValidLifeState | ValidBagSpace | ValidMap | ValidPosition | ValidAutoFight | ValidAutoPath | ValidRiding;
        if ((ms.validMask & need) != need || (cs.validMask & need) != need) return false;
        if (ms.dead || cs.dead || !ms.mapReady || ms.waitingChangeMap || !cs.mapReady || cs.waitingChangeMap) return false;
        return IsWindow(main.game.window) && IsWindow(child.game.window);
    }

    void ResetTradeRendezvousTravel(Account& a) {
        RuntimeState& rt = a.runtime;
        rt.tradeTravelPhase = 0;
        rt.tradeTravelTick = 0;
        rt.tradeTravelReady = false;
        ResetRobustTravel(rt);
    }

    bool TradeAccountAtRendezvous(const Account& a) const {
        if (!tradeRendezvous_.valid || !a.snapshotValid) return false;
        const Snapshot& s = a.snapshot;
        const std::uint32_t need = ValidLifeState | ValidMap | ValidPosition | ValidAutoPath | ValidRiding;
        if ((s.validMask & need) != need || s.dead || !s.mapReady || s.waitingChangeMap) return false;
        State state{};
        state.valid = true; state.mapReady = true; state.waitingMap = false;
        state.mapID = s.mapID; state.x = s.x; state.y = s.y;
        state.autoPathing = s.autoPathing != 0; state.riding = s.riding != 0;
        Target target{tradeRendezvous_.mapID, tradeRendezvous_.x, tradeRendezvous_.y, tradeRendezvousTolerance_};
        // CP10: mounted arrival is a valid rendezvous. Do not wait for dismount.
        return AtTarget(state, target) && !s.autoPathing;
    }

    void BeginTradeRendezvousTravel(Account& a, DWORD now, const wchar_t* who) {
        RuntimeState& rt = a.runtime;
        ResetTradeRendezvousTravel(a);
        rt.tradeTravelPhase = 4;
        rt.tradeTravelTick = now;
        rt.trainPositionMonitorArmed = false;
        rt.lastTrainPositionCheckTick = 0;
        rt.fightPhase = 0;
        rt.fightAttempts = 0;
        rt.fightRetryWaitTick = 0;

        // The old train AutoPath belongs to the normal core and must not survive into
        // a consolidation rendezvous. This StopPath is internal and does not touch F4 state.
        if (a.bridge.Attached() && a.snapshotValid && (a.snapshot.validMask & ValidAutoPath) && a.snapshot.autoPathing) {
            Response r{}; std::wstring ignored;
            (void)a.bridge.Call(Command::StopPath, 0, 0, 0, r, ignored, 700);
        }
        LogAccount(a, L"GD TỌA: HOLD " + std::wstring(who ? who : L"ACC") +
                      L" • hủy AutoPath bãi cũ → v0.3 Travel Guard bắt buộc AutoFight OFF → cùng đi TỌA GD.");
    }

    bool HandleTradeRendezvousTravel(Account& a, DWORD now, const wchar_t* who) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        const std::wstring tag = who ? who : L"ACC";

        if (!tradeRendezvous_.valid || !a.runtime.running || !a.snapshotValid || !IsWindow(a.game.window)) return false;
        if (rt.autoPathFightConflictLatched) {
            rt.status = L"GD TỌA • chờ ROUTE/FIGHT INVARIANT recovery";
            return true;
        }
        const std::uint32_t need = ValidLifeState | ValidMap | ValidPosition | ValidAutoPath | ValidRiding;
        if ((s.validMask & need) != need || s.dead || !s.mapReady || s.waitingChangeMap) return false;

        // Once ready, keep the first-arriving account parked at TỌA GD. Any stale/automatic
        // path that reappears is stopped before the coordinator can advance the transaction.
        if (rt.tradeTravelReady) {
            if (s.autoPathing) {
                Response r{}; std::wstring ignored;
                if (a.bridge.Attached()) (void)a.bridge.Call(Command::StopPath, 0, 0, 0, r, ignored, 700);
                rt.status = L"GD HOLD • " + tag + L" đã tới TỌA GD • chặn AutoPath bãi cũ";
                return true;
            }
            // CP9: tới TỌA GD giữ nguyên trạng thái cưỡi; không Dismount.
            if (!TradeAccountAtRendezvous(a)) {
                rt.tradeTravelReady = false;
                rt.tradeTravelPhase = 4;
                rt.tradeTravelTick = now;
                ResetRobustTravel(rt);
                rt.status = L"GD RELOCK • " + tag + L" lệch TỌA GD → quay lại";
                return true;
            }
            rt.status = L"GD HOLD • " + tag + L" đã tới TỌA GD • chờ acc còn lại";
            return true;
        }

        if (rt.tradeTravelPhase == 0) {
            BeginTradeRendezvousTravel(a, now, who);
            return true;
        }

        if (rt.tradeTravelPhase == 4) {
            bool arrived = false;
            (void)HandleRobustTravel(a, now, tradeRendezvous_, L"TỌA GD", arrived, tradeRendezvousTolerance_);
            if (!arrived) {
                rt.status = L"GD TỌA • " + tag + L" đang đi M" + std::to_wstring(tradeRendezvous_.mapID) +
                            L" " + std::to_wstring(tradeRendezvous_.x) + L"," + std::to_wstring(tradeRendezvous_.y);
                return true;
            }
            rt.tradeTravelPhase = 5;
            rt.tradeTravelTick = now;
            rt.status = L"GD TỌA • " + tag + L" đã tới • khóa path và verify";
            return true;
        }

        if (rt.tradeTravelPhase == 5) {
            if (s.autoPathing) {
                Response r{}; std::wstring ignored;
                if (a.bridge.Attached()) (void)a.bridge.Call(Command::StopPath, 0, 0, 0, r, ignored, 700);
                rt.tradeTravelTick = now;
                rt.status = L"GD HOLD • " + tag + L" StopPath tại TỌA GD";
                return true;
            }
            // CP9: tới TỌA GD giữ nguyên trên ngựa.
            if (!Elapsed(now, rt.tradeTravelTick, 450)) return true;

            // v0.3: reuse the same fail-closed guard at the rendezvous itself. No separate
            // trade stop-Auto state machine remains.
            if ((s.validMask & ValidAutoFight) == 0 || s.autoFight) {
                if (!EnsureAutoFightOffForTravel(a, now, L"TỌA GD")) {
                    rt.status = L"GD HOLD • " + tag + L" chờ Travel Guard xác nhận AutoFight OFF";
                    return true;
                }
            }
            if (!TradeAccountAtRendezvous(a)) {
                rt.tradeTravelPhase = 4; rt.tradeTravelTick = now; ResetRobustTravel(rt);
                return true;
            }
            rt.tradeTravelReady = true;
            rt.tradeTravelPhase = 0;
            rt.status = L"GD HOLD • " + tag + L" ĐÃ TỚI TỌA GD";
            LogAccount(a, L"GD TỌA PASS: " + tag + L" đã đứng tại TỌA GD; giữ HOLD chờ acc còn lại.");
            return true;
        }
        return true;
    }

    bool TradeQueueContains(DWORD pid) const {
        return std::find(tradeQueuePids_.begin(), tradeQueuePids_.end(), pid) != tradeQueuePids_.end();
    }

    bool TradeTravelContains(DWORD pid) const {
        return std::find(tradeTravelPids_.begin(), tradeTravelPids_.end(), pid) != tradeTravelPids_.end();
    }

    Account* EarliestQueuedChild() {
        Account* best = nullptr;
        for (DWORD pid : tradeQueuePids_) {
            Account* child = AccountByPid(pid);
            if (!child || child->profile.tradeRole < kFirstChildTradeRole ||
                child->profile.tradeRole > kLastChildTradeRole) continue;
            if (!best || EarlierWorkflowEntry(
                    child->runtime.tradeWorkflowEntrySeq, child->profile.tradeRole - 1,
                    best->runtime.tradeWorkflowEntrySeq, best->profile.tradeRole - 1)) {
                best = child;
            }
        }
        return best;
    }

    void ReleaseTradeHold(Account& a) {
        a.tradeHeld = false;
        a.runtime.tradeWorkflowEntrySeq = 0;
        ResetTradeRendezvousTravel(a);
    }

    void RemoveTradeQueuePid(DWORD pid) {
        tradeQueuePids_.erase(std::remove(tradeQueuePids_.begin(), tradeQueuePids_.end(), pid),
                              tradeQueuePids_.end());
    }

    void RemoveTradeTravelPid(DWORD pid) {
        tradeTravelPids_.erase(std::remove(tradeTravelPids_.begin(), tradeTravelPids_.end(), pid),
                               tradeTravelPids_.end());
    }

    void ReleaseWorkflowChild(Account& child) {
        const DWORD pid = child.game.pid;
        RemoveTradeQueuePid(pid);
        RemoveTradeTravelPid(pid);
        ReleaseTradeHold(child);
    }

    void ReleaseTradeHolds() {
        for (auto& item : accounts_) {
            if (item && item->tradeHeld) ReleaseTradeHold(*item);
        }
        tradeQueuePids_.clear();
        tradeTravelPids_.clear();
    }

    void ResetTradeTxn(DWORD cooldownUntil = 0) {
        tradeTxn_ = TradeTxn{};
        tradeTxn_.cooldownUntil = cooldownUntil;
    }

    void FinalizeAbortTradeState(const std::wstring& reason, DWORD now) {
        Account* main = AccountByPid(tradeTxn_.mainPid);
        Account* child = AccountByPid(tradeTxn_.childPid);
        const DWORD abortedChildPid = tradeTxn_.childPid;
        const int abortedSlot = tradeTxn_.childSlot;
        const std::uint64_t abortedTicket = child ? child->runtime.tradeWorkflowEntrySeq : 0;

        // An ABORTED CON must leave the current trade workflow. Keeping the old FIFO ticket
        // here traps MAIN into retrying the same failed CON forever and prevents the next
        // waiting CON from taking its turn. Release only this child; preserve all other FIFO.
        if (child) {
            ReleaseWorkflowChild(*child);
            LogAccount(*child, L"ABORT CLEAN X HOÀN TẤT • nhả FIFO #" +
                               std::to_wstring(abortedTicket) + L" • rời workflow giao dịch → về Auto Train");
        } else if (abortedChildPid != 0) {
            RemoveTradeQueuePid(abortedChildPid);
            RemoveTradeTravelPid(abortedChildPid);
        }

        mainCapacityPlan_ = MainCapacityPlan{};
        const DWORD mainPid = main ? main->game.pid : 0;
        ResetTradeTxn(now + 2500);
        tradeTxn_.mainPid = mainPid;

        // MAIN stays reserved as MAIN, but the failed pair is gone. Idle coordinator can
        // select the next FIFO child after the short existing cooldown.
        if (main && main->runtime.running) {
            main->tradeHeld = true;
            LogAccount(*main, L"ABORT CLEAN X HOÀN TẤT • bỏ CON" +
                              std::to_wstring(abortedSlot) + L" / FIFO #" +
                              std::to_wstring(abortedTicket) +
                              L" • MAIN sạch → nhả quyền cho CON FIFO kế • cooldown 2500ms");
        }
        SetTradeStatus(L"HỦY • " + reason +
                       L" • ABORT CLEAN X xong • bỏ CON lỗi, lấy CON FIFO kế • queue=" +
                       std::to_wstring(tradeQueuePids_.size()));
    }

    void AbortTrade(const std::wstring& reason, DWORD now) {
        Account* main = AccountByPid(tradeTxn_.mainPid);
        Account* child = AccountByPid(tradeTxn_.childPid);
        if (main) LogAccount(*main, L"GD ABORT: " + reason);
        if (child) LogAccount(*child, L"GD ABORT: " + reason);
        AddLocalReport(L"TRADE ABORT", child ? TelegramAccountLabel(*child) : L"-",
                       reason + L" • luôn chạy ABORT CLEAN X best-effort trên MAIN + CON trước khi retry/nhả workflow • " +
                       LocalDateTimeText());

        // Cleanup is triggered by AbortTrade only. If an abort is reported again while
        // cleanup already owns the pair, do not restart its 5s budget or release holds.
        if (tradeTxn_.phase == TradePhase::Cleanup) {
            if (tradeTxn_.cleanupAbortReason.empty()) tradeTxn_.cleanupAbortReason = reason;
            return;
        }

        // Keep every still-running participant held while cleanup scans it. Missing/closed
        // clients are simply skipped by the cleanup sweep; one missing side must not prevent
        // best-effort cleanup on the other side.
        if (main && main->runtime.running) main->tradeHeld = true;
        if (child && child->runtime.running) child->tradeHeld = true;

        const bool hasCleanupParticipant =
            (main && main->runtime.running && IsWindow(main->game.window)) ||
            (child && child->runtime.running && IsWindow(child->game.window));
        if (hasCleanupParticipant) {
            tradeTxn_.phase = TradePhase::Cleanup;
            tradeTxn_.cleanupAbortPending = true;
            tradeTxn_.cleanupAbortReason = reason;
            tradeTxn_.cleanupStage = 0;
            tradeTxn_.cleanupDueTick = now;
            tradeTxn_.cleanupStartedTick = 0;
            tradeTxn_.cleanupSweepChanged = false;
            tradeTxn_.cleanupSweepUnresolved = false;
            tradeTxn_.cleanupSweepCount = 0;
            tradeTxn_.cleanupCleanSweepStreak = 0;
            SetTradeStatus(L"ABORT CLEAN X • giữ MAIN + CON • có X thì tắt, không có thì bỏ qua");
            return;
        }
        FinalizeAbortTradeState(reason, now);
    }

    void FinishTradeQuota(DWORD now) {
        Account* main = AccountByPid(tradeTxn_.mainPid);
        Account* child = AccountByPid(tradeTxn_.childPid);
        const int finishedSlot = tradeTxn_.childSlot;
        const int passes = tradeTxn_.childPlan.passesTotal;
        const int items = passes * trade_quota_v18_logic::kItemsPerPass;
        if (main) LogAccount(*main, L"GD QUOTA xong CON" + std::to_wstring(finishedSlot) +
                                  L" • " + std::to_wstring(passes) + L" pass x9 • MAIN quota còn " +
                                  std::to_wstring(mainCapacityPlan_.passesRemaining));
        if (child) LogAccount(*child, L"GD QUOTA HOÀN TẤT • " + std::to_wstring(items) +
                                    L" item • dư " + std::to_wstring(tradeTxn_.childPlan.remainder) +
                                    L" item không chạy partial • nhả workflow về train.");
        if (main && child) TelegramRecordTradeCompleted(*main, *child, items, passes);
        if (child) ReleaseWorkflowChild(*child);
        const DWORD mainPid = main ? main->game.pid : 0;
        ResetTradeTxn(now + 1500); tradeTxn_.mainPid = mainPid;
        if (main && main->runtime.running) main->tradeHeld = true;
        SetTradeStatus(L"HOÀN TẤT CON" + std::to_wstring(finishedSlot) +
                       L" • MAIN quota còn " + std::to_wstring(mainCapacityPlan_.passesRemaining) +
                       L" • queue " + std::to_wstring(tradeQueuePids_.size()));
    }

    void SkipChildNoFullPass(Account& child, DWORD now) {
        const int slot = tradeTxn_.childSlot;
        const int eligible = tradeTxn_.childPlan.eligibleCount;
        const int remainder = tradeTxn_.childPlan.remainder;
        LogAccount(child, L"TRADE QUOTA SKIP • eligible=" + std::to_wstring(eligible) +
                          L" • không đủ 9 • không mở bảng giao dịch • quay train ngay.");
        AddLocalReport(L"TRADE QUOTA SKIP", TelegramAccountLabel(child),
                       L"CON" + std::to_wstring(slot) + L" • eligible=" + std::to_wstring(eligible) +
                       L" • remainder=" + std::to_wstring(remainder) + L" • fullPass=0");
        ReleaseWorkflowChild(child);
        const DWORD mainPid = tradeTxn_.mainPid; ResetTradeTxn(); tradeTxn_.mainPid = mainPid;
        SetTradeStatus(L"SKIP CON" + std::to_wstring(slot) + L" • <9 item đủ điều kiện • lấy CON kế");
    }

    void PrepareNextPass(DWORD now) {
        ++tradeTxn_.sequencePass;
        tradeTxn_.sequenceIndex = 0;
        tradeTxn_.sequenceRepeatDone = 0;
        tradeTxn_.sequenceGroupRepeatDone = 0;
        tradeTxn_.sequenceDueTick = 0;
        tradeTxn_.sequenceAfterPutUpPending = false;
        tradeTxn_.sequenceAfterPutUpStartedTick = 0;
        tradeTxn_.sequenceAfterPutUpNextTryTick = 0;
        tradeTxn_.sequencePostPutUpRecoveryPending = false;
        tradeTxn_.sequencePostPutUpRecoveryStartedTick = 0;
        tradeTxn_.sequenceUiRowStartTick = 0;
        tradeTxn_.sequenceUiInvokeStartedTick = 0;
        tradeTxn_.sequenceUiNextTryTick = 0;
        tradeTxn_.sequenceUiRowIndex = static_cast<std::size_t>(-1);
        tradeTxn_.cleanupStage = 0;
        tradeTxn_.cleanupDueTick = 0;
        tradeTxn_.cleanupStartedTick = 0;
        tradeTxn_.cleanupSweepChanged = false;
        tradeTxn_.cleanupSweepUnresolved = false;
        tradeTxn_.cleanupSweepCount = 0;
            tradeTxn_.cleanupCleanSweepStreak = 0;
        tradeTxn_.cleanupAbortPending = false;
        tradeTxn_.cleanupAbortReason.clear();
        tradeTxn_.sequenceTelemetryCounted = false;
        tradeTxn_.postTradeClickCompleted = false;
        tradeTxn_.postTradeClickTarget = 0;
        tradeTxn_.postTradeClickRepeatDone = 0;
        tradeTxn_.postTradeClickDueTick = 0;
        tradeTxn_.postTradeClickSkipReported = false;
        tradeTxn_.targetStartedTick = now;
        tradeTxn_.targetRetryTick = now + 250;
        tradeTxn_.targetLastSelectTick = 0;
        tradeTxn_.targetAttempts = 0;
    }

    void PauseTradeForMainSell(TradePhase resumePhase, DWORD now, const std::wstring& reason) {
        Account* main = AccountByPid(tradeTxn_.mainPid);
        Account* child = AccountByPid(tradeTxn_.childPid);
        tradeTxn_.resumeAfterSell = resumePhase;
        tradeTxn_.phase = TradePhase::SellPause;
        if (main) LogAccount(*main, L"MAIN QUOTA=0 • tạm dừng tại biên an toàn để chạy macro bán theo số item scan • " + reason);
        if (child) LogAccount(*child, L"GIỮ NGUYÊN GD DỞ • đứng tại TỌA GD, không hủy/không về bãi • chờ MAIN bán xong.");
        AddLocalReport(L"TRADE PAUSE / SELL", child ? TelegramAccountLabel(*child) : L"-",
                       L"MAIN quota hết • " + reason +
                       L" • giữ nguyên CON/vé/slot, bán xong target lại MAIN • " +
                       LocalDateTimeText());
        SetTradeStatus(L"Đang giao dịch với CON" + std::to_wstring(tradeTxn_.childSlot) + L" • MAIN quota=0 • macro bán theo số item scan");
        (void)now;
    }

    static int ClampMainMacroDelay(int ms){return main_macro_sell_logic::ClampDelayMs(ms);}
    static bool MainMacroPointValid(const ClickPoint&p){return p.valid&&p.x>=0&&p.y>=0&&p.baseW>0&&p.baseH>0;}

    bool LoadMainMacroSellConfig(Account&main,MainMacroSellConfig&cfg,bool allowMigration=true){
        cfg={}; cfg.delayMs=600;
        const std::wstring&sec=main.profile.section;
        const bool configured=ReadIniInt(sec,L"MainMacroSellConfigured",0)!=0;
        if(configured){
            cfg.point.x=ReadIniInt(sec,L"MainMacroSellX",-1); cfg.point.y=ReadIniInt(sec,L"MainMacroSellY",-1);
            cfg.point.baseW=ReadIniInt(sec,L"MainMacroSellW",0); cfg.point.baseH=ReadIniInt(sec,L"MainMacroSellH",0);
            cfg.point.valid=cfg.point.x>=0&&cfg.point.y>=0&&cfg.point.baseW>0&&cfg.point.baseH>0;
            cfg.delayMs=ClampMainMacroDelay(ReadIniInt(sec,L"MainMacroSellDelay",600));
            return true;
        }
        if(!allowMigration)return true;
        int count=std::clamp(ReadIniInt(sec,L"SellMacroCount",0),0,64);
        const int source=fixed_slot_sell_logic::LegacyIdleClickSourceIndex((std::size_t)count);
        if(source>=0){
            const std::wstring pre=L"Sell_"+std::to_wstring(source)+L"_";
            ClickPoint legacy{}; legacy.x=ReadIniInt(sec,pre+L"X",-1); legacy.y=ReadIniInt(sec,pre+L"Y",-1);
            legacy.baseW=ReadIniInt(sec,pre+L"W",0); legacy.baseH=ReadIniInt(sec,pre+L"H",0);
            legacy.valid=legacy.x>=0&&legacy.y>=0&&legacy.baseW>0&&legacy.baseH>0;
            if(MainMacroPointValid(legacy)){
                cfg.point=legacy; cfg.delayMs=ClampMainMacroDelay(ReadIniInt(sec,pre+L"Delay",600));
                WriteIniInt(sec,L"MainMacroSellConfigured",1); WriteIniInt(sec,L"MainMacroSellX",legacy.x); WriteIniInt(sec,L"MainMacroSellY",legacy.y);
                WriteIniInt(sec,L"MainMacroSellW",legacy.baseW); WriteIniInt(sec,L"MainMacroSellH",legacy.baseH); WriteIniInt(sec,L"MainMacroSellDelay",cfg.delayMs); FlushIni();
                LogAccount(main,L"MAIN MACRO SELL • đã migrate tọa/delay SellMacro cũ • bỏ Repeat");
            }
        }
        return true;
    }
    void SaveMainMacroSellConfig(Account&main,const MainMacroSellConfig&cfg){
        const std::wstring&sec=main.profile.section; WriteIniInt(sec,L"MainMacroSellConfigured",1);
        WriteIniInt(sec,L"MainMacroSellX",cfg.point.valid?cfg.point.x:-1); WriteIniInt(sec,L"MainMacroSellY",cfg.point.valid?cfg.point.y:-1);
        WriteIniInt(sec,L"MainMacroSellW",cfg.point.valid?cfg.point.baseW:0); WriteIniInt(sec,L"MainMacroSellH",cfg.point.valid?cfg.point.baseH:0);
        WriteIniInt(sec,L"MainMacroSellDelay",ClampMainMacroDelay(cfg.delayMs)); FlushIni();
    }
    bool CaptureCursorForMain(Account&a,ClickPoint&p){POINT q{};if(!GetCursorPos(&q)||!ScreenToClient(a.game.window,&q))return false;RECT r{};if(!GetClientRect(a.game.window,&r)||r.right<=0||r.bottom<=0)return false;p={q.x,q.y,r.right-r.left,r.bottom-r.top,true};return q.x>=0&&q.y>=0&&q.x<r.right&&q.y<r.bottom;}
    void MainSellDialogStatus(const std::wstring&t){if(mainMacroSellStatus_)SetWindowTextW(mainMacroSellStatus_,t.c_str());}
    void RefreshMainMacroSellUi(){
        Account*a=AccountByPid(mainMacroSellDialogPid_);if(!a)return;LoadMainMacroSellConfig(*a,mainMacroSellDialogConfig_,true);
        if(mainMacroSellXEdit_)SetWindowTextW(mainMacroSellXEdit_,mainMacroSellDialogConfig_.point.valid?std::to_wstring(mainMacroSellDialogConfig_.point.x).c_str():L"");
        if(mainMacroSellYEdit_)SetWindowTextW(mainMacroSellYEdit_,mainMacroSellDialogConfig_.point.valid?std::to_wstring(mainMacroSellDialogConfig_.point.y).c_str():L"");
        if(mainMacroSellDelayEdit_)SetWindowTextW(mainMacroSellDelayEdit_,std::to_wstring(mainMacroSellDialogConfig_.delayMs).c_str());
        if(mainMacroSellPointLabel_){const std::wstring t=mainMacroSellDialogConfig_.point.valid?PointDescription(mainMacroSellDialogConfig_.point):L"CHƯA LẤY TỌA";SetWindowTextW(mainMacroSellPointLabel_,t.c_str());}
    }
    bool SaveMainMacroSellUi(){
        Account*a=AccountByPid(mainMacroSellDialogPid_);if(!a||a->profile.tradeRole!=kMainTradeRole){MainSellDialogStatus(L"chỉ MAIN được cấu hình macro bán");return false;}
        wchar_t bx[32]{},by[32]{},bd[32]{};GetWindowTextW(mainMacroSellXEdit_,bx,_countof(bx));GetWindowTextW(mainMacroSellYEdit_,by,_countof(by));GetWindowTextW(mainMacroSellDelayEdit_,bd,_countof(bd));
        MainMacroSellConfig cfg=mainMacroSellDialogConfig_;cfg.delayMs=ClampMainMacroDelay(_wtoi(bd));
        if(bx[0]&&by[0]){RECT rc{};if(!GetClientRect(a->game.window,&rc)||rc.right<=0||rc.bottom<=0){MainSellDialogStatus(L"không đọc được client acc");return false;}const int x=_wtoi(bx),y=_wtoi(by);if(x<0||y<0||x>=rc.right||y>=rc.bottom){MainSellDialogStatus(L"X/Y ngoài client acc");return false;}cfg.point={x,y,rc.right,rc.bottom,true};}
        mainMacroSellDialogConfig_=cfg;SaveMainMacroSellConfig(*a,cfg);
        if(a->runtime.sellPhase==3&&a->macroSell.phase==MainMacroSellRuntime::Phase::Blocked&&MainMacroPointValid(cfg.point)){a->macroSell=MainMacroSellRuntime{};a->runtime.sellPhase=0;idleSellEpochDone_=false;}RefreshMainMacroSellUi();
        MainSellDialogStatus(L"ĐÃ LƯU • MAIN bán Cách 1 raw click • delay="+std::to_wstring(cfg.delayMs)+L"ms");return true;
    }
    void BeginMainMacroSellCapture(){Account*a=AccountByPid(mainMacroSellDialogPid_);if(!a){MainSellDialogStatus(L"acc không còn tồn tại");return;}mainMacroSellCaptureActive_=true;mainMacroSellCapturePid_=a->game.pid;MainSellDialogStatus(L"ARM F8 • đưa chuột vào đúng TỌA MACRO BÁN trong acc");}
    void CaptureMainMacroSellF8(){if(!mainMacroSellCaptureActive_)return;Account*a=AccountByPid(mainMacroSellCapturePid_);if(!a){mainMacroSellCaptureActive_=false;MainSellDialogStatus(L"F8 FAIL • acc mất");return;}ClickPoint p{};if(!CaptureCursorForMain(*a,p)){MainSellDialogStatus(L"F8: chuột phải nằm trong client acc");return;}mainMacroSellCaptureActive_=false;mainMacroSellDialogConfig_.point=p;SaveMainMacroSellConfig(*a,mainMacroSellDialogConfig_);if(a->runtime.sellPhase==3&&a->macroSell.phase==MainMacroSellRuntime::Phase::Blocked){a->macroSell=MainMacroSellRuntime{};a->runtime.sellPhase=0;idleSellEpochDone_=false;}RefreshMainMacroSellUi();MainSellDialogStatus(L"F8 PASS • đã lưu TỌA MACRO BÁN");}
    bool MainMacroSellClick(Account&a,const ClickPoint&p,std::wstring&e){int x=-1,y=-1;if(!NormalizeClickPointForBridge(a.game,p,x,y,e))return false;Response r{};return a.bridge.Call(Command::ClickInternalPoint,x,y,0,r,e,2200,true);}
    void TestMainMacroSellClick(){Account*a=AccountByPid(mainMacroSellDialogPid_);if(!a){MainSellDialogStatus(L"acc không còn tồn tại");return;}if(!SaveMainMacroSellUi())return;if(!MainMacroPointValid(mainMacroSellDialogConfig_.point)){MainSellDialogStatus(L"CHƯA CÓ TỌA MACRO BÁN");return;}std::wstring e;if(!MainMacroSellClick(*a,mainMacroSellDialogConfig_.point,e)){MainSellDialogStatus(L"TEST FAIL • "+e);return;}MainSellDialogStatus(L"TEST PASS • đã gửi đúng 1 click macro");}
    void ClearMainMacroSellPoint(){Account*a=AccountByPid(mainMacroSellDialogPid_);if(!a)return;mainMacroSellDialogConfig_.point={};SaveMainMacroSellConfig(*a,mainMacroSellDialogConfig_);RefreshMainMacroSellUi();MainSellDialogStatus(L"ĐÃ XÓA TỌA • delay vẫn giữ");}
    static LRESULT CALLBACK MainSellSettingsWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){App*self=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){auto*cs=reinterpret_cast<CREATESTRUCTW*>(lp);self=reinterpret_cast<App*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}return self?self->HandleMainSellSettingsWindow(hwnd,msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);}
    LRESULT HandleMainSellSettingsWindow(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){(void)lp;if(msg==WM_COMMAND){switch(LOWORD(wp)){case IDC_MAIN_MACRO_SELL_CAPTURE:if(HIWORD(wp)==BN_CLICKED)BeginMainMacroSellCapture();return 0;case IDC_MAIN_MACRO_SELL_TEST:if(HIWORD(wp)==BN_CLICKED)TestMainMacroSellClick();return 0;case IDC_MAIN_MACRO_SELL_CLEAR:if(HIWORD(wp)==BN_CLICKED)ClearMainMacroSellPoint();return 0;case IDC_MAIN_MACRO_SELL_SAVE:if(HIWORD(wp)==BN_CLICKED)SaveMainMacroSellUi();return 0;case IDC_MAIN_MACRO_SELL_CLOSE:DestroyWindow(hwnd);return 0;}}if(msg==WM_CLOSE){mainMacroSellCaptureActive_=false;DestroyWindow(hwnd);return 0;}if(msg==WM_NCDESTROY){if(mainSellSettingsWindow_==hwnd)mainSellSettingsWindow_=nullptr;mainMacroSellXEdit_=mainMacroSellYEdit_=mainMacroSellDelayEdit_=mainMacroSellPointLabel_=mainMacroSellStatus_=nullptr;return DefWindowProcW(hwnd,msg,wp,lp);}return DefWindowProcW(hwnd,msg,wp,lp);}
    void OpenMainSellSettingsDialog(){
        Account*a=SelectedAccount();if(!a||a->profile.tradeRole!=kMainTradeRole){Log(L"TÙY CHỈNH BÁN chỉ áp dụng MAIN.");return;}mainMacroSellDialogPid_=a->game.pid;
        const wchar_t* sellDialogTitle = L"TÙY CHỈNH BÁN • MAIN MACRO COUNT-DRIVEN";
        if(mainSellSettingsWindow_&&IsWindow(mainSellSettingsWindow_)){SetWindowTextW(mainSellSettingsWindow_,sellDialogTitle);ShowWindow(mainSellSettingsWindow_,SW_SHOW);SetForegroundWindow(mainSellSettingsWindow_);RefreshMainMacroSellUi();MainSellDialogStatus(L"COUNT-DRIVEN • MAIN bán Cách 1 raw click");return;}
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=MainSellSettingsWndProc;wc.hInstance=instance_;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"ThanLongMainMacroSellV18";
        if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){Log(L"Không tạo được class TÙY CHỈNH BÁN");return;}
        mainSellSettingsWindow_=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,sellDialogTitle,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,650,440,hwnd_,nullptr,instance_,this);if(!mainSellSettingsWindow_)return;
        HFONT f=reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));auto mk=[&](const wchar_t*cls,const wchar_t*txt,DWORD style,int x,int y,int w,int h,int id){HWND z=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,mainSellSettingsWindow_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);if(z)SendMessageW(z,WM_SETFONT,reinterpret_cast<WPARAM>(f),TRUE);return z;};
        mk(L"STATIC",L"TỌA MACRO BÁN (một điểm duy nhất)",SS_LEFT|SS_CENTERIMAGE,18,18,300,26,0);mainMacroSellPointLabel_=mk(L"STATIC",L"CHƯA LẤY TỌA",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,18,50,600,32,0);
        mk(L"STATIC",L"X",SS_CENTER|SS_CENTERIMAGE,18,94,25,28,0);mainMacroSellXEdit_=mk(L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,48,94,85,28,IDC_MAIN_MACRO_SELL_X);mk(L"STATIC",L"Y",SS_CENTER|SS_CENTERIMAGE,142,94,25,28,0);mainMacroSellYEdit_=mk(L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_CENTER,172,94,85,28,IDC_MAIN_MACRO_SELL_Y);
        mk(L"BUTTON",L"LẤY TỌA F8",BS_PUSHBUTTON,275,94,150,28,IDC_MAIN_MACRO_SELL_CAPTURE);mk(L"BUTTON",L"TEST 1 CLICK",BS_PUSHBUTTON,435,94,115,28,IDC_MAIN_MACRO_SELL_TEST);mk(L"BUTTON",L"XÓA TỌA",BS_PUSHBUTTON,558,94,60,28,IDC_MAIN_MACRO_SELL_CLEAR);
        mk(L"STATIC",L"TIME GIỮA 2 CLICK BÁN (ms)",SS_LEFT|SS_CENTERIMAGE,18,142,230,28,0);mainMacroSellDelayEdit_=mk(L"EDIT",L"600",WS_BORDER|ES_NUMBER|ES_CENTER,258,142,95,28,IDC_MAIN_MACRO_SELL_DELAY);mk(L"STATIC",L"scheduler non-blocking • không còn Repeat",SS_LEFT|SS_CENTERIMAGE,365,142,253,28,0);
        mk(L"STATIC",L"MAIN bán Cách 1: TRANG BỊ + KHÔNG KHÓA • scan đúng 1 lượt / sell episode.",SS_LEFT|SS_CENTERIMAGE,18,188,600,28,0);
        mainMacroSellStatus_=mk(L"STATIC",L"",SS_LEFT|SS_CENTERIMAGE|WS_BORDER,18,226,600,45,IDC_MAIN_MACRO_SELL_STATUS);mk(L"BUTTON",L"LƯU",BS_DEFPUSHBUTTON,395,286,100,30,IDC_MAIN_MACRO_SELL_SAVE);mk(L"BUTTON",L"ĐÓNG",BS_PUSHBUTTON,510,286,108,30,IDC_MAIN_MACRO_SELL_CLOSE);
        LoadMainMacroSellConfig(*a,mainMacroSellDialogConfig_,true);RefreshMainMacroSellUi();MainSellDialogStatus(L"COUNT-DRIVEN • MAIN bán Cách 1 raw click");ShowWindow(mainSellSettingsWindow_,SW_SHOW);UpdateWindow(mainSellSettingsWindow_);
    }
    void ToggleMainSellSettings(){OpenMainSellSettingsDialog();}

    bool CountMainUnlockedEquipment(Account&a,int&eligible,int&free,std::wstring&d){
        eligible=0;free=-1;std::vector<BagItemSnapshot>items;if(!ReadAllBagSemantic(a,items,free,d,true))return false;
        for(const auto&i:items)if(main_macro_sell_logic::Eligible(i.position,i.isEquip!=0,i.bound!=0))++eligible;
        d=L"MAIN bag scan PASS • TRANG BỊ + KHÔNG KHÓA="+std::to_wstring(eligible);return true;
    }
    bool RefreshMainCapacityPlan(Account& main, int freeSlots, const wchar_t* reason) {
        const unsigned nextEpoch = mainCapacityPlan_.epoch + 1;
        MainCapacityPlan next = trade_quota_v18_logic::MakeMainPlan(main.game.pid, freeSlots, nextEpoch);
        if (!next.valid) return false;
        mainCapacityPlan_ = next;
        LogAccount(main, L"MAIN QUOTA PLAN • epoch=" + std::to_wstring(mainCapacityPlan_.epoch) +
                         L" • FreeBag=" + std::to_wstring(mainCapacityPlan_.freeSlotsAtPlan) +
                         L" • pass9=" + std::to_wstring(mainCapacityPlan_.passesRemaining) +
                         (reason ? L" • " + std::wstring(reason) : L""));
        return true;
    }
    bool EnsureMainCapacityPlan(Account& main) {
        if (mainCapacityPlan_.valid && mainCapacityPlan_.mainPid == main.game.pid) return true;
        if (!main.snapshotValid || (main.snapshot.validMask & ValidBagSpace) == 0 || main.snapshot.freeBagSpace < 0) return false;
        return RefreshMainCapacityPlan(main, main.snapshot.freeBagSpace, L"freeze đầu epoch");
    }
    bool BuildChildTradePlanOnce(Account& child, DWORD now) {
        if (tradeTxn_.childPlan.valid && tradeTxn_.childPlan.pid == child.game.pid) return true;
        std::vector<BagItemSnapshot> items; int freeBag = -1; std::wstring detail;
        if (!ReadAllBagSemantic(child, items, freeBag, detail, true)) {
            AbortTrade(L"TRADE QUOTA scan CON FAIL • " + detail, now); return false;
        }
        int eligible = 0;
        for (const auto& item : items) if (trade_quota_v18_logic::EligibleEquipment(item.position, item.isEquip != 0, item.bound != 0)) ++eligible;
        tradeTxn_.childPlan = trade_quota_v18_logic::MakeChildPlan(child.game.pid, eligible);
        LogAccount(child, L"CHILD QUOTA PLAN • eligible trang bị không khóa=" + std::to_wstring(eligible) +
                          L" • pass9=" + std::to_wstring(tradeTxn_.childPlan.passesTotal) +
                          L" • dư=" + std::to_wstring(tradeTxn_.childPlan.remainder));
        AddLocalReport(L"CHILD QUOTA PLAN", TelegramAccountLabel(child),
                       L"eligible=" + std::to_wstring(eligible) + L" • pass9=" +
                       std::to_wstring(tradeTxn_.childPlan.passesTotal) + L" • remainder=" +
                       std::to_wstring(tradeTxn_.childPlan.remainder));
        return true;
    }
    void ClearMainMacroSellRuntime(Account&main){main.macroSell=MainMacroSellRuntime{};main.runtime.sellPhase=0;if(activeMainMacroSellPid_==main.game.pid){KillTimer(hwnd_,kMainMacroSellTimer);activeMainMacroSellPid_=0;activeMainSellContextChildPid_=0;activeMainSellContextPass_=0;}}
    void BlockMainMacroSell(Account&main,const std::wstring&why){auto&rt=main.macroSell;rt.phase=MainMacroSellRuntime::Phase::Blocked;rt.blockedFree=main.snapshotValid?main.snapshot.freeBagSpace:-1;main.runtime.sellPhase=3;KillTimer(hwnd_,kMainMacroSellTimer);activeMainMacroSellPid_=0;main.runtime.status=L"MAIN SELL BLOCKED • "+why;ReportSellBlockOnce(main,60,why);}
    bool BeginMainMacroSell(Account&main,bool idleSweep=false){
        if(!TradeAccountAtRendezvous(main)){BlockMainMacroSell(main,L"MAIN không đứng đúng TỌA GD");return false;}MainMacroSellConfig cfg{};LoadMainMacroSellConfig(main,cfg,true);if(!MainMacroPointValid(cfg.point)){BlockMainMacroSell(main,L"chưa có TỌA MACRO BÁN • vào TÙY CHỈNH BÁN và F8");return false;}
        mainMacroSellActiveConfig_=cfg;main.macroSell=MainMacroSellRuntime{};main.macroSell.phase=MainMacroSellRuntime::Phase::Scan;main.macroSell.idleSweep=idleSweep;main.runtime.sellPhase=1;main.runtime.sellBlockReportCode=0;activeMainMacroSellPid_=main.game.pid;
        if(tradeTxn_.phase!=TradePhase::Idle){activeMainSellContextChildPid_=tradeTxn_.childPid;activeMainSellContextPass_=tradeTxn_.sequencePass;}else{Account*next=EarliestQueuedChild();activeMainSellContextChildPid_=next?next->game.pid:0;activeMainSellContextPass_=1;}
        if(!SetTimer(hwnd_,kMainMacroSellTimer,kMainMacroSellTimerMs,nullptr)){BlockMainMacroSell(main,L"không tạo được seller timer");return false;}
        LogAccount(main,(idleSweep?L"IDLE SELL SWEEP START":L"MAIN QUOTA=0 • SELL START")+std::wstring(L" • scan 1 lượt • predicate=isEquip&&!bound • 1 tọa macro • delay=")+std::to_wstring(cfg.delayMs)+L"ms");return true;
    }
    void FinishMainMacroSell(Account&main,int freshFree){
        const bool idle=main.macroSell.idleSweep;const int done=main.macroSell.doneClicks;const int target=main.macroSell.targetClicks;
        if(!RefreshMainCapacityPlan(main,freshFree,idle?L"sau idle sell":L"sau macro sell")){BlockMainMacroSell(main,L"fresh FreeBag không thể tạo MAIN quota");return;}
        if(idle)idleSellEpochDone_=true;
        LogAccount(main,L"MAIN MACRO SELL DONE • click="+std::to_wstring(done)+L"/"+std::to_wstring(target)+
                        L" • fresh FreeBag="+std::to_wstring(freshFree)+L" • quota pass9="+std::to_wstring(mainCapacityPlan_.passesRemaining));
        ClearMainMacroSellRuntime(main);
        main.runtime.status=idle?L"IDLE SELL SWEEP xong • quota đã re-plan":
            (mainCapacityPlan_.passesRemaining>0?L"MAIN SELL xong • quota mới sẵn sàng":L"MAIN SELL xong nhưng quota vẫn 0");
    }
    void TickActiveMainMacroSell(){
        Account*main=activeMainMacroSellPid_?AccountByPid(activeMainMacroSellPid_):nullptr;if(!main||!main->runtime.running||main->profile.tradeRole!=kMainTradeRole||main->runtime.sellPhase==0){KillTimer(hwnd_,kMainMacroSellTimer);activeMainMacroSellPid_=0;return;}auto&rt=main->macroSell;const DWORD now=GetTickCount();std::wstring e;
        if(rt.phase==MainMacroSellRuntime::Phase::Scan){int eligible=0,free=-1;if(!CountMainUnlockedEquipment(*main,eligible,free,e)){BlockMainMacroSell(*main,e);return;}rt.targetClicks=eligible;rt.doneClicks=0;rt.nextTick=now;if(eligible<=0){if(rt.idleSweep){idleSellEpochDone_=true;LogAccount(*main,L"IDLE SELL SWEEP • scan=0 • kết thúc, không click");ClearMainMacroSellRuntime(*main);main->runtime.status=L"MAIN đứng chờ • không có item bán";return;}BlockMainMacroSell(*main,L"MAIN KHÔNG CÓ TRANG BỊ KHÔNG KHÓA ĐỂ BÁN NHƯNG KHÔNG ĐỦ 9 Ô");return;}rt.phase=MainMacroSellRuntime::Phase::Clicking;TelegramRecordSellEpisode(*main);main->runtime.status=L"MAIN SELL • target "+std::to_wstring(eligible)+L" macro click";return;}
        if(rt.phase==MainMacroSellRuntime::Phase::Clicking){
            if(rt.nextTick!=0&&static_cast<LONG>(now-rt.nextTick)<0)return;
            if(!MainSellClickPhaseAllowed()){rt.nextTick=now+50;main->runtime.status=L"MAIN SELL GATE • đang có TRADE/RECOVERY nên cấm click bán";return;}
            if(!MainMacroSellClick(*main,mainMacroSellActiveConfig_.point,e)){if(BridgeLooksUnresponsive(e))EnterClientFreeze(*main,L"Bridge timeout khi MAIN macro sell",now);else(void)StartAccountUiRecovery(*main,AccountUiRecoveryPlan::GenericFailClosed,L"MAIN MACRO SELL fail-closed • "+e,now);return;}
            ++rt.doneClicks;rt.nextTick=GetTickCount()+static_cast<DWORD>(mainMacroSellActiveConfig_.delayMs);main->runtime.status=L"MAIN SELL CÁCH 1 • macro "+std::to_wstring(rt.doneClicks)+L"/"+std::to_wstring(rt.targetClicks);if(rt.doneClicks>=rt.targetClicks)rt.phase=MainMacroSellRuntime::Phase::RefreshCapacity;return;}
        if(rt.phase==MainMacroSellRuntime::Phase::RefreshCapacity){if(rt.nextTick!=0&&static_cast<LONG>(now-rt.nextTick)<0)return;if(!ReadSnapshot(*main,e,1200)||!main->snapshotValid||(main->snapshot.validMask&ValidBagSpace)==0){rt.nextTick=GetTickCount()+500;main->runtime.status=L"MAIN SELL • chờ fresh FreeBag sau click cuối";return;}FinishMainMacroSell(*main,main->snapshot.freeBagSpace);return;}
    }
    bool TickMainFullSellBatch(Account&main,DWORD now){
        (void)now;
        if(main.runtime.sellPhase!=0)return true;
        if(!EnsureMainCapacityPlan(main)){main.runtime.status=L"MAIN QUOTA • chờ FreeBag hợp lệ để lập epoch";return true;}
        if(mainCapacityPlan_.passesRemaining>0)return false;
        (void)BeginMainMacroSell(main,false);return true;
    }
    void ReportSellBlockOnce(Account& main, int code, const std::wstring& detail) {
        if (main.runtime.sellBlockReportCode == code) return;
        main.runtime.sellBlockReportCode = code;
        AddLocalReport(L"SELL BLOCKED", TelegramAccountLabel(main), detail + L" • fail-closed, chưa gửi click • " + LocalDateTimeText());
    }

    bool ExecuteTradeMenuOpenClickTick(Account& child, DWORD now) {
        if (tradeTxn_.postTradeClickCompleted) return false;

        const int repeat = std::clamp(shortcutSettings_.postTradeClickRepeat, 0, 999);
        const DWORD delay = static_cast<DWORD>(std::clamp(shortcutSettings_.postTradeClickDelayMs, 0, 60000));
        if (!shortcutSettings_.postTradeClickEnabled || repeat == 0) {
            tradeTxn_.postTradeClickCompleted = true;
            tradeTxn_.postTradeClickDueTick = 0;
            return false;
        }
        if (!shortcutSettings_.postTradeClick.valid) {
            if (!tradeTxn_.postTradeClickSkipReported) {
                tradeTxn_.postTradeClickSkipReported = true;
                LogAccount(child, L"CLICK SAU TARGET MAIN bỏ qua • đã BẬT nhưng chưa có tọa F8 hợp lệ.");
            }
            tradeTxn_.postTradeClickCompleted = true;
            tradeTxn_.postTradeClickDueTick = 0;
            return false;
        }
        if (tradeTxn_.postTradeClickDueTick != 0 &&
            static_cast<LONG>(now - tradeTxn_.postTradeClickDueTick) < 0) return true;

        // Delay after the final click is deliberate: the player interaction menu
        // needs one UI frame/window before the semantic "Giao dịch" callback scans it.
        if (tradeTxn_.postTradeClickRepeatDone >= repeat) {
            tradeTxn_.postTradeClickCompleted = true;
            tradeTxn_.postTradeClickDueTick = 0;
            SetTradeStatus(L"TRADE • CLICK SAU TARGET MAIN xong CON" +
                           std::to_wstring(tradeTxn_.childSlot) + L" • chuẩn bị chọn Giao dịch");
            return false;
        }

        std::wstring error;
        const bool ok = CoordinatorInternalPointAction(
            child, shortcutSettings_.postTradeClick,
            L"CLICK SAU TARGET MAIN • CON" + std::to_wstring(tradeTxn_.childSlot),
            error);
        if (!ok) {
            // This click is only the menu opener. The following semantic callback is
            // still the authoritative gate; on failure that gate resets this opener
            // so the next retry performs the click again instead of spinning on text.
            LogAccount(child, L"CLICK SAU TARGET MAIN click lỗi • sẽ retry lại trước callback • " + error);
        }

        ++tradeTxn_.postTradeClickRepeatDone;
        tradeTxn_.postTradeClickDueTick = GetTickCount() + delay;
        SetTradeStatus(L"TRADE • CLICK SAU TARGET MAIN → CON" +
                       std::to_wstring(tradeTxn_.childSlot) + L" • " +
                       std::to_wstring(tradeTxn_.postTradeClickRepeatDone) + L"/" +
                       std::to_wstring(repeat));
        return true;
    }

    static bool SemanticBridgeRequestStillInFlight(const std::wstring& detail) {
        return detail.find(L"Bridge timeout") != std::wstring::npos ||
               detail.find(L"Bridge còn bận sau timeout") != std::wstring::npos ||
               detail.find(L"Bridge busy") != std::wstring::npos;
    }

    bool CallFastTravelSemantic(Account& account, TravelSemantic semantic, bool probeOnly, std::wstring& detail) {
        Response response{}; std::wstring error;
        if (!account.bridge.Call(Command::ClickTravelSemantic, static_cast<int>(semantic), probeOnly ? 1 : 0, 0, response, error, 250, true)) {
            detail = error; return false;
        }
        detail = response.detail[0] ? std::wstring(response.detail) : L"SEMANTIC PASS";
        return true;
    }

    int InvokeUiDirectNow(Account& account, UiDirectTarget target, std::wstring& detail) {
        Response response{}; std::wstring error;
        if (!account.bridge.Call(Command::InvokeUiDirect, static_cast<int>(target), 0, 0, response, error, 250, true)) {
            detail = error; return -1;
        }
        detail = response.detail[0] ? std::wstring(response.detail) : L"UI DIRECT";
        return response.resultCode == static_cast<std::int32_t>(ActionResult::ActionInvoked) ? 1 : 0;
    }

    int ProbeUiDirectNow(Account& account, UiDirectTarget target, std::wstring& detail) {
        Response response{}; std::wstring error;
        if (!account.bridge.Call(Command::ProbeUiDirect, static_cast<int>(target), 0, 0, response, error, 250, true)) {
            detail = error; return -1;
        }
        detail = response.detail[0] ? std::wstring(response.detail) : L"UI DIRECT PROBE";
        return response.value0 != 0 ? 1 : 0;
    }

    bool AccountUiRecoveryActive(const Account& account) const {
        return account.runtime.uiRecoveryPlan != AccountUiRecoveryPlan::None;
    }

    void ClearAccountUiRecovery(RuntimeState& rt) {
        rt.uiRecoveryPlan=AccountUiRecoveryPlan::None;rt.uiRecoveryStage=0;rt.uiRecoveryStartedTick=0;rt.uiRecoveryNextTick=0;
        rt.uiRecoverySweepChanged=false;rt.uiRecoverySweepUnresolved=false;rt.uiRecoverySweepCount=0;rt.uiRecoveryCleanSweepStreak=0;rt.uiRecoveryReason.clear();
    }

    bool StartAccountUiRecovery(Account& account, AccountUiRecoveryPlan plan, const std::wstring& reason, DWORD now) {
        if(plan==AccountUiRecoveryPlan::None||!account.runtime.running||!IsWindow(account.game.window)||!account.bridge.Attached()||account.runtime.clientFreezeActive)return false;
        const bool activeTradePair=tradeTxn_.phase!=TradePhase::Idle&&(account.game.pid==tradeTxn_.mainPid||account.game.pid==tradeTxn_.childPid);
        if(plan==AccountUiRecoveryPlan::PostRevive&&activeTradePair){
            account.runtime.postReviveRecoveryPending=true;
            account.runtime.status=L"POST-REVIVE RECOVERY • chờ TRADE cleanup xong rồi mới chạy chuỗi X";
            AbortTrade(L"POST-REVIVE handoff • "+reason,now);return true;
        }
        if(AccountUiRecoveryActive(account)){
            if(plan==AccountUiRecoveryPlan::PostRevive)account.runtime.uiRecoveryPlan=plan;
            account.runtime.uiRecoveryReason=reason;return true;
        }
        // Generic recovery inside an active trade pair is delegated to the proven trade cleanup.
        if(activeTradePair){AbortTrade(L"HARD RECOVERY handoff • "+reason,now);return true;}
        if(account.profile.tradeRole==kMainTradeRole&&account.runtime.sellPhase!=0)ClearMainMacroSellRuntime(account);
        RuntimeState& rt=account.runtime;rt.postReviveRecoveryPending=false;rt.uiRecoveryPlan=plan;rt.uiRecoveryStage=plan==AccountUiRecoveryPlan::PostRevive?0:1;
        rt.uiRecoveryStartedTick=plan==AccountUiRecoveryPlan::PostRevive?0:now;rt.uiRecoveryNextTick=now;rt.uiRecoverySweepChanged=false;rt.uiRecoverySweepUnresolved=false;rt.uiRecoverySweepCount=0;rt.uiRecoveryCleanSweepStreak=0;rt.uiRecoveryReason=reason;
        rt.status=plan==AccountUiRecoveryPlan::PostRevive?L"POST-REVIVE RECOVERY • chờ ALIVE rồi XÁC NHẬN → X Tay nải → X popup → X giao dịch":L"HARD UI RECOVERY • X Tay nải → X popup → X giao dịch";
        LogAccount(account,rt.status+L" • reason="+reason);return true;
    }

    void FinishAccountUiRecovery(Account& account, bool timedOut) {
        RuntimeState& rt=account.runtime;const AccountUiRecoveryPlan plan=rt.uiRecoveryPlan;const std::wstring reason=rt.uiRecoveryReason;
        ClearAccountUiRecovery(rt);
        // Resume from a safe state rather than from the failed raw click/index.
        ResetShortcutRoute(rt);ResetRobustTravel(rt);ResetTravelFightGuard(rt);
        rt.routeOwnershipResetPending=true;rt.trainPositionMonitorArmed=false;rt.lastTrainPositionCheckTick=0;rt.lastAutoFightCheckTick=0;rt.fightPhase=0;rt.fightAttempts=0;rt.fightRetryWaitTick=0;
        rt.status=timedOut?L"UI RECOVERY TIME MAX 5000ms • tiếp tục từ state an toàn":L"UI RECOVERY sạch • tiếp tục từ state an toàn";
        LogAccount(account,rt.status+L" • "+reason+(plan==AccountUiRecoveryPlan::PostRevive?L" • POST-REVIVE":L""));
    }

    bool TickAccountUiRecovery(Account& account, DWORD now) {
        RuntimeState& rt=account.runtime;if(!AccountUiRecoveryActive(account))return false;
        if(rt.uiRecoveryPlan==AccountUiRecoveryPlan::GenericFailClosed&&account.snapshotValid&&(account.snapshot.validMask&ValidLifeState)&&account.snapshot.dead){
            LogAccount(account,L"HARD UI RECOVERY • phát hiện DEAD → nhường P2 ĐẦU THAI; POST-REVIVE sẽ dọn UI sau");ClearAccountUiRecovery(rt);return false;
        }
        if(rt.uiRecoveryPlan==AccountUiRecoveryPlan::PostRevive){
            if(!account.snapshotValid||(account.snapshot.validMask&ValidLifeState)==0||account.snapshot.dead){
                rt.uiRecoveryStartedTick=0;rt.uiRecoveryNextTick=now+50;
                rt.status=L"POST-REVIVE RECOVERY • chờ ALIVE authoritative trước khi chạy chuỗi X";
                return true;
            }
            if(rt.uiRecoveryStartedTick==0){
                rt.uiRecoveryStartedTick=now;rt.uiRecoveryNextTick=now+300;
                rt.status=L"POST-REVIVE RECOVERY • ALIVE PASS • chờ UI settle 300ms rồi chạy chuỗi X";
                return true;
            }
        }
        if(static_cast<LONG>(now-rt.uiRecoveryNextTick)<0)return true;
        if(rt.uiRecoveryStartedTick==0)rt.uiRecoveryStartedTick=now;
        if(Elapsed(now,rt.uiRecoveryStartedTick,5000)){FinishAccountUiRecovery(account,true);return true;}

        // Extra discard-confirm is deliberately scoped to POST-REVIVE only.
        if(rt.uiRecoveryStage==0){
            std::wstring detail;
            const bool probe=CallFastTravelSemantic(account,TravelSemantic::DiscardConfirm,true,detail);
            if(probe){
                if(!CallFastTravelSemantic(account,TravelSemantic::DiscardConfirm,false,detail)){
                    if(SemanticBridgeRequestStillInFlight(detail)){rt.uiRecoveryNextTick=now+50;return true;}
                    rt.uiRecoverySweepUnresolved=true;rt.uiRecoveryCleanSweepStreak=0;
                    LogAccount(account,L"POST-REVIVE RECOVERY • XÁC NHẬN fail-closed cục bộ • vẫn chạy các X sau • "+detail);
                }else LogAccount(account,L"POST-REVIVE RECOVERY • XÁC NHẬN nếu còn PASS");
            }else if(SemanticBridgeRequestStillInFlight(detail)){rt.uiRecoveryNextTick=now+50;return true;}
            else LogAccount(account,L"POST-REVIVE RECOVERY • không có XÁC NHẬN → tiếp tục X Tay nải");
            rt.uiRecoveryStage=1;rt.uiRecoveryNextTick=now+100;return true;
        }

        struct RecoveryAction{UiDirectTarget target;const wchar_t* label;};
        static constexpr RecoveryAction actions[]={
            {UiDirectTarget::CloseBag,L"X Tay nải"},
            {UiDirectTarget::CloseItemPopup,L"X popup item"},
            {UiDirectTarget::CloseTrade,L"X Giao dịch"},
        };
        if(rt.uiRecoveryStage>=1&&rt.uiRecoveryStage<=3){
            RecoveryAction action=actions[rt.uiRecoveryStage-1];std::wstring detail;const int result=InvokeUiDirectNow(account,action.target,detail);
            if(result<0&&SemanticBridgeRequestStillInFlight(detail)){rt.uiRecoveryNextTick=now+50;rt.status=std::wstring(L"UI RECOVERY • ")+action.label+L" Bridge pending";return true;}
            if(result>0){rt.uiRecoverySweepChanged=true;rt.uiRecoveryCleanSweepStreak=0;rt.status=std::wstring(L"UI RECOVERY • ")+action.label+L" PASS";LogAccount(account,rt.status);}
            else if(result<0){rt.uiRecoverySweepUnresolved=true;rt.uiRecoveryCleanSweepStreak=0;rt.status=std::wstring(L"UI RECOVERY • ")+action.label+L" fail-closed cục bộ • vẫn quét X khác";LogAccount(account,rt.status+L" • "+detail);}
            else rt.status=std::wstring(L"UI RECOVERY • ")+action.label+L" NOT FOUND";
            ++rt.uiRecoveryStage;rt.uiRecoveryNextTick=now+(result>0?100:(result<0?50:0));return true;
        }
        if(rt.uiRecoveryStage>=4){
            ++rt.uiRecoverySweepCount;
            if(!rt.uiRecoverySweepChanged&&!rt.uiRecoverySweepUnresolved){
                ++rt.uiRecoveryCleanSweepStreak;
                if(rt.uiRecoveryCleanSweepStreak>=2){FinishAccountUiRecovery(account,false);return true;}
                rt.status=L"UI RECOVERY • sweep sạch 1/2 • chờ 100ms xác nhận lần 2";
            }else rt.uiRecoveryCleanSweepStreak=0;
            rt.uiRecoveryStage=1;rt.uiRecoverySweepChanged=false;rt.uiRecoverySweepUnresolved=false;rt.uiRecoveryNextTick=now+100;return true;
        }
        FinishAccountUiRecovery(account,true);return true;
    }

    bool MainSellClickPhaseAllowed() const {
        return tradeTxn_.phase == TradePhase::Idle || tradeTxn_.phase == TradePhase::SellPause;
    }

    bool CompleteTradePass(Account& main, Account& child, DWORD now) {
        if (!tradeTxn_.sequenceTelemetryCounted) {
            tradeTxn_.sequenceTelemetryCounted = true;
            if (telegramStats_.active) {
                ++telegramStats_.tradeTotal;
                ++telegramStats_.tradesByChildPid[child.game.pid];
            }
        }
        // A completed saved macro is authoritative for one full x9 pass. Cleanup is abort-only.
        if (!trade_quota_v18_logic::ConsumeFullPass(mainCapacityPlan_, tradeTxn_.childPlan)) {
            AbortTrade(L"TRADE QUOTA invariant fail khi consume pass", now);
            return false;
        }
        LogAccount(main, L"TRADE QUOTA PASS #" + std::to_wstring(tradeTxn_.sequencePass) +
                         L" • x9 • SEQUENCE PASS • MAIN còn=" + std::to_wstring(mainCapacityPlan_.passesRemaining) +
                         L" • CON còn=" + std::to_wstring(tradeTxn_.childPlan.passesRemaining));
        AddLocalReport(L"TRADE QUOTA PASS", TelegramAccountLabel(child),
                       L"Pass #" + std::to_wstring(tradeTxn_.sequencePass) + L" • x9 • MAIN remaining=" +
                       std::to_wstring(mainCapacityPlan_.passesRemaining) + L" • CON remaining=" +
                       std::to_wstring(tradeTxn_.childPlan.passesRemaining));
        if (trade_quota_v18_logic::ChildDone(tradeTxn_.childPlan)) {
            FinishTradeQuota(now);
            return true;
        }
        PrepareNextPass(now);
        if (trade_quota_v18_logic::MainNeedsSell(mainCapacityPlan_)) {
            PauseTradeForMainSell(TradePhase::TargetMain, now, L"MAIN quota vừa về 0 sau full pass; giữ nguyên CON còn quota");
            return true;
        }
        tradeTxn_.phase = TradePhase::TargetMain;
        SetTradeStatus(L"TRADE QUOTA • CON" + std::to_wstring(tradeTxn_.childSlot) +
                       L" • pass kế #" + std::to_wstring(tradeTxn_.sequencePass) +
                       L" • M=" + std::to_wstring(mainCapacityPlan_.passesRemaining) +
                       L" C=" + std::to_wstring(tradeTxn_.childPlan.passesRemaining));
        return true;
    }

    bool ExecuteAbortTradeCleanupTick(Account* main, Account* child, DWORD now) {
        constexpr DWORD kRecoveryMaxMs = 5000;
        if (tradeTxn_.cleanupDueTick != 0 && static_cast<LONG>(now - tradeTxn_.cleanupDueTick) < 0) return true;
        if (tradeTxn_.cleanupStartedTick == 0) tradeTxn_.cleanupStartedTick = now;

        // Abort cleanup is a ONE-PASS state machine. A target is DONE as soon as its X
        // was closed successfully OR it was not found. DONE targets are never scanned
        // again in the same abort. Only a truly unresolved/busy target is retried.
        struct CleanupAction { Account* account; UiDirectTarget target; const wchar_t* label; };
        CleanupAction actions[] = {
            {main,  UiDirectTarget::CloseItemPopup, L"MAIN X popup item"},
            {main,  UiDirectTarget::CloseTrade,     L"MAIN X Giao dịch"},
            {main,  UiDirectTarget::CloseBag,       L"MAIN X Tay nải"},
            {child, UiDirectTarget::CloseItemPopup, L"CON X popup item"},
            {child, UiDirectTarget::CloseTrade,     L"CON X Giao dịch"},
            {child, UiDirectTarget::CloseBag,       L"CON X Tay nải"},
        };

        auto logBoth = [&](const std::wstring& text) {
            if (main) LogAccount(*main, text);
            if (child && child != main) LogAccount(*child, text);
        };
        auto finishRecovery = [&]() -> bool {
            const std::wstring abortReason = tradeTxn_.cleanupAbortReason;
            tradeTxn_.cleanupStage = 0;
            tradeTxn_.cleanupDueTick = 0;
            tradeTxn_.cleanupStartedTick = 0;
            tradeTxn_.cleanupSweepChanged = false;
            tradeTxn_.cleanupSweepUnresolved = false;
            tradeTxn_.cleanupSweepCount = 0;
            tradeTxn_.cleanupCleanSweepStreak = 0;
            FinalizeAbortTradeState(abortReason.empty() ? L"abort cleanup" : abortReason, now);
            return true;
        };

        if (Elapsed(now, tradeTxn_.cleanupStartedTick, kRecoveryMaxMs)) {
            logBoth(L"ABORT CLEAN X • TIME MAX 5000ms • kết thúc best-effort, nhả CON lỗi và trả MAIN cho FIFO kế");
            return finishRecovery();
        }

        if (tradeTxn_.cleanupStage >= static_cast<int>(_countof(actions))) {
            logBoth(L"ABORT CLEAN X • DONE 1 PASS • tất cả target đã ĐÃ TẮT / NOT FOUND / SKIP → nhả workflow ngay");
            return finishRecovery();
        }

        CleanupAction& action = actions[tradeTxn_.cleanupStage];
        // Missing/stopped window means there is nothing left to close for this target.
        // Mark it DONE and continue; never restart the sweep because of a NOT FOUND/SKIP.
        if (!action.account || !action.account->runtime.running || !IsWindow(action.account->game.window) ||
            !action.account->bridge.Attached()) {
            const std::wstring skipped = std::wstring(L"ABORT CLEAN X • ") + action.label +
                                         L" • SKIP (client không sẵn sàng) → DONE";
            SetTradeStatus(skipped);
            if (action.account) LogAccount(*action.account, skipped);
            ++tradeTxn_.cleanupStage;
            tradeTxn_.cleanupDueTick = now;
            return true;
        }

        std::wstring detail;
        const int result = InvokeUiDirectNow(*action.account, action.target, detail);
        if (result < 0) {
            // Only unresolved/busy is retried, and only on this exact target. The global
            // 5s recovery cap still guarantees cleanup can never loop forever.
            tradeTxn_.cleanupSweepUnresolved = true;
            const std::wstring waiting = std::wstring(L"ABORT CLEAN X • ") + action.label +
                                         L" • WAIT unresolved/busy → retry đúng target • " + detail;
            SetTradeStatus(waiting);
            if (!SemanticBridgeRequestStillInFlight(detail)) LogAccount(*action.account, waiting);
            tradeTxn_.cleanupDueTick = now + 50;
            return true;
        }

        tradeTxn_.cleanupSweepUnresolved = false;
        if (result > 0) {
            const std::wstring closed = std::wstring(L"ABORT CLEAN X • ") + action.label +
                                        L" • ĐÃ TẮT → DONE, không scan lại";
            SetTradeStatus(closed);
            LogAccount(*action.account, closed);
        } else {
            // NOT FOUND is authoritative CLEAN for this target.
            const std::wstring notFound = std::wstring(L"ABORT CLEAN X • ") + action.label +
                                          L" • KHÔNG CÓ → DONE, không scan lại • " + detail;
            SetTradeStatus(notFound);
            LogAccount(*action.account, notFound);
        }
        ++tradeTxn_.cleanupStage;
        tradeTxn_.cleanupDueTick = now + (result > 0 ? 100 : 0);
        return true;
    }

    bool ExecuteTradeSequenceTick(Account& main, Account& child, DWORD now) {
        EnsureSharedChildTradeSequence();
        std::vector<TradeSequenceStep>& seq = childTradeSequence_;
        if (tradeTxn_.sequenceIndex >= seq.size()) {
            // Success path never runs X cleanup. A finished macro consumes the pass and
            // immediately continues normal trade coordination. Any subsequent error will
            // enter AbortTrade(), which is now the single cleanup trigger.
            return CompleteTradePass(main, child, now);
        }

        TradeSequenceStep& stored = seq[tradeTxn_.sequenceIndex];
        const TradeSequenceStep* effective = &stored;
        Account* target = &child;
        if (stored.target == 1) {
            effective = ResolveMainReference(stored);
            target = &main;
            if (!effective) {
                AbortTrade(L"MAIN reference hỏng tại bước " + std::to_wstring(tradeTxn_.sequenceIndex + 1), now);
                return false;
            }
        }
        const int repeatLimit = std::max(1, effective->repeat);
        auto resetUiRowRuntime = [&]() {
            tradeTxn_.sequenceUiRowStartTick = 0;
            tradeTxn_.sequenceUiInvokeStartedTick = 0;
            tradeTxn_.sequenceUiNextTryTick = 0;
            tradeTxn_.sequenceUiRowIndex = static_cast<std::size_t>(-1);
        };
        auto finalizeOccurrence = [&]() {
            ++tradeTxn_.sequenceRepeatDone;
            // UI DIRECT rows are NO SLEEP. Raw-coordinate rows keep their proven user Delay.
            tradeTxn_.sequenceDueTick = effective->actionKind == 1
                ? GetTickCount()
                : GetTickCount() + static_cast<DWORD>(std::clamp(effective->delayMs, 50, 60000));
            resetUiRowRuntime();
            if (tradeTxn_.sequenceRepeatDone >= repeatLimit) {
                tradeTxn_.sequenceRepeatDone = 0;
                if (stored.groupId > 0) {
                    const std::size_t groupStart = TradeGroupStart(seq, tradeTxn_.sequenceIndex);
                    const std::size_t groupEnd = TradeGroupEnd(seq, tradeTxn_.sequenceIndex);
                    if (tradeTxn_.sequenceIndex < groupEnd) {
                        ++tradeTxn_.sequenceIndex;
                    } else {
                        ++tradeTxn_.sequenceGroupRepeatDone;
                        const int groupLimit = std::max(1, seq[groupStart].groupRepeat);
                        if (tradeTxn_.sequenceGroupRepeatDone < groupLimit) tradeTxn_.sequenceIndex = groupStart;
                        else { tradeTxn_.sequenceIndex = groupEnd + 1; tradeTxn_.sequenceGroupRepeatDone = 0; }
                    }
                } else {
                    ++tradeTxn_.sequenceIndex; tradeTxn_.sequenceGroupRepeatDone = 0;
                }
            }
        };

        const std::wstring who = stored.target == 1 ? L"MAIN" : (L"CON" + std::to_wstring(tradeTxn_.childSlot));

        // A successful ĐẶT LÊN callback is authoritative. ReadState/Bridge may still
        // be marked FREEZE for a short settle window, so do not dispatch the next raw
        // row (which would be rejected by DispatchInternalPointActionDirect and cause
        // a false AbortTrade). Wait for the normal 2s client-stable gate to clear.
        if (tradeTxn_.sequencePostPutUpRecoveryPending) {
            if (child.runtime.clientFreezeActive) {
                if (tradeTxn_.sequencePostPutUpRecoveryStartedTick != 0 &&
                    Elapsed(now, tradeTxn_.sequencePostPutUpRecoveryStartedTick, kTradePostPutUpRecoveryMaxMs)) {
                    const std::wstring why = L"GD AFTER • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) +
                        L" • ĐẶT LÊN CALLBACK PASS nhưng CLIENT FREEZE >" +
                        std::to_wstring(kTradePostPutUpRecoveryMaxMs) + L"ms • recovery timeout";
                    LogAccount(child, why + L" → TRADE RECOVERY");
                    tradeTxn_.sequencePostPutUpRecoveryPending = false;
                    tradeTxn_.sequencePostPutUpRecoveryStartedTick = 0;
                    AbortTrade(why, now);
                    return false;
                }
                SetTradeStatus(L"GD AFTER • ĐẶT LÊN CALLBACK PASS • HOLD CLIENT FREEZE • chờ RECOVERED, KHÔNG ABORT");
                return true;
            }

            LogAccount(child, L"GD AFTER • ĐẶT LÊN CALLBACK PASS • CLIENT RECOVERED → tiếp tục macro, KHÔNG ABORT");
            tradeTxn_.sequencePostPutUpRecoveryPending = false;
            tradeTxn_.sequencePostPutUpRecoveryStartedTick = 0;
            finalizeOccurrence();
            return true;
        }

        if (tradeTxn_.sequenceAfterPutUpPending) {
            if (tradeTxn_.sequenceAfterPutUpNextTryTick != 0 && static_cast<LONG>(now - tradeTxn_.sequenceAfterPutUpNextTryTick) < 0) return true;
            std::wstring semanticDetail;
            const bool semanticOk = CallFastTravelSemantic(child, TravelSemantic::PutUpPopup, false, semanticDetail);
            if (semanticOk) {
                LogAccount(child, L"GD AFTER • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) + L" • ĐẶT LÊN CALLBACK PASS");
                tradeTxn_.sequenceAfterPutUpPending = false;
                tradeTxn_.sequenceAfterPutUpStartedTick = 0; tradeTxn_.sequenceAfterPutUpNextTryTick = 0;
                if (child.runtime.clientFreezeActive) {
                    tradeTxn_.sequencePostPutUpRecoveryPending = true;
                    tradeTxn_.sequencePostPutUpRecoveryStartedTick = now;
                    SetTradeStatus(L"GD AFTER • ĐẶT LÊN CALLBACK PASS • client còn FREEZE → HOLD, KHÔNG ABORT");
                    LogAccount(child, L"GD AFTER • ĐẶT LÊN CALLBACK PASS • client còn FREEZE → HOLD trade state, chờ CLIENT RECOVERED • KHÔNG ABORT");
                    return true;
                }
                finalizeOccurrence(); return true;
            }
            if (SemanticBridgeRequestStillInFlight(semanticDetail)) {
                if (tradeTxn_.sequenceAfterPutUpStartedTick != 0 &&
                    Elapsed(now, tradeTxn_.sequenceAfterPutUpStartedTick, kTradePutUpCallbackWaitMaxMs)) {
                    const std::wstring why = L"GD AFTER • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) +
                        L" • ĐẶT LÊN CALLBACK WAIT >" + std::to_wstring(kTradePutUpCallbackWaitMaxMs) +
                        L"ms • recovery timeout";
                    LogAccount(child, why + L" → TRADE RECOVERY, không tiếp tục macro mù");
                    tradeTxn_.sequenceAfterPutUpPending = false;
                    tradeTxn_.sequenceAfterPutUpStartedTick = 0;
                    tradeTxn_.sequenceAfterPutUpNextTryTick = 0;
                    AbortTrade(why, now);
                    return false;
                }
                SetTradeStatus(L"GD AFTER • ĐẶT LÊN CALLBACK WAIT • Bridge/client còn bận → HOLD tối đa 8000ms, KHÔNG ABORT");
                tradeTxn_.sequenceAfterPutUpNextTryTick = now + 50;
                return true;
            }

            // A completed semantic request that returned a real error is not WAIT.
            // Fail closed immediately; the 8-second grace applies only while the
            // same Bridge request is genuinely still in flight/busy.
            const std::wstring why = L"GD AFTER • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) +
                L" • ĐẶT LÊN CALLBACK FAIL • " + semanticDetail;
            LogAccount(child, why + L" → TRADE RECOVERY, không tiếp tục macro mù");
            tradeTxn_.sequenceAfterPutUpPending = false;
            tradeTxn_.sequenceAfterPutUpStartedTick = 0;
            tradeTxn_.sequenceAfterPutUpNextTryTick = 0;
            AbortTrade(why, now);
            return false;
        }

        if (tradeTxn_.sequenceDueTick != 0 && static_cast<LONG>(now - tradeTxn_.sequenceDueTick) < 0) return true;

        if (effective->actionKind == 1) {
            if (tradeTxn_.sequenceUiRowIndex != tradeTxn_.sequenceIndex) {
                tradeTxn_.sequenceUiRowIndex = tradeTxn_.sequenceIndex;
                tradeTxn_.sequenceUiRowStartTick = now;
                tradeTxn_.sequenceUiInvokeStartedTick = 0;
                tradeTxn_.sequenceUiNextTryTick = now + static_cast<DWORD>(std::clamp(effective->minTimeMs, 0, 60000));
            }
            if (tradeTxn_.sequenceUiNextTryTick != 0 && static_cast<LONG>(now - tradeTxn_.sequenceUiNextTryTick) < 0) return true;
            if (tradeTxn_.sequenceUiInvokeStartedTick == 0) tradeTxn_.sequenceUiInvokeStartedTick = now;
            std::wstring uiDetail;
            const UiDirectTarget uiTarget = static_cast<UiDirectTarget>(effective->uiDirectTarget);
            const int direct = InvokeUiDirectNow(*target, uiTarget, uiDetail);
            if (direct > 0) {
                LogAccount(*target, L"GD UI DIRECT • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) + L" • " + TradeUiDirectLabel(effective->uiDirectTarget) + L" • PASS • MIN=" + std::to_wstring(effective->minTimeMs) + L"ms");
                finalizeOccurrence(); return true;
            }
            if (tradeTxn_.sequenceUiInvokeStartedTick != 0 && Elapsed(now, tradeTxn_.sequenceUiInvokeStartedTick, 2000)) {
                if (direct < 0 && SemanticBridgeRequestStillInFlight(uiDetail)) {
                    tradeTxn_.sequenceUiNextTryTick = now + 50;
                    return true;
                }
                const std::wstring why=L"GD UI DIRECT • ROW "+std::to_wstring(tradeTxn_.sequenceIndex+1)+L" • "+TradeUiDirectLabel(effective->uiDirectTarget)+L" • TIMEOUT 2000ms • fail-closed • "+uiDetail;
                LogAccount(*target,why+L" → TRADE RECOVERY, không SKIP");
                AbortTrade(why,now);return false;
            }
            tradeTxn_.sequenceUiNextTryTick = now + 50;
            return true;
        }

        std::wstring error;
        SetTradeStatus(L"TRADE CLICK → " + who + L" • dòng " + std::to_wstring(tradeTxn_.sequenceIndex + 1) + L"/" + std::to_wstring(seq.size()));
        if (!CoordinatorRawMacroPointAction(*target, effective->point,
                L"GD CON dùng chung • " + TradeRoleLabel(child.profile.tradeRole) + L" • dòng " + std::to_wstring(tradeTxn_.sequenceIndex + 1), error)) {
            AbortTrade(L"click GD dòng " + std::to_wstring(tradeTxn_.sequenceIndex + 1) + L" FAIL: " + error, now);
            return false;
        }

        const int rowRepeatOrdinal = std::min(repeatLimit, tradeTxn_.sequenceRepeatDone + 1);
        int groupRepeatOrdinal = 1, groupRepeatLimit = 1;
        if (stored.groupId > 0) {
            const std::size_t traceGroupStart = TradeGroupStart(seq, tradeTxn_.sequenceIndex);
            groupRepeatLimit = std::max(1, seq[traceGroupStart].groupRepeat);
            groupRepeatOrdinal = std::min(groupRepeatLimit, tradeTxn_.sequenceGroupRepeatDone + 1);
        }
        const int passTotal = std::max(1, tradeTxn_.childPlan.passesTotal);
        std::wstring trace = L"GD TRACE • PASS " + std::to_wstring(tradeTxn_.sequencePass) + L"/" + std::to_wstring(passTotal) +
                             L" • ROW " + std::to_wstring(tradeTxn_.sequenceIndex + 1) + L"/" + std::to_wstring(seq.size()) +
                             L" • ROW_REPEAT " + std::to_wstring(rowRepeatOrdinal) + L"/" + std::to_wstring(repeatLimit);
        if (stored.groupId > 0) trace += L" • GROUP " + std::to_wstring(stored.groupId) + L" • GROUP_REPEAT " + std::to_wstring(groupRepeatOrdinal) + L"/" + std::to_wstring(groupRepeatLimit);
        else trace += L" • GROUP -";
        trace += L" • TARGET " + who + L" • DELAY " + std::to_wstring(std::clamp(effective->delayMs, 50, 60000)) + L"ms";
        if (stored.target == 1) trace += L" • MAIN_REF " + std::to_wstring(stored.mainRef + 1);
        if (stored.target == 0 && stored.afterAction == 1) trace += L" • AFTER=ĐẶT LÊN";
        LogAccount(*target, trace);

        if (stored.target == 0 && stored.afterAction == 1) {
            tradeTxn_.sequenceAfterPutUpPending = true;
            tradeTxn_.sequenceAfterPutUpStartedTick = now;
            tradeTxn_.sequenceAfterPutUpNextTryTick = now;
            return true;
        }
        finalizeOccurrence();
        return true;
    }

    bool KeepMainStationary(Account& main, DWORD now) {
        if (!main.snapshotValid || !main.runtime.running || !IsWindow(main.game.window)) return false;
        const Snapshot& s = main.snapshot;
        if ((s.validMask & ValidLifeState) && s.dead) return false;
        if (!s.mapReady || s.waitingChangeMap) return false;
        main.tradeHeld = true;

        if ((s.validMask & ValidAutoPath) && s.autoPathing) {
            if (main.bridge.Attached()) {
                Response response{};
                std::wstring error;
                (void)main.bridge.Call(Command::StopPath, 0, 0, 0, response, error, 700);
            }
            main.runtime.status = L"MAIN ĐỨNG IM • đã chặn AutoPath phát sinh";
            return false;
        }
        if ((s.validMask & ValidRiding) && s.riding && tradeRendezvous_.valid &&
            s.mapID == tradeRendezvous_.mapID) {
            State state{};
            state.valid = true;
            state.mapReady = true;
            state.mapID = s.mapID;
            state.x = s.x;
            state.y = s.y;
            Target target{tradeRendezvous_.mapID, tradeRendezvous_.x,
                          tradeRendezvous_.y, tradeRendezvousTolerance_};
            if (AtTarget(state, target)) {
                // CP10: MAIN mounted tại đúng TỌA GD được coi là PARKED/READY.
                main.runtime.status = L"MAIN ĐỨNG IM • đã tới TỌA GD • giữ nguyên trên ngựa";
                return true;
            }
        }
        (void)now;
        return TradeAccountAtRendezvous(main);
    }

    void ReportChildAdmissionWait(Account& child, const std::wstring& reason) {
        if (child.runtime.tradeAdmissionReport == reason) return;
        child.runtime.tradeAdmissionReport = reason;
        LogAccount(child, L"FULL • Ở LẠI BÃI CHỜ SLOT • " + reason);
        AddLocalReport(L"CON FULL / WAIT", TelegramAccountLabel(child),
                       reason + L" • chưa có FIFO vì chưa rời/đến TỌA GD • " +
                       LocalDateTimeText());
    }

    void TickTradeCoordinator(DWORD now) {
        tradeEnabled_ = true; // v9.9: DỒN ĐỒ is mandatory for the whole running session.

        // Abort cleanup must be allowed to finish even if MAIN/CON disappeared after the abort.
        // It holds every surviving participant and only then releases normal FSM ownership.
        if (tradeTxn_.phase == TradePhase::Cleanup) {
            Account* cleanupMain = AccountByPid(tradeTxn_.mainPid);
            Account* cleanupChild = AccountByPid(tradeTxn_.childPid);
            if (cleanupMain && cleanupMain->runtime.running) cleanupMain->tradeHeld = true;
            if (cleanupChild && cleanupChild->runtime.running) cleanupChild->tradeHeld = true;
            (void)ExecuteAbortTradeCleanupTick(cleanupMain, cleanupChild, now);
            return;
        }

        Account* main = AccountByTradeRole(kMainTradeRole);
        if (!main || !main->runtime.running) {
            mainCapacityPlan_ = MainCapacityPlan{};
            if (!tradeTravelPids_.empty() || tradeTxn_.phase != TradePhase::Idle) AbortTrade(L"không còn MAIN đang RUN", now);
            SetTradeStatus(L"CHỜ • hãy gán và START đúng một MAIN"); return;
        }
        main->tradeHeld = true;

        // CP8 ATOMIC TRADE SEQUENCE FAST PATH
        // Once Sequence starts, it is a pure saved click macro. Do not run
        // rendezvous/AutoPath/queue/gameplay guards between macro rows.
        if (tradeTxn_.phase == TradePhase::Sequence) {
            Account* sequenceMain = AccountByPid(tradeTxn_.mainPid);
            Account* sequenceChild = AccountByPid(tradeTxn_.childPid);
            const bool technicalReady = sequenceMain && sequenceChild &&
                sequenceMain->runtime.running && sequenceChild->runtime.running &&
                IsWindow(sequenceMain->game.window) && IsWindow(sequenceChild->game.window);
            if (!technicalReady) {
                AbortTrade(L"mất MAIN/CON kỹ thuật trong chuỗi giao dịch", now);
                return;
            }
            sequenceMain->tradeHeld = true;
            sequenceChild->tradeHeld = true;
            (void)ExecuteTradeSequenceTick(*sequenceMain, *sequenceChild, now);
            return;
        }

        if (!tradeRendezvous_.valid) {
            SetTradeStatus(L"CHỜ • chưa lưu TỌA GD; MAIN không tự di chuyển");
            return;
        }

        const bool mainParked = KeepMainStationary(*main, now);

        // Remove only stopped/invalid workflow children. Death/revive keeps the reserved
        // slot and its arrival ticket, so a temporary death never lets CON5 chen vào.
        for (std::size_t i = 0; i < tradeTravelPids_.size();) {
            const DWORD pid = tradeTravelPids_[i];
            Account* child = AccountByPid(pid);
            const bool valid = child &&
                child->profile.tradeRole >= kFirstChildTradeRole &&
                child->profile.tradeRole <= kLastChildTradeRole &&
                child->runtime.running && IsWindow(child->game.window);
            if (valid) {
                ++i;
                continue;
            }
            if (pid == tradeTxn_.childPid && tradeTxn_.phase != TradePhase::Idle) {
                AbortTrade(L"CON active bị dừng/mất role/mất cửa sổ", now);
                return;
            }
            if (child) ReleaseTradeHold(*child);
            RemoveTradeQueuePid(pid);
            tradeTravelPids_.erase(tradeTravelPids_.begin() + static_cast<std::ptrdiff_t>(i));
        }

        std::wstring sequenceReason;
        const bool sequenceReady = TradeSequenceReady(sequenceReason);

        // Admission has no FIFO ticket. Exactly four FULL children at most may leave train.
        // The CON1→CON30 scan is deterministic when several bags become FULL in one tick.
        for (int slot = 1; slot <= kChildTradeCount; ++slot) {
            Account* child = AccountByTradeRole(slot + 1);
            if (!child || TradeTravelContains(child->game.pid)) continue;
            const bool fullRequested = child->snapshot.freeBagSpace == 0 ||
                                       image_scan_test::FullBagTravelLatched(child->game.window, slot);
            const bool fullReady = TradeStateReady(*child) && fullRequested;
            if (!fullReady) {
                if (child) child->runtime.tradeAdmissionReport.clear();
                continue;
            }
            if (!image_scan_test::FullBagYieldReady(child->game.window, slot)) {
                ReportChildAdmissionWait(*child, L"SCAN VK đang xử lý nốt item/đóng tay nải trước khi về TỌA GD");
                continue;
            }
            if (!sequenceReady) {
                ReportChildAdmissionWait(*child, L"Chuỗi giao dịch chưa sẵn sàng: " + sequenceReason);
                continue;
            }
            if (tradeTravelPids_.size() >= kMaxTravelingChildren) {
                ReportChildAdmissionWait(*child, L"Đã đủ 4/4 CON đang chạy/đợi/giao dịch");
                continue;
            }
            if (!ShouldAdmitFullChild(true, child->snapshot.freeBagSpace,
                                      tradeTravelPids_.size())) continue;
            tradeTravelPids_.push_back(child->game.pid);
            image_scan_test::NotifyTradeFlowStarted(child->game.window, slot);
            child->tradeHeld = true;
            child->runtime.tradeWorkflowEntrySeq = 0;
            child->runtime.tradeAdmissionReport.clear();
            BeginTradeRendezvousTravel(*child, now,
                                       (L"CON" + std::to_wstring(slot)).c_str());
            const std::wstring detail = L"FULL → nhận slot chạy về TỌA GD " +
                std::to_wstring(tradeTravelPids_.size()) + L"/4 • CHƯA có số FIFO.";
            LogAccount(*child, detail);
            AddLocalReport(L"CON FULL / SLOT", TelegramAccountLabel(*child),
                           detail + L" • " + LocalDateTimeText());
        }

        // Progress every admitted traveler in CON slot order. A ticket is minted only
        // after physical arrival. If arrivals are observed in the same tick, smaller CON wins.
        for (int slot = 1; slot <= kChildTradeCount; ++slot) {
            Account* child = AccountByTradeRole(slot + 1);
            if (!child || !TradeTravelContains(child->game.pid)) continue;

            const bool isActiveSequence =
                child->game.pid == tradeTxn_.childPid &&
                (tradeTxn_.phase == TradePhase::Sequence || tradeTxn_.phase == TradePhase::Cleanup);
            if (!isActiveSequence) {
                (void)HandleTradeRendezvousTravel(
                    *child, now, (L"CON" + std::to_wstring(slot)).c_str());
            }

            const bool arrived = child->runtime.tradeTravelReady &&
                                 TradeAccountAtRendezvous(*child);
            if (ShouldAssignArrivalTicket(arrived, TradeQueueContains(child->game.pid))) {
                child->runtime.tradeWorkflowEntrySeq = ++tradeWorkflowEntryCounter_;
                tradeQueuePids_.push_back(child->game.pid);
                LogAccount(*child, L"ĐÃ TỚI TỌA GD → nhận FIFO #" +
                                   std::to_wstring(child->runtime.tradeWorkflowEntrySeq) +
                                   L" • vị trí đợi " + std::to_wstring(tradeQueuePids_.size()) +
                                   L" • cùng tick ưu tiên CON số nhỏ.");
            }
        }

        if (!sequenceReady) {
            bool hasFull = false;
            for (int slot = 1; slot <= kChildTradeCount; ++slot) {
                Account* child = AccountByTradeRole(slot + 1);
                if (child && TradeStateReady(*child) && child->snapshot.freeBagSpace == 0) {
                    hasFull = true;
                    break;
                }
            }
            if (hasFull || !tradeTravelPids_.empty()) {
                SetTradeStatus(L"CHỜ • " + sequenceReason + L" • CON mới không được rời bãi");
                return;
            }
        }

        if (!mainParked) {
            if (!main->runtime.mainParkReportSent) {
                main->runtime.mainParkReportSent = true;
                AddLocalReport(L"MAIN NOT PARKED", TelegramAccountLabel(*main),
                               L"MAIN phải đứng đúng TỌA GD; tool chỉ StopPath và không AutoPath MAIN • " +
                               LocalDateTimeText());
            }
            SetTradeStatus(L"MAIN phải đứng đúng TỌA GD • tool chỉ StopPath, không AutoPath MAIN • " +
                           std::to_wstring(tradeTravelPids_.size()) + L"/4 CON đang chạy/đợi");
            return;
        }
        main->runtime.mainParkReportSent = false;
        if (mainCapacityPlan_.valid && mainCapacityPlan_.mainPid != main->game.pid) mainCapacityPlan_ = MainCapacityPlan{};
        if (!EnsureMainCapacityPlan(*main)) {
            SetTradeStatus(L"MAIN QUOTA • chờ snapshot FreeBag đầu epoch");
            return;
        }

        Account* activeMain = tradeTxn_.phase == TradePhase::Idle
            ? nullptr : AccountByPid(tradeTxn_.mainPid);
        Account* activeChild = tradeTxn_.phase == TradePhase::Idle
            ? nullptr : AccountByPid(tradeTxn_.childPid);

        if (tradeTxn_.phase == TradePhase::SellPause) {
            if (!activeMain || !activeChild || !TradeTravelContains(activeChild->game.pid) || !TradeQueueContains(activeChild->game.pid)) {
                AbortTrade(L"mất MAIN/CON khi đang giữ quota qua seller", now); return;
            }
            if (TickMainFullSellBatch(*activeMain, now)) return;
            if (!mainCapacityPlan_.valid || mainCapacityPlan_.passesRemaining <= 0) return;
            if (!tradeTxn_.childPlan.valid || tradeTxn_.childPlan.passesRemaining <= 0) { AbortTrade(L"CON quota mất khi resume seller", now); return; }
            const TradePhase resume = tradeTxn_.resumeAfterSell; tradeTxn_.resumeAfterSell = TradePhase::TargetMain; tradeTxn_.phase = resume;
            LogAccount(*activeChild,L"MAIN quota mới="+std::to_wstring(mainCapacityPlan_.passesRemaining)+L" • giữ đúng CON còn="+std::to_wstring(tradeTxn_.childPlan.passesRemaining)+L" pass.");
            AddLocalReport(L"TRADE QUOTA RESUME",TelegramAccountLabel(*activeChild),L"M="+std::to_wstring(mainCapacityPlan_.passesRemaining)+L" • C="+std::to_wstring(tradeTxn_.childPlan.passesRemaining)+L" • same child");
            return;
        }

        if (tradeTxn_.phase == TradePhase::Rendezvous) {
            if (!activeMain || !activeChild || !TradeQueueContains(activeChild->game.pid) ||
                !TradeTravelContains(activeChild->game.pid)) {
                AbortTrade(L"mất cặp active trước giao dịch", now);
                return;
            }
            if (!TradePairReadyForPreparation(*activeMain, *activeChild) ||
                !TradeAccountAtRendezvous(*activeChild)) {
                SetTradeStatus(L"GIỮ CON" + std::to_wstring(tradeTxn_.childSlot) +
                               L" • chờ state an toàn tại TỌA GD");
                return;
            }
            if (!BuildChildTradePlanOnce(*activeChild, now)) return;
            if (tradeTxn_.childPlan.passesRemaining <= 0) { SkipChildNoFullPass(*activeChild, now); return; }
            if (mainCapacityPlan_.passesRemaining <= 0) {
                PauseTradeForMainSell(TradePhase::TargetMain, now, L"CON có quota nhưng MAIN quota=0 trước pass đầu"); return;
            }
            tradeTxn_.phase = TradePhase::TargetMain;
            tradeTxn_.targetStartedTick = now;
            tradeTxn_.targetRetryTick = now;
            tradeTxn_.targetLastSelectTick = 0;
            tradeTxn_.targetAttempts = 0;
            return;
        }

        if (tradeTxn_.phase == TradePhase::TargetMain) {
            if (!activeMain || !activeChild || !TradeQueueContains(activeChild->game.pid) ||
                !TradePairReadyForPreparation(*activeMain, *activeChild) ||
                !TradeAccountAtRendezvous(*activeChild)) {
                AbortTrade(L"mất acc/state khi target MAIN", now);
                return;
            }
            if (!tradeTxn_.childPlan.valid || tradeTxn_.childPlan.passesRemaining <= 0) { AbortTrade(L"CON quota không hợp lệ trước TARGET", now); return; }
            if (!mainCapacityPlan_.valid || mainCapacityPlan_.passesRemaining <= 0) {
                PauseTradeForMainSell(TradePhase::TargetMain, now, L"MAIN quota=0 trước TARGET"); return;
            }
            if (activeMain->snapshot.roleID <= 0) {
                AbortTrade(L"RoleID MAIN không hợp lệ", now);
                return;
            }
            if (Elapsed(now, tradeTxn_.targetStartedTick, kTradeTargetTimeoutMs)) {
                AbortTrade(L"timeout target MAIN theo RoleID", now);
                return;
            }
            if (tradeTxn_.targetRetryTick != 0 &&
                static_cast<LONG>(now - tradeTxn_.targetRetryTick) < 0) return;

            Response response{};
            std::wstring error;
            ++tradeTxn_.targetAttempts;
            const bool allowSelect = tradeTxn_.targetLastSelectTick == 0 ||
                                     Elapsed(now, tradeTxn_.targetLastSelectTick, 2000);
            const bool ok = activeChild->bridge.Call(
                Command::SelectTargetByRoleID, activeMain->snapshot.roleID,
                allowSelect ? 1 : 0, 0, response, error, 1100);
            if (!ok) {
                if (BridgeLooksUnresponsive(error))
                    EnterClientFreeze(*activeChild, L"Bridge timeout khi target MAIN", now);
                tradeTxn_.targetRetryTick = GetTickCount() + kTradeTargetRetryMs;
                return;
            }
            if (response.value1 != 0) tradeTxn_.targetLastSelectTick = GetTickCount();
            if (response.resultCode != static_cast<std::int32_t>(ActionResult::ActionInvoked)) {
                tradeTxn_.targetRetryTick = GetTickCount() + kTradeTargetRetryMs;
                return;
            }

            if (!tradeTxn_.postTradeClickCompleted &&
                ExecuteTradeMenuOpenClickTick(*activeChild, now)) {
                return;
            }

            Response tradeCallbackResponse{};
            std::wstring tradeCallbackError;
            const bool tradeCallbackOk = activeChild->bridge.Call(
                Command::ClickTravelSemantic, static_cast<int>(TravelSemantic::Trade),
                0, 0, tradeCallbackResponse, tradeCallbackError, 1100);
            if (!tradeCallbackOk ||
                tradeCallbackResponse.resultCode != static_cast<std::int32_t>(ActionResult::ActionInvoked)) {
                tradeTxn_.targetRetryTick = GetTickCount() + kTradeTargetRetryMs;
                // Menu may have failed to open or may have been rebuilt. Force the
                // configured CON click to run again before the next semantic retry.
                tradeTxn_.postTradeClickCompleted = false;
                tradeTxn_.postTradeClickRepeatDone = 0;
                tradeTxn_.postTradeClickDueTick = tradeTxn_.targetRetryTick;
                tradeTxn_.postTradeClickSkipReported = false;
                const std::wstring detail = !tradeCallbackError.empty() ? tradeCallbackError :
                    (tradeCallbackResponse.detail[0] ? std::wstring(tradeCallbackResponse.detail) : L"chưa thấy dòng Giao dịch");
                LogAccount(*activeChild, L"GIAO DỊCH CALLBACK WAIT • " + detail);
                return;
            }
            LogAccount(*activeChild, L"GIAO DỊCH CALLBACK PASS • " + std::wstring(tradeCallbackResponse.detail));

            tradeTxn_.phase = TradePhase::Sequence;
            tradeTxn_.sequenceIndex = 0;
            tradeTxn_.sequenceRepeatDone = 0;
            tradeTxn_.sequenceGroupRepeatDone = 0;
            tradeTxn_.sequenceDueTick = GetTickCount();
            tradeTxn_.sequenceUiRowStartTick = 0;
            tradeTxn_.sequenceUiInvokeStartedTick = 0;
            tradeTxn_.sequenceUiNextTryTick = 0;
            tradeTxn_.sequenceUiRowIndex = static_cast<std::size_t>(-1);
            tradeTxn_.cleanupStage = 0;
            tradeTxn_.cleanupDueTick = 0;
            tradeTxn_.cleanupStartedTick = 0;
            tradeTxn_.cleanupSweepChanged = false;
            tradeTxn_.cleanupSweepUnresolved = false;
            tradeTxn_.cleanupSweepCount = 0;
            tradeTxn_.cleanupCleanSweepStreak = 0;
            tradeTxn_.cleanupAbortPending = false;
            tradeTxn_.cleanupAbortReason.clear();
            tradeTxn_.sequenceTelemetryCounted = false;
            tradeTxn_.postTradeClickCompleted = false;
            tradeTxn_.postTradeClickTarget = 0;
            tradeTxn_.postTradeClickRepeatDone = 0;
            tradeTxn_.postTradeClickDueTick = 0;
            tradeTxn_.postTradeClickSkipReported = false;
            LogAccount(*activeChild, L"TARGET MAIN PASS " + std::to_wstring(tradeTxn_.sequencePass) +
                                      L" • quota M=" + std::to_wstring(mainCapacityPlan_.passesRemaining) +
                                      L" C=" + std::to_wstring(tradeTxn_.childPlan.passesRemaining) +
                                      L" • CLICK SAU TARGET → Giao dịch → macro dòng 1");
            return;
        }

        // CP8: Sequence is handled by the atomic fast path above before any gameplay guard.

        if (tradeTxn_.phase != TradePhase::Idle) {
            AbortTrade(L"state giao dịch không hợp lệ", now);
            return;
        }

        // V18 Task B: queue-empty transition gets exactly one idle sell sweep.
        // Any non-empty queue rearms the next empty epoch; no infinite idle clicking.
        if (!tradeQueuePids_.empty()) idleSellEpochDone_ = false;
        if (tradeQueuePids_.empty()) {
            if (main->runtime.sellPhase == 0 && !idleSellEpochDone_) {
                if (!BeginMainMacroSell(*main, true)) return;
            }
            SetTradeStatus(main->runtime.sellPhase != 0 ?
                L"MAIN idle sweep • scan/count + macro bán" :
                L"MAIN đứng chờ • idle sell epoch đã xong");
            return;
        }

        if (tradeQueuePids_.empty()) {
            SetTradeStatus(L"MAIN đứng chờ • queue CON rỗng");
            return;
        }
        if (tradeTxn_.cooldownUntil != 0 &&
            static_cast<LONG>(now - tradeTxn_.cooldownUntil) < 0) {
            SetTradeStatus(L"FIFO " + std::to_wstring(tradeQueuePids_.size()) +
                           L" • chờ cooldown giữa 2 CON");
            return;
        }

        Account* nextChild = EarliestQueuedChild();
        if (!nextChild) return;
        if (main->runtime.sellPhase != 0 && main->macroSell.idleSweep) {
            LogAccount(*main, L"MAIN SELL → TRADE TAKEOVER • dừng idle-sell runtime cũ; sau giao dịch sẽ scan tay nải mới thay vì resume ordinal cũ");
            ClearMainMacroSellRuntime(*main);
            idleSellEpochDone_ = false;
        }
        tradeTxn_.mainPid = main->game.pid;
        tradeTxn_.childPid = nextChild->game.pid;
        tradeTxn_.childSlot = nextChild->profile.tradeRole - 1;
        tradeTxn_.sequencePass = 1;
        tradeTxn_.childPlan = ChildTradePlan{};
        tradeTxn_.phase = TradePhase::Rendezvous;
        tradeTxn_.resumeAfterSell = TradePhase::TargetMain;
        tradeTxn_.cooldownUntil = 0;
        SetTradeStatus(L"FIFO #" +
                       std::to_wstring(nextChild->runtime.tradeWorkflowEntrySeq) +
                       L" • tới lượt CON" + std::to_wstring(tradeTxn_.childSlot) +
                       L" • queue " + std::to_wstring(tradeQueuePids_.size()) +
                       L" • slot workflow " + std::to_wstring(tradeTravelPids_.size()) + L"/4");
        AddLocalReport(L"TRADE START", TelegramAccountLabel(*nextChild),
                       L"FIFO #" + std::to_wstring(nextChild->runtime.tradeWorkflowEntrySeq) +
                       L" • CON" + std::to_wstring(tradeTxn_.childSlot) +
                       L" • pass #1 • MAIN quota=" +
                       std::to_wstring(mainCapacityPlan_.passesRemaining));
    }


    void ClearEditor() {
        SetText(selected_, L"ACC ĐANG CHỈNH: chưa chọn");
        SetText(live_, L"STATE: chưa có");
        if (tradeRoleCombo_) SendMessageW(tradeRoleCombo_, CB_SETCURSEL, 0, 0);
        SetText(targetName_, L"");
        if (spotCombo_) SendMessageW(spotCombo_, CB_SETCURSEL, -1, 0);
        SetText(targetText_, L"CHƯA CHỌN");
        SetText(tolerance_, L"120");
        SendMessageW(enableRevive_, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(enableConfirm_, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(enableFight_, BM_SETCHECK, BST_UNCHECKED, 0);
        for (HWND h : pointLabels_) if (h) SetText(h, L"CHƯA LẤY");
        UpdateRoleActionButtons();
    }

    void LoadSelectedProfileToUi() {
        Account* a = SelectedAccount();
        if (!a) { ClearEditor(); return; }
        ResolveProfileTarget(a->profile);
        SetText(selected_, L"ACC ĐANG CHỈNH: " + AccountTag(*a));
        if (tradeRoleCombo_) SendMessageW(tradeRoleCombo_, CB_SETCURSEL,
                                           a->profile.tradeRole == kMainTradeRole ? 1 : 0, 0);
        RefreshSpotCombo();
        SetText(targetName_, a->profile.selectedSpot);
        SetText(tolerance_, std::to_wstring(a->profile.tolerance));
        SendMessageW(enableRevive_, BM_SETCHECK, a->profile.enableRevive ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(enableConfirm_, BM_SETCHECK, a->profile.enableConfirm ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(enableFight_, BM_SETCHECK, a->profile.enableFight ? BST_CHECKED : BST_UNCHECKED, 0);
        if (a->profile.target.valid) {
            SetText(targetText_, L"M" + std::to_wstring(a->profile.target.mapID) + L" • " +
                                std::to_wstring(a->profile.target.x) + L"," + std::to_wstring(a->profile.target.y));
        } else {
            SetText(targetText_, L"CHƯA CHỌN");
        }
        for (int i : {static_cast<int>(ClickSlot::AutoMenu), static_cast<int>(ClickSlot::Attack), static_cast<int>(ClickSlot::StopAuto2)}) {
            if (pointLabels_[static_cast<std::size_t>(i)])
                SetText(pointLabels_[static_cast<std::size_t>(i)], PointDescription(a->profile.points[static_cast<std::size_t>(i)]));
        }
        UpdateRoleActionButtons();
        UpdateSelectedLive();
    }

    void PersistSelectedEditor() {
        Account* a = SelectedAccount();
        if (!a) return;
        int tol = _wtoi(GetText(tolerance_).c_str());
        if (tol < 20) tol = 20;
        if (tol > 2000) tol = 2000;
        a->profile.tolerance = tol;
        a->profile.enableRevive = SendMessageW(enableRevive_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        a->profile.enableConfirm = SendMessageW(enableConfirm_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        a->profile.enableFight = SendMessageW(enableFight_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SaveProfile(a->profile);
        const int row = SelectedIndex();
        if (row >= 0) UpdateAccountRow(row, *a);
    }

    void SaveTargetForSelected() {
        Account* a = SelectedAccount();
        if (!a) { Log(L"Chưa chọn acc"); return; }
        PersistSelectedEditor();
        std::wstring error;
        if (!ReadSnapshot(*a, error, 1200)) { LogAccount(*a, L"Không đọc được state để lưu bãi: " + error); return; }
        const Snapshot& s = a->snapshot;
        if (!s.mapReady || s.waitingChangeMap ||
            (s.validMask & (ValidMap | ValidPosition)) != (ValidMap | ValidPosition)) {
            LogAccount(*a, L"State chưa ổn định, không lưu bãi.");
            return;
        }
        std::wstring name = GetText(targetName_);
        if (name.empty()) name = L"Bãi M" + std::to_wstring(s.mapID) + L" " + std::to_wstring(s.x) + L"," + std::to_wstring(s.y);
        TargetProfile spot{name, s.mapID, s.x, s.y, true};
        const int existing = FindSpotIndex(spots_, name);
        if (existing >= 0) spots_[static_cast<std::size_t>(existing)] = spot;
        else spots_.push_back(spot);
        SaveSharedSpots(spots_);
        a->profile.selectedSpot = name;
        a->profile.target = spot;
        SaveProfile(a->profile);
        RefreshSpotCombo();
        LoadSelectedProfileToUi();
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            if (_wcsicmp(accounts_[i]->profile.selectedSpot.c_str(), name.c_str()) == 0) {
                accounts_[i]->profile.target = spot;
                SaveProfile(accounts_[i]->profile);
                UpdateAccountRow(static_cast<int>(i), *accounts_[i]);
            }
        }
        LogAccount(*a, L"COORD CAPTURE RAW TRAIN • đã lưu/cập nhật bãi CHUNG: " + name + L" • M" + std::to_wstring(s.mapID) + L" • " +
                       std::to_wstring(s.x) + L"," + std::to_wstring(s.y));
    }

    void UpdateTradeRendezvousLabel() {
        if (!tradeRendezvousLabel_) return;
        if (!tradeRendezvous_.valid) {
            SetText(tradeRendezvousLabel_, L"CHƯA LẤY TỌA GD");
            return;
        }
        SetText(tradeRendezvousLabel_, L"M" + std::to_wstring(tradeRendezvous_.mapID) + L" • " +
                                      std::to_wstring(tradeRendezvous_.x) + L"," + std::to_wstring(tradeRendezvous_.y));
    }

    void CaptureTradeRendezvous() {
        Account* source = SelectedAccount();
        if (!source) source = AccountByTradeRole(1);
        if (!source) { Log(L"TỌA GD: hãy chọn một acc hoặc gán MAIN trước."); return; }
        std::wstring error;
        if (!ReadSnapshot(*source, error, 1200)) { LogAccount(*source, L"TỌA GD: không đọc được state: " + error); return; }
        const Snapshot& state = source->snapshot;
        if (!state.mapReady || state.waitingChangeMap ||
            (state.validMask & (ValidMap | ValidPosition)) != (ValidMap | ValidPosition)) {
            LogAccount(*source, L"TỌA GD: state Map/X/Y chưa ổn định, không lưu.");
            return;
        }
        tradeRendezvous_.name = L"TỌA GD";
        tradeRendezvous_.mapID = state.mapID;
        tradeRendezvous_.x = state.x;
        tradeRendezvous_.y = state.y;
        tradeRendezvous_.valid = true;
        WriteIniInt(L"Global", L"TradeRendezvousMap", tradeRendezvous_.mapID);
        WriteIniInt(L"Global", L"TradeRendezvousX", tradeRendezvous_.x);
        WriteIniInt(L"Global", L"TradeRendezvousY", tradeRendezvous_.y);
        WriteIniInt(L"Global", L"TradeRendezvousValid", 1);
        WriteIniInt(L"Global", L"TradeRendezvousTolerance", tradeRendezvousTolerance_);
        FlushIni();
        UpdateTradeRendezvousLabel();
        LogAccount(*source, L"COORD CAPTURE RAW GD • M" + std::to_wstring(tradeRendezvous_.mapID) + L" • " +
                            std::to_wstring(tradeRendezvous_.x) + L"," + std::to_wstring(tradeRendezvous_.y) +
                            L" • TOL=" + std::to_wstring(tradeRendezvousTolerance_));
    }

    void BeginCapture(ClickSlot slot) {
        Account* a = SelectedAccount();
        if (!a) { Log(L"Chưa chọn acc để lấy tọa độ"); return; }
        shortcutPostTradeCapture_ = false;
        
        captureSlot_ = slot;
        captureTradeSequenceIndex_ = -1;
        captureTradeSequenceMode_ = 0;
        captureTradeSequenceMainRef_ = -1;
        capturePid_ = a->game.pid;
        const int index = static_cast<int>(slot);
        LogAccount(*a, L"Đang chờ F8 để lấy điểm " + std::wstring(kClickLabels[static_cast<std::size_t>(index)]) + L".");
        SetText(selected_, L"LẤY TỌA ĐỘ CHO " + AccountTag(*a) + L" • đưa chuột vào nút rồi F8");
    }

    void CaptureHotkeyPoint() {

        const bool hasMode = partyBuildCaptureIndex_ >= 0 || shortcutPostTradeCapture_ || shortcutKunlunCaptureIndex_ >= 0 ||
                             captureSlot_ != ClickSlot::None ||
                             captureTradeSequenceIndex_ >= 0;
        if (!hasMode || capturePid_ == 0) return;
        Account* captureAccount = AccountByPid(capturePid_);
        if (!captureAccount || !IsWindow(captureAccount->game.window)) {
            Log(L"Lấy tọa độ thất bại: acc/cửa sổ đã mất.");
            captureSlot_ = ClickSlot::None; capturePid_ = 0;
            captureTradeSequenceIndex_ = -1; captureTradeSequenceMode_ = 0; captureTradeSequenceMainRef_ = -1;
            shortcutKunlunCaptureIndex_ = -1; shortcutPostTradeCapture_ = false;  partyBuildCaptureIndex_ = -1;
            return;
        }
        POINT screen{};
        if (!GetCursorPos(&screen)) return;
        POINT client = screen;
        if (!ScreenToClient(captureAccount->game.window, &client)) return;
        RECT rc{};
        if (!GetClientRect(captureAccount->game.window, &rc)) return;
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (client.x < 0 || client.y < 0 || client.x >= width || client.y >= height) {
            LogAccount(*captureAccount, L"F8 bỏ qua: con trỏ không nằm trong client game của acc đích.");
            return;
        }
        const ClickPoint captured{client.x, client.y, width, height, true};
        if (partyBuildCaptureIndex_ >= 0 && partyBuildCaptureIndex_ < 3) {
            const int index = partyBuildCaptureIndex_;
            partyBuildSettings_.clicks[static_cast<std::size_t>(index)] = captured;
            SavePartyBuildSettings(partyBuildSettings_);
            LoadPartyBuildSettingsToUi();
            const wchar_t* label = index == 0 ? L"KEY CLICK 1" : (index == 1 ? L"KEY CLICK 2" : L"MẶT MEMBER");
            LogAccount(*captureAccount, std::wstring(L"AUTO PT F8 PASS • ") + label + L" = " + PointDescription(captured));
        } else if (shortcutPostTradeCapture_) {
            shortcutSettings_.postTradeClick = captured;
            SaveShortcutSettings(shortcutSettings_);
            LoadShortcutSettingsToUi();
            LogAccount(*captureAccount, L"CLICK SAU TARGET MAIN: đã lưu tọa dùng cho CON = " + PointDescription(captured));
        } else if (shortcutKunlunCaptureIndex_ >= 0 && shortcutKunlunCaptureIndex_ < 3) {
            const int index = shortcutKunlunCaptureIndex_;
            shortcutSettings_.kunlunExitClicks[static_cast<std::size_t>(index)].point = captured;
            SaveShortcutSettings(shortcutSettings_);
            LoadShortcutSettingsToUi();
            LogAccount(*captureAccount, L"ĐƯỜNG TẮT CLS: đã lưu TryClickUI " + std::to_wstring(index + 1) +
                                         L"/3 dùng chung ALL ACC = " + PointDescription(captured));
        } else if (captureTradeSequenceIndex_ >= 0) {
            bool saved = false;
            if (captureTradeSequenceMode_ == 1) {
                if (captureTradeSequenceIndex_ < static_cast<int>(mainTradeSequence_.size())) {
                    mainTradeSequence_[static_cast<std::size_t>(captureTradeSequenceIndex_)].point = captured;
                    SaveMainTradeSequence();
                    saved = true;
                }
            } else if (captureTradeSequenceMode_ == 2) {
                if (captureTradeSequenceMainRef_ >= 0) {
                    if (captureTradeSequenceMainRef_ < static_cast<int>(mainTradeSequence_.size())) {
                        mainTradeSequence_[static_cast<std::size_t>(captureTradeSequenceMainRef_)].point = captured;
                        SaveMainTradeSequence();
                        saved = true;
                    }
                } else {
                    EnsureSharedChildTradeSequence();
                    if (captureTradeSequenceIndex_ < static_cast<int>(childTradeSequence_.size())) {
                        childTradeSequence_[static_cast<std::size_t>(captureTradeSequenceIndex_)].point = captured;
                        SaveSharedChildTradeSequence();
                        saved = true;
                    }
                }
            }

            if (!saved) {
                LogAccount(*captureAccount, L"F8 chuỗi GD thất bại: dòng/đích capture đã đổi hoặc không còn tồn tại.");
            } else {
                if (tradeEditor_ && IsWindow(tradeEditor_) && tradeEditorMode_ == captureTradeSequenceMode_) {
                    RefreshTradeSequenceList();
                    if (tradeSeqList_ && captureTradeSequenceIndex_ < ListView_GetItemCount(tradeSeqList_)) {
                        ListView_SetItemState(tradeSeqList_, captureTradeSequenceIndex_,
                                              LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_EnsureVisible(tradeSeqList_, captureTradeSequenceIndex_, FALSE);
                        LoadTradeSequenceRowToEditor(captureTradeSequenceIndex_);
                    }
                }
                LogAccount(*captureAccount, L"Đã lưu chuỗi GD dòng " + std::to_wstring(captureTradeSequenceIndex_ + 1) + L" qua F8 = " + PointDescription(captured));
            }
        } else {
            const int index = static_cast<int>(captureSlot_);
            if (index >= 0 && index < static_cast<int>(kClickKeys.size())) {
                if (index < static_cast<int>(captureAccount->profile.points.size())) {
                    captureAccount->profile.points[static_cast<std::size_t>(index)] = captured;
                    SaveProfile(captureAccount->profile);
                    LogAccount(*captureAccount, L"Đã lưu " + std::wstring(kClickLabels[static_cast<std::size_t>(index)]) + L" = " + PointDescription(captured));
                }
            }
        }
        LoadSelectedProfileToUi();
        captureSlot_ = ClickSlot::None; capturePid_ = 0;
        captureTradeSequenceIndex_ = -1; captureTradeSequenceMode_ = 0; captureTradeSequenceMainRef_ = -1;
        shortcutKunlunCaptureIndex_ = -1; shortcutPostTradeCapture_ = false;  partyBuildCaptureIndex_ = -1;
    }

    bool DispatchInternalPointActionDirect(Account& a, const ClickPoint& savedPoint,
                                           const std::wstring& request,
                                           std::wstring& error,
                                           bool rawMacro = false) {
        if (a.runtime.clientFreezeActive) {
            error = L"client/map đang FREEZE; hidden action bị chặn";
            return false;
        }
        if (!IsWindow(a.game.window)) {
            error = L"Cửa sổ game không còn tồn tại";
            return false;
        }

        int normalizedX = -1;
        int normalizedY = -1;
        if (!NormalizeClickPointForBridge(a.game, savedPoint,
                                          normalizedX, normalizedY, error)) {
            return false;
        }

        std::wstring attachError;
        if (!EnsureAttach(a, attachError)) {
            error = L"Không attach được Bridge cho hidden action: " + attachError;
            return false;
        }

        Response response{};
        const bool ok = a.bridge.Call(Command::ClickInternalPoint,
                                      normalizedX, normalizedY, rawMacro ? 1 : 0,
                                      response, error, 2200);
        const DWORD completedAt = GetTickCount();
        if (!ok) {
            if (BridgeLooksUnresponsive(error)) {
                EnterClientFreeze(a, L"Bridge timeout khi chạy hidden point action", completedAt);
            }
            return false;
        }

        LogAccount(a, std::wstring(rawMacro ? L"RAW MACRO DISPATCH PASS • " : L"HIDDEN ACTION DISPATCH PASS • ") + request +
                      L" • normalized=" + std::to_wstring(normalizedX) + L"," +
                      std::to_wstring(normalizedY) + L" • " + response.detail);
        return true;
    }

    // v0.6.1.9: hidden InputSync actions do not share a Windows-input resource.
    // Business workflows serialize themselves (SELL per account, TRADE per active pair),
    // so there is no global input/owner/sequence lease between unrelated clients.
    bool CoordinatorInternalPointAction(Account& target, const ClickPoint& savedPoint,
                                        const std::wstring& request,
                                        std::wstring& error) {
        if (RecorderBlocksAccount(target)) {
            error = L"acc đang REC cấu hình; hidden action của chính acc này tạm giữ";
            return false;
        }
        return DispatchInternalPointActionDirect(target, savedPoint, request, error);
    }

    // Trade-sequence-only raw macro path. It preserves process/window/Bridge/license
    // guards, coordinate scaling and InputSync dispatch, but does not use UI
    // raycast/_uiDragging evidence as a business condition for advancing the macro.
    bool CoordinatorRawMacroPointAction(Account& target, const ClickPoint& savedPoint,
                                        const std::wstring& request,
                                        std::wstring& error) {
        if (RecorderBlocksAccount(target)) {
            error = L"acc đang REC cấu hình; raw macro của chính acc này tạm giữ";
            return false;
        }
        return DispatchInternalPointActionDirect(target, savedPoint, request, error, true);
    }

    bool QueuePriorityAutoClick(Account& a, ClickSlot slot, PriorityAutoOwner owner,
                                const std::wstring& reason) {
        RuntimeState& rt = a.runtime;
        if (slot != ClickSlot::AutoMenu && slot != ClickSlot::Attack && slot != ClickSlot::StopAuto2) return false;
        if (owner == PriorityAutoOwner::None) return false;
        if (!rt.running || rt.clientFreezeActive || !a.snapshotValid || !IsWindow(a.game.window)) return false;
        if (slot == ClickSlot::Attack &&
            !travel_fight_guard_logic::CanDispatchFightStart(
                (a.snapshot.validMask & ValidAutoPath) != 0,
                a.snapshot.autoPathing != 0)) {
            rt.status = L"PRIORITY #3 AUTO • chặn bật AutoFight vì AutoPath chưa authoritative OFF";
            return false;
        }
        if (rt.priorityAutoCompletedSlot != ClickSlot::None &&
            (rt.priorityAutoCompletedSlot != slot ||
             rt.priorityAutoCompletedOwner != owner)) {
            LogAccount(a, L"PRIORITY #3 AUTO: bỏ result cũ " + std::wstring(kClickLabels[static_cast<std::size_t>(rt.priorityAutoCompletedSlot)]) + L" do workflow đã đổi pha.");
            rt.priorityAutoCompletedSlot = ClickSlot::None;
            rt.priorityAutoCompletedOwner = PriorityAutoOwner::None;
            rt.priorityAutoCompletedOk = false;
            rt.priorityAutoCompletedTick = 0;
        }
        if (rt.priorityAutoRequestSlot == slot &&
            rt.priorityAutoRequestOwner == owner) return true;
        if (rt.priorityAutoRequestSlot != ClickSlot::None || rt.priorityAutoCompletedSlot != ClickSlot::None) return false;
        rt.priorityAutoRequestSlot = slot;
        rt.priorityAutoRequestOwner = owner;
        rt.priorityAutoPointPhase = 0;
        rt.priorityAutoPointTick = 0;
        rt.status = L"PRIORITY #3 AUTO INPUTSYNC • đã xếp hàng " +
                    std::wstring(kClickLabels[static_cast<std::size_t>(slot)]);
        if (!reason.empty()) LogAccount(a, L"PRIORITY #3 AUTO QUEUE: " + reason);
        return true;
    }

    bool ConsumePriorityAutoResult(Account& a, ClickSlot slot, PriorityAutoOwner owner,
                                   bool& ok, DWORD& clickedAt) {
        RuntimeState& rt = a.runtime;
        if (rt.priorityAutoCompletedSlot != slot ||
            rt.priorityAutoCompletedOwner != owner) return false;
        ok = rt.priorityAutoCompletedOk;
        clickedAt = rt.priorityAutoCompletedTick;
        rt.priorityAutoCompletedSlot = ClickSlot::None;
        rt.priorityAutoCompletedOwner = PriorityAutoOwner::None;
        rt.priorityAutoCompletedOk = false;
        rt.priorityAutoCompletedTick = 0;
        return true;
    }

    bool PriorityAutoClick(Account& a) {
        RuntimeState& rt = a.runtime;
        const ClickSlot requestedSlot = rt.priorityAutoRequestSlot;
        const PriorityAutoOwner requestedOwner = rt.priorityAutoRequestOwner;
        if (requestedSlot == ClickSlot::None) return false;
        if (requestedOwner == PriorityAutoOwner::None) {
            rt.priorityAutoRequestSlot = ClickSlot::None;
            return false;
        }
        if (!rt.running || rt.clientFreezeActive || !a.snapshotValid || !IsWindow(a.game.window)) return false;
        const Snapshot& s = a.snapshot;

        auto complete = [&](bool ok, DWORD completedAt) {
            rt.priorityAutoRequestSlot = ClickSlot::None;
            rt.priorityAutoRequestOwner = PriorityAutoOwner::None;
            rt.priorityAutoCompletedSlot = requestedSlot;
            rt.priorityAutoCompletedOwner = requestedOwner;
            rt.priorityAutoCompletedOk = ok;
            rt.priorityAutoCompletedTick = completedAt;
            rt.priorityAutoPointPhase = 0;
            rt.priorityAutoPointTick = 0;
        };
        const bool unsafeFightStart = requestedSlot == ClickSlot::Attack &&
            (!s.mapReady || s.waitingChangeMap ||
             ((s.validMask & ValidLifeState) && s.dead) ||
             !travel_fight_guard_logic::CanDispatchFightStart(
                 (s.validMask & ValidAutoPath) != 0, s.autoPathing != 0));
        const bool staleTrainStart = requestedSlot == ClickSlot::Attack &&
            requestedOwner == PriorityAutoOwner::Train &&
            AutoFightCheckBusy(a, GetTickCount());
        if (unsafeFightStart || staleTrainStart) {
            complete(false, GetTickCount());
            rt.status = L"PRIORITY #3 AUTO • hủy request cũ vì state không còn cho phép bật Fight";
            LogAccount(a, L"PRIORITY #3 AUTO SAFETY: hủy AUTO→ĐÁNH QUÁI trước dispatch vì "
                          L"AutoPath/map/workflow không còn ở state đã xếp hàng; không dùng request cũ.");
            return false;
        }

        if (!s.mapReady || s.waitingChangeMap ||
            ((s.validMask & ValidLifeState) && s.dead)) return false;

        // v0.6.1.7: both AUTO->Attack and AUTO->Stop are menu-choice sequences.
        // StopAuto2 is not a standalone visible control while the AUTO menu is closed.
        // The v0.6.1.4 direct-Stop shortcut could therefore raycast empty UI exactly when
        // Trade/Travel Guard tried to leave a training spot. Restore the proven v0.5
        // lifecycle, but keep every phase on the hidden InputSync dispatcher.
        constexpr auto autoChoicePlan = internal_ui_click_logic::AutoMenuChoicePlan();
        const bool autoMenuChoiceSequence =
            requestedSlot == ClickSlot::Attack || requestedSlot == ClickSlot::StopAuto2;
        ClickSlot pointSlot = requestedSlot;
        if (autoMenuChoiceSequence) {
            if (rt.priorityAutoPointPhase < 0 ||
                rt.priorityAutoPointPhase >= static_cast<int>(autoChoicePlan.size())) {
                rt.priorityAutoPointPhase = 0;
                rt.priorityAutoPointTick = 0;
            }
            const auto& step = autoChoicePlan[static_cast<std::size_t>(rt.priorityAutoPointPhase)];
            if (step.waitBeforeMs > 0 &&
                !Elapsed(GetTickCount(), rt.priorityAutoPointTick,
                         static_cast<DWORD>(step.waitBeforeMs))) {
                return false;
            }
            pointSlot = step.point == internal_ui_click_logic::AutoMenuChoicePoint::AutoMenu
                ? ClickSlot::AutoMenu : requestedSlot;
        }

        const int pointIndex = static_cast<int>(pointSlot);
        std::wstring error;
        if (pointIndex < 0 || pointIndex >= static_cast<int>(a.profile.points.size())) {
            complete(false, GetTickCount());
            LogAccount(a, L"PRIORITY #3 AUTO INPUTSYNC FAIL: slot điểm không hợp lệ");
            return false;
        }

        int normalizedX = -1;
        int normalizedY = -1;
        if (!NormalizeClickPointForBridge(
                a.game, a.profile.points[static_cast<std::size_t>(pointIndex)],
                normalizedX, normalizedY, error)) {
            complete(false, GetTickCount());
            LogAccount(a, L"PRIORITY #3 AUTO INPUTSYNC FAIL tọa độ " +
                          std::wstring(kClickLabels[static_cast<std::size_t>(pointIndex)]) +
                          L": " + error);
            return false;
        }

        Response response{};
        const bool ok = a.bridge.Call(Command::ClickInternalPoint,
                                      normalizedX, normalizedY, 0,
                                      response, error, 2200);
        const DWORD clickedAt = GetTickCount();

        if (autoMenuChoiceSequence && rt.priorityAutoPointPhase == 0 && ok) {
            rt.priorityAutoPointPhase = 1;
            rt.priorityAutoPointTick = clickedAt;
            const std::wstring next = requestedSlot == ClickSlot::Attack
                ? L"ĐÁNH QUÁI" : L"DỪNG AUTO 2";
            rt.status = L"P3 AUTO INPUTSYNC • click 1/2 AUTO xong • chờ mở menu để " + next;
            LogAccount(a, L"PRIORITY #3 AUTO INPUTSYNC PASS click 1/2: AUTO → chờ " + next +
                          L" • " + std::wstring(response.detail));
            return true;
        }

        complete(ok, clickedAt);
        if (ok) {
            const std::wstring phase = requestedSlot == ClickSlot::Attack
                ? L"click 2/2: ĐÁNH QUÁI" :
                requestedSlot == ClickSlot::StopAuto2
                    ? L"click 2/2: DỪNG AUTO 2"
                    : std::wstring(kClickLabels[static_cast<std::size_t>(pointIndex)]);
            LogAccount(a, L"PRIORITY #3 AUTO INPUTSYNC PASS " + phase +
                          L" • TryClickUI→EndUIDrag • không chiếm chuột Windows.");
        } else {
            if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout khi click AUTO InputSync", clickedAt);
            LogAccount(a, L"PRIORITY #3 AUTO INPUTSYNC FAIL tại " +
                          std::wstring(kClickLabels[static_cast<std::size_t>(pointIndex)]) +
                          L": " + error);
        }
        return ok;
    }

    void ResetTravelFightGuard(RuntimeState& rt) {
        rt.travelFightGuardPhase = 0;
        rt.travelFightGuardTick = 0;
        rt.travelFightStopAttempts = 0;
    }

    bool EnsureAutoFightOffForTravel(Account& a, DWORD now, const wchar_t* context) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        const std::wstring where = context ? context : L"di chuyển";

        if ((s.validMask & ValidAutoFight) == 0) {
            rt.status = L"TRAVEL GUARD • chờ AutoFight authoritative trước StartPath tới " + where;
            return false;
        }
        if (travel_fight_guard_logic::CanDispatchMovement(true, s.autoFight != 0)) {
            if (rt.travelFightGuardPhase != 0 || rt.travelFightStopAttempts != 0) {
                LogAccount(a, L"TRAVEL GUARD PASS: AutoFight OFF authoritative → nhả StartPath tới " + where);
            }
            ResetTravelFightGuard(rt);
            return true;
        }

        bool ok = false;
        DWORD clickedAt = 0;
        switch (rt.travelFightGuardPhase) {
            case 0:
                if (QueuePriorityAutoClick(a, ClickSlot::StopAuto2,
                                           PriorityAutoOwner::TravelGuardStop,
                                           L"TRAVEL GUARD: click điểm DỪNG AUTO nội bộ trước " + where)) {
                    rt.travelFightGuardPhase = 1;
                    rt.status = L"TRAVEL GUARD • AutoFight ON → chờ click DỪNG InputSync";
                }
                return false;
            case 1:
                if (!ConsumePriorityAutoResult(a, ClickSlot::StopAuto2,
                                               PriorityAutoOwner::TravelGuardStop,
                                               ok, clickedAt)) return false;
                if (!ok) { rt.travelFightGuardPhase = 0; return false; }
                rt.travelFightGuardPhase = 2;
                rt.travelFightGuardTick = clickedAt;
                ++rt.travelFightStopAttempts;
                rt.status = L"TRAVEL GUARD • đã click DỪNG InputSync lần " +
                            std::to_wstring(rt.travelFightStopAttempts) + L" • verify OFF";
                return false;
            case 2:
                if (!Elapsed(now, rt.travelFightGuardTick, kPriorityAutoVerifyMs)) return false;
                if (travel_fight_guard_logic::NeedsAnotherStopBeforeReset(
                        rt.travelFightStopAttempts)) {
                    rt.travelFightGuardPhase = 0;
                    return false;
                }
                rt.travelFightGuardPhase = 3;
                rt.travelFightStopAttempts = 0;
                LogAccount(a, L"TRAVEL GUARD: click DỪNG 2 lần vẫn ON → chạy AUTO→ĐÁNH QUÁI InputSync reset rồi lặp DỪNG.");
                return false;
            case 3:
                if (QueuePriorityAutoClick(a, ClickSlot::Attack,
                                           PriorityAutoOwner::TravelGuardReset,
                                           L"TRAVEL GUARD RESET: chạy AUTO→ĐÁNH QUÁI InputSync")) {
                    rt.travelFightGuardPhase = 4;
                }
                return false;
            case 4:
                if (!ConsumePriorityAutoResult(a, ClickSlot::Attack,
                                               PriorityAutoOwner::TravelGuardReset,
                                               ok, clickedAt)) return false;
                if (!ok) { rt.travelFightGuardPhase = 3; return false; }
                rt.travelFightGuardPhase = 5;
                rt.travelFightGuardTick = clickedAt;
                return false;
            case 5:
                if (!Elapsed(now, rt.travelFightGuardTick, kPriorityAutoVerifyMs)) return false;
                rt.travelFightGuardPhase = 0;
                rt.travelFightStopAttempts = 0;
                rt.status = L"TRAVEL GUARD • reset AUTO→ĐÁNH QUÁI xong • lặp DỪNG nội bộ";
                return false;
            default:
                ResetTravelFightGuard(rt);
                return false;
        }
    }

    void ResetAutoPathFightConflict(RuntimeState& rt) {
        rt.autoPathFightConflictLatched = false;
        rt.autoPathFightConflictTick = 0;
        rt.autoPathFightConflictStopAttempts = 0;
    }

    bool HandleAutoPathFightInvariant(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        const std::uint32_t need = ValidAutoPath | ValidAutoFight;

        if ((s.validMask & need) != need) {
            if (rt.autoPathFightConflictLatched) {
                rt.status = L"ROUTE/FIGHT INVARIANT • chờ AutoPath + AutoFight authoritative";
                return true;
            }
            return false;
        }
        if ((s.validMask & ValidLifeState) && s.dead) return false;

        const bool conflict = travel_fight_guard_logic::HasAutoPathFightConflict(
            s.autoPathing != 0, s.autoFight != 0);
        if (conflict && !rt.autoPathFightConflictLatched) {
            rt.autoPathFightConflictLatched = true;
            rt.autoPathFightConflictTick = 0;
            rt.autoPathFightConflictStopAttempts = 0;
            ResetTravelFightGuard(rt);
            LogAccount(a, L"ROUTE/FIGHT INVARIANT VIOLATION: phát hiện AutoPath ON + AutoFight ON"
                          L" → StopPath fail-closed, sau đó DỪNG x2/reset cho tới khi cả hai OFF.");
        }

        if (!rt.autoPathFightConflictLatched) return false;

        if (travel_fight_guard_logic::ConflictRecoveryComplete(
                rt.autoPathFightConflictLatched,
                s.autoPathing != 0, s.autoFight != 0)) {
            ResetAutoPathFightConflict(rt);
            ResetTravelFightGuard(rt);
            rt.status = L"ROUTE/FIGHT INVARIANT PASS • AutoPath OFF + AutoFight OFF";
            LogAccount(a, L"ROUTE/FIGHT INVARIANT RECOVERED: cả AutoPath và AutoFight đều OFF"
                          L" → route kế tiếp phải đi lại qua Travel Guard.");
            return true; // one-cycle barrier before any new route decision
        }

        if (s.autoPathing) {
            if (rt.autoPathFightConflictTick == 0 ||
                Elapsed(now, rt.autoPathFightConflictTick,
                        kAutoPathFightConflictRetryMs)) {
                std::wstring attachError;
                Response response{};
                std::wstring error;
                bool ok = EnsureAttach(a, attachError);
                if (!ok) {
                    error = L"không attach được Bridge: " + attachError;
                } else {
                    ok = a.bridge.Call(Command::StopPath, 0, 0, 0,
                                       response, error, 900);
                }
                rt.autoPathFightConflictTick = now;
                ++rt.autoPathFightConflictStopAttempts;
                if (ok) {
                    LogAccount(a, L"ROUTE/FIGHT INVARIANT: đã gửi StopPath nội bộ lần " +
                                  std::to_wstring(rt.autoPathFightConflictStopAttempts) +
                                  L" • chờ AutoPath OFF authoritative.");
                } else {
                    if (BridgeLooksUnresponsive(error)) {
                        EnterClientFreeze(a, L"Bridge timeout khi dập AutoPath/Fight conflict", now);
                    }
                    LogAccount(a, L"ROUTE/FIGHT INVARIANT: StopPath fail-closed lần " +
                                  std::to_wstring(rt.autoPathFightConflictStopAttempts) +
                                  L" • " + error);
                }
            }
            rt.status = L"ROUTE/FIGHT INVARIANT • đang dập AutoPath trước khi tắt AutoFight";
            return true;
        }

        // AutoPath is now OFF. Reuse the proven stop-stop-reset loop and do not
        // release any route until AutoFight is authoritatively OFF as well.
        if (!EnsureAutoFightOffForTravel(a, now, L"khôi phục invariant AutoPath/AutoFight")) {
            rt.status = L"ROUTE/FIGHT INVARIANT • AutoPath OFF • đang DỪNG AutoFight x2/reset";
            return true;
        }
        return true;
    }

    void TestClick(ClickSlot slot) {
        Account* a = SelectedAccount();
        if (!a) { Log(L"TEST: chưa chọn acc"); return; }
        std::wstring attachError;
        if (!EnsureAttach(*a, attachError)) {
            LogAccount(*a, L"TEST nội bộ không attach được Bridge: " + attachError);
            return;
        }
        Command command = Command::None;
        int arg0 = 0;
        int arg1 = 0;
        std::wstring error;
        switch (slot) {
            case ClickSlot::AutoMenu:
            case ClickSlot::Attack:
            case ClickSlot::StopAuto2: {
                const int index = static_cast<int>(slot);
                if (index < 0 || index >= static_cast<int>(a->profile.points.size()) ||
                    !NormalizeClickPointForBridge(
                        a->game, a->profile.points[static_cast<std::size_t>(index)],
                        arg0, arg1, error)) {
                    LogAccount(*a, L"TEST INPUTSYNC " +
                                   std::wstring(kClickLabels[static_cast<std::size_t>(index)]) +
                                   L" FAIL tọa độ: " + error);
                    return;
                }
                command = Command::ClickInternalPoint;
                break;
            }
            default: break;
        }
        if (command == Command::None) return;
        Response response{};
        const bool ok = a->bridge.Call(command, arg0, arg1, 0, response, error, 2200);
        const int index = static_cast<int>(slot);
        LogAccount(*a, L"TEST NỘI BỘ " + std::wstring(kClickLabels[static_cast<std::size_t>(index)]) +
                       (ok ? L" PASS • không chiếm chuột • " + std::wstring(response.detail)
                           : L" FAIL • " + error));
    }

    void StartChecked() {
        if (gatherModeActive_ || partyBuildModeActive_) {
            Log(gatherModeActive_ ? L"TẬP TRUNG đang ON • START auto thường bị chặn; hãy tắt TẬP TRUNG trước." :
                                   L"TỰ TẠO PT đang ON • START auto thường bị chặn; hãy tắt TỰ TẠO PT trước.");
            return;
        }
        if (!ThanLongLicenseActionAllowed()) {
            Log(L"LICENSE CORE GUARD • START bị chặn vì license không còn hợp lệ/fresh");
            MessageBoxW(hwnd_, L"License không còn hợp lệ hoặc đã quá kỳ xác minh 12 giờ. Các tính năng bên trong đang bị khóa.",
                        kTitle, MB_ICONERROR | MB_OK);
            return;
        }
        PersistSelectedEditor();
        const bool hadRunningBeforeStart = AnyRunningAccount();
        int started = 0;
        const int count = ListView_GetItemCount(clientList_);
        for (int i = 0; i < count && i < static_cast<int>(accounts_.size()); ++i) {
            if (!ListView_GetCheckState(clientList_, i)) continue;
            Account& a = *accounts_[static_cast<std::size_t>(i)];
            const bool isMain = a.profile.tradeRole == kMainTradeRole;
            if (!isMain && !a.profile.target.valid) {
                LogAccount(a, L"Không start: CON chưa chọn bãi train.");
                continue;
            }
            std::wstring error;
            if (!EnsureAttach(a, error)) {
                LogAccount(a, L"Không start: " + error);
                continue;
            }
            a.deathSessionLatched = false;
            a.runtime.running = true;
            ResetRuntime(a.runtime);
            a.runtime.running = true;
            if (const int scanSlot = ChildScanSlot(a); scanSlot > 0)
                image_scan_test::ResetAutoFilter(a.game.window, scanSlot);
            if (isMain) {
                a.tradeHeld = true;
                a.runtime.status = L"MAIN đứng chờ tại TỌA GD";
                if (a.snapshotValid && (a.snapshot.validMask & ValidAutoPath) && a.snapshot.autoPathing) {
                    Response response{};
                    std::wstring ignored;
                    (void)a.bridge.Call(Command::StopPath, 0, 0, 0, response, ignored, 700);
                }
                LogAccount(a, L"BẮT ĐẦU MAIN • đứng im tại TỌA GD • không có CON thì chờ; CON tới ưu tiên giao dịch; trade dùng quota x9; quota MAIN=0 thì macro bán và re-plan từ fresh FreeBag.");
            } else {
                a.tradeHeld = false;
                a.runtime.routeOwnershipResetPending = true;
                a.runtime.status = L"Đang giám sát • chuẩn hóa ownership AutoPath";
                LogAccount(a, L"BẮT ĐẦU CON • bãi " + a.profile.target.name + L" • M" +
                               std::to_wstring(a.profile.target.mapID) + L" • " +
                               std::to_wstring(a.profile.target.x) + L"," + std::to_wstring(a.profile.target.y));
            }
            ++started;
            UpdateAccountRow(i, a);
        }
        if (started == 0) {
            Log(L"Không có acc hợp lệ được start. Hãy tick checkbox, gán MAIN/CON và chọn bãi cho CON.");
        } else if (!hadRunningBeforeStart) {
            BeginTelegramSession();
            Log(L"SESSION 10.6 bắt đầu từ nút START.");
        }
    }


    void StopAccount(Account& a) {
        const bool wasFrozen = a.runtime.clientFreezeActive;
        if (const int scanSlot = ChildScanSlot(a); scanSlot > 0)
            image_scan_test::StopAutoFilter(a.game.window, scanSlot);
        a.deathSessionLatched = false;
        a.tradeHeld = false;
        a.runtime.running = false;
        ResetRuntime(a.runtime);
        a.runtime.running = false;
        a.runtime.status = L"Đã dừng";
        if (a.bridge.Attached() && !wasFrozen) {
            Response r{};
            std::wstring ignored;
            (void)a.bridge.Call(Command::StopPath, 0, 0, 0, r, ignored, 700);
        }
        LogAccount(a, L"Đã dừng. Không tự đổi trạng thái ngựa.");
    }

    void StopChecked() {
        if (gatherModeActive_ || partyBuildModeActive_) {
            Log(gatherModeActive_ ? L"TẬP TRUNG đang ON • dùng nút TẬP TRUNG để tắt mode độc quyền." :
                                   L"TỰ TẠO PT đang ON • dùng nút TỰ TẠO PT để tắt mode độc quyền.");
            return;
        }
        const bool hadRunningBeforeStop = AnyRunningAccount();
        int stopped = 0;
        const int count = ListView_GetItemCount(clientList_);
        for (int i = 0; i < count && i < static_cast<int>(accounts_.size()); ++i) {
            if (!ListView_GetCheckState(clientList_, i)) continue;
            StopAccount(*accounts_[static_cast<std::size_t>(i)]);
            UpdateAccountRow(i, *accounts_[static_cast<std::size_t>(i)]);
            ++stopped;
        }
        if (tradeTxn_.phase != TradePhase::Idle) {
            Account* main = AccountByPid(tradeTxn_.mainPid);
            Account* child = AccountByPid(tradeTxn_.childPid);
            if ((main && !main->runtime.running) || (child && !child->runtime.running)) {
                AbortTrade(L"người dùng DỪNG AUTO acc thuộc workflow giao dịch", GetTickCount());
            }
        }
        if (stopped == 0) Log(L"Không có acc nào được tick để dừng.");
        if (hadRunningBeforeStop && !AnyRunningAccount() && telegramStats_.active) {
            (void)SendTelegramSummary(L"DỪNG TOÀN BỘ ACC", true);
            const std::wstring stopMsg = L"⏹ AUTO SESSION STOP\nThời gian: " + LocalDateTimeText();
            AddLocalReport(L"SESSION STOP", L"-", stopMsg);
            if (false) {
                (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage,
                    stopMsg, L"SESSION STOP", L"-");
            }
            telegramStats_.active = false;
        }
    }

    static bool BridgeLooksUnresponsive(const std::wstring& error) {
        return error.find(L"timeout") != std::wstring::npos ||
               error.find(L"Bridge còn bận") != std::wstring::npos ||
               error.find(L"Bridge busy") != std::wstring::npos;
    }

    bool WindowResponsive(const GameClient& game) const {
        if (!game.window || !IsWindow(game.window)) return false;
        DWORD_PTR ignored = 0;
        const LRESULT ok = SendMessageTimeoutW(game.window, WM_NULL, 0, 0,
                                               SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                               kWindowResponsiveProbeMs, &ignored);
        return ok != 0;
    }

    void EnterClientFreeze(Account& a, const wchar_t* reason, DWORD now) {
        RuntimeState& rt = a.runtime;
        const bool first = !rt.clientFreezeActive;
        rt.clientFreezeActive = true;
        if (rt.clientFreezeSinceTick == 0) rt.clientFreezeSinceTick = now;
        rt.clientStableSinceTick = 0;
        rt.candidateCount = 0;
        rt.qualifiedMap = 0;
        rt.stallSinceTick = 0;
        rt.fightPhase = 0;
        if (first) {
            LogAccount(a, L"FREEZE ACTION: " + std::wstring(reason ? reason : L"client/map chưa ổn định"));
            TelegramRecordCriticalFreeze(a, reason);
        }
    }

    void MarkReadStateFailure(Account& a, const std::wstring& error, DWORD now) {
        RuntimeState& rt = a.runtime;
        EnterClientFreeze(a, L"ReadState/Bridge không phản hồi", now);
        TelegramRecordCriticalFreeze(a, L"ReadState/Bridge không phản hồi");
        ++rt.readStateFailStreak;
        rt.clientStableSinceTick = 0;
        rt.status = L"CLIENT KHÔNG PHẢN HỒI • FREEZE ACTION";
        if (rt.lastReadFailureLogTick == 0 || now - rt.lastReadFailureLogTick >= kReadFailLogIntervalMs) {
            LogAccount(a, L"ReadState fail x" + std::to_wstring(rt.readStateFailStreak) + L": " + error +
                          L" • FREEZE, không gửi action mới");
            rt.lastReadFailureLogTick = now;
        }
    }

    bool HoldUntilClientStable(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;

        if (!s.mapReady || s.waitingChangeMap) {
            EnterClientFreeze(a, L"game đang chuyển map", now);
            rt.clientStableSinceTick = 0;
            rt.status = L"ĐANG CHUYỂN MAP • FREEZE ACTION";
            return true;
        }

        if (!rt.clientFreezeActive) {
            rt.readStateFailStreak = 0;
            rt.lastReadFailureLogTick = 0;
            return false;
        }

        if (!WindowResponsive(a.game)) {
            TelegramRecordCriticalFreeze(a, L"Cửa sổ game không phản hồi");
            rt.clientStableSinceTick = 0;
            rt.status = L"CỬA SỔ GAME CHƯA PHẢN HỒI • FREEZE ACTION";
            return true;
        }

        if (rt.clientStableSinceTick == 0) {
            rt.clientStableSinceTick = now;
            rt.status = L"MAP/CLIENT ĐÃ PHẢN HỒI • chờ ổn định 2.0s";
            return true;
        }
        if (!Elapsed(now, rt.clientStableSinceTick, kClientStableResumeMs)) {
            const DWORD elapsed = now - rt.clientStableSinceTick;
            const DWORD remainMs = elapsed >= kClientStableResumeMs ? 0 : kClientStableResumeMs - elapsed;
            rt.status = L"CLIENT ĐANG ỔN ĐỊNH • chờ " + std::to_wstring((remainMs + 99) / 100) + L"00ms";
            return true;
        }

        rt.clientFreezeActive = false;
        rt.clientFreezeSinceTick = 0;
        rt.clientStableSinceTick = 0;
        TelegramRecordFreezeRecovered(a);
        rt.readStateFailStreak = 0;
        rt.lastReadFailureLogTick = 0;
        rt.lastActionTick = 0;
        rt.lastAction = Action::Wait;
        LogAccount(a, L"CLIENT ỔN ĐỊNH LIÊN TỤC 2s → mở khóa action, tiếp tục auto.");
        rt.status = L"Client ổn định 2s • tiếp tục auto";
        return false;
    }

    bool CooldownReady(RuntimeState& rt, Action a, DWORD now) {
        DWORD delay = 1500;
        if (a == Action::Mount || a == Action::Dismount) delay = 4000;
        if (a == Action::StartPath) delay = 5000;
        if (a != rt.lastAction) {
            rt.lastAction = a;
            rt.lastActionTick = 0;
        }
        return rt.lastActionTick == 0 || now - rt.lastActionTick >= delay;
    }

    bool SendDecision(Account& a, Action action, const TargetProfile& t, const wchar_t* context, int diagnosticTolerance = 0) {
        RuntimeState& rt = a.runtime;
        if (rt.clientFreezeActive) {
            rt.status = L"FREEZE ACTION • bỏ qua route/mount command";
            return false;
        }
        const DWORD now = GetTickCount();
        // Single authoritative movement gate: neither Mount nor StartPath may be
        // emitted while AutoFight is ON/unreadable. StartPath additionally waits for
        // the hard AutoPath+Fight conflict recovery to observe both states OFF.
        if (action == Action::Mount || action == Action::StartPath) {
            if (action == Action::StartPath && rt.autoPathFightConflictLatched) {
                rt.status = L"ROUTE/FIGHT INVARIANT • cấm StartPath khi recovery chưa hoàn tất";
                return false;
            }
            if (action == Action::StartPath &&
                (!a.snapshotValid || (a.snapshot.validMask & ValidRiding) == 0 ||
                 !CanStartPath(a.snapshot.riding != 0))) {
                rt.status = L"MOUNT REQUIRED • chặn StartPath vì chưa xác nhận đang trên ngựa";
                LogAccount(a, L"MOUNT REQUIRED: StartPath bị chặn fail-closed • phải thấy IsRiding=1 trước mọi AutoPath.");
                return false;
            }
            const wchar_t* movementContext = context ? context :
                (action == Action::Mount ? L"lên ngựa" : L"AutoPath");
            if (!EnsureAutoFightOffForTravel(a, now, movementContext)) return false;
        }
        if (!CooldownReady(rt, action, now)) return false;
        Response r{};
        std::wstring error;
        bool ok = false;
        const std::wstring where = context ? context : L"đích";
        switch (action) {
            case Action::Mount:
                ok = a.bridge.Call(Command::ToggleRide, 1, 0, 0, r, error, 1000);
                if (ok) rt.status = L"Đang lên ngựa • " + where;
                break;
            case Action::Dismount:
                ok = a.bridge.Call(Command::ToggleRide, 0, 0, 0, r, error, 1000);
                if (ok) rt.status = L"Tới " + where + L" • xuống ngựa";
                break;
            case Action::StartPath:
                ok = a.bridge.Call(Command::StartPath, t.mapID, t.x, t.y, r, error, 1300);
                if (ok) rt.status = L"Đang AutoPath tới " + where;
                break;
            case Action::StopPath:
                ok = a.bridge.Call(Command::StopPath, 0, 0, 0, r, error, 900);
                if (ok) rt.status = L"Tới " + where + L" • StopPath";
                break;
            default:
                return false;
        }
        if (ok) {
            rt.lastActionTick = now;
            if (action == Action::StartPath) rt.lastStartPathPassTick = now;
        } else if (action == Action::StartPath) {
            // v1.6: a transient Bridge rejection must not create a fake 5-second AutoPath success window.
            rt.lastActionTick = now > 3500 ? now - 3500 : 0;
        } else {
            rt.lastActionTick = now;
        }
        if (ok && action == Action::StartPath) {
            const long long dx = static_cast<long long>(a.snapshot.x) - t.x;
            const long long dy = static_cast<long long>(a.snapshot.y) - t.y;
            const long long d2 = dx * dx + dy * dy;
            const long long distance = static_cast<long long>(std::llround(std::sqrt(static_cast<long double>(d2))));
            const std::wstring tolText = diagnosticTolerance < 0
                ? L"PORTAL_SPECIAL"
                : std::to_wstring(diagnosticTolerance > 0 ? diagnosticTolerance : a.profile.tolerance);
            LogAccount(a, L"COORD STARTPATH PASS • CURRENT M" + std::to_wstring(a.snapshot.mapID) +
                          L"/" + std::to_wstring(a.snapshot.x) + L"," + std::to_wstring(a.snapshot.y) +
                          L" • TARGET M" + std::to_wstring(t.mapID) + L"/" + std::to_wstring(t.x) + L"," + std::to_wstring(t.y) +
                          L" • DX=" + std::to_wstring(dx) + L" DY=" + std::to_wstring(dy) +
                          L" DISTANCE=" + std::to_wstring(distance) + L" TOL=" + tolText +
                          L" • BRIDGE=" + std::wstring(r.detail));
        }
        if (!ok && BridgeLooksUnresponsive(error)) {
            EnterClientFreeze(a, L"Bridge action timeout/busy", now);
        }
        if (ok && action == Action::StartPath && t.mapID != a.snapshot.mapID) {
            // Arm cross-map confirmation from the command itself. Movement/autoPath
            // evidence is still required before any Confirm click is allowed.
            if (!rt.crossMapRouteArmed) rt.crossMapRouteMoved = false;
            rt.crossMapRouteArmed = true;
        }
        if (!ok) {
            rt.status = L"ROUTE ACTION FAIL • " + where + L" • " + error;
            LogAccount(a, L"Route action fail-closed: " + error);
            if(!BridgeLooksUnresponsive(error))(void)StartAccountUiRecovery(a,AccountUiRecoveryPlan::GenericFailClosed,L"Route action fail-closed • "+error,now);
        }
        return ok;
    }

    bool CompleteToolOwnedRoute(RuntimeState& rt, bool atTarget, bool autoPathing, bool riding) {
        // Intermediate map changes MUST keep ownership armed so the Lâu Lan P1 watchdog
        // can still prove that this is the tool-owned cross-map route. Release ownership
        // only at the physical final destination: in tolerance and AutoPath OFF.
        // Riding state is intentionally irrelevant after arrival (CP10).
        if (!travel_fight_guard_logic::IsPhysicalRouteCompletion(atTarget, autoPathing, riding)) return false;
        rt.crossMapRouteArmed = false;
        rt.crossMapRouteMoved = false;
        rt.crossMapSeenAutoPath = false;
        rt.stallSinceTick = 0;
        rt.confirmAttempts = 0;
        rt.lastLauLanConfirmTick = 0;
        return true;
    }

    void ObserveMovement(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (rt.lastObservedMap != s.mapID) {
            const bool keepToolOwnedCrossMapRoute = rt.crossMapRouteArmed;
            rt.lastObservedMap = s.mapID;
            rt.lastObservedX = s.x;
            rt.lastObservedY = s.y;
            rt.lastMovementTick = now;
            // v0.5: a World Flow / robust travel route is still the SAME tool-owned route
            // after crossing an intermediate map. Do not lose the Lâu Lan gate watchdog
            // merely because MapID changed. Crossing a map is itself proof the route moved.
            if (keepToolOwnedCrossMapRoute) {
                rt.crossMapRouteMoved = true;
                if (s.autoPathing) rt.crossMapSeenAutoPath = true;
            } else {
                rt.crossMapSeenAutoPath = false;
                rt.crossMapRouteMoved = false;
            }
            rt.stallSinceTick = 0;
            rt.confirmAttempts = 0;
            rt.lastLauLanConfirmTick = 0;
            rt.fightPhase = 0;
            rt.fightAttempts = 0;
            rt.wasAtTarget = false;
            return;
        }
        if (rt.crossMapRouteArmed && s.autoPathing) rt.crossMapSeenAutoPath = true;
        const long long dx = static_cast<long long>(s.x) - rt.lastObservedX;
        const long long dy = static_cast<long long>(s.y) - rt.lastObservedY;
        if (dx * dx + dy * dy >= 25) {
            if (rt.crossMapRouteArmed) rt.crossMapRouteMoved = true;
            rt.lastMovementTick = now;
            rt.lastObservedX = s.x;
            rt.lastObservedY = s.y;
            rt.stallSinceTick = 0;
        }
    }

    void ResetRuntimeForLifeBoundary(Account& a) {
        // World Flow/FIFO ownership lives partly outside RuntimeState (tradeHeld + queue).
        // Preserve only the immutable FIFO ticket across death/alive hard resets; all travel
        // phases restart cleanly so the same held account can AutoPath to TỌA GD again.
        const std::uint64_t workflowTicket = a.runtime.tradeWorkflowEntrySeq;
        const bool preserveWorkflowTicket = a.tradeHeld && workflowTicket != 0;
        ResetRuntime(a.runtime);
        if (preserveWorkflowTicket) a.runtime.tradeWorkflowEntrySeq = workflowTicket;
    }

    bool HandleDeath(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;

        // Life state is authoritative for the death-session boundary. If it becomes
        // temporarily unavailable while a death session is latched, fail closed and
        // preserve the latch/timers instead of silently returning to normal automation.
        if ((s.validMask & ValidLifeState) == 0) {
            if (a.deathSessionLatched) {
                rt.status = L"DEATH SESSION • chờ life-state authoritative";
                return true;
            }
            return false;
        }

        if (!s.dead) {
            if (!a.deathSessionLatched) return false;
            // SECOND boundary reset: the character is alive again on a stable client
            // snapshot. Wipe every revive/travel/fight/sell/confirm/watchdog phase and
            // resume exactly like a fresh BẮT ĐẦU, while AccountProfile/settings and
            // the existing Bridge attachment remain intact.
            ResetRuntimeForLifeBoundary(a);
            a.deathSessionLatched = false;
            rt.routeOwnershipResetPending = true;
            rt.status = L"ALIVE • cold restart + chuẩn hóa ownership AutoPath";
            LogAccount(a, L"POST-REVIVE COLD START: ResetRuntime toàn bộ • giữ nguyên setting/bãi/click • phiên auto mới.");
            return true;
        }

        if (!a.deathSessionLatched) {
            // FIRST boundary reset: a new authoritative death is a hard session
            // boundary. Never carry ANY runtime state from the previous life. The
            // lifecycle latch is outside RuntimeState so this full reset cannot cause
            // a repeated-reset loop while the same dead snapshot remains true.
            ResetRuntimeForLifeBoundary(a);
            a.deathSessionLatched = true;
            rt.deadSinceTick = now;
            rt.status = L"DEAD • hard reset runtime đời trước";
            LogAccount(a, L"NEW DEATH SESSION: HARD ResetRuntime toàn bộ • coi như AUTO vừa được bật lại từ đầu.");
        }

        rt.status = L"Nhân vật đang chết";
        if (!a.profile.enableRevive) {
            rt.status = L"CHẾT • chờ Đầu thai thủ công";
            return true;
        }
        if (rt.revivePhase == 0 && Elapsed(now, rt.deadSinceTick, 500) &&
            (rt.lastReviveClickTick == 0 || Elapsed(now, rt.lastReviveClickTick, 5000))) {
            // The Revive callback is emitted by the per-account P2 priority pass. Keep this
            // path fail-closed for the same client without blocking unrelated windows.
            rt.status = L"ĐẦU THAI đến hạn • chờ P2 cục bộ của chính acc";
            return true;
        }
        if (rt.revivePhase == 1 && Elapsed(now, rt.revivePhaseTick, 900)) {
            // Map Confirm is not injected as a special revive action; global P1 Lâu Lan
            // watchdog owns its own internal MessageBox callback.
            rt.revivePhase = 2;
            rt.revivePhaseTick = now;
            rt.status = L"Đầu thai đã gửi • chờ sống lại; World Flow vẫn HOLD và sẽ resume";
            return true;
        }
        if (rt.revivePhase == 2 && Elapsed(now, rt.revivePhaseTick, 4500)) {
            rt.revivePhase = 0;
            rt.revivePhaseTick = now;
        }
        return true;
    }

    bool HandleRouteOwnershipReset(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!rt.routeOwnershipResetPending) return false;

        // This is the missing game-side half of a true cold start. ResetRuntime()
        // clears controller ownership flags, but the client may preserve AutoPath=ON
        // across death/revive. If we accepted that stale path as our route, then
        // crossMapRouteArmed would stay false forever and Confirm would fail closed.
        if ((s.validMask & ValidAutoPath) == 0) {
            rt.status = L"SESSION ROUTE RESET • chờ AutoPath authoritative";
            return true;
        }

        if (!s.autoPathing) {
            rt.routeOwnershipResetPending = false;
            rt.routeOwnershipStopTick = 0;
            rt.routeOwnershipStopAttempts = 0;
            rt.crossMapRouteArmed = false;
            rt.crossMapRouteMoved = false;
            rt.crossMapSeenAutoPath = false;
            rt.confirmAttempts = 0;
            rt.status = L"SESSION ROUTE RESET • AutoPath OFF • ownership sạch";
            if (!rt.routeOwnershipResetLogged) {
                LogAccount(a, L"SESSION ROUTE RESET PASS: AutoPath OFF → route kế tiếp phải do tool StartPath mới để arm Confirm.");
                rt.routeOwnershipResetLogged = true;
            }
            return true; // one-cycle barrier before normal route logic
        }

        rt.routeOwnershipResetLogged = false;
        if (rt.routeOwnershipStopAttempts >= kRouteOwnershipStopMaxAttempts &&
            rt.routeOwnershipStopTick != 0 &&
            Elapsed(now, rt.routeOwnershipStopTick, kRouteOwnershipStopRetryMs)) {
            rt.status = L"SESSION ROUTE RESET • AutoPath cũ vẫn ON sau 3 StopPath • fail-closed";
            return true;
        }

        if (rt.routeOwnershipStopTick == 0 || Elapsed(now, rt.routeOwnershipStopTick, kRouteOwnershipStopRetryMs)) {
            if (SendDecision(a, Action::StopPath, a.profile.target, L"session route ownership reset")) {
                ++rt.routeOwnershipStopAttempts;
                rt.routeOwnershipStopTick = now;
                rt.status = L"SESSION ROUTE RESET • phát hiện AutoPath cũ ON → StopPath, chờ verify OFF";
                LogAccount(a, L"SESSION ROUTE RESET: AutoPath=ON nhưng controller vừa cold-reset → StopPath để xóa path đời trước trước khi route mới.");
            } else {
                rt.status = L"SESSION ROUTE RESET • chờ gửi StopPath fail-closed";
            }
        } else {
            rt.status = L"SESSION ROUTE RESET • đã StopPath → chờ snapshot AutoPath OFF";
        }
        return true;
    }

    bool CurrentTravelDestinationMap(const Account& a, int& destinationMap) const {
        const RuntimeState& rt = a.runtime;

        if (rt.trainRecoveryPhase != 0) {
            destinationMap = a.profile.target.mapID;
            return destinationMap > 0;
        }

        // Normal training route uses the current profile target. Trade-held
        // accounts are advanced outside TickAccount and are already protected directly
        // by the shared Mount/StartPath Travel Guard.
        if (!a.tradeHeld) {
            destinationMap = a.profile.target.mapID;
            return destinationMap > 0;
        }
        return false;
    }

    bool HandleUnderworldAutoFightGuard(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        int destinationMap = 0;
        const bool hasTravelDestination = CurrentTravelDestinationMap(a, destinationMap);
        if (!travel_fight_guard_logic::ShouldGuardUnderworldExit(
                s.mapID, destinationMap, hasTravelDestination, kUnderworldMapId)) {
            rt.underworldGuardLogged = false;
            return false;
        }
        if ((s.validMask & ValidAutoFight) == 0) {
            rt.status = L"ĐỊA PHỦ M87 • chờ AutoFight authoritative • Travel Guard fail-closed";
            return true;
        }
        if (!s.autoFight) {
            if (!rt.underworldGuardLogged) {
                LogAccount(a, L"ĐỊA PHỦ M87: dùng Travel Guard chung • AutoFight OFF → route được phép tiếp tục.");
                rt.underworldGuardLogged = true;
            }
            ResetTravelFightGuard(rt);
            return false;
        }
        rt.underworldGuardLogged = false;
        if (!EnsureAutoFightOffForTravel(a, now, L"rời Địa Phủ M87")) {
            rt.status = L"ĐỊA PHỦ M87 • Travel Guard đang tắt AutoFight • CẤM route khi còn ON";
            return true;
        }
        return false;
    }

    bool HandleFightClicks(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!a.profile.enableFight) {
            rt.fightPhase = 0;
            rt.fightAttempts = 0;
            rt.fightRetryWaitTick = 0;
            return false;
        }
        if ((s.validMask & ValidAutoFight) == 0) {
            rt.status = L"Đúng bãi • chờ đọc trạng thái AutoFight";
            return true;
        }
        if (s.autoFight) {
            rt.fightPhase = 3;
            rt.fightAttempts = 0;
            rt.fightRetryWaitTick = 0;
            if (!rt.trainPositionMonitorArmed) {
                rt.trainPositionMonitorArmed = true;
                rt.lastTrainPositionCheckTick = now;
                // CP9: train-ready after revive/recovery must preserve the per-CON scan cursor.
                LogAccount(a, L"AutoFight ON • train ổn định • giữ cursor Scan VK nếu chưa từng vào WorldFlow GD.");
            }
            rt.lastAutoFightCheckTick = now;
            rt.status = L"Đúng bãi • AutoFight ON • periodic 60s OFF";
            return true;
        }
        if (rt.fightAttempts >= auto_fight_retry_logic::kImmediateAttemptLimit) {
            const auto retryDecision = auto_fight_retry_logic::DecideExhaustedRetry(
                now, rt.fightRetryWaitTick, kAutoFightRecheckMs);
            if (retryDecision == auto_fight_retry_logic::ExhaustedRetryDecision::StartWait) {
                rt.fightRetryWaitTick = now;
                rt.fightPhase = 3;
                rt.status = L"P3 AUTO→Đánh quái thử 2 lần • bắt đầu chờ retry 60s";
                LogAccount(a, L"P3 AUTO RETRY: 2 lần chưa bật được AutoFight • neo timer 60s một lần, không reset mỗi tick.");
                return true;
            }
            if (retryDecision == auto_fight_retry_logic::ExhaustedRetryDecision::KeepWaiting) {
                const DWORD elapsedMs = now - rt.fightRetryWaitTick;
                const DWORD remainSec = elapsedMs >= kAutoFightRecheckMs
                    ? 0 : (kAutoFightRecheckMs - elapsedMs + 999) / 1000;
                rt.status = L"P3 AUTO→Đánh quái thử 2 lần • retry sau " +
                            std::to_wstring(remainSec) + L"s";
                return true;
            }
            rt.fightAttempts = 0;
            rt.fightPhase = 0;
            rt.fightRetryWaitTick = 0;
            LogAccount(a, L"P3 AUTO RETRY 60s: AutoFight vẫn OFF → cấp lại 2 lần AUTO→Đánh quái.");
        }
        if (rt.fightPhase == 3) rt.fightPhase = 0;

        bool ok = false;
        DWORD clickedAt = 0;
        if (rt.fightPhase == 0) {
            if (ConsumePriorityAutoResult(a, ClickSlot::Attack,
                                          PriorityAutoOwner::Train,
                                          ok, clickedAt)) {
                ++rt.fightAttempts;
                if (ok) {
                    rt.fightPhase = 2;
                    rt.fightPhaseTick = clickedAt;
                    rt.status = L"P3 AUTO INPUTSYNC • đủ 2 click • verify AutoFight";
                } else {
                    rt.status = L"P3 AUTO INPUTSYNC • sequence fail lần " +
                                std::to_wstring(rt.fightAttempts) + L"/2";
                }
                return true;
            }
            (void)QueuePriorityAutoClick(a, ClickSlot::Attack,
                                         PriorityAutoOwner::Train,
                                         L"TRAIN: InputSync AUTO→ĐÁNH QUÁI");
            rt.status = L"P3 AUTO INPUTSYNC • chờ Priority #3 chạy click 1→2";
            return true;
        }
        if (rt.fightPhase == 2 && Elapsed(now, rt.fightPhaseTick, 1500)) {
            if (s.autoFight) {
                rt.fightPhase = 3;
                rt.fightAttempts = 0;
                rt.fightRetryWaitTick = 0;
                rt.lastAutoFightCheckTick = now;
                if (!rt.trainPositionMonitorArmed) {
                    rt.trainPositionMonitorArmed = true;
                    rt.lastTrainPositionCheckTick = now;
                    // CP9: do not reset scan cursor here; START and WorldFlow are the reset boundaries.
                }
                rt.status = L"AutoFight ON • P3 InputSync bật thành công • periodic 60s OFF";
                LogAccount(a, L"PRIORITY #3 AUTO→ĐÁNH QUÁI InputSync verify ON • sau đó không check lại 60s.");
                return true;
            }
            if (rt.fightAttempts < 2) {
                rt.fightPhase = 0;
                rt.fightPhaseTick = now;
                return true;
            }
        }
        return true;
    }

    bool LauLanGateConfirmDue(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!a.profile.enableConfirm) return false;
        if (!a.snapshotValid || rt.clientFreezeActive || globalPaused_ || RecorderBlocksAccount(a)) return false;
        if ((s.validMask & (ValidMap | ValidPosition | ValidAutoPath | ValidLifeState)) !=
            (ValidMap | ValidPosition | ValidAutoPath | ValidLifeState)) return false;
        if (!s.mapReady || s.waitingChangeMap || s.dead) return false;

        // Lâu Lan watchdog logic, enabled on the same four additional maps.
        if (s.mapID != kLauLanMapId &&
            s.mapID != kNhanNamMapId &&
            s.mapID != kTruongBachSonMapId &&
            s.mapID != kLieuTayMapId &&
            s.mapID != kThuyKinhHoMapId) {
            rt.stallSinceTick = 0;
            rt.confirmAttempts = 0;
            rt.lastLauLanConfirmTick = 0;
            return false;
        }

        // Only a tool-owned cross-map route that actually moved can arm the gate watchdog.
        // Standing still in Lâu Lan without a live route can never cause a blind XN click.
        if (!rt.crossMapRouteArmed || !rt.crossMapRouteMoved || !rt.crossMapSeenAutoPath) return false;
        // The gate condition is specifically: AutoPath is still ON but position has stalled.
        // World Flow / SELL / GD UI sub-state must not suppress this P1 observer.
        if (!s.autoPathing) return false;
        if (rt.lastMovementTick == 0 || !Elapsed(now, rt.lastMovementTick, kLauLanGateStallMs)) {
            rt.stallSinceTick = 0;
            return false;
        }
        if (rt.stallSinceTick == 0) rt.stallSinceTick = rt.lastMovementTick;
        if (rt.lastLauLanConfirmTick != 0 && !Elapsed(now, rt.lastLauLanConfirmTick, kLauLanConfirmRetryMs)) return false;
        return true;
    }

    bool PriorityLauLanGateConfirmClick(Account& a, DWORD now) {
        if (!LauLanGateConfirmDue(a, now)) return false;
        std::wstring error;
        Response response{};
        const bool ok = a.bridge.Call(Command::ConfirmMap, 0, 0, 0, response, error, 2200);
        const DWORD clickedAt = GetTickCount();

        a.runtime.lastLauLanConfirmTick = clickedAt;
        if (ok) {
            ++a.runtime.confirmAttempts;
            a.runtime.lastMovementTick = clickedAt; // require a fresh full 3s stall before retry
            a.runtime.stallSinceTick = clickedAt;
            a.runtime.status = L"LÂU LAN M5 • AutoPath đứng ~3s → CALLBACK XN NỘI BỘ • lần " +
                               std::to_wstring(a.runtime.confirmAttempts);
            LogAccount(a, L"LÂU LAN GATE WATCHDOG P1: tìm MessageBox + gọi callback nút đồng ý nội bộ • KHÔNG foreground/chuột • lần " +
                          std::to_wstring(a.runtime.confirmAttempts));
            TelegramRecordLauLanConfirm(a, a.runtime.confirmAttempts);
            return true;
        }
        if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout khi XN map nội bộ", clickedAt);
        LogAccount(a, L"LÂU LAN GATE XN NỘI BỘ FAIL: " + error);
        return false;
    }

    bool PrimeDeathSessionForPriorityRevive(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!a.runtime.running || !a.snapshotValid || !IsWindow(a.game.window) || rt.clientFreezeActive) return false;
        if (!s.mapReady || s.waitingChangeMap) return false;
        if ((s.validMask & ValidLifeState) == 0 || !s.dead) return false;
        if (a.deathSessionLatched) return true;

        // Keep the existing FIRST-death boundary in the priority pre-pass so a click
        // sequence in another window cannot postpone detecting this account's death.
        // FILTER V4 yields immediately; revive P2 owns priority and CloseBag runs only AFTER revive PASS.
        if (const int scanSlot = ChildScanSlot(a); scanSlot > 0)
            image_scan_test::NotifyDeath(a.game.window, scanSlot);
        // HandleDeath() sees deathSessionLatched and therefore does not reset twice.
        ResetRuntimeForLifeBoundary(a);
        a.deathSessionLatched = true;
        rt.deadSinceTick = now;
        rt.status = L"DEAD • hard reset runtime đời trước • chờ ĐẦU THAI P2";
        LogAccount(a, L"NEW DEATH SESSION: HARD ResetRuntime toàn bộ • P2 ĐẦU THAI đã nhận death trước auto click thường.");
        return true;
    }

    bool PriorityReviveDue(Account& a, DWORD now) {
        if (!PrimeDeathSessionForPriorityRevive(a, now)) return false;
        const RuntimeState& rt = a.runtime;
        if (!a.profile.enableRevive) return false;
        if (rt.revivePhase != 0 || rt.deadSinceTick == 0) return false;
        if (!Elapsed(now, rt.deadSinceTick, 500)) return false;
        return rt.lastReviveClickTick == 0 || Elapsed(now, rt.lastReviveClickTick, 5000);
    }

    bool PriorityReviveClick(Account& a, DWORD now) {
        if (!PriorityReviveDue(a, now)) return false;
        std::wstring error;
        Response response{};
        const bool ok = a.bridge.Call(Command::Revive, 0, 0, 0, response, error, 2200);
        const DWORD clickedAt = GetTickCount();

        if (ok) {
            a.runtime.lastReviveClickTick = clickedAt;
            a.runtime.revivePhase = 1;
            a.runtime.revivePhaseTick = clickedAt;
            a.runtime.status = L"ĐẦU THAI NỘI BỘ PASS • callback đúng acc chết • không chiếm chuột";
            LogAccount(a, L"ĐẦU THAI P2 PASS: Bridge xác minh IsDeath rồi gọi UIButton.HandleClickEvent nội bộ; chuỗi acc khác không mất index/repeat.");
            if (const int scanSlot = ChildScanSlot(a); scanSlot > 0)
                image_scan_test::NotifyReviveClicked(MakeImageScanTarget(a), scanSlot);
            (void)StartAccountUiRecovery(a,AccountUiRecoveryPlan::PostRevive,L"ĐẦU THAI PASS",clickedAt);
            return true;
        }
        if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout khi Đầu thai nội bộ", clickedAt);
        LogAccount(a, L"ĐẦU THAI NỘI BỘ FAIL: " + error);
        return false;
    }

    bool AutoFightCheckBusy(const Account& a, DWORD now) const {
        const RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        // Hard exclusion gate: an AutoFight check/click sequence may run only when the
        // account is completely idle at the train spot. Do not interleave with any
        // route, mount, death, sell, recovery or another click operation.
        if (rt.sellPhase != 0 || rt.trainRecoveryPhase != 0 || rt.revivePhase != 0) return true;
        if (a.tradeHeld || rt.tradeTravelPhase != 0 || rt.tradeTravelReady) return true;
        if (rt.travelFightGuardPhase != 0 || rt.travelFightStopAttempts != 0) return true;
        if (rt.autoPathFightConflictLatched) return true;
        if (rt.travelMountAttempts != 0 || rt.travelFightBoostPhase != 0 ||
            rt.travelFootFallback) return true;
        if (rt.crossMapRouteArmed || rt.crossMapRouteMoved) return true;
        // CP10: riding is not a busy condition after AutoPath has completed.
        // Keep blocking only live movement/map-transition states.
        if (s.autoPathing || s.waitingChangeMap || !s.mapReady) return true;
        if ((s.validMask & ValidLifeState) && s.dead) return true;
        return false;
    }

    void ResetRobustTravel(RuntimeState& rt) {
        rt.travelMountAttempts = 0;
        rt.travelMountTick = 0;
        rt.travelMountCycle = 0;
        rt.travelFightBoostPhase = 0;
        rt.travelFightBoostTick = 0;
        rt.travelFootFallback = false;
        rt.travelFootTick = 0;
    }

    bool HandleMountFightBoost(Account& a, DWORD now, const wchar_t* context) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        const std::wstring where = context ? context : L"đích";
        if ((s.validMask & ValidAutoFight) == 0) {
            rt.status = L"MOUNT RECOVERY • chờ AutoFight authoritative trước boost 10s";
            return true;
        }

        bool ok = false;
        DWORD clickedAt = 0;
        if (rt.travelFightBoostPhase == 0) {
            if (s.autoFight) {
                rt.travelFightBoostPhase = 5;
                rt.travelFightBoostTick = now;
                rt.status = L"MOUNT RECOVERY • AutoFight đã ON → đánh thêm 10s";
                LogAccount(a, L"MOUNT RECOVERY: 2 lần lên ngựa fail; AutoFight đang ON → tính 10s đánh quái.");
                return true;
            }
            if (QueuePriorityAutoClick(a, ClickSlot::Attack,
                                       PriorityAutoOwner::MountRecovery,
                                       L"MOUNT RECOVERY: chạy AUTO→ĐÁNH QUÁI InputSync trước boost 10s")) {
                rt.travelFightBoostPhase = 1;
            }
            return true;
        }
        if (rt.travelFightBoostPhase == 1) {
            if (!ConsumePriorityAutoResult(a, ClickSlot::Attack,
                                           PriorityAutoOwner::MountRecovery,
                                           ok, clickedAt)) return true;
            if (!ok) { rt.travelFightBoostPhase = 0; return true; }
            rt.travelFightBoostPhase = 4;
            rt.travelFightBoostTick = clickedAt;
            rt.status = L"MOUNT RECOVERY • đã chạy 2 click AUTO nội bộ • verify ON";
            return true;
        }
        if (rt.travelFightBoostPhase == 4) {
            if (s.autoFight) {
                rt.travelFightBoostPhase = 5;
                rt.travelFightBoostTick = now;
                rt.status = L"MOUNT RECOVERY • AutoFight ON → đánh thêm 10s";
                LogAccount(a, L"MOUNT RECOVERY: AUTO→ĐÁNH QUÁI InputSync verify ON → đánh thêm 10s trước lần lên ngựa kế.");
                return true;
            }
            if (Elapsed(now, rt.travelFightBoostTick, 1500)) {
                rt.travelFightBoostPhase = 0;
                rt.status = L"MOUNT RECOVERY • chưa bật được Đánh quái → P3 retry";
            }
            return true;
        }
        if (rt.travelFightBoostPhase == 5) {
            if (!Elapsed(now, rt.travelFightBoostTick, kMountFightBoostMs)) {
                const DWORD sec = (now - rt.travelFightBoostTick) / 1000;
                rt.status = L"MOUNT RECOVERY • đánh quái " + std::to_wstring(sec) + L"/10s • " + where;
                return true;
            }
            // After the 10-second fight boost, use exactly the same fail-closed Travel Guard
            // to stop AutoFight before the second mount x2 cycle begins.
            if (!EnsureAutoFightOffForTravel(a, now, L"sau boost 10s trước lên ngựa lại")) {
                rt.status = L"MOUNT RECOVERY • đủ 10s → P3 DỪNG AUTO • chờ OFF";
                return true;
            }
            rt.travelFightBoostPhase = 0;
            rt.travelFightBoostTick = 0;
            rt.travelMountCycle = 1;
            rt.travelMountAttempts = 0;
            rt.travelMountTick = 0;
            rt.status = L"MOUNT RECOVERY • AutoFight OFF → lặp lại lên ngựa x2";
            LogAccount(a, L"MOUNT RECOVERY: đánh 10s xong + AutoFight OFF → bắt đầu chu kỳ lên ngựa x2 lần thứ hai.");
            return true;
        }
        rt.travelFightBoostPhase = 0;
        return true;
    }

    bool HandleRobustTravelDirect(Account& a, DWORD now, const TargetProfile& targetProfile,
                                  const wchar_t* context, bool& arrived, int toleranceOverride = 0) {
        arrived = false;
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        State logic{};
        logic.valid = true; logic.mapReady = true; logic.waitingMap = false;
        logic.mapID = s.mapID; logic.x = s.x; logic.y = s.y;
        logic.riding = s.riding != 0; logic.autoPathing = s.autoPathing != 0;
        const int travelTolerance = toleranceOverride > 0 ? toleranceOverride : a.profile.tolerance;
        Target target{targetProfile.mapID, targetProfile.x, targetProfile.y, travelTolerance};
        const std::wstring where = context ? context : L"đích";

        if (AtTarget(logic, target)) {
            if (s.autoPathing) {
                (void)SendDecision(a, Action::StopPath, targetProfile, context);
                return true;
            }
            // CP9: mọi AutoPath tới tọa chỉ StopPath nếu cần, tuyệt đối không Dismount.
            ResetRobustTravel(rt);
            ResetTravelFightGuard(rt);
            (void)CompleteToolOwnedRoute(rt, true, s.autoPathing != 0, s.riding != 0);
            arrived = true;
            return true;
        }

        // Once mount #1/#2 have both timed out in the first cycle, do not immediately
        // walk. v0.3 performs P3 AUTO→Đánh quái for 10s, stops it via Travel Guard,
        // then grants a fresh second mount x2 cycle.
        if (rt.travelFightBoostPhase != 0) {
            return HandleMountFightBoost(a, now, context);
        }

        const DWORD phaseElapsed = rt.travelFootFallback
            ? (rt.travelFootTick == 0 ? 0 : now - rt.travelFootTick)
            : (rt.travelMountTick == 0 ? 0 : now - rt.travelMountTick);
        const MountAssistAction assist = DecideMountAssist(s.riding != 0, s.autoPathing != 0,
                                                           rt.travelMountAttempts, rt.travelFootFallback,
                                                           phaseElapsed, kMountRetryWaitMs);
        if (s.riding) {
            const int completedCycle = rt.travelMountCycle;
            ResetRobustTravel(rt);
            if (assist == MountAssistAction::StartPath) {
                (void)SendDecision(a, Action::StartPath, targetProfile, context, travelTolerance);
            } else {
                rt.status = L"Đang cưỡi ngựa AutoPath tới " + where;
            }
            if (completedCycle == 1) LogAccount(a, L"MOUNT RECOVERY PASS: lên ngựa thành công sau boost 10s.");
            return true;
        }
        if (rt.travelFootFallback) {
            // Compatibility cleanup for a stale runtime created by an older build.
            // Walking AutoPath is no longer permitted under any route.
            if (s.autoPathing) (void)SendDecision(a, Action::StopPath, targetProfile, context);
            ResetRobustTravel(rt);
            ResetTravelFightGuard(rt);
            rt.status = L"MOUNT REQUIRED • đã hủy trạng thái chạy bộ cũ, quay lại chu kỳ lên ngựa";
            return true;
        }
        if (assist == MountAssistAction::Wait) {
            rt.status = rt.travelMountAttempts <= 1 ? L"Chờ lên ngựa lần 1 • tối đa 5s" : L"Chờ lên ngựa lần 2 • tối đa 5s";
            return true;
        }
        if (assist == MountAssistAction::Mount) {
            if (SendDecision(a, Action::Mount, targetProfile, context)) {
                ++rt.travelMountAttempts;
                if (rt.travelMountAttempts > 2) rt.travelMountAttempts = 2;
                rt.travelMountTick = now;
                rt.status = rt.travelMountAttempts == 1 ? L"Lên ngựa lần 1 • chờ 5s" : L"Lên ngựa lần 2 • chờ 5s";
            } else {
                rt.status = L"Chờ gửi lệnh lên ngựa • chưa tính lần thử";
            }
            return true;
        }

        // DecideMountAssist reaches MountCycleFailed here only after two mount attempts timed out.
        if (rt.travelMountCycle == 0) {
            rt.travelFightBoostPhase = 0;
            return HandleMountFightBoost(a, now, context);
        }

        // Second mount x2 cycle also failed. StartPath remains forbidden on foot;
        // reset and retry the mount workflow instead of silently walking.
        ResetRobustTravel(rt);
        ResetTravelFightGuard(rt);
        rt.status = L"MOUNT REQUIRED • 2 chu kỳ chưa lên được ngựa • lặp lại, tuyệt đối không StartPath chạy bộ";
        LogAccount(a, L"MOUNT REQUIRED: Mount x2 → Fight10s → Mount x2 vẫn fail • reset chu kỳ; KHÔNG chạy bộ AutoPath.");
        return true;
    }

    void ResetShortcutRoute(RuntimeState& rt) {
        rt.shortcutKind = ShortcutKind::None;
        rt.shortcutPhase = 0;
        rt.shortcutFinalMap = 0;
        rt.shortcutExpectedMap = 0;
        rt.shortcutSourceMap = 0;
        rt.shortcutClickIndex = 0;
        rt.shortcutTick = 0;
        rt.shortcutAttempts = 0;
        ResetRobustTravel(rt);
        ResetTravelFightGuard(rt);
    }

    bool ShortcutBridgeCall(Account& a, Command command, int arg0, const std::wstring& label,
                            DWORD now, int timeoutMs = 2200) {
        if (a.runtime.clientFreezeActive) return false;
        std::wstring error;
        if (!EnsureAttach(a, error)) {
            a.runtime.status = L"ĐƯỜNG TẮT • không attach được Bridge: " + error;
            return false;
        }
        Response r{};
        if (!a.bridge.Call(command, arg0, 0, 0, r, error, timeoutMs)) {
            if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout trong đường tắt", now);
            a.runtime.status = L"ĐƯỜNG TẮT • " + label + L" chưa pass: " + error;
            return false;
        }
        LogAccount(a, L"ĐƯỜNG TẮT PASS • " + label + L" • " + r.detail);
        return true;
    }

    void FailShortcutRoute(Account& a, const std::wstring& reason) {
        a.runtime.shortcutPhase = 99;
        a.runtime.status = L"ĐƯỜNG TẮT FAIL-CLOSED • " + reason;
        ResetRobustTravel(a.runtime);
        ResetTravelFightGuard(a.runtime);
        LogAccount(a, L"ĐƯỜNG TẮT FAIL-CLOSED: " + reason + L" • không click/chuyển tiếp mù.");
        if(a.bridge.Attached()&&IsWindow(a.game.window)&&!a.runtime.clientFreezeActive)(void)StartAccountUiRecovery(a,AccountUiRecoveryPlan::GenericFailClosed,L"ĐƯỜNG TẮT FAIL-CLOSED • "+reason,GetTickCount());
    }

    TargetProfile ShortcutWorldTarget(const wchar_t* name, int mapID, int worldX, int worldY) const {
        TargetProfile t{}; t.name = name; t.mapID = mapID; t.x = worldX; t.y = worldY;
        t.valid = mapID > 0 && worldX > 0 && worldY > 0;
        return t;
    }

    bool ShortcutTravelLeg(Account& a, DWORD now, const TargetProfile& leg, const wchar_t* label, bool& arrived) {
        arrived = false;
        if (!leg.valid) {
            FailShortcutRoute(a, std::wstring(L"chưa gán tọa: ") + label + L" • lấy vị trí đúng nguồn cấu hình trước");
            return true;
        }
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!a.snapshotValid || !s.mapReady || s.waitingChangeMap ||
            (s.validMask & (ValidMap | ValidPosition | ValidAutoPath | ValidRiding)) !=
            (ValidMap | ValidPosition | ValidAutoPath | ValidRiding)) {
            rt.status = std::wstring(L"ĐƯỜNG TẮT • chờ state ổn định trước FSM tới ") + label;
            return true;
        }

        // v3.2: use exactly the same travel machine as normal sell/train travel.
        // Never bypass mount assist with a direct StartPath for a shortcut waypoint.
        return HandleRobustTravelDirect(a, now, leg, label, arrived, kPreciseWorldTolerance);
    }

    bool HandleKunLunExitTryClickRoute(Account& a, DWORD now, const TargetProfile& finalTarget,
                                       const TargetProfile& npcPoint) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (rt.shortcutPhase == 99) return true;

        if (rt.shortcutPhase <= 1) {
            bool reached = false;
            (void)ShortcutTravelLeg(a, now, npcPoint, L"NPC RỜI Côn Lôn Sơn", reached);
            if (!reached) { rt.shortcutPhase = 1; return true; }
            for (std::size_t i = 0; i < shortcutSettings_.kunlunExitClicks.size(); ++i) {
                const TimedClickPoint& click = shortcutSettings_.kunlunExitClicks[i];
                if (!click.point.valid) {
                    FailShortcutRoute(a, L"thiếu tọa TryClickUI " + std::to_wstring(i + 1) +
                                         L"/3 của NPC RỜI Côn Lôn; phải F8 đủ Mở NPC → Đại Lý → Xác nhận");
                    return true;
                }
                if (click.timeMs < 0 || click.timeMs > 60000 || click.delayMs < 0 || click.delayMs > 60000) {
                    FailShortcutRoute(a, L"Time/Delay click " + std::to_wstring(i + 1) + L"/3 ngoài khoảng 0..60000 ms");
                    return true;
                }
            }
            rt.shortcutSourceMap = s.mapID;
            rt.shortcutClickIndex = 0;
            rt.shortcutPhase = 2;
            rt.shortcutTick = now;
            rt.shortcutAttempts = 0;
            rt.status = L"ĐƯỜNG TẮT CLS • đã tới đúng NPC rời Côn Lôn • chuẩn bị chuỗi 3 TryClickUI";
            LogAccount(a, L"ĐƯỜNG TẮT CLS ARM 3-CLICK • NPC RỜI Côn Lôn (khác Xa Truyền Bình ID387 đi vào Côn Lôn) • không còn callback Đại Lý/Xác nhận.");
            return true;
        }

        if (rt.shortcutPhase == 2) {
            if (rt.shortcutClickIndex < 0 || rt.shortcutClickIndex >= 3) {
                FailShortcutRoute(a, L"index chuỗi 3 TryClickUI không hợp lệ");
                return true;
            }
            const int index = rt.shortcutClickIndex;
            const TimedClickPoint& click = shortcutSettings_.kunlunExitClicks[static_cast<std::size_t>(index)];
            const DWORD previousDelay = index == 0 ? 0u :
                static_cast<DWORD>(shortcutSettings_.kunlunExitClicks[static_cast<std::size_t>(index - 1)].delayMs);
            const DWORD waitBefore = previousDelay + static_cast<DWORD>(click.timeMs);
            if (!Elapsed(now, rt.shortcutTick, waitBefore)) {
                rt.status = L"ĐƯỜNG TẮT CLS • chờ timing click " + std::to_wstring(index + 1) + L"/3";
                return true;
            }

            std::wstring error;
            static constexpr const wchar_t* labels[3] = {L"mở NPC", L"chọn Đại Lý", L"Xác nhận"};
            if (!DispatchInternalPointActionDirect(a, click.point,
                    L"CLS TryClickUI " + std::to_wstring(index + 1) + L"/3 • " + labels[index], error)) {
                FailShortcutRoute(a, L"TryClickUI " + std::to_wstring(index + 1) + L"/3 thất bại: " + error);
                return true;
            }
            ++rt.shortcutClickIndex;
            rt.shortcutTick = now;
            if (rt.shortcutClickIndex == 3) {
                rt.shortcutPhase = 3;
                rt.status = L"ĐƯỜNG TẮT CLS • đã click đủ 3/3 • chờ Delay click 3 rồi mới check MapID đổi";
                LogAccount(a, L"ĐƯỜNG TẮT CLS 3-CLICK PASS • đã chạy đủ Mở NPC → Đại Lý → Xác nhận; bây giờ mới bắt đầu check chuyển map.");
            } else {
                rt.status = L"ĐƯỜNG TẮT CLS • click " + std::to_wstring(rt.shortcutClickIndex) +
                            L"/3 PASS • chờ Delay + Time của click kế tiếp";
            }
            return true;
        }

        if (rt.shortcutPhase == 3) {
            const DWORD finalDelay = static_cast<DWORD>(shortcutSettings_.kunlunExitClicks[2].delayMs);
            if (!Elapsed(now, rt.shortcutTick, finalDelay)) return true;
            if (a.snapshotValid && (s.validMask & ValidMap) == ValidMap &&
                s.mapID != rt.shortcutSourceMap && s.mapReady && !s.waitingChangeMap) {
                LogAccount(a, L"ĐƯỜNG TẮT CLS CHUYỂN MAP PASS • M" + std::to_wstring(rt.shortcutSourceMap) +
                              L" → M" + std::to_wstring(s.mapID) + L" • trả về AutoPath đích M" +
                              std::to_wstring(finalTarget.mapID));
                ResetShortcutRoute(rt);
                return false;
            }
            if (Elapsed(now, rt.shortcutTick, finalDelay + 15000u)) {
                FailShortcutRoute(a, L"đã chạy đủ 3 TryClickUI nhưng MapID vẫn chưa đổi sau thời gian chờ");
            } else {
                rt.status = L"ĐƯỜNG TẮT CLS • đã đủ 3 click • chờ MapID đổi rồi mới AutoPath đích";
            }
            return true;
        }
        return true;
    }

    bool HandleShortcutNpcRoute(Account& a, DWORD now, const TargetProfile& finalTarget,
                                const TargetProfile& npcPoint, int npcID,
                                TravelSemantic semantic, int expectedMap, const wchar_t* label,
                                TravelSemantic intermediateSemantic = TravelSemantic::None,
                                bool destinationChangesMapImmediately = false,
                                int postMapX = 0, int postMapY = 0) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (rt.shortcutPhase == 99) return true;

        if (rt.shortcutPhase <= 1) {
            bool reached = false;
            (void)ShortcutTravelLeg(a, now, npcPoint, label, reached);
            if (!reached) { rt.shortcutPhase = 1; return true; }
            rt.shortcutPhase = 2; rt.shortcutTick = now; rt.shortcutAttempts = 0;
            rt.status = std::wstring(L"ĐƯỜNG TẮT • đã tới ") + label + L" • chuẩn bị mở NPC";
            return true;
        }

        if (rt.shortcutPhase == 2) {
            const bool opened = ShortcutBridgeCall(a, Command::ClickNpc, npcID,
                                                   L"ClickNPC(" + std::to_wstring(npcID) + L")", now);
            if (!opened) {
                ++rt.shortcutAttempts;
                if (rt.shortcutAttempts >= 3)
                    FailShortcutRoute(a, L"không mở được NPC ID " + std::to_wstring(npcID) + L" sau 3 lần có kiểm soát");
                return true;
            }
            rt.shortcutPhase = 3; rt.shortcutTick = now; rt.shortcutAttempts = 0;
            return true;
        }

        if (rt.shortcutPhase == 3) {
            // Do not read the menu in the same/next fast tick as ClickNPC. The
            // v0.1.8 probe had the dialog already open; the production route
            // must first allow the NPC to instantiate its GameDialog tree.
            const DWORD semanticWait = rt.shortcutAttempts == 0
                ? kShortcutNpcUiReadyMs : kShortcutSemanticRetryMs;
            if (!Elapsed(now, rt.shortcutTick, semanticWait)) return true;
            const TravelSemantic firstSemantic = intermediateSemantic == TravelSemantic::None
                ? semantic : intermediateSemantic;
            const std::wstring firstLabel = intermediateSemantic == TravelSemantic::None
                ? L"callback dòng điểm đến semantic"
                : L"callback dòng trung gian Đến các môn phái";
            if (ShortcutBridgeCall(a, Command::ClickTravelSemantic, static_cast<int>(firstSemantic),
                                   firstLabel, now, 7000)) {
                if (intermediateSemantic != TravelSemantic::None) {
                    rt.shortcutPhase = 6;
                } else if (destinationChangesMapImmediately) {
                    rt.shortcutPhase = 5;
                    rt.shortcutExpectedMap = expectedMap;
                    rt.status = L"ĐƯỜNG TẮT • điểm đến tự chuyển map • bỏ Xác nhận • chờ M" +
                                std::to_wstring(expectedMap);
                } else {
                    rt.shortcutPhase = 4;
                }
                rt.shortcutTick = now; rt.shortcutAttempts = 0;
                return true;
            }
            ++rt.shortcutAttempts; rt.shortcutTick = now;
            // Reopening the NPC while its first GameDialog is still being
            // constructed resets the menu and races the resolver. v0.1.8 does
            // one open followed by read-only polling, so production follows the
            // same read-only polling contract for every remaining semantic ClickNPC route.
            if (rt.shortcutAttempts >= kShortcutSemanticMaxAttempts) {
                FailShortcutRoute(a, L"không tìm được đúng 1 dòng điểm đến semantic sau " +
                                  std::to_wstring(kShortcutSemanticMaxAttempts) +
                                  L" lần; xem log SEMANTIC_SCAN để biết NPC/dialog nào đang mở");
            } else {
                rt.status = L"ĐƯỜNG TẮT • đã mở NPC • chờ GameDialog/dòng semantic " +
                            std::to_wstring(rt.shortcutAttempts) + L"/" +
                            std::to_wstring(kShortcutSemanticMaxAttempts);
            }
            return true;
        }

        if (rt.shortcutPhase == 6) {
            // Ngải Ni Ngoã Nhĩ is a two-level GameDialog: first "Đến các môn
            // phái", then the exact faction destination. Never reopen the NPC
            // between these two callbacks because that would reset menu level 1.
            const DWORD semanticWait = rt.shortcutAttempts == 0
                ? kShortcutNpcUiReadyMs : kShortcutSemanticRetryMs;
            if (!Elapsed(now, rt.shortcutTick, semanticWait)) return true;
            if (ShortcutBridgeCall(a, Command::ClickTravelSemantic, static_cast<int>(semantic),
                                   L"callback dòng phái đích ở menu cấp 2", now, 7000)) {
                if (destinationChangesMapImmediately) {
                    // HỎA DIỆM NO-CONFIRM: callback phái đích tự chuyển map. Không quét/callback ConfirmTravelSemantic.
                    rt.shortcutPhase = 5;
                    rt.shortcutExpectedMap = expectedMap;
                    rt.status = L"ĐƯỜNG TẮT HỎA • đã callback Thiên Sơn • KHÔNG XÁC NHẬN • chờ M" +
                                std::to_wstring(expectedMap);
                    LogAccount(a, L"ĐƯỜNG TẮT HỎA: callback Thiên Sơn PASS • game tự dịch sang Thiên Sơn • bỏ bước Xác nhận.");
                } else {
                    rt.shortcutPhase = 4;
                }
                rt.shortcutTick = now; rt.shortcutAttempts = 0;
                return true;
            }
            ++rt.shortcutAttempts; rt.shortcutTick = now;
            if (rt.shortcutAttempts >= kShortcutSemanticMaxAttempts) {
                FailShortcutRoute(a, L"đã chọn Đến các môn phái nhưng " +
                                  std::to_wstring(kShortcutSemanticMaxAttempts) +
                                  L"s vẫn không thấy đúng dòng phái đích ở menu cấp 2");
            } else {
                rt.status = L"ĐƯỜNG TẮT • đã chọn Đến các môn phái • chờ dòng phái đích " +
                            std::to_wstring(rt.shortcutAttempts) + L"/" +
                            std::to_wstring(kShortcutSemanticMaxAttempts);
            }
            return true;
        }

        if (rt.shortcutPhase == 4) {
            // Use the complete v0.1.8 donor resolver: all ACTIVE UIObject instances,
            // HandleClickEvent capability, UI delta and positive confirmation semantic.
            const DWORD confirmWait = rt.shortcutAttempts == 0
                ? kShortcutConfirmUiReadyMs : kShortcutConfirmRetryMs;
            if (!Elapsed(now, rt.shortcutTick, confirmWait)) return true;
            if (ShortcutBridgeCall(a, Command::ConfirmTravelSemantic, 0,
                                   L"callback Xác nhận theo UI-delta v0.1.8", now, 5000)) {
                rt.shortcutPhase = 5; rt.shortcutTick = now; rt.shortcutAttempts = 0;
                rt.shortcutExpectedMap = expectedMap;
                return true;
            }
            ++rt.shortcutAttempts; rt.shortcutTick = now;
            if (rt.shortcutAttempts >= kShortcutConfirmMaxAttempts) {
                FailShortcutRoute(a, L"đã callback dòng điểm đến nhưng " +
                                  std::to_wstring(kShortcutConfirmMaxAttempts) +
                                  L"s vẫn chưa callback được popup ConfirmMap chuẩn");
            } else {
                rt.status = L"ĐƯỜNG TẮT • đã callback điểm đến • chờ popup ConfirmMap " +
                            std::to_wstring(rt.shortcutAttempts) + L"/" +
                            std::to_wstring(kShortcutConfirmMaxAttempts);
            }
            return true;
        }

        if (rt.shortcutPhase == 5) {
            if (s.mapID == expectedMap && s.mapReady && !s.waitingChangeMap) {
                if(postMapX>0&&postMapY>0){rt.shortcutPhase=7;rt.shortcutTick=now;rt.shortcutAttempts=0;rt.status=L"ĐƯỜNG TẮT HỎA • đã vào Thiên Sơn → tới điểm trung chuyển 3073,2338";return true;}
                LogAccount(a, std::wstring(L"ĐƯỜNG TẮT CHUYỂN MAP PASS • check authoritative M") + std::to_wstring(expectedMap) + L" → tiếp tục AutoPath đích M" + std::to_wstring(finalTarget.mapID));
                ResetShortcutRoute(rt);return false;
            }
            if (Elapsed(now, rt.shortcutTick, 15000)) {
                FailShortcutRoute(a, destinationChangesMapImmediately
                    ? L"đã callback Thiên Sơn (không có bước xác nhận) nhưng sau 15s chưa check được MapID đích M" + std::to_wstring(expectedMap)
                    : L"đã callback xác nhận nhưng sau 15s chưa check được MapID đích M" + std::to_wstring(expectedMap));
            } else {
                rt.status = destinationChangesMapImmediately
                    ? L"ĐƯỜNG TẮT HỎA • chờ check map M" + std::to_wstring(expectedMap) + L" sau callback Thiên Sơn • không xác nhận"
                    : L"ĐƯỜNG TẮT • chờ check map M" + std::to_wstring(expectedMap) + L" sau xác nhận";
            }
            return true;
        }
        if(rt.shortcutPhase==7){
            const TargetProfile transit=ShortcutWorldTarget(L"Thiên Sơn điểm trung chuyển",expectedMap,postMapX,postMapY);
            bool reached=false;(void)ShortcutTravelLeg(a,now,transit,L"Thiên Sơn 3073,2338",reached);if(!reached)return true;
            LogAccount(a,L"ĐƯỜNG TẮT HỎA PASS • đã tới Thiên Sơn 3073,2338 → tiếp tục AutoPath đích.");ResetShortcutRoute(rt);return false;
        }
        return true;
    }

    bool HandleShortcutTravel(Account& a, DWORD now, const TargetProfile& finalTarget) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (!shortcutSettings_.enabled) {
            if (rt.shortcutKind != ShortcutKind::None) ResetShortcutRoute(rt);
            return false;
        }
        if (rt.shortcutKind != ShortcutKind::None && rt.shortcutFinalMap != finalTarget.mapID) {
            LogAccount(a, L"ĐƯỜNG TẮT: đích đổi giữa chừng → reset waypoint cũ, tính lại theo MapID mới.");
            ResetShortcutRoute(rt);
        }
        if (rt.shortcutKind == ShortcutKind::None) {
            const bool currentKunlun = s.mapID == 75 || s.mapID == 76;
            const bool finalKunlun = finalTarget.mapID == 75 || finalTarget.mapID == 76;
            const bool currentFire = s.mapID == 55 || s.mapID == 70;
            if (currentKunlun && !finalKunlun) {
                rt.shortcutKind = ShortcutKind::KunLunExit;
            } else if (IsPrimaryShortcutOriginMap(s.mapID) && finalKunlun) {
                rt.shortcutKind = ShortcutKind::KunLunEnter;
            } else if (IsPrimaryShortcutOriginMap(s.mapID) && (finalTarget.mapID == 55 || finalTarget.mapID == 70)) {
                rt.shortcutKind = ShortcutKind::FireEnter;
            } else if (currentFire && finalTarget.mapID != 55 && finalTarget.mapID != 70) {
                rt.shortcutKind = ShortcutKind::FireExit;
            } else {
                return false;
            }
            rt.shortcutFinalMap = finalTarget.mapID;
            rt.shortcutPhase = 1; rt.shortcutTick = now; rt.shortcutAttempts = 0;
            ResetRobustTravel(rt); ResetTravelFightGuard(rt);
            LogAccount(a, L"ĐƯỜNG TẮT ARM • current M" + std::to_wstring(s.mapID) + L" → final M" + std::to_wstring(finalTarget.mapID) +
                          L" • kind=" + std::to_wstring(static_cast<int>(rt.shortcutKind)));
        }

        switch (rt.shortcutKind) {
            case ShortcutKind::KunLunExit: {
                const TargetProfile npc = ShortcutWorldTarget(L"NPC RỜI Côn Lôn Sơn", 75, shortcutSettings_.kunlunNpcX, shortcutSettings_.kunlunNpcY);
                return HandleKunLunExitTryClickRoute(a, now, finalTarget, npc);
            }
            case ShortcutKind::KunLunEnter: {
                const SellNpcPosition* xaPos = nullptr;
                for (std::size_t i = 0; i < kSellNpcs.size(); ++i) {
                    if (kSellNpcs[i].npcID == kXaTruyenBinhNpcId) {
                        xaPos = &sellNpcPositions_[i];
                        break;
                    }
                }
                if (!xaPos || !xaPos->valid) {
                    FailShortcutRoute(a, L"Xa Truyền Bình ID 387 chưa có tọa ngoài màn hình chính • chọn NPC bán Xa Truyền Bình rồi LẤY VỊ TRÍ");
                    return true;
                }
                const TargetProfile npc = ShortcutWorldTarget(L"Xa Truyền Bình", 5, xaPos->x, xaPos->y);
                return HandleShortcutNpcRoute(a, now, finalTarget, npc, kXaTruyenBinhNpcId, TravelSemantic::KunLunSon, 75, L"Xa Truyền Bình");
            }
            case ShortcutKind::FireEnter: {
                const TargetProfile npc = ShortcutWorldTarget(L"Ngải Ni Ngoã Nhĩ", 5, shortcutSettings_.ngaiX, shortcutSettings_.ngaiY);
                return HandleShortcutNpcRoute(a, now, finalTarget, npc, kNgaiNiNgoaNhiNpcId,
                                              TravelSemantic::ThienSon, kThienSonMapId, L"Ngải Ni Ngoã Nhĩ",
                                              TravelSemantic::DenCacMonPhai, true,
                                              kThienSonTransitX, kThienSonTransitY);
            }
            case ShortcutKind::FireExit: {
                const TargetProfile tt = ShortcutWorldTarget(L"Thiên Sơn điểm trung chuyển", kThienSonMapId, kThienSonTransitX, kThienSonTransitY);
                bool reached = false;
                (void)ShortcutTravelLeg(a, now, tt, L"Thiên Sơn 3073,2338", reached);
                if (!reached) return true;
                LogAccount(a, L"ĐƯỜNG TẮT HỎA PASS • đã tới Thiên Sơn 3073,2338 → tiếp tục AutoPath đích bình thường.");
                ResetShortcutRoute(rt); return false;
            }
            case ShortcutKind::None: return false;
        }
        return false;
    }

    bool HandleRobustTravel(Account& a, DWORD now, const TargetProfile& targetProfile,
                            const wchar_t* context, bool& arrived, int toleranceOverride = 0) {
        arrived = false;
        if (HandleShortcutTravel(a, now, targetProfile)) return true;
        return HandleRobustTravelDirect(a, now, targetProfile, context, arrived, toleranceOverride);
    }

    void BeginTrainRecovery(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        rt.trainPositionMonitorArmed = false;
        rt.lastTrainPositionCheckTick = 0;
        rt.trainRecoveryPhase = 4;
        rt.fightPhase = 0;
        rt.fightAttempts = 0;
        ResetRobustTravel(rt);
        ResetTravelFightGuard(rt);
        LogAccount(a, L"CHECK 1 PHÚT: lệch bãi → v0.3 Travel Guard bắt buộc AutoFight OFF trước mọi StartPath → quay lại tọa train.");
    }

    bool HandleTrainRecovery(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        if (rt.trainRecoveryPhase == 0) return false;

        bool arrived = false;
        (void)HandleRobustTravel(a, now, a.profile.target, L"bãi train", arrived);
        if (arrived) {
            rt.trainRecoveryPhase = 0;
            rt.wasAtTarget = false;
            rt.fightPhase = 0;
            rt.fightAttempts = 0;
            rt.status = L"Đã về bãi • chuẩn bị bật lại Đánh quái";
            LogAccount(a, L"Đã quay lại bãi sau check lệch • chuẩn bị AUTO→Đánh quái.");
        }
        return true;
    }


    // 10.2 — independent seller for role NONE only.
    // MAIN keeps the current quota/macro seller unchanged; CON never enters this FSM.
    TargetProfile IndependentSellNpcTarget(const Account& a) const {
        const int presetIndex = (a.runtime.retiredNoneSellPreset >= 0 && a.runtime.retiredNoneSellPreset < static_cast<int>(kSellNpcs.size()))
            ? a.runtime.retiredNoneSellPreset : 0;
        const SellNpcPreset& npc = kSellNpcs[static_cast<std::size_t>(presetIndex)];
        TargetProfile target{};
        target.name = npc.name;
        target.mapID = npc.mapID;
        const SellNpcPosition& pos = sellNpcPositions_[static_cast<std::size_t>(presetIndex)];
        target.x = pos.x;
        target.y = pos.y;
        target.valid = pos.valid;
        return target;
    }

    bool IndependentSellConfigured(Account& a, MainMacroSellConfig& cfg, std::wstring& reason) {
        cfg = {};
        if (a.profile.tradeRole != 0) {
            reason = L"seller độc lập chỉ áp dụng role NONE";
            return false;
        }
        (void)LoadMainMacroSellConfig(a, cfg, true);
        if (!MainMacroPointValid(cfg.point)) {
            reason = L"chưa có TỌA MACRO BÁN • chọn acc NONE → TÙY CHỈNH BÁN → LẤY TỌA F8";
            return false;
        }
        const TargetProfile npc = IndependentSellNpcTarget(a);
        if (!npc.valid) {
            reason = L"NPC bán chưa có tọa độ • nhập X/Y hoặc LẤY TỌA NPC BÁN";
            return false;
        }
        reason.clear();
        return true;
    }

    void BeginIndependentAutoSell(Account& a, DWORD now) {
        if (a.profile.tradeRole != 0) return;
        RuntimeState& rt = a.runtime;
        rt.sellPhase = 4;
        rt.sellPhaseTick = now;
        rt.sellOpenAttempts = 0;
        rt.sellMacroIndex = 0;
        rt.sellMacroRepeatDone = 0;
        rt.sellMacroNextTick = 0;
        rt.sellMacroCompletionDueTick = 0;
        rt.sellMacroPass = 0;
        rt.sellLastFreeBag = a.snapshot.freeBagSpace;
        rt.sellBagStableSince = 0;
        rt.trainPositionMonitorArmed = false;
        rt.lastTrainPositionCheckTick = 0;
        rt.trainRecoveryPhase = 0;
        rt.fightPhase = 0;
        rt.fightAttempts = 0;
        rt.wasAtTarget = false;
        rt.crossMapSeenAutoPath = false;
        rt.stallSinceTick = 0;
        rt.confirmAttempts = 0;
        rt.crossMapRouteArmed = false;
        rt.crossMapRouteMoved = false;
        ResetRobustTravel(rt);
        ResetTravelFightGuard(rt);
        if (a.bridge.Attached()) {
            Response r{}; std::wstring error;
            if (!a.bridge.Call(Command::StopPath, 0, 0, 0, r, error, 700) && BridgeLooksUnresponsive(error)) {
                EnterClientFreeze(a, L"Bridge timeout lúc bắt đầu Auto Sell NONE", now);
            }
        }
        const int presetIndex = (a.runtime.retiredNoneSellPreset >= 0 && a.runtime.retiredNoneSellPreset < static_cast<int>(kSellNpcs.size()))
            ? a.runtime.retiredNoneSellPreset : 0;
        LogAccount(a, L"NONE • TÚI FULL → bắt đầu Auto bán 3.6 • " +
                      std::wstring(kSellNpcs[static_cast<std::size_t>(presetIndex)].name));
    }

    bool RunIndependentSellMacro(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        MainMacroSellConfig cfg{};
        std::wstring configuredReason;
        if (!IndependentSellConfigured(a, cfg, configuredReason)) {
            rt.sellPhase = 10;
            rt.status = L"AUTO BÁN NONE BLOCKED • " + configuredReason;
            LogAccount(a, rt.status);
            return true;
        }

        const DWORD fixedDelay = static_cast<DWORD>(std::clamp(cfg.delayMs, 50, 60000));
        const DWORD delay = rt.sellMacroIndex < 4 ? 350u : (rt.sellMacroIndex == 4 ? fixedDelay : 180u);
        if (rt.sellMacroNextTick != 0 && !Elapsed(now, rt.sellMacroNextTick, delay)) return true;

        Response response{};
        std::wstring error;
        if (rt.sellMacroIndex < 4) {
            if (!a.bridge.Call(Command::AdvanceBackgroundSell, 0, 0, 0, response, error, 2400)) {
                if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout lúc mở UI bán NONE", now);
                ++rt.sellOpenAttempts;
                rt.sellMacroNextTick = now;
                rt.status = L"AUTO BÁN NONE • chờ đúng control UI • thử " +
                            std::to_wstring(rt.sellOpenAttempts) + L"/12";
                if (rt.sellOpenAttempts >= 12) {
                    rt.sellPhase = 10;
                    LogAccount(a, L"AUTO BÁN NONE FAIL khi mở chuỗi shop: " + error + L" • dừng fail-closed");
                }
                return true;
            }
            rt.sellOpenAttempts = 0;
            rt.sellMacroIndex = std::clamp(response.value0, 0, 4);
            rt.sellMacroNextTick = now;
            rt.status = L"AUTO BÁN NONE • UI semantic stage " + std::to_wstring(rt.sellMacroIndex) +
                        L"/4 • " + std::wstring(response.detail);
            return true;
        }

        if (rt.sellMacroIndex == 4) {
            int normalizedX = -1, normalizedY = -1;
            if (!NormalizeClickPointForBridge(a.game, cfg.point, normalizedX, normalizedY, error)) {
                rt.sellPhase = 10;
                rt.status = L"AUTO BÁN NONE FAIL • tọa độ ô trang bị không hợp lệ";
                LogAccount(a, rt.status + L" • " + error);
                return true;
            }
            const int clickTarget = fixed_slot_sell_logic::EffectiveClickCount(a.sellStep5LearnedRepeat);
            if (!a.bridge.Call(Command::SellNextBagItem, normalizedX, normalizedY, 0,
                               response, error, 2600)) {
                if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout lúc bán ô trang bị NONE", now);
                ++rt.sellOpenAttempts;
                rt.sellMacroNextTick = now;
                rt.status = L"AUTO BÁN NONE • callback ô lỗi " + std::to_wstring(rt.sellOpenAttempts) + L"/6";
                if (rt.sellOpenAttempts >= 6) {
                    rt.sellPhase = 10;
                    LogAccount(a, L"AUTO BÁN NONE FAIL callback item: " + error + L" • dừng fail-closed");
                }
                return true;
            }
            rt.sellOpenAttempts = 0;
            rt.sellMacroNextTick = now;
            rt.sellLastFreeBag = response.value0;
            ++rt.sellMacroRepeatDone;
            if (rt.sellMacroRepeatDone >= clickTarget) {
                rt.sellMacroIndex = 5;
                rt.sellMacroRepeatDone = 0;
                rt.sellMacroCompletionDueTick = now + fixedDelay;
                rt.status = L"AUTO BÁN NONE • đủ callback ô cố định • bắt đầu đóng shop/tay nải";
            } else {
                rt.status = L"AUTO BÁN NONE • ô cố định " + std::to_wstring(rt.sellMacroRepeatDone) +
                            L"/" + std::to_wstring(clickTarget) +
                            L" • FreeBag=" + std::to_wstring(response.value0);
            }
            return true;
        }

        if (rt.sellMacroIndex == 5) {
            if (rt.sellMacroCompletionDueTick != 0 &&
                static_cast<LONG>(now - rt.sellMacroCompletionDueTick) < 0) {
                rt.status = L"AUTO BÁN NONE • callback cuối xong • chờ hết delay";
                return true;
            }
            rt.sellMacroCompletionDueTick = 0;
            if (rt.sellMacroRepeatDone >= 4) {
                rt.sellPhase = 7;
                rt.sellPhaseTick = now;
                rt.sellBagStableSince = 0;
                rt.status = L"AUTO BÁN NONE xong • chờ FreeBagSpace xác nhận";
                return true;
            }
            if (!a.bridge.Call(Command::CloseBackgroundSell, 0, 0, 0, response, error, 2200)) {
                if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout lúc đóng UI bán NONE", now);
                ++rt.sellOpenAttempts;
                rt.sellMacroNextTick = now;
                if (rt.sellOpenAttempts >= 4) {
                    rt.sellPhase = 7;
                    rt.sellPhaseTick = now;
                    rt.sellBagStableSince = 0;
                    LogAccount(a, L"AUTO BÁN NONE: không đóng hết UI sau 4 lần • vẫn chuyển sang verify túi");
                }
                return true;
            }
            rt.sellOpenAttempts = 0;
            rt.sellMacroNextTick = now;
            if (response.resultCode == static_cast<std::int32_t>(ActionResult::NothingToClose)) {
                rt.sellPhase = 7;
                rt.sellPhaseTick = now;
                rt.sellBagStableSince = 0;
                rt.status = L"AUTO BÁN NONE • UI đã đóng • verify túi";
            } else {
                ++rt.sellMacroRepeatDone;
                rt.status = L"AUTO BÁN NONE • đã đóng " + std::to_wstring(rt.sellMacroRepeatDone) + L" lớp UI";
            }
            return true;
        }
        return true;
    }

    bool HandleIndependentAutoSell(Account& a, DWORD now) {
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        if (a.profile.tradeRole != 0 || rt.sellPhase == 0) return false;

        if (rt.sellPhase == 4) {
            const TargetProfile npcTarget = IndependentSellNpcTarget(a);
            if (!npcTarget.valid) {
                rt.status = L"AUTO BÁN NONE • NPC bán chưa có tọa độ";
                return true;
            }
            bool arrived = false;
            (void)HandleRobustTravel(a, now, npcTarget, L"NPC bán NONE", arrived, kPreciseWorldTolerance);
            if (arrived) {
                rt.lastAction = Action::Hold;
                rt.sellPhase = 5;
                rt.sellPhaseTick = now;
                rt.status = L"NONE đã tới NPC • chuẩn bị mở bán nền";
            }
            return true;
        }

        if (rt.sellPhase == 5) {
            if (!Elapsed(now, rt.sellPhaseTick, 500)) return true;
            const int presetIndex = (a.runtime.retiredNoneSellPreset >= 0 && a.runtime.retiredNoneSellPreset < static_cast<int>(kSellNpcs.size()))
                ? a.runtime.retiredNoneSellPreset : 0;
            const SellNpcPreset& npc = kSellNpcs[static_cast<std::size_t>(presetIndex)];
            Response response{}; std::wstring error;
            if (!a.bridge.Call(Command::BeginBackgroundSell, npc.npcID, 0, 0, response, error, 2200)) {
                if (BridgeLooksUnresponsive(error)) EnterClientFreeze(a, L"Bridge timeout/busy khi mở phiên bán NONE", now);
                ++rt.sellOpenAttempts;
                if (rt.sellOpenAttempts >= 2) {
                    rt.sellPhase = 10;
                    rt.status = L"AUTO BÁN NONE • không mở được NPC • chờ thủ công";
                } else {
                    rt.sellPhaseTick = now;
                }
                return true;
            }
            rt.sellOpenAttempts = 0;
            ++rt.sellMacroPass;
            rt.sellPhase = 6;
            rt.sellPhaseTick = now;
            rt.sellMacroIndex = 0;
            rt.sellMacroRepeatDone = 0;
            rt.sellMacroNextTick = 0;
            rt.sellMacroCompletionDueTick = 0;
            rt.status = L"AUTO BÁN NONE • đã ClickNPC nội bộ ID " + std::to_wstring(npc.npcID);
            return true;
        }

        if (rt.sellPhase == 6) {
            if (!Elapsed(now, rt.sellPhaseTick, 1200)) return true;
            return RunIndependentSellMacro(a, now);
        }

        if (rt.sellPhase == 7) {
            if ((s.validMask & ValidBagSpace) == 0) {
                rt.status = L"AUTO BÁN NONE • chờ FreeBagSpace authoritative";
                return true;
            }
            if (s.freeBagSpace > 0) {
                if (rt.sellLastFreeBag != s.freeBagSpace) {
                    rt.sellLastFreeBag = s.freeBagSpace;
                    rt.sellBagStableSince = now;
                } else if (rt.sellBagStableSince == 0) {
                    rt.sellBagStableSince = now;
                } else if (Elapsed(now, rt.sellBagStableSince, 1500)) {
                    a.sellStep5LearnedRepeat = s.freeBagSpace;
                    rt.sellPhase = 8;
                    rt.sellPhaseTick = now;
                    rt.crossMapSeenAutoPath = false;
                    rt.crossMapRouteArmed = false;
                    rt.crossMapRouteMoved = false;
                    rt.stallSinceTick = 0;
                    rt.confirmAttempts = 0;
                    ResetRobustTravel(rt);
                    rt.status = L"AUTO BÁN NONE • bán xong → quay bãi train";
                    LogAccount(a, L"AUTO BÁN NONE XONG • FreeBagSpace=" + std::to_wstring(s.freeBagSpace) +
                                  L" ổn định 1.5s • lần sau callback ô cố định=" +
                                  std::to_wstring(fixed_slot_sell_logic::EffectiveClickCount(a.sellStep5LearnedRepeat)));
                    TelegramRecordSellEpisode(a);
                }
                return true;
            }
            if (Elapsed(now, rt.sellPhaseTick, 3500)) {
                if (rt.sellMacroPass < 2) {
                    rt.sellPhase = 5;
                    rt.sellPhaseTick = now;
                    rt.sellOpenAttempts = 0;
                    rt.status = L"AUTO BÁN NONE • túi vẫn full → chạy bán lần 2";
                } else {
                    rt.sellPhase = 10;
                    rt.status = L"AUTO BÁN NONE • bán 2 lần nhưng túi vẫn full • chờ thủ công";
                }
            }
            return true;
        }

        if (rt.sellPhase == 8) {
            bool arrived = false;
            (void)HandleRobustTravel(a, now, a.profile.target, L"bãi train sau bán NONE", arrived);
            if (arrived) {
                rt.sellPhase = 0;
                rt.fightPhase = 0;
                rt.fightAttempts = 0;
                rt.wasAtTarget = false;
                rt.trainPositionMonitorArmed = false;
                rt.lastTrainPositionCheckTick = 0;
                rt.lastAction = Action::Hold;
                rt.status = L"NONE đã về bãi • tiếp tục AUTO train";
                LogAccount(a, L"AUTO BÁN NONE • đã về bãi train • trả quyền cho train FSM.");
                return false;
            }
            return true;
        }

        if (rt.sellPhase == 10) {
            if ((s.validMask & ValidBagSpace) && s.freeBagSpace > 0) {
                rt.sellPhase = 8;
                rt.sellPhaseTick = now;
                ResetRobustTravel(rt);
                rt.status = L"AUTO BÁN NONE • túi đã có ô trống → quay bãi train";
            }
            return true;
        }
        return true;
    }

    bool AutoLootCriticalBusy(const Account& a) const {
        const RuntimeState& rt = a.runtime;
        const Snapshot& st = a.snapshot;
        if (partyBuildModeActive_ || RecorderBlocksAccount(a)) return true;
        if (rt.clientFreezeActive || AccountUiRecoveryActive(a) || rt.postReviveRecoveryPending) return true;
        if (rt.autoLootBlockedThisTick) return true;
        if (rt.revivePhase != 0 || rt.deadSinceTick != 0 || rt.trainRecoveryPhase != 0) return true;
        if (rt.routeOwnershipResetPending || rt.autoPathFightConflictLatched || st.autoPathing) return true;
        if (rt.travelFightGuardPhase != 0 || rt.travelFightBoostPhase != 0 || rt.shortcutKind != ShortcutKind::None) return true;
        if (rt.tradeTravelPhase != 0 || a.tradeHeld) return true;
        if (rt.sellPhase != 0 || activeMainMacroSellPid_ == a.game.pid) return true;
        if (rt.priorityAutoRequestSlot != ClickSlot::None || rt.priorityAutoPointPhase != 0) return true;
        if (tradeTxn_.phase != TradePhase::Idle &&
            (tradeTxn_.mainPid == a.game.pid || tradeTxn_.childPid == a.game.pid)) return true;
        return false;
    }

    auto_loot_logic::RuntimeGate AutoLootGateFor(const Account& a) const {
        auto_loot_logic::Inputs in{};
        in.enabled = autoLootEnabled_;
        in.toolRunning = a.runtime.running;
        in.snapshotValid = a.snapshotValid;
        in.mapReady = a.snapshot.mapReady != 0;
        in.waitingChangeMap = a.snapshot.waitingChangeMap != 0;
        in.lifeValid = (a.snapshot.validMask & ValidLifeState) != 0;
        in.dead = a.snapshot.dead != 0;
        in.autoFightValid = (a.snapshot.validMask & ValidAutoFight) != 0;
        in.autoFight = a.snapshot.autoFight != 0;
        in.bagValid = (a.snapshot.validMask & ValidBagSpace) != 0;
        in.freeBagSpace = a.snapshot.freeBagSpace;
        in.criticalBusy = AutoLootCriticalBusy(a);
        return auto_loot_logic::Evaluate(in);
    }

    void AutoLootTechnicalError(Account& a, DWORD now, const std::wstring& detail) {
        RuntimeState& rt = a.runtime;
        if (!rt.autoLootErrorLatched || rt.autoLootLastErrorLogTick == 0 || now - rt.autoLootLastErrorLogTick >= 30000u) {
            LogAccount(a, L"AUTO NHẶT • lỗi kỹ thuật bridge/semantic • " + detail);
            rt.autoLootLastErrorLogTick = now;
        }
        rt.autoLootErrorLatched = true;
    }

    void TickAutoLootBackground(DWORD now) {
        if (!autoLootEnabled_ || globalPaused_) return;
        const std::size_t count = accounts_.empty() ? 1 : accounts_.size();
        for (std::size_t index = 0; index < accounts_.size(); ++index) {
            Account& a = *accounts_[index];
            RuntimeState& rt = a.runtime;
            if (AutoLootGateFor(a) != auto_loot_logic::RuntimeGate::Active) {
                // Preserve the user's ON setting, but re-stagger on every automatic resume
                // (AutoFight ON again, bag no longer full, or critical workflow released).
                rt.autoLootNextTick = 0;
                continue;
            }

            if (rt.autoLootNextTick == 0) {
                const DWORD stagger = auto_loot_logic::InitialStaggerMs(index, count, autoLootIntervalMs_);
                rt.autoLootNextTick = now + stagger;
                if (stagger != 0) continue;
            }
            if (!auto_loot_logic::TickDue(now, rt.autoLootNextTick)) continue;

            Response response{};
            std::wstring detail;
            const bool called = a.bridge.Attached() &&
                a.bridge.Call(Command::PickNearestLoot, 0, 0, 0, response, detail, 700);
            rt.autoLootNextTick = now + static_cast<DWORD>(autoLootIntervalMs_);
            if (!called) {
                AutoLootTechnicalError(a, now, detail.empty() ? L"bridge call thất bại" : detail);
                continue;
            }

            const auto result = static_cast<ActionResult>(response.resultCode);
            if (result == ActionResult::NoCandidate || result == ActionResult::ActionInvoked) {
                rt.autoLootErrorLatched = false;
                continue; // NoCandidate is normal and intentionally silent.
            }
            AutoLootTechnicalError(a, now, detail.empty() ? L"resultCode không hợp lệ" : detail);
        }
    }

    void TickAccount(Account& a) {
        if (!a.runtime.running) return;
        RuntimeState& rt = a.runtime;
        const Snapshot& s = a.snapshot;
        const DWORD now = GetTickCount();

        if (!s.mapReady || s.waitingChangeMap) {
            rt.candidateCount = 0;
            rt.qualifiedMap = 0;
            rt.stallSinceTick = 0;
            rt.fightPhase = 0;
            rt.status = L"Đang chuyển map • chặn action/click";
            return;
        }
        const std::uint32_t need = ValidMap | ValidPosition | ValidRiding | ValidAutoPath;
        if ((s.validMask & need) != need) {
            rt.status = L"State chưa đủ";
            return;
        }

        // Movement observation is serviced globally before P1/P2/P3 so World Flow-held
        // accounts receive the exact same Lâu Lan stall detection as normal accounts.
        if (HandleDeath(a, now)) return;
        if (HandleRouteOwnershipReset(a, now)) return;
        if (HandleUnderworldAutoFightGuard(a, now)) return;
        // P1 XN is PER-ACCOUNT PRIORITY ONLY and now uses a per-client internal callback, so it
        // remains eligible during World Flow HOLD without touching the Windows cursor.

        if (rt.qualifiedMap != s.mapID) {
            if (rt.candidateMap == s.mapID) ++rt.candidateCount;
            else { rt.candidateMap = s.mapID; rt.candidateCount = 1; }
            if (rt.candidateCount < 2) {
                rt.status = L"Ổn định Map 1/2";
                return;
            }
            rt.qualifiedMap = s.mapID;
            rt.candidateCount = 0;
        }

        // 10.2 independent Auto Sell is role NONE only. It has priority over normal
        // train/recovery and the background loot scheduler, matching the approved 3.6 flow.
        if (a.profile.tradeRole == 0 && rt.sellPhase != 0) {
            if (HandleIndependentAutoSell(a, now)) return;
        }
        if (a.profile.tradeRole == 0 && !a.tradeHeld &&
            (s.validMask & ValidBagSpace) &&
            ShouldAutoSell(tradeEnabled_, a.profile.tradeRole, a.profile.enableSell, s.freeBagSpace)) {
            MainMacroSellConfig independentCfg{};
            std::wstring sellReason;
            if (!IndependentSellConfigured(a, independentCfg, sellReason)) {
                rt.status = L"TÚI FULL • AUTO BÁN NONE chưa sẵn sàng: " + sellReason;
                return;
            }
            BeginIndependentAutoSell(a, now);
            if (HandleIndependentAutoSell(a, now)) return;
        }

        if (rt.trainRecoveryPhase != 0) {
            if (HandleTrainRecovery(a, now)) return;
        }

        // Steady train after the ONE initial AutoFight startup. Periodic coordinate and
        // AutoFight 60s checks are intentionally disabled. FILTER V4 is the low-priority
        // background work for enabled CONs; death/full/trade ownership remains above it.
        if (rt.trainPositionMonitorArmed) {
            const int scanSlot = ChildScanSlot(a);
            if (scanSlot > 0 && image_scan_test::IsChildAutoFilterEnabled(scanSlot)) {
                if ((s.validMask & ValidBagSpace) == 0) {
                    rt.status = L"SCAN VK • chờ BagSpace authoritative";
                    return;
                }
                const bool bagFull = s.freeBagSpace == 0;
                const auto scan = image_scan_test::TickAutoFilter(MakeImageScanTarget(a), scanSlot, bagFull);
                rt.autoLootBlockedThisTick = scan.ownsInput;
                if (scan.ownsInput) {
                    if (!scan.status.empty()) rt.status = scan.status;
                    else rt.status = L"Train ổn định • SCAN VK đang giữ input";
                    if (scan.state == image_scan_test::AutoFilterState::Error)
                        rt.status += L" • train vẫn tiếp tục";
                    return;
                }
            }
            if (scanSlot > 0 && image_scan_test::IsChildAutoFilterEnabled(scanSlot))
                rt.status = L"Train ổn định • SCAN VK nền";
            else
                rt.status = L"Train ổn định • periodic tọa/AutoFight 60s OFF";
            return;
        }

        // V3.0 SHORTCUT FIRST: the initial AUTO TRAIN route must use the same shortcut selector
        // already used by sell-return/recovery. Only when no shortcut applies may normal Decide()
        // issue a direct route to the final training target.
        if (HandleShortcutTravel(a, now, a.profile.target)) return;

        State logic{};
        logic.valid = true;
        logic.mapReady = true;
        logic.waitingMap = false;
        logic.mapID = s.mapID;
        logic.x = s.x;
        logic.y = s.y;
        logic.riding = s.riding != 0;
        logic.autoPathing = s.autoPathing != 0;
        Target target{a.profile.target.mapID, a.profile.target.x, a.profile.target.y, a.profile.tolerance};
        const bool atTarget = AtTarget(logic, target);
        if (!atTarget) {
            rt.trainPositionMonitorArmed = false;
            rt.lastTrainPositionCheckTick = 0;
            rt.lastAutoFightCheckTick = 0;
            if (rt.wasAtTarget) {
                rt.fightPhase = 0;
                rt.fightAttempts = 0;
                rt.fightRetryWaitTick = 0;
            }
            rt.wasAtTarget = false;
        }

        const Action action = Decide(logic, target);
        if (action == Action::Hold) {
            rt.lastAction = Action::Hold;
            (void)CompleteToolOwnedRoute(rt, true, s.autoPathing != 0, s.riding != 0);
            if (!rt.wasAtTarget) {
                rt.fightPhase = 0;
                rt.fightAttempts = 0;
                rt.fightRetryWaitTick = 0;
                LogAccount(a, L"Đã tới bãi và ổn định.");
            }
            rt.wasAtTarget = true;
            if (HandleFightClicks(a, now)) return;
            rt.status = L"Đúng bãi • giám sát tọa độ";
            return;
        }
        if (action == Action::Wait) {
            if (s.autoPathing) rt.status = L"Đang AutoPath tới bãi";
            return;
        }
        SendDecision(a, action, a.profile.target, L"bãi train");
    }

    void RefreshAccountIdentityIfNeeded(Account& a) {
        if (!a.snapshotValid) return;
        const std::wstring oldSection = a.profile.section;
        const std::wstring newSection = ProfileSection(a.snapshot, a.game.pid);
        if (oldSection == newSection) return;
        const int runtimeRole = a.profile.tradeRole;
        const bool runtimeSell = a.profile.enableSell;
        AccountProfile persistent = LoadProfile(newSection);
        const bool persistentHasData = persistent.displayParty != 0 ||
            !persistent.selectedSpot.empty() || persistent.target.valid || persistent.enableSell ||
            std::any_of(persistent.points.begin(), persistent.points.end(), [](const ClickPoint& p){ return p.valid; });
        if (!persistentHasData) {
            persistent = a.profile;
            persistent.section = newSection;
        } else {
            if (persistent.displayParty == 0 && a.profile.displayParty != 0) persistent.displayParty = a.profile.displayParty;
            if (persistent.selectedSpot.empty() && !a.profile.selectedSpot.empty()) persistent.selectedSpot = a.profile.selectedSpot;
            if (!persistent.target.valid && a.profile.target.valid) persistent.target = a.profile.target;
            for (std::size_t i = 0; i < persistent.points.size(); ++i) {
                if (!persistent.points[i].valid && a.profile.points[i].valid) persistent.points[i] = a.profile.points[i];
            }
        }
        persistent.section = newSection;
        persistent.tradeRole = runtimeRole;
        persistent.enableSell = runtimeRole == kMainTradeRole ? runtimeSell : false;
        if (runtimeRole == kMainTradeRole) { persistent.displayParty = 0; persistent.partyKey = false; }
        SaveProfile(persistent);
        a.profile = persistent;
        const std::wstring designated = LoadDesignatedMainIdentity();
        if (runtimeRole == kMainTradeRole &&
            (designated == oldSection || designated == PidProfileSection(a.game.pid)))
            SaveDesignatedMainIdentity(newSection);
        MigrateLegacySpot(a.profile);
        a.displayName = DisplayName(a.snapshot, a.game.pid);
    }

    void UpdateSelectedLive() {
        Account* a = SelectedAccount();
        if (!a) return;
        if (a->profile.tradeRole == kMainTradeRole) {
            SetText(live_, L"STATE: " + MainVisibleStatus(*a));
            return;
        }
        if (!a->snapshotValid) {
            SetText(live_, L"STATE: chưa đọc được snapshot");
            return;
        }
        const Snapshot& s = a->snapshot;
        std::wstring text = L"STATE " + AccountTag(*a) + L" • " + TradeRoleLabel(a->profile.tradeRole) + L" • M" + std::to_wstring(s.mapID) + L" • " +
                            std::to_wstring(s.x) + L"," + std::to_wstring(s.y) +
                            L" • Ngựa " + (s.riding ? L"ON" : L"OFF") +
                            L" • Path " + (s.autoPathing ? L"ON" : L"OFF");
        if (s.validMask & ValidLifeState) text += L" • " + std::wstring(s.dead ? L"CHẾT" : L"SỐNG");
        if (s.validMask & ValidAutoFight) text += L" • Đánh quái " + std::wstring(s.autoFight ? L"ON" : L"OFF");
        if (s.validMask & ValidBagSpace) text += L" • Túi trống " + std::to_wstring(s.freeBagSpace);
        if (a->profile.enableConfirm) text += (s.mapID == kLauLanMapId ? L" • XN LL watchdog ON" : L" • XN LL idle");
        if (globalPaused_) text += L" • F4 PAUSE";
        if (a->runtime.clientFreezeActive) text += L" • FREEZE ACTION";
        if (!s.mapReady || s.waitingChangeMap) text = L"STATE " + AccountTag(*a) + L" • ĐANG CHUYỂN MAP • FREEZE ACTION";
        SetText(live_, text);
    }

    void RefreshLicenseTitle() {
        const long long remaining = ThanLongLicenseRemainingSeconds();
        const long long bucket = remaining < 0 ? -1 : remaining / 60;
        if (bucket == lastLicenseTitleBucket_) return;
        lastLicenseTitleBucket_ = bucket;
        const std::wstring title = std::wstring(kTitle) + L" • KEY: " + FormatLicenseRemaining(remaining);
        SetWindowTextW(hwnd_, title.c_str());
    }

    void Tick() {
        RefreshLicenseTitle();
        if (!ThanLongLicenseActionAllowed()) {
            if (!licenseCoreHoldLatched_) {
                Log(L"LICENSE CORE GUARD • scheduler fail-closed: khóa mọi action mutating bên trong tool");
                licenseCoreHoldLatched_ = true;
            }
            for (auto& a : accounts_) {
                if (a->runtime.running) {
                    a->runtime.running = false;
                    a->runtime.status = L"LICENSE LOCK • chờ xác minh lại";
                }
            }
            return;
        }
        licenseCoreHoldLatched_ = false;

        // Snapshots + movement-observation run first, then v0.6.1 services semantic
        // background priorities before coordinate-based trade clicks.
        for (auto& item : accounts_) if (item) item->runtime.autoLootBlockedThisTick = false;
        std::vector<bool> snapshotReady(accounts_.size(), false);
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            const bool selected = static_cast<int>(i) == SelectedIndex();
            if (!a.runtime.running && !selected) continue;
            std::wstring error;
            const DWORD now = GetTickCount();
            if (!ReadSnapshot(a, error, a.runtime.running ? 700 : 900)) {
                if (a.runtime.running) MarkReadStateFailure(a, error, now);
                else a.runtime.status = L"Mất state/bridge";
                continue;
            }
            snapshotReady[i] = true;
            RefreshAccountIdentityIfNeeded(a);
            // Read-only movement observation MUST run even when BĐPT World Flow holds
            // this account. This feeds the Lâu Lan 3s stall watchdog before any priority click.
            const std::uint32_t observeNeed = ValidMap | ValidPosition | ValidAutoPath;
            if ((a.snapshot.validMask & observeNeed) == observeNeed && a.snapshot.mapReady && !a.snapshot.waitingChangeMap) {
                ObserveMovement(a, GetTickCount());
            }
            // Telegram v0.6 observer: same authoritative snapshot; network stays on worker thread.
            if (a.runtime.running) ObserveTelegramAccountState(a, GetTickCount());
        }

        // GOLD HISTORY is independent from TELE LOG and samples MAIN read-only state even when Telegram is OFF.
        TickGoldHistory(GetTickCount());

        // v0.6.1.9 priorities remain scoped per account, not global input barriers.
        // For each PID preserve local safety order P1 XN -> P2 revive -> P3 AUTO, while
        // unrelated windows never wait merely because another PID has a higher-priority action.
        if (!globalPaused_) {
            for (std::size_t i = 0; i < accounts_.size(); ++i) {
                if (i >= snapshotReady.size() || !snapshotReady[i]) continue;
                Account& a = *accounts_[i];
                if (!a.runtime.running || RecorderBlocksAccount(a)) continue;
                const DWORD priorityNow = GetTickCount();
                if (partyBuildModeActive_) continue; // exclusive: no revive/XN/P3 automation while KEY builds PT.
                if (AccountUiRecoveryActive(a) || a.runtime.postReviveRecoveryPending) continue;
                if (PriorityLauLanGateConfirmClick(a, priorityNow)) continue;
                if (PriorityReviveClick(a, priorityNow)) continue;
                if (!partyBuildModeActive_ && a.runtime.priorityAutoRequestSlot != ClickSlot::None &&
                    (!gatherModeActive_ || (gatherModeActive_ && IsGatherPid(a.game.pid)))) {
                    // Gather is a temporary common train spot, so P3 AUTO->Đánh quái is
                    // allowed only for gather-owned PIDs after they reach the shared target.
                    (void)PriorityAutoClick(a);
                }
            }
        }

        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            Account& a = *accounts_[i];
            const bool selected = static_cast<int>(i) == SelectedIndex();
            if (!a.runtime.running && !selected) {
                UpdateAccountRow(static_cast<int>(i), a);
                continue;
            }
            if (!snapshotReady[i]) {
                UpdateAccountRow(static_cast<int>(i), a);
                continue;
            }
            const DWORD now = GetTickCount();
            if (a.runtime.running) {
                // Hidden actions are per-client. REC pauses only the window(s) being
                // captured; unrelated accounts keep their normal FSM ticks. F4 remains global.
                if (RecorderBlocksAccount(a)) {
                    a.runtime.status = L"BĐPT RECORDING CỤC BỘ • chỉ acc này tạm giữ để ghi thao tác tay";
                } else if (HoldUntilClientStable(a, now)) {
                    UpdateAccountRow(static_cast<int>(i), a);
                    continue;
                } else if (!globalPaused_) {
                    if (a.runtime.postReviveRecoveryPending) {
                        const bool tradeOwnsRecovery=tradeTxn_.phase!=TradePhase::Idle&&(a.game.pid==tradeTxn_.mainPid||a.game.pid==tradeTxn_.childPid);
                        if(tradeOwnsRecovery){a.runtime.status=L"POST-REVIVE RECOVERY • chờ TRADE cleanup hoàn tất";}
                        else{(void)StartAccountUiRecovery(a,AccountUiRecoveryPlan::PostRevive,L"ĐẦU THAI PASS sau TRADE cleanup",now);if(AccountUiRecoveryActive(a))(void)TickAccountUiRecovery(a,now);}
                    } else if (AccountUiRecoveryActive(a)) {
                        (void)TickAccountUiRecovery(a,now);
                    } else if (!partyBuildModeActive_ && HandleAutoPathFightInvariant(a, now)) {
                        // Hard invariant owns this tick for normal/gather flows; PartyBuild is exclusive.
                    } else if (partyBuildModeActive_) {
                        a.runtime.status = L"AUTO PT • KEY đang được PartyBuild điều phối";
                    } else if (gatherModeActive_) {
                        TickGatherAccount(a, now);
                    } else if (a.tradeHeld) {
                        // World Flow HOLD never owns the life observer. P2 may have invoked
                        // Đầu thai above; keep advancing DEAD -> revive phases -> ALIVE cold restart
                        // here, without releasing FIFO/World Flow ownership.
                        if (!HandleDeath(a, now)) {
                            a.runtime.status = a.runtime.tradeTravelReady
                                ? L"BĐPT HOLD • đã tới TỌA GD • chờ đúng FIFO • LIFE/XN vẫn check"
                                : L"BĐPT WORLD FLOW • đang đi TỌA GD • LIFE/XN vẫn check ưu tiên";
                        }
                    } else TickAccount(a);
                } else a.runtime.status = L"TẠM DỪNG F4 • BĐPT không cấp tick cho acc";
            }
            UpdateAccountRow(static_cast<int>(i), a);
        }
        if (!globalPaused_ && partyBuildModeActive_) TickPartyBuild(GetTickCount());
        if (!globalPaused_ && !gatherModeActive_ && !partyBuildModeActive_) {
            Account* activeMain = AccountByPid(tradeTxn_.mainPid);
            Account* activeChild = AccountByPid(tradeTxn_.childPid);
            const bool tradeRecorderBlocked = (activeMain && RecorderBlocksAccount(*activeMain)) ||
                                              (activeChild && RecorderBlocksAccount(*activeChild));
            const bool tradeRecoveryBlocked=(activeMain&&AccountUiRecoveryActive(*activeMain))||(activeChild&&AccountUiRecoveryActive(*activeChild));
            if (!tradeRecorderBlocked&&!tradeRecoveryBlocked) TickTradeCoordinator(GetTickCount());
            else if(tradeRecoveryBlocked) SetTradeStatus(L"HARD UI RECOVERY • giữ workflow GD cho tới khi UI sạch");
            else SetTradeStatus(L"RECORDING CỤC BỘ • giữ workflow GD liên quan; acc khác vẫn chạy");
        }
        // 10.2 AUTO NHẶT is deliberately last among gameplay mutations. It re-checks
        // authoritative AutoFight/bag state and critical ownership after the coordinator tick.
        TickAutoLootBackground(GetTickCount());

        // Coordinator can change FIFO/WorldFlow state after the first observation.
        for (std::size_t i = 0; i < accounts_.size(); ++i) {
            if (i < snapshotReady.size() && snapshotReady[i] && accounts_[i]->runtime.running)
                ObserveTelegramAccountState(*accounts_[i], GetTickCount());
        }
        TickTelegramSchedules(GetTickCount());
        UpdateSelectedLive();
    }

    void OnListNotification(const NMHDR* hdr) {
        if (!hdr) return;
        if (hdr->hwndFrom == mainTab_ && hdr->code == TCN_SELCHANGE) {
            const int index = TabCtrl_GetCurSel(mainTab_);
            SwitchMainTab(index);
            return;
        }
        if (clientList_ && hdr->hwndFrom == ListView_GetHeader(clientList_) && hdr->code == HDN_ITEMCLICKW) {
            const auto* h = reinterpret_cast<const NMHEADERW*>(hdr);
            if (h->iItem == 3) TogglePidColumn();
            return;
        }
        if (hdr->hwndFrom == clientList_ && hdr->code == LVN_LINKCLICK) {
            const auto* link = reinterpret_cast<const NMLVLINK*>(hdr);
            CheckPartyGroup(link->iSubItem);
            return;
        }
        if (hdr->hwndFrom == clientList_ && hdr->code == LVN_ITEMCHANGED) {
            const auto* n = reinterpret_cast<const NMLISTVIEW*>(hdr);
            if ((n->uChanged & LVIF_STATE) != 0 && (n->uNewState & LVIS_SELECTED) != 0) {
                PersistSelectedEditorSafeBeforeSwitch(n->iItem);
                LoadSelectedProfileToUi();
            }
            return;
        }
    }

    void ToggleGlobalPause() {
        globalPaused_ = !globalPaused_;
        if (globalPaused_) {
            for (auto& item : accounts_) {
                Account& a = *item;
                if (!a.runtime.running) continue;
                if (a.bridge.Attached() && !a.runtime.clientFreezeActive) {
                    Response r{}; std::wstring ignored;
                    (void)a.bridge.Call(Command::StopPath, 0, 0, 0, r, ignored, 700);
                }
                a.runtime.status = L"TẠM DỪNG F4";
            }
            Log(L"F4 → TẠM DỪNG toàn bộ acc đang RUN; StopPath đã gửi, không tự đổi combat.");
            const std::wstring pauseMsg = L"⏸ F4 PAUSE\nThời gian: " + LocalDateTimeText();
            AddLocalReport(L"F4 PAUSE", L"-", pauseMsg);
            if (false)
                (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, pauseMsg, L"F4 PAUSE", L"-");
        } else {
            for (auto& item : accounts_) if (item->runtime.running) item->runtime.status = L"Tiếp tục sau F4";
            Log(L"F4 → TIẾP TỤC toàn bộ acc đang RUN.");
            const std::wstring resumeMsg = L"▶️ F4 RESUME\nThời gian: " + LocalDateTimeText();
            AddLocalReport(L"F4 RESUME", L"-", resumeMsg);
            if (false)
                (void)QueueTelegramRequest(telegram_notify::TaskKind::SendMessage, resumeMsg, L"F4 RESUME", L"-");
        }
    }

    void PersistSelectedEditorSafeBeforeSwitch(int newIndex) {
        // LVN_ITEMCHANGED arrives after selection state changes, so we cannot reliably know the old row here.
        // All meaningful editor mutations are persisted immediately on their own events/capture/save.
        (void)newIndex;
    }

    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_CREATE:
                BuildUi();
                return 0;
            case kTelegramResultMessage:
                HandleTelegramWorkerResult(lp);
                return 0;
            case WM_NOTIFY:
                OnListNotification(reinterpret_cast<const NMHDR*>(lp));
                return 0;
            case WM_COMMAND:
                switch (LOWORD(wp)) {
                    case IDC_CLEAR_LOG:
                        if (HIWORD(wp) == BN_CLICKED && log_) SetWindowTextW(log_, L"");
                        break;
                    case IDC_LOG_ENABLED:
                        if (HIWORD(wp) == BN_CLICKED) ToggleMainLogEnabled();
                        break;
                    case IDC_TG_SHOW_TOKEN:
                        if (HIWORD(wp) == BN_CLICKED) ToggleTelegramTokenVisible();
                        break;
                    case IDC_TG_SAVE:
                        if (HIWORD(wp) == BN_CLICKED) (void)PersistTelegramSettingsFromUi(true);
                        break;
                    case IDC_TG_TEST_BOT:
                        if (HIWORD(wp) == BN_CLICKED) TelegramTestBot();
                        break;
                    case IDC_TG_DISCOVER_CHAT:
                        if (HIWORD(wp) == BN_CLICKED) TelegramDiscoverChatId();
                        break;
                    case IDC_TG_SEND_TEST:
                        if (HIWORD(wp) == BN_CLICKED) TelegramSendTest();
                        break;
                    case IDC_TG_SEND_SUMMARY:
                        if (HIWORD(wp) == BN_CLICKED) TelegramSendSummaryNow();
                        break;
                    case IDC_TG_CLEAR_LOG:
                        if (HIWORD(wp) == BN_CLICKED) ClearTelegramLog();
                        break;
                    case IDC_TG_COPY_LOG:
                        if (HIWORD(wp) == BN_CLICKED) CopyTelegramLog();
                        break;
                    case IDC_TG_EXPORT_LOG:
                        if (HIWORD(wp) == BN_CLICKED) ExportTelegramLog();
                        break;
                    case IDC_TG_LOG_ENABLED:
                        if (HIWORD(wp) == BN_CLICKED) ToggleTelegramLogEnabled();
                        break;
                    case IDC_EXPORT_LOG:
                        if (HIWORD(wp) == BN_CLICKED) ExportMainLog();
                        break;
                    case IDC_TG_ENABLED:
                    case IDC_TG_NOTIFY_DEATH:
                    case IDC_TG_NOTIFY_REVIVE:
                    case IDC_TG_NOTIFY_SELL_COMPLETE:
                    case IDC_TG_NOTIFY_SELL_SUMMARY:
                    case IDC_TG_NOTIFY_TRADE:
                    case IDC_TG_NOTIFY_FREEZE:
                    case IDC_TG_NOTIFY_FIFO:
                    case IDC_TG_NOTIFY_LAULAN:
                    case IDC_TG_NOTIFY_WORLDFLOW_TIMEOUT:
                    case IDC_TG_NOTIFY_TOOL_STATE:
                    case IDC_TG_NOTIFY_SESSION_SUMMARY:
                    case IDC_TG_NOTIFY_FUN_ALERTS:
                    case IDC_TG_MONEY_1M:
                    case IDC_TG_MONEY_5M:
                    case IDC_TG_MONEY_60M:
                    case IDC_TG_MONEY_6H:
                    case IDC_TG_MONEY_24H:
                    case IDC_TG_INTERVAL_ENABLED:
                    case IDC_TG_DAILY_ENABLED:
                        if (HIWORD(wp) == BN_CLICKED) (void)PersistTelegramSettingsFromUi(false);
                        break;
                    case IDC_TG_TOKEN:
                    case IDC_TG_CHAT_ID:
                    case IDC_TG_INTERVAL_MINUTES:
                    case IDC_TG_DAILY_TIME1:
                    case IDC_TG_DAILY_TIME2:
                    case IDC_TG_DAILY_TIME3:
                    case IDC_TG_DAILY_TIME4:
                    case IDC_TG_WORLDFLOW_TIMEOUT_SEC:
                        if (HIWORD(wp) == EN_KILLFOCUS) (void)PersistTelegramSettingsFromUi(false);
                        break;
                    case IDC_SCAN:
                        ScanClients();
                        break;
                    case IDC_TEST_IMAGE_SCAN:
                        RequestImageScanTest();
                        break;
                    case IDC_FILTER_MODE2:
                        if(HIWORD(wp)==BN_CLICKED) ToggleFilterMode2();
                        break;
                    case IDC_SCAN_BAG_SEMANTIC:
                        if(HIWORD(wp)==BN_CLICKED) OpenBagScanWindow();
                        break;
                    case IDC_AUTO_LOOT_TOGGLE:
                        if (HIWORD(wp) == BN_CLICKED) ToggleAutoLootDeveloper();
                        break;
                    case IDC_AUTO_LOOT_INTERVAL:
                        if (HIWORD(wp) == EN_KILLFOCUS) PersistAutoLootIntervalFromUi();
                        break;
                    case IDC_LOOT_SCAN:
                        if (HIWORD(wp) == BN_CLICKED) RunLootProbe(false);
                        break;
                    case IDC_LOOT_PICK:
                        if (HIWORD(wp) == BN_CLICKED) RunLootProbe(true);
                        break;
                    case IDC_SELECT_ALL_ACCOUNTS:
                        if (HIWORD(wp) == BN_CLICKED) SetAllAccountChecks(true);
                        break;
                    case IDC_CLEAR_ALL_ACCOUNTS:
                        if (HIWORD(wp) == BN_CLICKED) SetAllAccountChecks(false);
                        break;
                    case IDC_ASSIGN_PARTY:
                        if (HIWORD(wp) == BN_CLICKED) AssignPartyToChecked();
                        break;
                    case IDC_APPLY_SPOT_PARTY:
                        if (HIWORD(wp) == BN_CLICKED) ApplySpotToParty();
                        break;
                    case IDC_APPLY_SPOT_ALL_CON:
                        if (HIWORD(wp) == BN_CLICKED) ApplySpotToAllCon();
                        break;
                    case IDC_GATHER_CAPTURE:
                        if (HIWORD(wp) == BN_CLICKED) CaptureGatherTarget();
                        break;
                    case IDC_GATHER_TOGGLE:
                        if (HIWORD(wp) == BN_CLICKED) ToggleGatherMode();
                        break;
                    case IDC_SET_PARTY_KEY:
                        if (HIWORD(wp) == BN_CLICKED) SetPartyKeyForSelected();
                        break;
                    case IDC_PARTY_BUILD_TOGGLE:
                        if (HIWORD(wp) == BN_CLICKED) TogglePartyBuildMode();
                        break;
                    case IDC_PB_CAPTURE_CLICK1:
                        if (HIWORD(wp) == BN_CLICKED) BeginPartyBuildCapture(0);
                        break;
                    case IDC_PB_CAPTURE_CLICK2:
                        if (HIWORD(wp) == BN_CLICKED) BeginPartyBuildCapture(1);
                        break;
                    case IDC_PB_CAPTURE_FACE:
                        if (HIWORD(wp) == BN_CLICKED) BeginPartyBuildCapture(2);
                        break;
                    case IDC_PB_TEST_CLICK1:
                        if (HIWORD(wp) == BN_CLICKED) TestPartyBuildClick(0);
                        break;
                    case IDC_PB_TEST_CLICK2:
                        if (HIWORD(wp) == BN_CLICKED) TestPartyBuildClick(1);
                        break;
                    case IDC_PB_TEST_FACE:
                        if (HIWORD(wp) == BN_CLICKED) TestPartyBuildClick(2);
                        break;
                    case IDC_PB_DELAY_CLICK1:
                    case IDC_PB_DELAY_CLICK2:
                    case IDC_PB_DELAY_FACE:
                    case IDC_PB_TARGET_RETRY:
                    case IDC_PB_INVITE_RETRY:
                        if (HIWORD(wp) == EN_KILLFOCUS) PersistPartyBuildSettingsFromUi();
                        break;
                    case IDC_TRADE_ROLE:
                        if (HIWORD(wp) == CBN_SELCHANGE) ApplySelectedTradeRole();
                        break;
                    case IDC_CONSOLIDATE_TOGGLE:
                        if (HIWORD(wp) == BN_CLICKED) ToggleConsolidationMode();
                        break;
                    case IDC_COMPACT_TOGGLE:
                        if (HIWORD(wp) == BN_CLICKED) ToggleCompactMode();
                        break;
                    case IDC_SELL_SEQUENCE:
                        ToggleMainSellSettings();
                        break;
                    case IDC_MAIN_TRADE_SEQUENCE:
                        OpenTradeSequenceEditor(1);
                        break;
                    case IDC_CHILD_TRADE_SEQUENCE:
                        OpenTradeSequenceEditor(2);
                        break;
                    case IDC_TRADE_RENDEZVOUS_CAPTURE:
                        CaptureTradeRendezvous();
                        break;
                    case IDC_COPY_CLICKS:
                        CopyClicksFromAnotherAccount();
                        break;
                   case IDC_START_CHECKED:
                        StartChecked();
                        break;
                    case IDC_STOP_CHECKED:
                        StopChecked();
                        break;
                    case IDC_SAVE_TARGET:
                        SaveTargetForSelected();
                        break;
                    case IDC_DELETE_SPOT:
                        DeleteSelectedSharedSpot();
                        break;
                    case IDC_SPOT_COMBO:
                        if (HIWORD(wp) == CBN_SELCHANGE) SelectSharedSpotForAccount();
                        break;
                    case IDC_CAPTURE_AUTO:
                        BeginCapture(ClickSlot::AutoMenu);
                        break;
                    case IDC_CAPTURE_ATTACK:
                        BeginCapture(ClickSlot::Attack);
                        break;
                    case IDC_CAPTURE_STOP_AUTO_2:
                        BeginCapture(ClickSlot::StopAuto2);
                        break;
                    case IDC_TEST_AUTO:
                        TestClick(ClickSlot::AutoMenu);
                        break;
                    case IDC_TEST_ATTACK:
                        TestClick(ClickSlot::Attack);
                        break;
                    case IDC_TEST_STOP_AUTO_2:
                        TestClick(ClickSlot::StopAuto2);
                        break;
                    case IDC_ENABLE_SHORTCUT:
                        if (HIWORD(wp) == BN_CLICKED) {
                            shortcutSettings_.enabled = SendMessageW(enableShortcut_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            SaveShortcutSettings(shortcutSettings_);
                            Log(std::wstring(L"ĐƯỜNG TẮT: ") + (shortcutSettings_.enabled ? L"BẬT" : L"TẮT"));
                            if (shortcutSettings_.enabled) OpenShortcutSettingsWindow();
                            else if (shortcutWindow_ && IsWindow(shortcutWindow_)) ShowWindow(shortcutWindow_, SW_HIDE);
                        }
                        break;
                    case IDC_SHORTCUT_SETTINGS:
                        if (HIWORD(wp) == BN_CLICKED) OpenShortcutSettingsWindow();
                        break;
                    case IDC_ENABLE_REVIVE:
                    case IDC_ENABLE_CONFIRM:
                    case IDC_ENABLE_FIGHT:
                                if (HIWORD(wp) == BN_CLICKED) PersistSelectedEditor();
                        break;
                    case IDC_TOLERANCE:
                        if (HIWORD(wp) == EN_KILLFOCUS) PersistSelectedEditor();
                        break;
                }
                return 0;
            case WM_HOTKEY:
                if (static_cast<int>(wp) == kCaptureHotkeyId) {
                    if (mainMacroSellCaptureActive_) { CaptureMainMacroSellF8(); return 0; }
                    CaptureHotkeyPoint();
                    return 0;
                }
                if (static_cast<int>(wp) == kPauseHotkeyId) {
                    ToggleGlobalPause();
                    return 0;
                }
                break;
            case WM_TIMER:
                if (wp == kRecordTimer) { PollRecorder(); return 0; }
                if (wp == kMainMacroSellTimer) { if (!globalPaused_) TickActiveMainMacroSell(); return 0; }
                if (wp == kTimer) Tick();
                return 0;
            case WM_CLOSE:
                if (recorderMode_ != RecorderMode::None) StopRecorder(true);
                DestroyWindow(hwnd_);
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd_, kMainMacroSellTimer); activeMainMacroSellPid_=0;
                if (mainSellSettingsWindow_ && IsWindow(mainSellSettingsWindow_)) DestroyWindow(mainSellSettingsWindow_);
                if (recorderMode_ != RecorderMode::None) StopRecorder(false);
                // Auto-save every persistent input before exit. Captures already save
                // immediately; this final pass also commits the currently edited macro row.
                PersistSelectedEditor();
                SaveSharedSellNpcPositions(sellNpcPositions_);
                for (auto& a : accounts_) SaveProfile(a->profile);
                (void)PersistTelegramSettingsFromUi(false);
                telegramWorker_.Stop();
                {
                    MSG pending{};
                    while (PeekMessageW(&pending, hwnd_, kTelegramResultMessage, kTelegramResultMessage, PM_REMOVE))
                        delete reinterpret_cast<telegram_notify::Result*>(pending.lParam);
                }
                FlushIni();
                UnregisterHotKey(hwnd_, kCaptureHotkeyId);
                UnregisterHotKey(hwnd_, kPauseHotkeyId);
                for (auto& a : accounts_) a->bridge.Close();
                if (shortcutDarkBrush_) { DeleteObject(shortcutDarkBrush_); shortcutDarkBrush_ = nullptr; }
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND clientList_ = nullptr;
    HWND selectAllButton_ = nullptr;
    HWND clearAllButton_ = nullptr;
    HWND partyCombo_ = nullptr;
    HWND assignPartyButton_ = nullptr;
    HWND gatherToggleButton_ = nullptr;
    HWND gatherLabel_ = nullptr;
    HWND setPartyKeyButton_ = nullptr;
    HWND partyBuildToggleButton_ = nullptr;
    bool pidExpanded_ = false;
    HWND selected_ = nullptr;
    HWND live_ = nullptr;
    HWND tradeRoleCombo_ = nullptr;
    HWND tradeEnable_ = nullptr;
    long long lastLicenseTitleBucket_ = -2;
    bool licenseCoreHoldLatched_ = false;
    HWND tradeStatus_ = nullptr;
    HWND tradeEditor_ = nullptr;
    HWND tradeSeqList_ = nullptr;
    HWND tradeSeqTarget_ = nullptr;
    HWND tradeSeqDesc_ = nullptr;
    HWND tradeSeqDelay_ = nullptr;
    HWND tradeSeqRepeat_ = nullptr;
    HWND tradeSeqGroupRepeat_ = nullptr;
    HWND tradeSeqAfterPutUp_ = nullptr;
    HWND tradeSeqAction_ = nullptr;
    HWND tradeSeqMinTime_ = nullptr;
    HWND tradeSeqPostCleanup_ = nullptr;
    HWND tradeRecordButton_ = nullptr;
    HWND tradeRecordStatus_ = nullptr;
    HWND targetName_ = nullptr;
    HWND spotCombo_ = nullptr;
    HWND targetText_ = nullptr;
    HWND tolerance_ = nullptr;
    HWND enableRevive_ = nullptr;
    HWND enableConfirm_ = nullptr;
    HWND enableShortcut_ = nullptr;
    HWND shortcutSettingsButton_ = nullptr;
    HWND enableFight_ = nullptr;
    HWND mainSellSettingsWindow_ = nullptr;
    HWND mainMacroSellXEdit_ = nullptr;
    HWND mainMacroSellYEdit_ = nullptr;
    HWND mainMacroSellDelayEdit_ = nullptr;
    HWND mainMacroSellPointLabel_ = nullptr;
    HWND mainMacroSellStatus_ = nullptr;
    DWORD mainMacroSellDialogPid_ = 0;
    MainMacroSellConfig mainMacroSellDialogConfig_{};
    MainMacroSellConfig mainMacroSellActiveConfig_{};
    bool mainMacroSellCaptureActive_ = false;
    DWORD mainMacroSellCapturePid_ = 0;
    DWORD activeMainMacroSellPid_ = 0;
    DWORD activeMainSellContextChildPid_ = 0;
    int activeMainSellContextPass_ = 0;
    MainCapacityPlan mainCapacityPlan_{};
    bool idleSellEpochDone_ = false;
    HWND logCaption_ = nullptr;
    HWND clearLogButton_ = nullptr;
    HWND sellSequenceButton_ = nullptr;
    HWND mainTradeSequenceButton_ = nullptr;
    HWND childTradeSequenceButton_ = nullptr;
    HWND tradeRendezvousCaptureButton_ = nullptr;
    HWND tradeRendezvousLabel_ = nullptr;
    HWND scanButton_ = nullptr;
    HWND startCheckedButton_ = nullptr;
    HWND stopCheckedButton_ = nullptr;
    HWND compactButton_ = nullptr;
    std::array<HWND, 5> pointLabels_{};
    HWND log_ = nullptr;
    HWND mainLogToggleButton_ = nullptr;
    bool mainLogEnabled_ = ReadIniInt(L"UiPerformance", L"MainLogEnabled", 0) != 0;
    HWND exportLogButton_ = nullptr;
    HWND imageScanButton_ = nullptr;
    HWND filterMode2Button_ = nullptr;
    HWND bagScanButton_ = nullptr;
    HWND bagScanWindow_ = nullptr;
    HWND bagScanItems_ = nullptr;
    HWND bagScanNames_ = nullptr;
    HWND bagScanNameEdit_ = nullptr;
    HWND bagScanStatus_ = nullptr;
    HWND autoLootGroup_ = nullptr;
    HWND autoLootToggleButton_ = nullptr;
    HWND autoLootIntervalLabel_ = nullptr;
    HWND autoLootIntervalEdit_ = nullptr;
    HWND lootProbeGroup_ = nullptr;
    HWND lootScanButton_ = nullptr;
    HWND lootPickButton_ = nullptr;
    HWND lootOutput_ = nullptr;
    bool autoLootEnabled_ = false;
    int autoLootIntervalMs_ = auto_loot_logic::kDefaultIntervalMs;
    std::vector<BagItemSnapshot> lastBagScanItems_{};
    EquipPointDb equipPointDb_{};
    bool equipPointDbReady_ = false;
    std::wstring equipPointDbError_{};
    std::vector<std::wstring> autoDropNames_{};
    HWND mainTab_ = nullptr;
    int mainTabIndex_ = 0;
    std::vector<HWND> telegramControls_{};
    std::vector<HWND> runtimeLogControls_{};
    std::vector<HWND> developerControls_{};
    std::vector<HWND> partyBuildDevControls_{};
    std::array<HWND, 3> partyBuildPointLabels_{};
    std::array<HWND, 3> partyBuildDelayEdits_{};
    HWND partyBuildTargetRetryEdit_ = nullptr;
    HWND partyBuildInviteRetryEdit_ = nullptr;
    int partyBuildCaptureIndex_ = -1;
    std::vector<std::pair<HWND, bool>> autoTabVisibility_{};
    std::vector<std::pair<HWND, bool>> compactVisibility_{};
    bool compactMode_ = false;

    // Global shortcut router/settings. It is opt-in and does not alter unrelated direct routes when disabled.
    ShortcutSettings shortcutSettings_ = LoadShortcutSettings();
    HWND shortcutWindow_ = nullptr;
    HWND shortcutTheme_ = nullptr;
    HWND shortcutSellerCombo_ = nullptr;
    HWND shortcutSellerCoordLabel_ = nullptr;
    std::array<HWND, 8> shortcutCoordEdits_{};
    std::array<HWND, 3> shortcutClickLabels_{};
    std::array<HWND, 3> shortcutClickTimeEdits_{};
    std::array<HWND, 3> shortcutClickDelayEdits_{};
    HWND shortcutPostTradeEnabled_ = nullptr;
    HWND shortcutPostTradePointLabel_ = nullptr;
    HWND shortcutPostTradeDelay_ = nullptr;
    HWND shortcutPostTradeRepeat_ = nullptr;
    int shortcutKunlunCaptureIndex_ = -1;
    bool shortcutPostTradeCapture_ = false;
    HBRUSH shortcutDarkBrush_ = nullptr;

    // Telegram observer/output subsystem; bound-gold reporting remains read-only.
    // It owns no game input, route, trade or consolidation scheduling state.
    HWND telegramEnabled_ = nullptr;
    HWND telegramToken_ = nullptr;
    HWND telegramShowToken_ = nullptr;
    HWND telegramChatId_ = nullptr;
    HWND telegramStatus_ = nullptr;
    HWND telegramLog_ = nullptr;
    HWND telegramLogToggleButton_ = nullptr;
    bool telegramLogEnabled_ = ReadIniInt(L"UiPerformance", L"TelegramLogEnabled", 0) != 0;
    HWND telegramNotifyDeath_ = nullptr;
    HWND telegramNotifyRevive_ = nullptr;
    HWND telegramNotifySellComplete_ = nullptr;
    HWND telegramNotifySellSummary_ = nullptr;
    HWND telegramNotifyTrade_ = nullptr;
    HWND telegramNotifyFreeze_ = nullptr;
    HWND telegramNotifyFifo_ = nullptr;
    HWND telegramNotifyLauLan_ = nullptr;
    HWND telegramNotifyWorldFlowTimeout_ = nullptr;
    HWND telegramNotifyToolState_ = nullptr;
    HWND telegramNotifySessionSummary_ = nullptr;
    HWND telegramNotifyFunAlerts_ = nullptr;
    std::array<HWND, 5> telegramMoneyMilestone_{};
    HWND telegramIntervalEnabled_ = nullptr;
    HWND telegramIntervalMinutes_ = nullptr;
    HWND telegramDailyEnabled_ = nullptr;
    std::array<HWND, 4> telegramDailyTime_{};
    HWND telegramWorldFlowTimeoutSec_ = nullptr;
    bool telegramTokenVisible_ = false;
    TelegramSettings telegramSettings_{};
    std::wstring telegramLoadWarning_{};
    telegram_notify::Worker telegramWorker_{};
    std::uint64_t telegramRequestCounter_ = 0;
    TelegramStats telegramStats_{};
    TelegramReportBaseline telegramReportBaseline_{};
    std::wstring telegramReportBaselineTime_{};
    std::map<DWORD, TelegramAccountWatch> telegramWatch_{};
    DWORD telegramLastIntervalSummaryTick_ = 0;
    std::array<std::uint64_t, 4> telegramLastDailyKeys_{};

    // Persistent MAIN-only bound-gold history. This store is intentionally separate from TELE LOG.
    gold_history::Store goldHistory_{};
    bool goldHistoryReady_ = false;
    std::wstring goldHistoryWarning_{};
    bool goldHistoryWarningLogged_ = false;
    DWORD goldHistoryRetryDueTick_ = 0;
    DWORD goldLastReadTick_ = 0;
    DWORD goldReadRetryDueTick_ = 0;
    DWORD goldMaintenanceTick_ = 0;
    bool currentMainGoldKnown_ = false;
    gold_history::Sample currentMainGoldSample_{};
    bool sessionMainGoldBaselineKnown_ = false;
    std::wstring sessionMainGoldIdentity_{};
    std::int64_t sessionMainGoldBaselineRaw_ = 0;
    std::int64_t sessionMainGoldBaselineUnix_ = 0;

    std::vector<std::unique_ptr<Account>> accounts_;
    std::vector<TargetProfile> spots_;
    std::array<SellNpcPosition, kSellNpcs.size()> sellNpcPositions_ = LoadSharedSellNpcPositions();
    ClickSlot captureSlot_ = ClickSlot::None;
    DWORD capturePid_ = 0;
    int captureTradeSequenceIndex_ = -1;
    int captureTradeSequenceMode_ = 0;
    int captureTradeSequenceMainRef_ = -1;
    bool globalPaused_ = false;
    bool gatherModeActive_ = false;
    TargetProfile gatherTarget_ = LoadGatherTarget();
    std::vector<DWORD> gatherPids_{};
    bool partyBuildModeActive_ = false;
    PartyBuildSettings partyBuildSettings_ = LoadPartyBuildSettings();
    std::vector<PartyBuildSession> partyBuildSessions_{};
    std::vector<std::wstring> partyBuildPreflightSummary_{};

    std::vector<TradeSequenceStep> mainTradeSequence_{};
    std::vector<TradeSequenceStep> childTradeSequence_{}; // v0.2.7 one GLOBAL workflow used by whichever CON is active.
    std::vector<DWORD> tradeTravelPids_{}; // Max 4 admitted FULL CON: travelling + arrived + active.
    std::vector<DWORD> tradeQueuePids_{};  // Arrived-only FIFO; ticket assigned at TỌA GD.
    std::uint64_t tradeWorkflowEntryCounter_ = 0;
    std::vector<TradeSequenceStep> legacyChildTradeTemplate_{};
    bool sharedChildTradeMigrationDone_ = false;
    int tradeEditorMode_ = 0; // 1=MAIN shared sequence, 2=GLOBAL ACC CON workflow; selected CON is capture/test donor only.
    DWORD tradeEditorChildPid_ = 0;
    struct TradeTxn {
        TradePhase phase = TradePhase::Idle;
        DWORD mainPid = 0; DWORD childPid = 0; int childSlot = 0;
        DWORD cooldownUntil = 0;
        DWORD targetStartedTick = 0;
        DWORD targetRetryTick = 0;
        DWORD targetLastSelectTick = 0;
        int targetAttempts = 0;
        TradePhase resumeAfterSell = TradePhase::TargetMain;
        std::size_t sequenceIndex = 0;
        int sequenceRepeatDone = 0;
        int sequenceGroupRepeatDone = 0;
        int sequencePass = 1;
        DWORD sequenceDueTick = 0;
        bool sequenceAfterPutUpPending = false;
        DWORD sequenceAfterPutUpStartedTick = 0;
        DWORD sequenceAfterPutUpNextTryTick = 0;
        // After the semantic PUT-UP callback has succeeded, the Bridge/ReadState can
        // remain in clientFreezeActive briefly while the game settles. Hold the trade
        // sequence here instead of treating the next raw macro row as a hard failure.
        bool sequencePostPutUpRecoveryPending = false;
        DWORD sequencePostPutUpRecoveryStartedTick = 0;
        // UI DIRECT row runtime. No Sleep: minTime is a not-before timestamp; then retry every 50ms,
        // with the same 2000ms maximum retry window as ĐẶT LÊN.
        DWORD sequenceUiRowStartTick = 0;
        DWORD sequenceUiInvokeStartedTick = 0;
        DWORD sequenceUiNextTryTick = 0;
        std::size_t sequenceUiRowIndex = static_cast<std::size_t>(-1);
        int cleanupStage = 0;
        DWORD cleanupDueTick = 0;
        DWORD cleanupStartedTick = 0;
        bool cleanupSweepChanged = false;
        bool cleanupSweepUnresolved = false;
        int cleanupSweepCount = 0;
        int cleanupCleanSweepStreak = 0;
        bool cleanupAbortPending = false;
        std::wstring cleanupAbortReason{};
        ChildTradePlan childPlan{};
        bool sequenceTelemetryCounted = false; // one completed saved macro = one full x9 pass.
        bool postTradeClickCompleted = false;
        int postTradeClickTarget = 0; // 0=MAIN, 1=active CON, 2=done after final delay.
        int postTradeClickRepeatDone = 0;
        DWORD postTradeClickDueTick = 0;
        bool postTradeClickSkipReported = false;
    } tradeTxn_{};
    bool tradeEnabled_ = true;
    TargetProfile tradeRendezvous_{};
    int tradeRendezvousTolerance_ = kPreciseWorldTolerance;
    RecorderMode recorderMode_ = RecorderMode::None;
    DWORD recorderPrimaryPid_ = 0;
    bool recorderMouseDown_ = false;
    std::vector<RecordedClick> recorderClicks_{};
    std::vector<TradeSequenceStep> tradeClipboard_{};
    int postTradeCleanupDelayMs_ = 1000;
    int tradeClipboardMode_ = 0;
    bool tradeSeqDragSelecting_ = false;
    bool tradeSeqDragUpdating_ = false;
    int tradeSeqDragStartRow_ = -1;

};

} // namespace

void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using SetContextFn = BOOL (WINAPI*)(HANDLE);
    SetContextFn setContext = nullptr;
    if (user32) ResolveProc(user32, "SetProcessDpiAwarenessContext", setContext);
    if (setContext) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4. Dynamic lookup keeps old SDKs buildable.
        (void)setContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)));
    } else {
        (void)SetProcessDPIAware();
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // Prevent DPI virtualization from corrupting cursor->client coordinate capture on scaled displays.
    EnableDpiAwareness();
    App app;
    if (!app.Create(instance)) return 2;
    app.Show(show);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
