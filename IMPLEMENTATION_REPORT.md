# V23 UI DIRECT — Global sabotage guard + fail-safe cleanup report

## Source boundary
This tree remains based on `SOURCE_SACH_V23_DISCARD_CONFIRM` only. The existing V23 UI DIRECT selector/invoke layer is retained; no old-version runtime tree is used as a build input.

## Approved changes in this revision

### 1) `Hủy giao dịch Phá hoại` is now a global MAIN guard
When the checkbox is enabled, MAIN scans the exact contextual `TradeRequestCancel` (`Hủy bỏ`) target continuously on a 50 ms non-blocking timer, including while idle, selling, waiting, trading, after a failed action, and during recovery.

There is deliberately **no `AUTHORIZED_CON_TRADE_REQUEST` state**. The legitimate request is protected by the existing controller order only:

`CON Trade semantic PASS -> TradePhase::Sequence row 1 -> MAIN TradeConfirm`

While that exact immediate MAIN confirm row is pending, the global cancel guard yields. At all other times, matching trade-request popups are cancelled.

Before CON sends a new trade request, MAIN drains stale/unwanted trade-request popups first. `FOUND` cancels and retries; fail-closed/busy blocks the CON invite until MAIN is clean.

### 2) Fail-closed is local; recovery still runs
A fail-closed UI target no longer kills later cleanup actions. Cleanup always continues to the other X targets, records unresolved targets, and sweeps again.

Role split remains:
- MAIN: `X Giao dịch`, `X popup item`.
- CON: `X Giao dịch`, `X popup item`, `X Tay nải`.
- MAIN bag is never closed by post-trade cleanup.

Cleanup now performs repeated sweeps until one full sweep is all `NOT FOUND`, or until the 5000 ms recovery time limit is reached. A bridge request genuinely in flight retries the same target without blocking sleeps.

### 3) Abort paths enter recovery when possible
If a trade abort happens after UI may already be open (`TargetMain`, `Sequence`, or `Cleanup`) and both game windows still exist, the controller first enters `Cleanup`, sweeps X targets, then releases the workflow. If one side is already technically unavailable, it falls back to immediate abort/release.

### 4) MAIN sell/trade overlap is gated
The existing MAIN bag scan/count mechanism is preserved. No new “resume click N” mechanism is added.

Before any raw sell click or semantic SELL callback, MAIN is allowed to act only when trade phase is:
- `Idle`, or
- the intentional `SellPause` state.

`Rendezvous`, `TargetMain`, `Sequence`, and `Cleanup` block seller clicks/callbacks.

If an idle sell sweep is active and a real CON takes over the trade workflow, the stale idle-sell runtime is stopped. After trading, the existing seller logic starts from `Phase::Scan` again and recalculates how many unlocked equipment items should be sold from the current bag state.

### 5) Existing count-driven sell logic remains authoritative
The existing flow is unchanged:
- `CountMainUnlockedEquipment()` scans MAIN bag.
- `targetClicks = eligible` for `isEquip && !bound`.
- After the sell episode, fresh `FreeBag` is read.
- MAIN quota is re-planned from the new bag state.
- `SellPause` continues to use the existing scan/count-driven macro.

## Safety properties
- Exact `Hủy bỏ` selector remains contextual and fail-closed; generic unrelated cancel buttons are not clicked.
- No blocking `Sleep` is introduced.
- Global sabotage fail-close does not stop X recovery.
- Cleanup fail-close is per-target, not whole-recovery failure.
- Post-trade cleanup still does not close MAIN bag.
- Seller click count is not artificially preserved across a trade takeover; the existing fresh bag scan is reused instead.
