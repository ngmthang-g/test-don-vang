#!/usr/bin/env python3
"""T20: repository-wide *active source* zero-reference and preservation gate.

Historical implementation reports are intentionally excluded: they describe
superseded features and must not be rewritten as if they were current code.
The scan covers every Git-tracked production/test C++ source and live build
metadata, not just a hand-picked controller substring. Run before Release CI.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Strict identifiers and retired UI/config entry points. Not matching generic
# words like 'sell', 'treatment', 'cancel', 'NPC' or 'Role::CloseTradeOrBag':
# they remain valid in active trade, game recovery and the KunLun shortcut.
FORBIDDEN = {
    "retired PK/TL/LM state": r"\b(?:pk_tl_lm|PkTlLm|TrainPk|trainPk|AlliancePk|SharedPkTlLmSettings|retiredTreatmentEnabled|retiredAlliancePkEnabled|TreatmentRoute|treatmentRoutePending|IDC_ENABLE_TRAIN_PK|IDC_ENABLE_TREATMENT|IDC_ENABLE_ALLIANCE_PK)\b",
    "retired NONE seller FSM": r"\b(?:IndependentAutoSell|IndependentSellNpcTarget|IndependentSellConfigured|RunIndependentSellMacro|retiredNoneSellPreset|sellStep5LearnedRepeat|sellMacroCompletionDueTick|sellBagStableSince|sellMacroRepeatDone|sellMacroNextTick|sellMacroPass|sellOpenAttempts)\b",
    "retired NONE seller UI": r"\b(?:IDC_ENABLE_SELL|IDC_SELL_NPC(?:_X|_Y|_POS|_CAPTURE)?|AutoSellerPresetForTrainingMap|ApplyAutoSellerForTrainingTarget|LoadSellNpcPositionToUi|PersistSellNpcPositionEditor|CaptureSellNpcPosition)\b",
    "retired background bridge": r"\b(?:BeginBackgroundSell|AdvanceBackgroundSell|SellNextBagItem|CloseBackgroundSell|BeginBackgroundTreatment|AdvanceBackgroundTreatment|CloseBackgroundTreatment|BackgroundSellState|BackgroundTreatmentState|SellFixedBagSlot)\b",
    "orphaned treatment/shop roles": r"\bRole::(?:Treatment|TreatmentConfirm|TreatmentAck|ShopEntry|MountShopEntry|MedicineShopEntry|SellTab|QuickSell|EquipmentTab)\b",
    "retired portability": r"(?i:\.(?:tlmaster|tlcfg|tlmap)\b)|\b(?:ImportConfig|ExportConfig|BuildPortableConfig|ParsePortableConfig)\b",
    "retired intro / interserver": r"\b(?:IDC_INTRO|IntroTab|IntroductionTab|TrainMapRoute|InterServerRoute|AutoTrainMap|IDC_TRAIN_MAP)\b",
    "retired spare click/sell way": r"\b(?:MainGapClick|BlankAutoClick|EnableBlankAutoClick|SellMethod2|SellWay2|TestBagProbe|IDC_BAG_PROBE|ThdcRoute|THDC)\b",
}

# Assert that deleted source/header files cannot quietly return as build inputs.
DELETED = (
    "src/pk_tl_lm_logic.h",
    "src/pk_tl_lm_logic_test.cpp",
    "src/thdc_route_logic.h",
)

# The two original 10.6 TravelNetwork files are restored unchanged (Git blob hash),
# not rewritten with a smaller substitute or accidentally removed again.
ORIGINAL_TRAVEL_BLOBS = {
    "src/travel_network_logic.h": "a9ccebfb75af576e1e778a7ea0108524359b077b",
    "src/travel_network_logic_test.cpp": "2373a5a1d7140617285caebecd4350bd61394938",
}

REQUIRED = {
    "generated_runtime/controller.cpp": (
        "CountMainUnlockedEquipment(",
        "RefreshMainCapacityPlan(",
        "EnsureMainCapacityPlan(",
        "BeginMainMacroSell(",
        "TickActiveMainMacroSell()",
        "ShortcutKind::KunLunEnter",
        "CaptureShortcutSellerPosition(",
        "ReadAllBagSemantic(",
        "image_scan_test::TickAutoFilter(",
        "image_scan_test::FullBagTravelLatched(",
        "image_scan_test::FullBagYieldReady(",
        "image_scan_test::NotifyDeath(",
        "image_scan_test::NotifyReviveClicked(",
        "HandleFightClicks(a, now)",
        "Command::ReadBagPage",
        "ShortcutKind::TravelNetworkExit",
        "ShortcutKind::TravelNetwork",
        "HandleTravelNetworkExit3Click(",
        "travel_network_logic::SelectReturnExit(",
        "travel_network_logic::SelectNpcTeleport(",
        "ToProtocolTravelSemantic(",
        "IsTravelNetworkCentralSourceMap(",
        "OpenTravelExitWindow()",
        "BuildTravelExitUi(",
        "PersistTravelExitUi(",
        "TravelExitWndProc(",
        "IDC_SC_EXIT_EDITOR",
        "shortcutTravelExitCaptureGroup_",
        'ReadIniInt(section, L"NamHaiExitX"',
        'ReadIniInt(section, L"MieuCuongExitX"',
        'ReadIniInt(section, L"HoangLongPhuExitX"',
        'ReadIniInt(section, L"ThachLamExitX"',
        'saveExitClicks(L"NamHaiExitClick"',
        'saveExitClicks(L"MieuCuongExitClick"',
        'saveExitClicks(L"HoangLongPhuExitClick"',
        'saveExitClicks(L"ThachLamExitClick"',
        "TravelSemantic::NamHai",
        "TravelSemantic::MieuCuong",
        "TravelSemantic::HoangLongPhu",
        "TravelSemantic::ThachLam",
        "TravelSemantic::DaiLy",
    ),
    "generated_runtime/bridge.cpp": (
        "case Command::ReadBagPage:",
        "case Command::ClickInternalPointRawTest:",
        "case Command::DragInternalPoint:",
        "case Command::PickNearestLoot:",
        "case Command::ClickTravelSemantic:",
        "case Command::ConfirmTravelSemantic:",
        "case TravelSemantic::NamHai:",
        "case TravelSemantic::MieuCuong:",
        "case TravelSemantic::HoangLongPhu:",
        "case TravelSemantic::ThachLam:",
        "case TravelSemantic::DaiLy:",
    ),
    "generated_runtime/protocol.h": (
        "NamHai = 11,",
        "MieuCuong = 12,",
        "HoangLongPhu = 13,",
        "ThachLam = 14,",
        "DaiLy = 15,",
    ),
    "src/travel_network_logic.h": (
        "SelectReturnExit(",
        "SelectNpcTeleport(",
        "kXaTruyenChiNpcId = 45",
        "kXaTruyenTinNpcId = 522",
    ),
    "src/trade_coordinator_logic.h": (
        "tradeRole == 1 && enableSell",
        "MainNeedsCapacitySell(freeBagSpace)",
        "CanStartTradePass(int freeBagSpace)",
        "ShouldAdmitFullChild(",
    ),
    "resources/app.rc": (
        '1 RT_MANIFEST "app.manifest"',
        '201 RCDATA "copyright_notice.txt"',
        '202 RCDATA "equip_points.csv"',
    ),
}

def paths() -> list[str]:
    p = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    return sorted(name.decode("utf-8") for name in p.stdout.split(b"\0") if name)


def is_active_source(name: str) -> bool:
    # Include production and native test sources. Exclude immutable historical
    # implementation reports and this Python scanner's own forbidden patterns.
    return (
        name.startswith(("src/", "generated_runtime/"))
        and Path(name).suffix.lower() in {".cpp", ".h", ".inl"}
    ) or name in {"CMakeLists.txt", "resources/app.rc", "README_BUILD.txt"}


def main() -> int:
    errors: list[str] = []
    tracked = paths()
    active = [p for p in tracked if is_active_source(p)]
    if len(active) < 55:
        errors.append(f"unexpectedly few tracked active sources: {len(active)}")

    for path in DELETED:
        if path in tracked or (ROOT / path).exists():
            errors.append(f"deleted source reintroduced: {path}")

    for file_name, expected_blob in ORIGINAL_TRAVEL_BLOBS.items():
        if file_name not in tracked:
            errors.append(f"original TravelNetwork file missing: {file_name}")
            continue
        try:
            actual_blob = subprocess.check_output(
                ["git", "rev-parse", f"HEAD:{file_name}"], cwd=ROOT,
                text=True, stderr=subprocess.PIPE,
            ).strip()
            if actual_blob != expected_blob:
                errors.append(f"TravelNetwork original 10.6 blob drift: {file_name} {actual_blob}")
        except subprocess.CalledProcessError:
            errors.append(f"TravelNetwork file unreadable as tracked HEAD blob: {file_name}")

    compiled = {name: re.compile(pattern) for name, pattern in FORBIDDEN.items()}
    for name in active:
        try:
            source = (ROOT / name).read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{name}: cannot read: {exc}")
            continue
        for title, pattern in compiled.items():
            found = pattern.search(source)
            if found:
                line = source.count("\n", 0, found.start()) + 1
                errors.append(f"{name}:{line}: {title}: {found.group()!r}")

    for name, anchors in REQUIRED.items():
        try:
            source = (ROOT / name).read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{name}: cannot read required source: {exc}")
            continue
        for anchor in anchors:
            if anchor not in source:
                errors.append(f"{name}: preservation anchor missing: {anchor!r}")

    # The live build documentation must not re-advertise deleted functionality.
    doc = (ROOT / "README_BUILD.txt").read_text(encoding="utf-8-sig")
    for token in ("Hủy giao dịch Phá hoại", "SOURCE_SACH_V23_DISCARD_CONFIRM"):
        if token in doc:
            errors.append(f"README_BUILD.txt: legacy build/functionality claim: {token}")

    controller = (ROOT / "generated_runtime/controller.cpp").read_text(encoding="utf-8-sig")
    for token in (
        "ShortcutKind::ThdcRoute", "ShortcutKind::InterserverGate",
        "HandleShortcutInterserverGate(", "HandleThdcRoute(",
        "thdc_route_logic::", "pk_tl_lm_logic::",
    ):
        if token in controller:
            errors.append(f"retired THDC/interserver route reintroduced: {token}")

    if errors:
        print("T20 FINAL ZERO-REFERENCE / PRESERVATION FAIL:")
        for error in errors:
            print(" - " + error)
        return 1

    print(
        f"T20 final static audit PASS: {len(active)} tracked active source/build files, "
        f"{len(compiled)} retired-feature families absent, "
        f"{sum(map(len, REQUIRED.values()))} preservation anchors PASS, "
        f"{len(ORIGINAL_TRAVEL_BLOBS)} original TravelNetwork blobs intact, "
        f"{len(DELETED)} removed files absent"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
