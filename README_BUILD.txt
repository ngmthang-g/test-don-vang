CÔNG CỤ HỖ TRỢ GAME RẢNH TAY — 10.6 — CLEAN BUILD

This repository is built from SOURCE_SACH_V23_DISCARD_CONFIRM as the sole runtime base.
The Test-UI sources were used only to port the already-proven UI DIRECT selector/invocation mechanism.
No older source tree is used as a build input.

Build requirements:
- Windows x64
- Visual Studio 2022 / MSVC
- CMake 3.24+

Build:
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release --parallel

Release outputs:
- build/Release/CongCuHoTroGameRanhTay_10.6.exe
- build/Release/ThanLongCleanRouteBridge.dll

Important behavior in this source:
- Weapon filter opens bag through UI DIRECT ButBag/ButBagClick.
- OPEN1/OPEN2 and coordinate CloseBag runtime have been removed.
- Filter end cleanup is X popup item (if present) then X Tay nải (if present).
- Trade UI DIRECT rows support editable MIN time and state-driven 50ms retry / 2000ms maximum behavior matching the existing ĐẶT LÊN callback contract.
- Post-trade cleanup: MAIN closes trade + item popup; CON closes trade + item popup + bag.
- MAIN sell can enable "Hủy giao dịch Phá hoại" and calls the exact "Hủy bỏ" request-popup control before continuing the same sell ordinal.
