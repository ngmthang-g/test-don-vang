CÔNG CỤ HỖ TRỢ GAME RẢNH TAY — 10.6 — GOLD-ONLY CLEANUP

Source of truth:
- Original exact version-10.6 user ZIP (SHA-256:
  ff04eb9b8504f320901ff0b567272bb3b38591cdfe973887e48af5687b97ed43).
- Approved microtask scope / history:
  https://github.com/ngmthang-g/test-don-vang/issues/1
- Cleanup branch: refactor/gold-only-cleanup.
- Frozen image-weapon filter C1/C2 runtime and EquipPoint resource: guarded in CI.
- Historical V23 reports in this repository are retained for provenance only.
  They are not current feature documentation or additional build inputs.

Build requirements:
- Windows x64, Visual Studio 2022/MSVC, CMake 3.24+, Python 3 for static gates.

Local Release build:
  python scripts/check_weapon_filter_freeze.py
  python scripts/check_build_resource_protocol_scope.py
  python scripts/check_final_gold_only_cleanup.py
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release --parallel

Release outputs (must ship together):
- build/Release/CongCuHoTroGameRanhTay_10.6.exe
- build/Release/ThanLongCleanRouteBridge.dll

Active workflows preserved after cleanup:
- Two separate weapon filters: C1 (FILTER V4 image/grid) and
  C2 (semantic bag scan). These are guarded against unintended changes.
- MAIN count-driven macro sell Cách 1, 9-slot capacity quota and same-CON
  re-plan. The standalone/role-NONE auto-seller is removed.
- MAIN and up to 30 CON with FULL-safe handoff, shared bag read, FIFO trading.
- Generic train/travel, AutoFight, revive, AutoLoot, Côn Lôn shortcuts,
  party, Telegram, Developer, Gold History and LOG/Compact.
- Internal game UI callbacks and recovery are not equivalent to activating
  deleted sabotage or shop/treatment background state machines.
- Protocol command IDs stay stable: slots 10–13 and 15–17 are reserved.

CI and artifact integrity:
- Workflow: .github/workflows/build-gold-only-baseline.yml
- Gates: C1/C2 frozen source, active protocol/resource scope, final zero-ref.
- All 24 native Release test executables must pass.
- Release package contains EXE, Bridge DLL and SHA256SUMS.txt; CI checks
  each PE's x64 machine type and verifies both recorded file hashes.
- Download package from the Actions run corresponding to the commit SHA.
- Passing native CI and synthetic snapshots is not evidence of successful
  live-game 2–4h/multi-CON stress execution; that is a separate T21 gate.
