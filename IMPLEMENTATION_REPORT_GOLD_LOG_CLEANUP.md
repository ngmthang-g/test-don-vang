# V23 GOLD + LOG + CLEANUP ORCHESTRATION FIX

Built only from HARD RECOVERY clean source SHA256 `0e1147c8eb7c9ff5f0af72ae0af017fc13ab5c06f00ffff0ef494909baf5235e`.

## Gold
- V22 reader is preserved: get_Money/get_BoundMoney on Leader with roleData fallback and 10,000 raw units/gold.
- Current report layout (60-minute delta, session total, old/current gold) is preserved.
- A busy/failed Bridge read retries in 1 second and no longer burns the normal 30-second sample slot, preventing stale reported gold under current-runtime contention.

## LOG load
- Main diagnostic LOG and LOCAL/TELE LOG each have a persisted ON/OFF button and default OFF.
- OFF stops only GUI log accumulation; automation, Telegram notifications/reporting, statistics and gold sampling continue.
- LOCAL/TELE LOG appends at the end instead of reading and rewriting the entire growing EDIT text on each event.

## X actions in workflow
- UI DIRECT selector implementation is intentionally unchanged. The fix is sequencing/ownership.
- FILTER END/EMPTY: optional discard confirm -> X Bag -> X item popup, settle delays, then two consecutive clean sweeps before restore-to-Skill/ReleaseTurn.
- POST-REVIVE waits for authoritative ALIVE, then 300ms UI settle before Confirm-if-present -> X Bag -> X item popup -> X Trade; two consecutive clean sweeps are required.
- Revive during active trade is latched until trade cleanup releases the pair.
- Generic recovery and post-trade cleanup use two clean sweeps and paced X actions.
- Global 50ms sabotage timer yields while trade FSM owns MAIN Bridge; TargetMain keeps its explicit pre-invite drain and seller keeps its own guard.

CI validates source boundary, bridge immutability, Windows x64 build and native tests. Live in-game validation remains required.
