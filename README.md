# Test Đơn Vàng — Auto Target ID (EXE build branch)
This branch only hosts the approved source ZIP and a Windows x64 GitHub Actions build workflow; `main` is untouched.

**Source archive:** `source/TestDonVang_TargetID_SOURCE_APPROVED.zip` (extracts to `TestDonVang_TargetID/`).

**Build:** GitHub Actions → **Build Target ID EXE** → latest successful run → download artifact `TestDonVang-TargetID-Windows-x64`.

Output is both `TestDonVang_TargetID.exe` and `ThanLongCleanRouteBridge.dll`. Put them side by side. The executable also uses the original licensing launcher.

**Scope:** each game account retains discovery/management and Log tab; independent scan/tick of nearby target RoleID; target, portrait click, trade callback, click 2, loop. Multiple accounts may share one target RoleID. Legacy auto-train and equipment filtering modes have been removed from the approved source.

**Verification status:** workflow compiles and runs automated logic tests. Real game memory layout, portrait UI, and target click behavior still require manual Windows/game validation. This build branch is not merged to `main`.
