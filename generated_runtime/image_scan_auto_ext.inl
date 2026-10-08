#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include "bag_filter_v2_logic.h"

namespace image_scan_test {
namespace {

FilterMode g_filterMode = FilterMode::V4;

enum class AutoPhase {
    Idle,
    WaitOpenBagDirect,
    WaitRestoreSwipe,
    WaitSwipe,
    WaitItemReady,
    WaitDiscardInvoke,
    WaitDiscardGone,
    WaitPopupGoneAfterConfirm,
    WaitCloseGone,
    WaitSemanticScan,
    WaitSemanticSwipe,
    WaitSemanticItemReady,
    WaitSemanticDiscardInvoke,
    WaitSemanticDiscardGone,
    WaitSemanticAfterConfirm,
    Recovery,
    CompletedUntilFull,
    FullYieldReady,
    Error,
};

enum class AutoClosePurpose { None, Full, YieldTurn, Completed };
enum class AutoRecoveryPlan { None, FilterEnd, FailClosed };

struct AutoKey {
    HWND hwnd=nullptr; int childSlot=0;
    bool operator<(const AutoKey& o) const { if(hwnd!=o.hwnd)return hwnd<o.hwnd; return childSlot<o.childSlot; }
    bool operator==(const AutoKey& o) const { return hwnd==o.hwnd && childSlot==o.childSlot; }
};

struct AutoSession {
    State scan{};
    AutoPhase phase=AutoPhase::Idle;
    AutoClosePurpose closePurpose=AutoClosePurpose::None;
    AutoRecoveryPlan recoveryPlan=AutoRecoveryPlan::None;
    int recoveryStage=0;
    ULONGLONG recoveryStartedTick=0;
    bool recoverySweepChanged=false;
    bool recoverySweepUnresolved=false;
    int recoverySweepCount=0;
    int recoveryCleanSweepStreak=0;
    bool recoverySwitchClicked=false;
    std::wstring recoveryReason{};
    bool initialized=false;
    bool currentItemActive=false;
    bool fullExitPending=false;
    bool bagOpened=false;
    bool bagOpenClickDispatched=false;
    bool startPrecheckCompleted=false;
    bool turnOwned=false;
    bool progressValid=false;
    bool semanticTurn=false;
    std::vector<int> semanticCandidates{};
    std::size_t semanticIndex=0;
    int semanticDepth=0;
    int semanticFreeBagSpace=-1;
    int logicalPosition=0;      // authoritative bag Position 0..99
    std::size_t slotIndex=0;   // physical configured coordinate, 0-based
    int scrollDepth=0;         // 0,1,2
    int restoreSwipesRemaining=0;
    std::size_t discardCount=0;
    ULONGLONG nextTick=0,deadlineTick=0;
    std::wstring status{};
};

std::map<AutoKey,AutoSession> g_autoSessions;
std::deque<AutoKey> g_waitQueue;
std::set<AutoKey> g_activeOwners;

bool ValidChildSlot(int childSlot){return childSlot>=1&&childSlot<=30;}
AutoSession* FindAutoSession(HWND hwnd,int childSlot,bool create){
    if(!hwnd||!ValidChildSlot(childSlot))return nullptr;AutoKey k{hwnd,childSlot};auto it=g_autoSessions.find(k);
    if(it!=g_autoSessions.end())return &it->second;if(!create)return nullptr;return &g_autoSessions.emplace(k,AutoSession{}).first->second;
}
void RemoveFromQueue(const AutoKey& k){g_waitQueue.erase(std::remove(g_waitQueue.begin(),g_waitQueue.end(),k),g_waitQueue.end());}
int MaxConcurrent(){EnsurePersistentConfigLoaded();return std::clamp(g_lastConfig.maxConcurrentScan,0,30);}
void PromoteWaiters(){
    const int limit=MaxConcurrent();
    if(limit<=0)return;
    while(static_cast<int>(g_activeOwners.size())<limit&&!g_waitQueue.empty()){
        AutoKey k=g_waitQueue.front();g_waitQueue.pop_front();
        auto it=g_autoSessions.find(k);if(it==g_autoSessions.end())continue;
        AutoSession& s=it->second;if(s.phase==AutoPhase::CompletedUntilFull||s.phase==AutoPhase::FullYieldReady||s.phase==AutoPhase::Error)continue;
        g_activeOwners.insert(k);s.turnOwned=true;s.status=L"SCAN VK • được cấp lượt scheduler";
    }
}
bool AcquireTurn(const AutoKey& k,AutoSession& s){
    if(MaxConcurrent()<=0){RemoveFromQueue(k);g_activeOwners.erase(k);s.turnOwned=false;s.status=L"SCAN VK • TẮT vì Tối đa CON scan = 0";return false;}
    if(g_activeOwners.find(k)!=g_activeOwners.end()){s.turnOwned=true;return true;}
    if(std::find(g_waitQueue.begin(),g_waitQueue.end(),k)==g_waitQueue.end())g_waitQueue.push_back(k);
    PromoteWaiters();
    if(g_activeOwners.find(k)!=g_activeOwners.end()){s.turnOwned=true;return true;}
    s.turnOwned=false;s.status=L"SCAN VK • chờ lượt • tối đa "+std::to_wstring(MaxConcurrent())+L" CON cùng scan";return false;
}
void ReleaseTurn(const AutoKey& k,AutoSession& s){g_activeOwners.erase(k);RemoveFromQueue(k);s.turnOwned=false;PromoteWaiters();}
void ResetTurnRuntime(AutoSession& s){s.scan=State{};s.phase=AutoPhase::Idle;s.closePurpose=AutoClosePurpose::None;s.recoveryPlan=AutoRecoveryPlan::None;s.recoveryStage=0;s.recoveryStartedTick=0;s.recoverySweepChanged=false;s.recoverySweepUnresolved=false;s.recoverySweepCount=0;s.recoveryCleanSweepStreak=0;s.recoverySwitchClicked=false;s.recoveryReason.clear();s.initialized=false;s.currentItemActive=false;s.fullExitPending=false;s.bagOpened=false;s.bagOpenClickDispatched=false;s.restoreSwipesRemaining=0;s.discardCount=0;s.nextTick=0;s.deadlineTick=0;s.semanticTurn=false;s.semanticCandidates.clear();s.semanticIndex=0;s.semanticDepth=0;s.semanticFreeBagSpace=-1;}
void ResetSession(AutoSession& s){ResetTurnRuntime(s);s.startPrecheckCompleted=false;s.progressValid=false;s.logicalPosition=0;s.slotIndex=0;s.scrollDepth=0;s.status.clear();}

bool AutoClickStep(AutoSession& s,const ClickStep& step,std::wstring& error){int cw=0,ch=0,x=0,y=0;if(!CurrentClientSize(s.scan.target.gameWindow,cw,ch)||!ResolveStepPoint(step,cw,ch,x,y)){error=L"không resolve được tọa click";return false;}return RawClick(s.scan,x,y,cw,ch,error);}
bool AutoDrag(AutoSession& s,std::wstring& error){
    if(!s.scan.target.hiddenDrag){error=L"hidden drag chưa được bridge cung cấp";return false;}
    int cw=0,ch=0,sx=0,sy=0,ex=0,ey=0;if(!CurrentClientSize(s.scan.target.gameWindow,cw,ch)||!ResolveStepPoint(s.scan.config.swipeStart,cw,ch,sx,sy)||!ResolveStepPoint(s.scan.config.swipeEnd,cw,ch,ex,ey)){error=L"F8 START/END vuốt chưa hợp lệ";return false;}
    if(!s.scan.target.hiddenDrag(s.scan.target.context,sx,sy,ex,ey,cw,ch,error))return false;
    s.scan.lastActionTick=GetTickCount64();return true;
}

bool AutoSemanticDiscard(AutoSession& s,bool probeOnly,std::wstring& detail){
    if(!s.scan.target.semanticDiscard){detail=L"semantic VỨT callback chưa có";return false;}
    const bool ok=s.scan.target.semanticDiscard(s.scan.target.context,probeOnly,detail);
    if(ok&&!probeOnly)s.scan.lastActionTick=GetTickCount64();
    return ok;
}

bool AutoSemanticDiscardConfirm(AutoSession& s,bool probeOnly,std::wstring& detail){
    if(!s.scan.target.semanticDiscardConfirm){detail=L"semantic XÁC NHẬN SAU VỨT callback chưa có";return false;}
    const bool ok=s.scan.target.semanticDiscardConfirm(s.scan.target.context,probeOnly,detail);
    if(ok&&!probeOnly)s.scan.lastActionTick=GetTickCount64();
    return ok;
}

int AutoUiDirect(AutoSession& s, cleanroute::UiDirectTarget target, bool invoke, std::wstring& detail){
    if(!s.scan.target.uiDirect){detail=L"UI DIRECT callback chưa có";return -1;}
    const int rc=s.scan.target.uiDirect(s.scan.target.context,target,invoke,detail);
    if(rc>0&&invoke)s.scan.lastActionTick=GetTickCount64();
    return rc;
}

bool AutoValidateAndLoad(AutoSession& s,std::wstring& error){
    EnsurePersistentConfigLoaded();s.scan.config=g_lastConfig;RebaseForCurrentClient(s.scan.config,s.scan.target.gameWindow);
    if(!s.semanticTurn&&s.scan.config.templatePath.empty()){error=L"chưa chọn ẢNH 1";return false;}
    if(s.scan.config.closeTemplatePath.empty()){error=L"chưa chọn ảnh X";return false;}
    if(!s.scan.target.semanticDiscard){error=L"semantic VỨT callback chưa có";return false;}
    if(!s.scan.target.uiDirect){error=L"UI DIRECT callback chưa có";return false;}
    if(!s.scan.config.bagUiSwitch.valid){error=L"PRECHECK TAY NẢI chưa gán tọa chuyển UI bằng F8";return false;}
    if(!s.scan.config.bagOpenClick.valid){error=L"MỞ TAY NẢI chưa gán tọa F8";return false;}
    if(s.scan.config.steps.size()!=42){error=L"AUTO SCAN cần đúng GRID 0..41";return false;}
    for(std::size_t ix=0;ix<42;++ix){const ClickStep& step=s.scan.config.steps[ix];if(!step.valid||step.baseW<=0||step.baseH<=0){error=L"GRID "+std::to_wstring(ix)+L" chưa gán F8";return false;}}
    if(!s.scan.config.swipeStart.valid||!s.scan.config.swipeEnd.valid){error=L"chưa gán F8 ĐẦU/CUỐI cho vuốt tay nải";return false;}
    if(!s.semanticTurn&&!LoadImageWic(s.scan.config.templatePath,s.scan.goodTpl,error)){error=L"ẢNH 1 • "+error;return false;}
    if(!LoadImageWic(s.scan.config.closeTemplatePath,s.scan.closeTpl,error)){error=L"ẢNH X • "+error;return false;}
    return true;
}
void AutoArm(AutoSession& s,AutoPhase p,ULONGLONG now,ULONGLONG legacyDelay,bool deadline=true){(void)legacyDelay;s.phase=p;s.nextTick=con_filter_nosleep_logic::NotBefore(now,s.scan.lastActionTick,s.scan.config.minFilterActionMs);s.deadlineTick=deadline?now+kStateTimeoutMs:0;}
bool AutoTimedOut(const AutoSession& s,ULONGLONG now){return s.deadlineTick!=0&&now>=s.deadlineTick;}
void AutoArmSemanticRetry(AutoSession& s,AutoPhase p,ULONGLONG now){s.phase=p;s.nextTick=now+50;s.deadlineTick=now+2000;}
bool AutoSemanticBridgeStillInFlight(const std::wstring& d){return d.find(L"Bridge timeout")!=std::wstring::npos||d.find(L"Bridge còn bận sau timeout")!=std::wstring::npos||d.find(L"Bridge busy")!=std::wstring::npos;}
bool AutoStartRecovery(const AutoKey& k,AutoSession& s,ULONGLONG now,AutoRecoveryPlan plan,AutoClosePurpose purpose,const std::wstring& reason);
void AutoSetError(const AutoKey& k,AutoSession& s,const std::wstring& e){
    if(s.phase==AutoPhase::Recovery){s.recoverySweepUnresolved=true;s.status=L"RECOVERY • fail-closed cục bộ • "+e;s.nextTick=GetTickCount64()+50;return;}
    if(s.initialized&&s.scan.target.gameWindow&&IsWindow(s.scan.target.gameWindow)&&s.scan.target.uiDirect){
        (void)AutoStartRecovery(k,s,GetTickCount64(),AutoRecoveryPlan::FailClosed,AutoClosePurpose::None,e);
        return;
    }
    s.phase=AutoPhase::Error;s.currentItemActive=false;s.status=L"SCAN VK ERROR • "+e;ReleaseTurn(k,s);
}

bool AutoStartSlot(AutoSession& s,ULONGLONG now,std::wstring& error){
    if(s.logicalPosition<0||s.logicalPosition>99||s.slotIndex>=42){error=L"Position/Grid scan ngoài phạm vi";return false;}
    const ClickStep& step=s.scan.config.steps[s.slotIndex];if(!AutoClickStep(s,step,error))return false;s.currentItemActive=true;AutoArm(s,AutoPhase::WaitItemReady,now,0);
    s.status=L"SCAN VK • Position "+std::to_wstring(s.logicalPosition)+L" (GRID "+std::to_wstring(s.slotIndex)+L") → chờ ảnh";return true;
}
bool AutoBeginAfterOpen(AutoSession& s,ULONGLONG now,std::wstring& error){
    if(s.semanticTurn){s.phase=AutoPhase::WaitSemanticScan;s.nextTick=now;s.deadlineTick=0;s.status=L"LỌC VK CÁCH 2 • mở bag xong → quét semantic 0–99 đúng 1 lần";return true;}
    if(s.restoreSwipesRemaining>0){if(!AutoDrag(s,error))return false;--s.restoreSwipesRemaining;AutoArm(s,AutoPhase::WaitRestoreSwipe,now,0,false);s.status=L"SCAN VK • restore tay nải • còn "+std::to_wstring(s.restoreSwipesRemaining)+L" lần vuốt";return true;}
    return AutoStartSlot(s,now,error);
}
bool AutoTryOpenBagDirect(AutoSession& s,ULONGLONG now,bool allowSwitch,std::wstring& error){
    std::wstring detail;
    const int probe=AutoUiDirect(s,cleanroute::UiDirectTarget::OpenBag,false,detail);
    if(probe>0){
        if(s.bagOpenClickDispatched){
            error=L"MỞ TAY NẢI • invariant lỗi: đã dispatch click F8 nhưng quay lại PRECHECK";return false;
        }
        if(!AutoClickStep(s,s.scan.config.bagOpenClick,error))return false;
        s.bagOpenClickDispatched=true;
        s.phase=AutoPhase::WaitOpenBagDirect;
        s.nextTick=con_filter_nosleep_logic::NotBefore(now+50,s.scan.lastActionTick,s.scan.config.minFilterActionMs);
        s.deadlineTick=now+2000;
        s.status=L"MỞ TAY NẢI • đã click F8 đúng 1 lần → chờ proof bag mở";
        return true;
    }
    if(probe<0&&!AutoSemanticBridgeStillInFlight(detail)){
        error=L"PRECHECK MỞ TAY NẢI • "+detail;return false;
    }
    if(allowSwitch&&probe==0&&!s.startPrecheckCompleted){
        if(!AutoClickStep(s,s.scan.config.bagUiSwitch,error))return false;
        s.startPrecheckCompleted=true;
        s.status=L"PRECHECK chưa thấy nút MỞ TAY NẢI • click tọa chuyển UI → chờ nút xuất hiện";
    }else{
        s.status=AutoSemanticBridgeStillInFlight(detail)?L"PRECHECK MỞ TAY NẢI • Bridge pending → chờ":L"PRECHECK chưa thấy nút MỞ TAY NẢI → chờ no-sleep";
    }
    s.phase=AutoPhase::WaitOpenBagDirect;
    s.nextTick=con_filter_nosleep_logic::NotBefore(now+50,s.scan.lastActionTick,s.scan.config.minFilterActionMs);
    if(s.deadlineTick==0)s.deadlineTick=now+2000;
    return true;
}

bool AutoRunStartPrecheck(AutoSession& s,ULONGLONG now,std::wstring& error){
    return AutoTryOpenBagDirect(s,now,!s.startPrecheckCompleted,error);
}
bool InitializeTurn(AutoSession& s,const Target& target,ULONGLONG now,std::wstring& error){
    // CP12: each scheduler turn that will OPEN BAG must PRECHECK again.
    // Preserve only the V4 cursor/depth; never carry a PRECHECK PASS across turns.
    const bool prog=s.progressValid;const int logical=s.logicalPosition,depth=s.scrollDepth;const std::size_t physical=s.slotIndex;
    ResetTurnRuntime(s);s.startPrecheckCompleted=false;s.progressValid=prog;s.logicalPosition=prog?logical:1;s.scrollDepth=prog?depth:0;s.slotIndex=prog?physical:0;s.scan.target=target;s.semanticTurn=(g_filterMode==FilterMode::SemanticBag);
    if(!AutoValidateAndLoad(s,error))return false;s.initialized=true;s.restoreSwipesRemaining=s.semanticTurn?0:s.scrollDepth;
    s.status=L"PRECHECK TAY NẢI • lượt mới → kiểm tra UI trước OPEN BAG";
    return AutoRunStartPrecheck(s,now,error);
}

constexpr ULONGLONG kAutoRecoveryMaxMs=5000;
bool AutoStartRecovery(const AutoKey& k,AutoSession& s,ULONGLONG now,AutoRecoveryPlan plan,AutoClosePurpose purpose,const std::wstring& reason){
    (void)k;
    s.closePurpose=purpose;s.currentItemActive=false;s.recoveryPlan=plan;s.recoveryStage=plan==AutoRecoveryPlan::FilterEnd?0:1;
    s.recoveryStartedTick=now;s.recoverySweepChanged=false;s.recoverySweepUnresolved=false;s.recoverySweepCount=0;s.recoveryCleanSweepStreak=0;s.recoverySwitchClicked=false;s.recoveryReason=reason;
    s.phase=AutoPhase::Recovery;s.nextTick=now;s.deadlineTick=0;
    s.status=plan==AutoRecoveryPlan::FilterEnd?L"FILTER END RECOVERY • XÁC NHẬN nếu còn → X Tay nải → X popup → multi-sweep":L"HARD RECOVERY • X Tay nải → X popup → X giao dịch";
    return true;
}
bool AutoStartClose(AutoSession& s,ULONGLONG now,AutoClosePurpose purpose,std::wstring& error){
    (void)error;AutoKey k{s.scan.target.gameWindow,0};
    return AutoStartRecovery(k,s,now,AutoRecoveryPlan::FilterEnd,purpose,L"kết thúc lượt lọc");
}
void FinishEndPipeline(const AutoKey& k,AutoSession& s){
    const AutoClosePurpose purpose=s.closePurpose;s.closePurpose=AutoClosePurpose::None;
    if(purpose==AutoClosePurpose::Full){s.phase=AutoPhase::FullYieldReady;s.status=L"SCAN VK • END xong • FULL → nhường WorldFlow";}
    else if(purpose==AutoClosePurpose::Completed){s.phase=AutoPhase::CompletedUntilFull;s.status=L"SCAN VK • đã scan hết phạm vi tối đa • chờ FULL";}
    else{
        if(s.semanticTurn){s.progressValid=false;s.logicalPosition=1;s.slotIndex=0;s.scrollDepth=0;s.phase=AutoPhase::Idle;s.initialized=false;s.semanticCandidates.clear();s.semanticIndex=0;s.semanticDepth=0;s.status=L"LỌC VK CÁCH 2 • batch xong → đóng sạch bag → nhường CON • lượt sau quét mới 0–99";}
        else{s.progressValid=true;s.phase=AutoPhase::Idle;s.initialized=false;s.status=L"SCAN VK • EMPTY → đã đóng sạch bag • nhớ ô "+std::to_wstring(s.logicalPosition)+L" • nhường CON";}
    }
    // CP12: releasing a completed turn invalidates the PRECHECK latch.
    // The next scheduler turn must verify/switch UI again before OPEN BAG.
    s.startPrecheckCompleted=false;
    ReleaseTurn(k,s);
}

void FinishAutoRecovery(const AutoKey& key,AutoSession& s,bool timedOut){
    const AutoRecoveryPlan plan=s.recoveryPlan;
    s.recoveryPlan=AutoRecoveryPlan::None;s.recoveryStage=0;s.recoveryStartedTick=0;s.recoverySweepChanged=false;s.recoverySweepUnresolved=false;s.recoverySweepCount=0;s.recoveryCleanSweepStreak=0;s.recoverySwitchClicked=false;s.recoveryReason.clear();
    s.bagOpened=false;s.currentItemActive=false;s.startPrecheckCompleted=false;
    if(plan==AutoRecoveryPlan::FilterEnd){
        if(timedOut)s.status=L"FILTER END RECOVERY • TIME MAX 5000ms • nhả lượt có kiểm soát";
        FinishEndPipeline(key,s);return;
    }
    const bool semantic=s.semanticTurn;
    s.phase=AutoPhase::Idle;s.initialized=false;s.deadlineTick=0;s.nextTick=0;
    if(semantic){s.progressValid=false;s.semanticCandidates.clear();s.semanticIndex=0;s.semanticDepth=0;}
    else s.progressValid=true;
    s.status=timedOut?L"HARD RECOVERY • TIME MAX 5000ms • về PRECHECK an toàn":L"HARD RECOVERY • UI sạch • về PRECHECK an toàn";
    ReleaseTurn(key,s);
}

bool AutoHandleRecovery(const AutoKey& key,AutoSession& s,ULONGLONG now){
    if(s.phase!=AutoPhase::Recovery||s.recoveryPlan==AutoRecoveryPlan::None)return false;
    if(now<s.nextTick)return true;
    if(s.recoveryStartedTick==0)s.recoveryStartedTick=now;
    const bool timedOut=now-s.recoveryStartedTick>=kAutoRecoveryMaxMs;
    if(timedOut){FinishAutoRecovery(key,s,true);return true;}

    // Extra XÁC NHẬN is intentionally scoped ONLY to FILTER END. Generic fail-close never invokes it.
    if(s.recoveryStage==0){
        std::wstring detail;
        if(AutoSemanticDiscardConfirm(s,true,detail)){
            if(!AutoSemanticDiscardConfirm(s,false,detail)&&AutoSemanticBridgeStillInFlight(detail)){s.nextTick=now+50;s.status=L"FILTER END • XÁC NHẬN Bridge pending";return true;}
            s.status=L"FILTER END • đã xử lý XÁC NHẬN nếu còn → X Tay nải";
        }else if(AutoSemanticBridgeStillInFlight(detail)){s.nextTick=now+50;s.status=L"FILTER END • probe XÁC NHẬN Bridge pending";return true;}
        else s.status=L"FILTER END • không có/không xác minh được XÁC NHẬN → không bấm bừa → X Tay nải";
        s.recoveryStage=1;s.nextTick=now+100;return true;
    }

    const bool generic=s.recoveryPlan==AutoRecoveryPlan::FailClosed;
    const int actionCount=generic?3:2;
    if(s.recoveryStage>=1&&s.recoveryStage<=actionCount){
        static constexpr cleanroute::UiDirectTarget targets[3]={cleanroute::UiDirectTarget::CloseBag,cleanroute::UiDirectTarget::CloseItemPopup,cleanroute::UiDirectTarget::CloseTrade};
        static constexpr const wchar_t* labels[3]={L"X Tay nải",L"X popup item",L"X Giao dịch"};
        const int ix=s.recoveryStage-1;std::wstring detail;const int rc=AutoUiDirect(s,targets[ix],true,detail);
        if(rc<0&&AutoSemanticBridgeStillInFlight(detail)){s.nextTick=now+50;s.status=std::wstring(L"RECOVERY • ")+labels[ix]+L" Bridge pending";return true;}
        if(rc>0){s.recoverySweepChanged=true;s.recoveryCleanSweepStreak=0;s.status=std::wstring(L"RECOVERY • ")+labels[ix]+L" PASS";}
        else if(rc<0){s.recoverySweepUnresolved=true;s.recoveryCleanSweepStreak=0;s.status=std::wstring(L"RECOVERY • ")+labels[ix]+L" fail-closed cục bộ • vẫn quét nút khác";}
        else s.status=std::wstring(L"RECOVERY • ")+labels[ix]+L" NOT FOUND";
        ++s.recoveryStage;s.nextTick=now+(rc>0?100:(rc<0?50:0));return true;
    }

    if(s.recoveryStage==actionCount+1){
        ++s.recoverySweepCount;
        if(!s.recoverySweepChanged&&!s.recoverySweepUnresolved){
            ++s.recoveryCleanSweepStreak;
            if(s.recoveryCleanSweepStreak>=2){
                if(s.recoveryPlan==AutoRecoveryPlan::FilterEnd){s.recoveryStage=10;s.nextTick=now;s.status=L"FILTER END • 2 sweep sạch → probe MỞ TAY NẢI để quyết định về bảng Skill";return true;}
                FinishAutoRecovery(key,s,false);return true;
            }
            s.status=L"RECOVERY • sweep sạch 1/2 • chờ 100ms xác nhận lần 2";
        }else s.recoveryCleanSweepStreak=0;
        s.recoveryStage=1;s.recoverySweepChanged=false;s.recoverySweepUnresolved=false;s.nextTick=now+100;return true;
    }

    if(s.recoveryPlan==AutoRecoveryPlan::FilterEnd&&s.recoveryStage==10){
        std::wstring detail;const int probe=AutoUiDirect(s,cleanroute::UiDirectTarget::OpenBag,false,detail);
        if(probe<0){s.recoverySweepUnresolved=true;s.nextTick=now+50;s.status=L"FILTER END • probe MỞ TAY NẢI fail-closed → retry";return true;}
        if(probe==0){FinishAutoRecovery(key,s,false);return true;}
        if(!s.scan.config.bagUiSwitch.valid){s.recoverySweepUnresolved=true;s.nextTick=now+50;s.status=L"FILTER END • thấy MỞ TAY NẢI nhưng tọa chuyển UI chưa hợp lệ";return true;}
        std::wstring error;if(!AutoClickStep(s,s.scan.config.bagUiSwitch,error)){s.recoverySweepUnresolved=true;s.nextTick=now+50;s.status=L"FILTER END • click tọa chuyển UI fail-closed • "+error;return true;}
        s.recoverySwitchClicked=true;s.recoveryStage=11;s.nextTick=con_filter_nosleep_logic::NotBefore(now+50,s.scan.lastActionTick,s.scan.config.minFilterActionMs);s.status=L"FILTER END • thấy nút MỞ TAY NẢI → KHÔNG mở • đã click tọa chuyển UI → verify";return true;
    }
    if(s.recoveryPlan==AutoRecoveryPlan::FilterEnd&&s.recoveryStage==11){
        std::wstring detail;const int probe=AutoUiDirect(s,cleanroute::UiDirectTarget::OpenBag,false,detail);
        if(probe<0){s.nextTick=now+50;s.status=L"FILTER END • verify UI Bridge pending/fail-closed";return true;}
        if(probe==0){FinishAutoRecovery(key,s,false);return true;}
        // Never click the switch repeatedly: one switch click only, then bounded verification.
        s.nextTick=now+50;s.status=L"FILTER END • chờ nút MỞ TAY NẢI biến mất sau tọa chuyển UI";return true;
    }
    FinishAutoRecovery(key,s,true);return true;
}

bool AutoHandleUiDirectPhase(const AutoKey& key,AutoSession& s,ULONGLONG now,std::wstring& error){
    if(s.phase==AutoPhase::WaitOpenBagDirect){
        std::wstring detail;
        if(s.bagOpenClickDispatched){
            const int proof=AutoUiDirect(s,cleanroute::UiDirectTarget::CloseBag,false,detail);
            if(proof>0){
                s.bagOpened=true;s.startPrecheckCompleted=true;s.deadlineTick=0;
                s.status=L"MỞ TAY NẢI • proof PASS sau 1 click F8";
                return AutoBeginAfterOpen(s,now,error);
            }
            const bool pending=AutoSemanticBridgeStillInFlight(detail);
            if(proof<0&&!pending){error=L"MỞ TAY NẢI proof lỗi • "+detail;return false;}
            if(AutoTimedOut(s,now)&&!pending){error=L"MỞ TAY NẢI TIMEOUT 2000ms sau 1 click F8 • không click lần hai • "+detail;return false;}
            s.nextTick=now+50;s.status=pending?L"MỞ TAY NẢI • proof Bridge pending → chờ":L"MỞ TAY NẢI • đã click 1 lần → chờ proof, không retry click";return true;
        }

        const int probe=AutoUiDirect(s,cleanroute::UiDirectTarget::OpenBag,false,detail);
        if(probe>0){
            if(!AutoClickStep(s,s.scan.config.bagOpenClick,error))return false;
            s.bagOpenClickDispatched=true;
            s.nextTick=con_filter_nosleep_logic::NotBefore(now+50,s.scan.lastActionTick,s.scan.config.minFilterActionMs);
            s.deadlineTick=now+2000;
            s.status=L"MỞ TAY NẢI • đã click F8 đúng 1 lần → chờ proof bag mở";
            return true;
        }
        const bool pending=AutoSemanticBridgeStillInFlight(detail);
        if(probe<0&&!pending){error=L"PRECHECK MỞ TAY NẢI • "+detail;return false;}
        if(AutoTimedOut(s,now)&&!pending){error=L"PRECHECK MỞ TAY NẢI TIMEOUT 2000ms • "+detail;return false;}
        s.nextTick=now+50;s.status=pending?L"PRECHECK MỞ TAY NẢI • Bridge pending → chờ":L"PRECHECK chưa thấy nút MỞ TAY NẢI → chờ, chưa click";return true;
    }
    if(s.phase==AutoPhase::Recovery)return AutoHandleRecovery(key,s,now);
    return false;
}

AutoFilterResult MakeAutoResult(const AutoSession& s){AutoFilterResult r{};r.slotNumber=s.logicalPosition>=1?static_cast<std::size_t>(s.logicalPosition):0;r.status=s.status;switch(s.phase){case AutoPhase::CompletedUntilFull:r.state=AutoFilterState::CompletedUntilFull;r.fullYieldReady=true;break;case AutoPhase::FullYieldReady:r.state=AutoFilterState::FullYieldReady;r.fullYieldReady=true;break;case AutoPhase::Error:r.state=AutoFilterState::Error;r.fullYieldReady=true;break;case AutoPhase::Idle:r.state=s.turnOwned?AutoFilterState::Idle:AutoFilterState::WaitingTurn;r.fullYieldReady=true;break;default:r.state=AutoFilterState::Busy;r.ownsInput=true;r.fullYieldReady=false;break;}return r;}

} // namespace

namespace {
bool g_persistentConfigLoadAttempted=false;
std::filesystem::path PersistentScanConfigPath(){wchar_t localAppData[4096]{};const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",localAppData,_countof(localAppData));if(n>0&&n<_countof(localAppData)){const std::filesystem::path dir=std::filesystem::path(localAppData)/L"ThanLongCleanRoute";std::error_code ec;std::filesystem::create_directories(dir,ec);return dir/L"WeaponScan.auto.tlscan";}wchar_t exe[MAX_PATH*4]{};GetModuleFileNameW(nullptr,exe,_countof(exe));return std::filesystem::path(exe).parent_path()/L"WeaponScan.auto.tlscan";}
void SavePersistentConfigImpl(){
    const auto path=PersistentScanConfigPath();std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)return;const Config& c=g_lastConfig;
    out<<"TL_SCAN_AUTO_V4\n";
    out<<"good_path="<<Utf8(c.templatePath)<<"\n"<<"close_path="<<Utf8(c.closeTemplatePath)<<"\n";
    out<<"good_roi="<<RoiLine(c.x,c.y,c.w,c.h,c.roiBaseW,c.roiBaseH)<<"\n";
    out<<"close_roi="<<RoiLine(c.closeRoi.x,c.closeRoi.y,c.closeRoi.w,c.closeRoi.h,c.closeRoi.baseW,c.closeRoi.baseH)<<"\n";
    out<<"threshold="<<c.thresholdPercent<<"\n"<<"min_filter_action_ms="<<con_filter_nosleep_logic::ClampMinActionMs(c.minFilterActionMs)<<"\n";
    out<<"precheck_switch="<<StepLine(c.bagUiSwitch)<<"\n";
    out<<"bag_open_click="<<StepLine(c.bagOpenClick)<<"\n";
    out<<"swipe_start="<<StepLine(c.swipeStart)<<"\n"<<"swipe_end="<<StepLine(c.swipeEnd)<<"\n";
    out<<"max_concurrent="<<std::clamp(c.maxConcurrentScan,0,30)<<"\n"<<"max_swipes="<<std::clamp(c.maxSwipes,1,2)<<"\n";
    out<<"children=";for(int i=0;i<30;++i){if(i)out<<',';out<<(c.childEnabled[static_cast<std::size_t>(i)]?1:0);}out<<"\n";
    out<<"step_count="<<c.steps.size()<<"\n";for(std::size_t i=0;i<c.steps.size();++i)out<<"step"<<i<<"="<<StepLine(c.steps[i])<<"\n";
}
void LoadPersistentConfigImpl(){
    std::ifstream in(PersistentScanConfigPath(),std::ios::binary);if(!in)return;std::string line;
    if(!std::getline(in,line)||(line!="TL_SCAN_AUTO_V1"&&line!="TL_SCAN_AUTO_V2"&&line!="TL_SCAN_AUTO_V3"&&line!="TL_SCAN_AUTO_V4"))return;
    Config c{};std::size_t stepCount=0;std::vector<std::pair<std::size_t,std::string>> pending;
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();const auto pos=line.find('=');if(pos==std::string::npos)continue;const std::string key=line.substr(0,pos),val=line.substr(pos+1);
        if(key=="good_path")c.templatePath=FromUtf8(val);else if(key=="discard_path")c.discardTemplatePath=FromUtf8(val);else if(key=="close_path")c.closeTemplatePath=FromUtf8(val);
        else if(key=="good_roi"){std::array<int,6>v{};if(ParseSix(val,v)){c.x=v[0];c.y=v[1];c.w=v[2];c.h=v[3];c.roiBaseW=v[4];c.roiBaseH=v[5];}}
        else if(key=="discard_roi"){std::array<int,6>v{};if(ParseSix(val,v))c.discardRoi={v[0],v[1],v[2],v[3],v[4],v[5]};}
        else if(key=="close_roi"){std::array<int,6>v{};if(ParseSix(val,v))c.closeRoi={v[0],v[1],v[2],v[3],v[4],v[5]};}
        else if(key=="threshold"){try{c.thresholdPercent=std::clamp(std::stoi(val),1,100);}catch(...){}}
        else if(key=="delay_discard"){try{c.discardClickDelayMs=std::clamp(std::stoi(val),50,10000);}catch(...){}}
        else if(key=="delay_x"){try{c.closeClickDelayMs=std::clamp(std::stoi(val),50,10000);}catch(...){}}
        else if(key=="precheck_switch")ParseStepLine(val,c.bagUiSwitch);else if(key=="bag_open_click")ParseStepLine(val,c.bagOpenClick);else if(key=="swipe_start")ParseStepLine(val,c.swipeStart);else if(key=="swipe_end")ParseStepLine(val,c.swipeEnd);
        else if(key=="max_concurrent"){try{c.maxConcurrentScan=std::clamp(std::stoi(val),0,30);}catch(...){}}
        else if(key=="max_swipes"){try{c.maxSwipes=std::clamp(std::stoi(val),1,2);}catch(...){}}
        else if(key=="swipe_delay"){try{c.swipeDelayMs=std::clamp(std::stoi(val),100,5000);}catch(...){}}
        else if(key=="min_filter_action_ms"){try{c.minFilterActionMs=con_filter_nosleep_logic::ClampMinActionMs(std::stoi(val));}catch(...){}}
        else if(key=="min_discard_confirm_ms"){try{c.minDiscardConfirmMs=std::max(0,std::stoi(val));}catch(...){}}
        else if(key=="children"){std::stringstream ss(val);std::string x;int i=0;while(std::getline(ss,x,',')&&i<30)c.childEnabled[static_cast<std::size_t>(i++)]=(x=="1");}
        else if(key=="step_count"){try{stepCount=static_cast<std::size_t>(std::max(0,std::stoi(val)));}catch(...){}}
        else if(key.rfind("step",0)==0){try{pending.emplace_back(static_cast<std::size_t>(std::stoul(key.substr(4))),val);}catch(...){}}
    }
    c.steps.resize(stepCount);for(const auto& it:pending)if(it.first<c.steps.size())ParseStepLine(it.second,c.steps[it.first]);g_lastConfig=c;
}
} // namespace

void EnsurePersistentConfigLoaded(){if(g_persistentConfigLoadAttempted)return;g_persistentConfigLoadAttempted=true;LoadPersistentConfigImpl();}
void SavePersistentConfig(){g_persistentConfigLoadAttempted=true;SavePersistentConfigImpl();}

bool ChildFilterConfigured(int childSlot){EnsurePersistentConfigLoaded();return ValidChildSlot(childSlot)&&g_lastConfig.childEnabled[static_cast<std::size_t>(childSlot-1)];}
bool AutoSessionNeedsTickForSlot(int childSlot){for(const auto& kv:g_autoSessions){if(kv.first.childSlot!=childSlot)continue;const AutoSession& s=kv.second;if(s.turnOwned||s.phase==AutoPhase::Recovery||s.bagOpened||s.currentItemActive)return true;}return false;}
bool IsChildAutoFilterEnabled(int childSlot){if(!ValidChildSlot(childSlot))return false;return (ChildFilterConfigured(childSlot)&&MaxConcurrent()>0)||AutoSessionNeedsTickForSlot(childSlot);}
bool HandleDisabledFilterScheduler(const Target& target,int childSlot,AutoFilterResult& out){
    out={};out.state=AutoFilterState::Disabled;out.fullYieldReady=true;
    if(!ValidChildSlot(childSlot)||!target.gameWindow||!IsWindow(target.gameWindow))return true;
    if(ChildFilterConfigured(childSlot)&&MaxConcurrent()>0)return false;
    AutoKey key{target.gameWindow,childSlot};AutoSession* s=FindAutoSession(key.hwnd,key.childSlot,false);
    if(!s||!(s->turnOwned||s->bagOpened||s->currentItemActive||s->phase==AutoPhase::Recovery)){RemoveFromQueue(key);g_activeOwners.erase(key);return true;}
    s->scan.target=target;EnsurePersistentConfigLoaded();s->scan.config=g_lastConfig;RebaseForCurrentClient(s->scan.config,target.gameWindow);const ULONGLONG now=GetTickCount64();
    if(s->phase!=AutoPhase::Recovery)(void)AutoStartRecovery(key,*s,now,AutoRecoveryPlan::FilterEnd,AutoClosePurpose::YieldTurn,L"scheduler lọc tắt (max CON = 0 hoặc CON bỏ tick)");
    if(now>=s->nextTick)(void)AutoHandleRecovery(key,*s,now);out=MakeAutoResult(*s);return true;
}

AutoFilterResult TickAutoFilterV4Internal(const Target& target,int childSlot,bool bagFull){
    AutoFilterResult disabled{};if(!target.hiddenClick||HandleDisabledFilterScheduler(target,childSlot,disabled))return disabled;
    AutoKey key{target.gameWindow,childSlot};AutoSession* ps=FindAutoSession(key.hwnd,key.childSlot,true);if(!ps)return disabled;AutoSession& s=*ps;s.scan.target=target;const ULONGLONG now=GetTickCount64();
    if(s.phase==AutoPhase::CompletedUntilFull){if(bagFull){s.fullExitPending=true;s.phase=AutoPhase::FullYieldReady;s.status=L"SCAN VK • phạm vi đã full VK + Bag FULL → về GD";}return MakeAutoResult(s);}if(s.phase==AutoPhase::FullYieldReady||s.phase==AutoPhase::Error)return MakeAutoResult(s);
    if(!AcquireTurn(key,s))return MakeAutoResult(s);
    if(!s.initialized){if(bagFull){s.fullExitPending=true;s.phase=AutoPhase::FullYieldReady;s.status=L"SCAN VK • FULL trước khi mở bag → nhường WorldFlow";ReleaseTurn(key,s);return MakeAutoResult(s);}std::wstring e;if(!InitializeTurn(s,target,now,e)){AutoSetError(key,s,e);return MakeAutoResult(s);}}
    if(bagFull)s.fullExitPending=true;std::wstring error;
    if(s.fullExitPending&&!s.currentItemActive&&(s.phase==AutoPhase::WaitOpenBagDirect||s.phase==AutoPhase::WaitRestoreSwipe||s.phase==AutoPhase::WaitSwipe)){if(!AutoStartClose(s,now,AutoClosePurpose::Full,error))AutoSetError(key,s,error);return MakeAutoResult(s);}if(now<s.nextTick)return MakeAutoResult(s);
    if(s.phase==AutoPhase::WaitOpenBagDirect||s.phase==AutoPhase::Recovery){if(!AutoHandleUiDirectPhase(key,s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitRestoreSwipe){if(s.restoreSwipesRemaining>0){if(!AutoDrag(s,error))AutoSetError(key,s,error);else{--s.restoreSwipesRemaining;AutoArm(s,AutoPhase::WaitRestoreSwipe,now,0,false);}return MakeAutoResult(s);}if(!AutoStartSlot(s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}if(s.phase==AutoPhase::WaitSwipe){if(!AutoStartSlot(s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}
    Image frame{};std::wstring backend;if(!CaptureClient(target.gameWindow,frame,backend,error)){AutoSetError(key,s,L"CAPTURE • "+error);return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitItemReady){Match good{};if(!ScanGoodOnFrame(s.scan,frame,good,error)){AutoSetError(key,s,L"ROI GOOD • "+error);return MakeAutoResult(s);}if(good.found){Match close{};if(!ScanCloseOnFrame(s.scan,frame,close,error)){AutoSetError(key,s,L"ROI X • "+error);return MakeAutoResult(s);}if(close.found){if(!RawClick(s.scan,close.x+s.scan.closeTpl.width/2,close.y+s.scan.closeTpl.height/2,frame.width,frame.height,error)){AutoSetError(key,s,L"CLICK X • "+error);return MakeAutoResult(s);}AutoArm(s,AutoPhase::WaitCloseGone,now,0);s.status=L"SCAN VK • Position "+std::to_wstring(s.logicalPosition)+L" GOOD → X • no-sleep";return MakeAutoResult(s);}if(AutoTimedOut(s,now))AutoSetError(key,s,L"GOOD nhưng X không xuất hiện");else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}std::wstring sem;if(AutoSemanticDiscard(s,true,sem)){if(!AutoSemanticDiscard(s,false,sem)){AutoArmSemanticRetry(s,AutoPhase::WaitDiscardInvoke,now);s.status=L"SCAN VK • semantic VỨT callback chậm/lỗi → retry tối đa 2000ms";return MakeAutoResult(s);}AutoArm(s,AutoPhase::WaitDiscardGone,now,0);s.status=L"SCAN VK • Position "+std::to_wstring(s.logicalPosition)+L" BAD → semantic VỨT";return MakeAutoResult(s);}const auto fail=con_filter_nosleep_logic::ClassifySemanticFailure(sem);if(fail==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic VỨT AMBIGUOUS");return MakeAutoResult(s);}if(fail==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){s.currentItemActive=false;s.progressValid=true;if(!AutoStartClose(s,now,s.fullExitPending?AutoClosePurpose::Full:AutoClosePurpose::YieldTurn,error))AutoSetError(key,s,error);else s.status=L"SCAN VK • NO GOOD + NO semantic VỨT = EMPTY";return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"SCAN VK • semantic VỨT Bridge còn pending → chờ late completion";s.nextTick=now+50;}else AutoSetError(key,s,L"semantic VỨT timeout • "+sem);}else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitDiscardInvoke){std::wstring sem;if(AutoSemanticDiscard(s,false,sem)){AutoArm(s,AutoPhase::WaitDiscardGone,now,0);s.status=L"SCAN VK • semantic VỨT retry PASS";return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"SCAN VK • semantic VỨT quá 2000ms nhưng request còn chạy → chờ thu hồi kết quả";s.nextTick=now+50;return MakeAutoResult(s);}AutoSetError(key,s,L"semantic VỨT invoke TIMEOUT 2000ms • không SKIP mù");return MakeAutoResult(s);}s.nextTick=now+50;return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitDiscardGone){std::wstring sem;if(AutoSemanticDiscard(s,true,sem)){if(AutoTimedOut(s,now))AutoSetError(key,s,L"semantic VỨT không biến mất");else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}const auto fail=con_filter_nosleep_logic::ClassifySemanticFailure(sem);if(fail==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic VỨT AMBIGUOUS sau callback");return MakeAutoResult(s);}if(fail==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){const ULONGLONG due=s.scan.lastActionTick+static_cast<ULONGLONG>(std::max(0,s.scan.config.minDiscardConfirmMs));if(s.deadlineTick<due+kStateTimeoutMs)s.deadlineTick=due+kStateTimeoutMs;if(now<due){s.nextTick=due;return MakeAutoResult(s);}std::wstring confirm;if(AutoSemanticDiscardConfirm(s,true,confirm)){if(!AutoSemanticDiscardConfirm(s,false,confirm)){AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT • "+confirm);return MakeAutoResult(s);}AutoArm(s,AutoPhase::WaitPopupGoneAfterConfirm,now,0);return MakeAutoResult(s);}const auto cf=con_filter_nosleep_logic::ClassifySemanticFailure(confirm);if(cf==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT AMBIGUOUS");return MakeAutoResult(s);}if(AutoTimedOut(s,now))AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT timeout • "+confirm);else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"SCAN VK • probe VỨT còn pending → chờ late completion";s.nextTick=now+50;}else AutoSetError(key,s,L"probe semantic VỨT timeout • "+sem);}else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitPopupGoneAfterConfirm){Match good{},close{};std::wstring e1,e2;const bool ok1=ScanGoodOnFrame(s.scan,frame,good,e1),ok2=ScanCloseOnFrame(s.scan,frame,close,e2);if(!ok1||!ok2){AutoSetError(key,s,L"SCAN sau VỨT • "+(!ok1?e1:e2));return MakeAutoResult(s);}std::wstring sem;const bool discardStill=AutoSemanticDiscard(s,true,sem);const auto fail=discardStill?con_filter_nosleep_logic::SemanticProbeFailure::Retry:con_filter_nosleep_logic::ClassifySemanticFailure(sem);if(!discardStill&&fail==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic VỨT AMBIGUOUS sau confirm");return MakeAutoResult(s);}if(!good.found&&!close.found&&!discardStill&&fail==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){++s.discardCount;s.currentItemActive=false;if(s.fullExitPending){if(!AutoStartClose(s,now,AutoClosePurpose::Full,error))AutoSetError(key,s,error);return MakeAutoResult(s);}if(!AutoStartSlot(s,now,error))AutoSetError(key,s,L"LẶP SAME SLOT • "+error);return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"SCAN VK • sau xác nhận, probe VỨT còn pending → chờ late completion";s.nextTick=now+50;}else AutoSetError(key,s,L"popup sau VỨT chưa đóng");}else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitCloseGone){Match close{};if(!ScanCloseOnFrame(s.scan,frame,close,error)){AutoSetError(key,s,L"ROI X • "+error);return MakeAutoResult(s);}if(!close.found){s.currentItemActive=false;s.discardCount=0;if(s.fullExitPending){s.progressValid=true;if(!AutoStartClose(s,now,AutoClosePurpose::Full,error))AutoSetError(key,s,error);return MakeAutoResult(s);}const int maxSwipes=std::clamp(s.scan.config.maxSwipes,1,2);if(s.logicalPosition==41&&maxSwipes>=1){s.logicalPosition=42;s.slotIndex=0;s.scrollDepth=1;s.progressValid=true;if(!AutoDrag(s,error))AutoSetError(key,s,L"VUỐT #1 • "+error);else{AutoArm(s,AutoPhase::WaitSwipe,now,0,false);s.status=L"SCAN VK • P0..41 → VUỐT #1 → P42";}return MakeAutoResult(s);}if(s.logicalPosition==83&&maxSwipes>=2){s.logicalPosition=84;s.slotIndex=21;s.scrollDepth=2;s.progressValid=true;if(!AutoDrag(s,error))AutoSetError(key,s,L"VUỐT #2 • "+error);else{AutoArm(s,AutoPhase::WaitSwipe,now,0,false);s.status=L"SCAN VK • P42..83 → VUỐT #2 → GRID 21 = P84";}return MakeAutoResult(s);}const bool done=(s.logicalPosition>=99)||(maxSwipes==1&&s.logicalPosition>=83);if(done){s.progressValid=true;if(!AutoStartClose(s,now,AutoClosePurpose::Completed,error))AutoSetError(key,s,error);return MakeAutoResult(s);}++s.logicalPosition;++s.slotIndex;if(!AutoStartSlot(s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}if(AutoTimedOut(s,now))AutoSetError(key,s,L"X không biến mất");else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}
    return MakeAutoResult(s);
}


bool AutoSemanticStartCandidate(const AutoKey& key,AutoSession& s,ULONGLONG now,std::wstring& error){
    // FULL has priority over opening the next candidate, but never interrupts currentItemActive.
    if(bag_filter_v2_logic::FullBoundaryReady(s.fullExitPending,s.currentItemActive)){
        return AutoStartClose(s,now,AutoClosePurpose::Full,error);
    }
    if(s.semanticIndex>=s.semanticCandidates.size()){
        s.currentItemActive=false;
        return AutoStartClose(s,now,s.fullExitPending?AutoClosePurpose::Full:AutoClosePurpose::YieldTurn,error);
    }
    const int pos=s.semanticCandidates[s.semanticIndex];
    const int depth=bag_filter_v2_logic::RequiredScrollDepth(pos);
    const int physical=bag_filter_v2_logic::PhysicalIndex(pos);
    if(depth<0||physical<0||physical>=42){error=L"candidate Position ngoài 0–99";return false;}
    if(depth<s.semanticDepth){error=L"candidate không tăng dần; từ chối vuốt ngược";return false;}
    if(depth>s.semanticDepth){
        const int nextDepth=bag_filter_v2_logic::NextScrollDepth(s.semanticDepth,depth);
        if(nextDepth<0){error=L"candidate depth không hợp lệ";return false;}
        if(!AutoDrag(s,error))return false;
        s.semanticDepth=nextDepth;
        AutoArm(s,AutoPhase::WaitSemanticSwipe,now,0,false);
        s.status=L"LỌC VK CÁCH 2 • VUỐT #"+std::to_wstring(s.semanticDepth)+L" để tới Position "+std::to_wstring(pos);
        return true;
    }
    const ClickStep& step=s.scan.config.steps[static_cast<std::size_t>(physical)];
    if(!AutoClickStep(s,step,error))return false;
    s.currentItemActive=true;
    AutoArm(s,AutoPhase::WaitSemanticItemReady,now,0);
    s.status=L"LỌC VK CÁCH 2 • Position "+std::to_wstring(pos)+L" → tọa vật lý "+std::to_wstring(physical)+L" → probe semantic VỨT";
    return true;
}

AutoFilterResult TickAutoFilterSemanticInternal(const Target& target,int childSlot,bool bagFull){
    EnsurePersistentConfigLoaded();AutoFilterResult disabled{};if(HandleDisabledFilterScheduler(target,childSlot,disabled))return disabled;
    AutoKey key{target.gameWindow,childSlot};AutoSession& s=*FindAutoSession(target.gameWindow,childSlot,true);const ULONGLONG now=GetTickCount64();
    if(s.phase==AutoPhase::FullYieldReady||s.phase==AutoPhase::Error)return MakeAutoResult(s);
    // Highest-priority FULL gate: do not acquire a filter slot and do not InitializeTurn/PRECHECK/OPEN BAG.
    if(!s.initialized&&bagFull){
        s.fullExitPending=true;s.phase=AutoPhase::FullYieldReady;
        s.status=L"LỌC VK CÁCH 2 • FULL trước lượt → không PRECHECK/OPEN BAG • nhường điều phối";
        return MakeAutoResult(s);
    }
    if(!s.turnOwned&&!AcquireTurn(key,s))return MakeAutoResult(s);
    if(!s.initialized){
        std::wstring e;if(!InitializeTurn(s,target,now,e)){AutoSetError(key,s,e);return MakeAutoResult(s);}
    }
    if(bagFull)s.fullExitPending=true;
    std::wstring error;
    if(now<s.nextTick)return MakeAutoResult(s);
    // If FULL appears after a turn started but no item transaction is active, stop before the next action.
    // A dirty/open bag uses UI DIRECT cleanup; unopened UI yields directly.
    const bool closePipeline=s.phase==AutoPhase::Recovery;
    if(bag_filter_v2_logic::FullBoundaryReady(s.fullExitPending,s.currentItemActive)&&!closePipeline){
        if(!s.bagOpened){
            s.phase=AutoPhase::FullYieldReady;s.status=L"LỌC VK CÁCH 2 • FULL trước candidate → nhường điều phối";ReleaseTurn(key,s);
        }else if(!AutoStartClose(s,now,AutoClosePurpose::Full,error)){AutoSetError(key,s,L"FULL cleanup Tay nải • "+error);}
        return MakeAutoResult(s);
    }
    if(s.phase==AutoPhase::WaitOpenBagDirect||s.phase==AutoPhase::Recovery){if(!AutoHandleUiDirectPhase(key,s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitSemanticScan){
        if(!target.readDropCandidates){AutoSetError(key,s,L"bridge semantic bag callback chưa có");return MakeAutoResult(s);}
        std::vector<int> positions;int freeSpace=-1;
        if(!target.readDropCandidates(target.context,target.bagContext,positions,freeSpace,error)){AutoSetError(key,s,L"QUÉT TAY NẢI 0–99 • "+error);return MakeAutoResult(s);}
        s.semanticCandidates=bag_filter_v2_logic::NormalizeCandidates(std::move(positions));s.semanticIndex=0;s.semanticDepth=0;s.semanticFreeBagSpace=freeSpace;
        if(freeSpace==0)s.fullExitPending=true;
        s.status=L"LỌC VK CÁCH 2 • scan 0–99 xong • cần vứt "+std::to_wstring(s.semanticCandidates.size())+L" item";
        if(!AutoSemanticStartCandidate(key,s,now,error))AutoSetError(key,s,error);
        return MakeAutoResult(s);
    }
    if(s.phase==AutoPhase::WaitSemanticSwipe){if(!AutoSemanticStartCandidate(key,s,now,error))AutoSetError(key,s,error);return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitSemanticItemReady){
        std::wstring sem;if(AutoSemanticDiscard(s,true,sem)){if(!AutoSemanticDiscard(s,false,sem)){AutoArmSemanticRetry(s,AutoPhase::WaitSemanticDiscardInvoke,now);s.status=L"LỌC VK CÁCH 2 • semantic VỨT callback chậm/lỗi → retry tối đa 2000ms";return MakeAutoResult(s);}AutoArm(s,AutoPhase::WaitSemanticDiscardGone,now,0);s.status=L"LỌC VK CÁCH 2 • Position "+std::to_wstring(s.semanticCandidates[s.semanticIndex])+L" → semantic VỨT";return MakeAutoResult(s);}
        const auto fail=con_filter_nosleep_logic::ClassifySemanticFailure(sem);if(fail==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic VỨT AMBIGUOUS");return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"LỌC VK CÁCH 2 • probe VỨT Bridge còn pending → chờ late completion";s.nextTick=now+50;}else{s.currentItemActive=false;++s.semanticIndex;s.status=L"LỌC VK CÁCH 2 • NO semantic VỨT → EMPTY/SKIP candidate";if(!AutoSemanticStartCandidate(key,s,now,error))AutoSetError(key,s,error);}}else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);
    }
    if(s.phase==AutoPhase::WaitSemanticDiscardInvoke){std::wstring sem;if(AutoSemanticDiscard(s,false,sem)){AutoArm(s,AutoPhase::WaitSemanticDiscardGone,now,0);s.status=L"LỌC VK CÁCH 2 • semantic VỨT retry PASS";return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"LỌC VK CÁCH 2 • semantic VỨT quá 2000ms nhưng request còn chạy → chờ thu hồi kết quả";s.nextTick=now+50;return MakeAutoResult(s);}AutoSetError(key,s,L"LỌC VK CÁCH 2 • semantic VỨT invoke TIMEOUT 2000ms • không SKIP candidate mù");return MakeAutoResult(s);}s.nextTick=now+50;return MakeAutoResult(s);}
    if(s.phase==AutoPhase::WaitSemanticDiscardGone){
        std::wstring sem;if(AutoSemanticDiscard(s,true,sem)){if(AutoTimedOut(s,now))AutoSetError(key,s,L"semantic VỨT không biến mất");else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}const auto fail=con_filter_nosleep_logic::ClassifySemanticFailure(sem);if(fail==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic VỨT AMBIGUOUS sau callback");return MakeAutoResult(s);}if(fail==con_filter_nosleep_logic::SemanticProbeFailure::NotFound){const ULONGLONG due=s.scan.lastActionTick+static_cast<ULONGLONG>(std::max(0,s.scan.config.minDiscardConfirmMs));if(s.deadlineTick<due+kStateTimeoutMs)s.deadlineTick=due+kStateTimeoutMs;if(now<due){s.nextTick=due;return MakeAutoResult(s);}std::wstring confirm;if(AutoSemanticDiscardConfirm(s,true,confirm)){if(!AutoSemanticDiscardConfirm(s,false,confirm)){AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT • "+confirm);return MakeAutoResult(s);}AutoArm(s,AutoPhase::WaitSemanticAfterConfirm,now,0,false);s.status=L"LỌC VK CÁCH 2 • semantic VỨT gone → semantic XÁC NHẬN → candidate tiếp";return MakeAutoResult(s);}const auto cf=con_filter_nosleep_logic::ClassifySemanticFailure(confirm);if(cf==con_filter_nosleep_logic::SemanticProbeFailure::Ambiguous){AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT AMBIGUOUS");return MakeAutoResult(s);}if(AutoTimedOut(s,now))AutoSetError(key,s,L"semantic XÁC NHẬN SAU VỨT timeout • "+confirm);else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);}if(AutoTimedOut(s,now)){if(AutoSemanticBridgeStillInFlight(sem)){s.status=L"LỌC VK CÁCH 2 • probe VỨT còn pending → chờ late completion";s.nextTick=now+50;}else AutoSetError(key,s,L"probe semantic VỨT timeout • "+sem);}else s.nextTick=now+kProbeIntervalMs;return MakeAutoResult(s);
    }
    if(s.phase==AutoPhase::WaitSemanticAfterConfirm){
        s.currentItemActive=false;++s.semanticIndex;
        if(!AutoSemanticStartCandidate(key,s,now,error))AutoSetError(key,s,error);
        return MakeAutoResult(s);
    }
    return MakeAutoResult(s);
}

AutoFilterResult TickAutoFilter(const Target& target,int childSlot,bool bagFull){
    return g_filterMode==FilterMode::SemanticBag
        ? TickAutoFilterSemanticInternal(target,childSlot,bagFull)
        : TickAutoFilterV4Internal(target,childSlot,bagFull);
}

FilterMode GetAutoFilterMode(){return g_filterMode;}
void SetAutoFilterMode(FilterMode mode){
    if(mode!=FilterMode::SemanticBag)mode=FilterMode::V4;
    if(g_filterMode==mode)return;
    g_filterMode=mode;
    g_waitQueue.clear();g_activeOwners.clear();g_autoSessions.clear();
}

bool FullBagTravelLatched(HWND gameWindow,int childSlot){AutoSession* s=FindAutoSession(gameWindow,childSlot,false);return s&&(s->fullExitPending||s->phase==AutoPhase::FullYieldReady);}
bool FullBagYieldReady(HWND gameWindow,int childSlot){if(!IsChildAutoFilterEnabled(childSlot))return true;AutoSession* s=FindAutoSession(gameWindow,childSlot,false);if(!s)return true;return s->phase==AutoPhase::FullYieldReady||s->phase==AutoPhase::CompletedUntilFull||s->phase==AutoPhase::Error||(!s->turnOwned&&s->phase==AutoPhase::Idle);}
void NotifyDeath(HWND gameWindow,int childSlot){AutoKey k{gameWindow,childSlot};AutoSession* s=FindAutoSession(gameWindow,childSlot,false);if(!s)return;const bool semantic=s->semanticTurn;s->progressValid=!semantic;ResetTurnRuntime(*s);s->status=semantic?L"LỌC VK CÁCH 2 • DEATH → bỏ snapshot 0–99, nhường scheduler":L"SCAN VK • DEATH → giữ cursor, nhường scheduler";ReleaseTurn(k,*s);}
void NotifyReviveClicked(const Target& target,int childSlot){
    AutoSession* s=FindAutoSession(target.gameWindow,childSlot,false);if(!s)return;const bool semantic=s->semanticTurn;const bool progress=s->progressValid;const int logical=s->logicalPosition;const std::size_t slot=s->slotIndex;const int depth=s->scrollDepth;
    ResetTurnRuntime(*s);s->progressValid=semantic?false:progress;s->logicalPosition=logical;s->slotIndex=slot;s->scrollDepth=depth;s->status=L"SCAN VK • ĐẦU THAI PASS → filter runtime đã reset; controller POST-REVIVE RECOVERY sẽ dọn UI trước khi tiếp tục";
}
void NotifyTradeFlowStarted(HWND gameWindow,int childSlot){AutoKey k{gameWindow,childSlot};AutoSession* s=FindAutoSession(gameWindow,childSlot,false);if(!s)return;ReleaseTurn(k,*s);ResetSession(*s);s->status=L"SCAN VK • vào WorldFlow GD → reset cursor về Position 0 • lượt lọc kế tiếp PRECHECK lại trước OPEN BAG";}
void ResetAutoFilter(HWND gameWindow,int childSlot){AutoKey k{gameWindow,childSlot};AutoSession* s=FindAutoSession(gameWindow,childSlot,true);if(!s)return;ReleaseTurn(k,*s);ResetSession(*s);}
void StopAutoFilter(HWND gameWindow,int childSlot){AutoKey k{gameWindow,childSlot};auto it=g_autoSessions.find(k);if(it!=g_autoSessions.end())ReleaseTurn(k,it->second);RemoveFromQueue(k);g_activeOwners.erase(k);g_autoSessions.erase(k);PromoteWaiters();}

} // namespace image_scan_test
