#!/usr/bin/env python3
"""T19: scope guard for retired 10.6 wire commands and live packaged resources.

No resource is removed when another live consumer exists. This is a static audit,
not an in-game integration test. Windows build and native tests follow separately.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "generated_runtime/protocol.h"
BRIDGE = ROOT / "generated_runtime/bridge.cpp"
CONTROLLER = ROOT / "generated_runtime/controller.cpp"
CMAKE = ROOT / "CMakeLists.txt"
WORKFLOW = ROOT / ".github/workflows/build-gold-only-baseline.yml"
RESOURCE = ROOT / "resources/app.rc"

# Published active numeric contract. Never compact gaps after deleting a feature.
COMMANDS = {
    "None": 0, "ReadState": 1, "ToggleRide": 2, "StartPath": 3,
    "StopPath": 4, "ClickNpc": 5, "ConfirmMap": 6, "Revive": 7,
    "StartAutoFight": 8, "StopAutoFight": 9, "ClickInternalPoint": 14,
    "ReadCurrency": 18, "ReadBagPage": 19, "DropBagItem": 20,
    "SellBagItem": 21, "SelectTargetByRoleID": 22,
    "ClickTravelSemantic": 23, "ConfirmTravelSemantic": 24,
    "ClickInternalPointRawTest": 26, "DragInternalPoint": 27,
    "ProbeNearbyLoot": 28, "PickNearestLoot": 29,
    "ProbeUiDirect": 32, "InvokeUiDirect": 33,
}
RETIRED = {10, 11, 12, 13, 15, 16, 17}
RETIRED_NAMES = (
    "BeginBackgroundSell", "AdvanceBackgroundSell", "SellNextBagItem",
    "CloseBackgroundSell", "BeginBackgroundTreatment",
    "AdvanceBackgroundTreatment", "CloseBackgroundTreatment",
    "BackgroundSellState", "BackgroundTreatmentState",
    "SellFixedBagSlot", "ReadFreeBagSpace",
)
PACKAGED = {
    '1 RT_MANIFEST "app.manifest"': "app.manifest",
    '201 RCDATA "copyright_notice.txt"': "copyright_notice.txt",
    '202 RCDATA "equip_points.csv"': "equip_points.csv",
}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def main() -> int:
    errors: list[str] = []
    try:
        protocol, bridge, controller, cmake, workflow, rc = (
            read(path) for path in
            (PROTOCOL, BRIDGE, CONTROLLER, CMAKE, WORKFLOW, RESOURCE)
        )
    except OSError as exc:
        print("T19 scope guard FAIL: missing source", exc)
        return 1

    enum_match = re.search(r"enum class Command\s*:\s*std::uint32_t\s*\{([^}]*)\}", protocol, re.S)
    if not enum_match:
        errors.append("protocol: Command enum missing")
    else:
        assignments = re.findall(r"^\s*(\w+)\s*=\s*(\d+)\s*,", enum_match.group(1), re.M)
        actual = {name: int(value) for name, value in assignments}
        if actual != COMMANDS:
            errors.append(f"protocol: wire command table drift: {actual!r}")
        if set(actual.values()) & RETIRED:
            errors.append("protocol: retired wire numeric ID reused")
    if "kProtocolVersion = 0x00030500u" not in protocol:
        errors.append("protocol: version drift")
    for retired_name in RETIRED_NAMES:
        if re.search(r"\b" + re.escape(retired_name) + r"\b", protocol + bridge + controller):
            errors.append(f"retired bridge/protocol/controller symbol remains: {retired_name}")
    for marker in (
        "case Command::ReadBagPage:", "case Command::StartPath:",
        "case Command::ClickInternalPoint:", "case Command::SellBagItem:",
        "case Command::ClickTravelSemantic:", "case Command::ConfirmTravelSemantic:",
        "case Command::ProbeNearbyLoot:", "case Command::PickNearestLoot:",
        "case Command::ClickInternalPointRawTest:", "case Command::DragInternalPoint:",
    ):
        if marker not in bridge:
            errors.append(f"active bridge dispatch removed: {marker}")
    for marker in (
        "CountMainUnlockedEquipment(", "RefreshMainCapacityPlan(",
        "BeginMainMacroSell(", "TickActiveMainMacroSell()",
        "image_scan_test::FullBagYieldReady(", "image_scan_test::TickAutoFilter(",
        "ShortcutKind::KunLunEnter", "Command::ReadBagPage",
    ):
        if marker not in controller:
            errors.append(f"active controller path removed: {marker}")

    for declaration, filename in PACKAGED.items():
        if declaration not in rc:
            errors.append(f"resource declaration missing: {declaration}")
        p = ROOT / "resources" / filename
        if not p.is_file() or p.stat().st_size == 0:
            errors.append(f"resource missing/empty: {filename}")
    if 'FindResourceW(instance_,MAKEINTRESOURCEW(202),RT_RCDATA)' not in controller:
        errors.append("EquipPoint RCDATA 202 loader removed")
    if not (ROOT / "resources/equip_points.csv").read_bytes().lstrip(b"\xef\xbb\xbf").startswith(b"ID,EquipPoint"):
        errors.append("EquipPoint CSV header drift")

    native = set(re.findall(r"add_executable\((\w+_tests)\s+", cmake))
    ci = set(re.findall(r"'(\w+_tests)'", workflow))
    if native != ci:
        errors.append(f"CMake/native CI test target mismatch: CMake-only={sorted(native-ci)} CI-only={sorted(ci-native)}")
    if "protocol_wire_contract_tests" not in native:
        errors.append("T19 protocol wire native test missing from CI")
    if 'resources/app.rc)' not in cmake:
        errors.append("resource compilation detached from main executable")

    if errors:
        print("T19 BUILD/RESOURCE/PROTOCOL SCOPE FAIL:")
        for error in errors:
            print(" - " + error)
        return 1
    print(
        f"T19 scope PASS: {len(COMMANDS)} active numeric IDs, {len(RETIRED)} reserved IDs, "
        f"{len(PACKAGED)} live RCDATA/manifest resources, {len(native)} aligned native test targets"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
