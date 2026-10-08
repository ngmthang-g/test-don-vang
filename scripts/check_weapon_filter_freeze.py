#!/usr/bin/env python3
"""T16: fail CI if the approved C1/C2 filter implementation drifts.

These are Git blob IDs from the T15 PASS checkpoint 70d94ef34056. The historical
10.6 ZIP has already undergone approved portable-config removal (T09/T10);
we freeze the surviving *runtime filter* files, not the superseded ZIP bytes.
This does not replace live-game image/screenshot regression.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Pin both paths' blob identities, not the commit SHA. A shallow Actions checkout
# can verify them without fetching prior commit history.
FROZEN_BLOBS = {
    "generated_runtime/image_scan_test.cpp": "9a06796d3f2b9dc69bcdac39e6f6b532c5d5b225",
    "generated_runtime/image_scan_test.h": "66aba897a38872029ff369556e34bfcec47db2cb",
    "generated_runtime/image_scan_auto_ext.inl": "2f51255b0cb53b6a5ea3e4d1509b30b5f7ea94f5",
    "src/bag_filter_v2_logic.h": "77f97dd89546f95d14298d6d5d1295cc1d50f30e",
    "src/con_filter_nosleep_logic.h": "81c2512f4065b5d6ec85f3ed960cc720eb1595bc",
    "src/sell_filter_grid_v2_logic.h": "3fd5b84b5657b53af6eb3634aeb126898b7b9570",
}

# Mutable controller/bridge/protocol have non-filter cleanup tasks ahead.
# Verify only approved filter integration / stable protocol contract anchors.
REQUIRED_ANCHORS = {
    "generated_runtime/controller.cpp": (
        'ReadIniInt(L"WeaponFilterV2",L"Mode",1)==2',
        "image_scan_test::FilterMode::SemanticBag",
        "image_scan_test::FilterMode::V4",
        "void ToggleFilterMode2()",
        'WriteIniInt(L"WeaponFilterV2",L"Mode",enable?2:1)',
        "bool ReadAllBagSemantic(",
        "Command::ReadBagPage",
        "bool BuildSemanticDropCandidates(",
        "bag_filter_v2_logic::ShouldDrop(d)",
        "image_scan_test::TickAutoFilter(",
        "image_scan_test::FullBagTravelLatched(",
        "image_scan_test::FullBagYieldReady(",
        "image_scan_test::NotifyDeath(",
        "image_scan_test::NotifyReviveClicked(",
        "image_scan_test::NotifyTradeFlowStarted(",
    ),
    "generated_runtime/protocol.h": (
        "ReadBagPage = 19,",
        "DropBagItem = 20,",
        "ClickInternalPointRawTest = 26,",
        "DragInternalPoint = 27,",
        "ProbeUiDirect = 32,",
        "InvokeUiDirect = 33,",
    ),
    "generated_runtime/bridge.cpp": (
        "case Command::ReadBagPage:",
        "case Command::ClickInternalPointRawTest:",
        "case Command::DragInternalPoint:",
    ),
}

ROOT = Path(__file__).resolve().parents[1]


def blob_sha(path: str) -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD:" + path],
        cwd=ROOT, capture_output=True, text=True, check=True
    )
    return result.stdout.strip().lower()


def main() -> int:
    errors = []
    for path, expected in FROZEN_BLOBS.items():
        try:
            actual = blob_sha(path)
        except (OSError, subprocess.CalledProcessError) as exc:
            errors.append(f"{path}: unable to read tracked Git blob: {exc}")
            continue
        if actual != expected:
            errors.append(f"{path}: changed (expected {expected}, got {actual})")
        elif not (ROOT / path).is_file():
            errors.append(f"{path}: checkout file missing")

    for path, anchors in REQUIRED_ANCHORS.items():
        try:
            text = (ROOT / path).read_text(encoding="utf-8-sig")
        except OSError as exc:
            errors.append(f"{path}: unreadable: {exc}")
            continue
        for anchor in anchors:
            if anchor not in text:
                errors.append(f"{path}: missing preserved filter contract: {anchor}")

    if errors:
        print("WEAPON FILTER FREEZE FAIL:")
        for problem in errors:
            print(" - " + problem)
        return 1
    print(
        f"WEAPON FILTER FREEZE PASS: {len(FROZEN_BLOBS)} byte-identical Git blobs, "
        f"{sum(map(len, REQUIRED_ANCHORS.values()))} live integration anchors"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
