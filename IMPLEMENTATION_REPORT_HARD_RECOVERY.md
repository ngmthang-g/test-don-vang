# V23 UI DIRECT HARD RECOVERY — implementation report

## Source boundary
This revision is transformed only from the exact clean `SOURCE_SACH_V23_UI_DIRECT_GLOBAL_GUARD.zip` release (SHA256 `67adf530225f9f27fb6d5eef5d801be8961901ff11b6e72165291bb8ad1adbae`). That release already proves the original V23 sole-base hash, UI DIRECT patch, and approved GLOBAL GUARD delta. No old-version runtime tree is used as build input.

## Approved V3 changes
- `Tối đa CON scan cùng lúc = 0` disables new FILTER turns while preserving each CON checkbox. A CON already holding a dirty/open FILTER turn is allowed to finish FILTER_END recovery before the scheduler becomes fully disabled.
- FILTER END / EMPTY uses one recovery executor: optional extra discard-confirm (only here), `X Tay nải`, `X popup item`, repeated sweeps until one whole sweep is NOT_FOUND or 5000ms TIME MAX, then probe-only `OpenBag`. If `OpenBag` is still visible, the existing `bagUiSwitch` coordinate is clicked once to return UI toward Skill, then probe verifies it. `OpenBag` is never invoked by this restore step.
- POST-REVIVE runs at controller priority: optional extra discard-confirm (only here), `X Tay nải`, `X popup item`, `X Giao dịch`, repeated sweeps. The old `NotifyReviveClicked()` one-shot close sequence was removed to avoid two cleanup owners.
- Generic gameplay/UI fail-close uses `X Tay nải -> X popup item -> X Giao dịch`, repeated sweeps. It deliberately does not invoke discard-confirm. Fail-close of one recovery target is local and never recursively starts another recovery.
- The controller recovery gate pauses normal account mutation while active. Missing/dead/unresponsive-client cases remain handled by their existing life/freeze guards rather than blind UI mutation.
- An active trade pair never runs controller generic cleanup in parallel. It hands off to the existing `AbortTrade -> TradePhase::Cleanup` multi-sweep, preserving the proven MAIN/CON role split and never closing MAIN bag.
- Terminal trade UI DIRECT / post-click PUT-UP timeouts now abort the pass into trade recovery instead of SKIP-and-continue with unproven UI state.
- MAIN seller raw/semantic terminal failures enter recovery; seller runtime is cleared so the next sell episode returns through the existing bag `Scan` and recalculates target click count. No new ordinal-resume mechanism was added.

## Cleanups removed / consolidated
- Removed FILTER `WaitCleanupPopup -> WaitCleanupBag -> FinishEndPipeline` one-shot cleanup.
- Removed duplicate best-effort popup/bag closes from `NotifyReviveClicked()`.
- Removed dead `AutoAdvanceV4AfterSemanticSkip()` path after terminal discard invoke failures were changed to recovery.
- Existing trade multi-sweep and MAIN sabotage guard remain specialized owners and are gated against the new controller recovery to avoid competing callbacks.

## Validation boundary
CI proves source provenance, static contract, Windows x64 compilation, and native logic test suites. Live behavior inside the game client still requires acceptance testing and is not claimed by CI.
