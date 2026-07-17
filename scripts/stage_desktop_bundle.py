#!/usr/bin/env python3
"""Stage the avar backend binary for electron-builder extraResources."""

from __future__ import annotations

import argparse
import platform
import shutil
import sys
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from paths import ROOT


def stage_avar_binary(exe: Path, stage_dir: Path) -> Path:
    exe = exe.resolve()
    if not exe.is_file():
        raise SystemExit(f"avar executable not found: {exe}")

    stage_dir.mkdir(parents=True, exist_ok=True)
    dest_name = "avar.exe" if platform.system() == "Windows" else "avar"
    dest = stage_dir / dest_name
    shutil.copy2(exe, dest)
    dest.chmod(0o755)
    return dest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="Built avar executable")
    parser.add_argument(
        "--stage-dir",
        type=Path,
        default=ROOT / "gui" / "build" / "bundle" / "avar",
        help="Directory electron-builder reads via extraResources",
    )
    args = parser.parse_args()

    dest = stage_avar_binary(args.exe, args.stage_dir)
    print(f"Staged backend at {dest}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
