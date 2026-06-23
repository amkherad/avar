#!/usr/bin/env python3
"""Package an Electron unpacked build and append it to avar.exe with a trailer."""

from __future__ import annotations

import argparse
import hashlib
import platform
import shutil
import struct
import sys
import tarfile
from pathlib import Path

MAGIC = b"AVAREMB1"
TRAILER_FMT = "<8sQ64s256s"  # magic, size, sha256 hex, exe_rel
TRAILER_SIZE = struct.calcsize(TRAILER_FMT)


def find_electron_unpacked(gui_dir: Path) -> Path:
    release = gui_dir / "release"
    if not release.is_dir():
        raise SystemExit(f"Electron release directory not found: {release}")

    system = platform.system()
    patterns: list[str]
    if system == "Windows":
        patterns = ["win-unpacked", "win-*-unpacked", "win-*/win-unpacked"]
    elif system == "Darwin":
        patterns = ["mac", "mac-*", "mac-arm64", "mac-x64", "mac-*-unpacked"]
    else:
        patterns = ["linux-unpacked", "linux-*-unpacked", "linux-*/linux-unpacked"]

    for pattern in patterns:
        for path in sorted(release.glob(pattern)):
            if path.is_dir():
                return path

    raise SystemExit(f"No Electron unpacked directory found under {release}")


def find_electron_exe_rel(unpacked: Path) -> str:
    system = platform.system()
    if system == "Windows":
        for name in ("Avar.exe", "avar.exe"):
            if (unpacked / name).is_file():
                return name
        raise SystemExit(f"No Electron executable found in {unpacked}")

    if system == "Darwin":
        for app in sorted(unpacked.glob("*.app")):
            macos_dir = app / "Contents" / "MacOS"
            if not macos_dir.is_dir():
                continue
            for candidate in macos_dir.iterdir():
                if candidate.is_file() and candidate.stat().st_mode & 0o111:
                    return candidate.relative_to(unpacked).as_posix()
        raise SystemExit(f"No macOS Electron executable found in {unpacked}")

    for name in ("avar", "Avar", "avar-gui"):
        path = unpacked / name
        if path.is_file() and path.stat().st_mode & 0o111:
            return name

    for path in sorted(unpacked.iterdir()):
        if path.is_file() and path.stat().st_mode & 0o111:
            return path.name

    raise SystemExit(f"No Linux Electron executable found in {unpacked}")


def create_archive(unpacked: Path, archive_path: Path) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    if archive_path.exists():
        archive_path.unlink()

    with tarfile.open(archive_path, "w") as tar:
        tar.add(unpacked, arcname=".")


def append_archive(exe_path: Path, archive_path: Path, exe_rel: str) -> None:
    data = archive_path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    exe_rel_bytes = exe_rel.encode("utf-8")
    if len(exe_rel_bytes) >= 256:
        raise SystemExit(f"Electron relative path too long: {exe_rel}")

    with exe_path.open("ab") as exe:
        exe.write(data)
        trailer = struct.pack(
            TRAILER_FMT,
            MAGIC,
            len(data),
            digest.encode("ascii"),
            exe_rel_bytes.ljust(256, b"\0"),
        )
        exe.write(trailer)

    print(
        f"Appended {len(data)} bytes ({exe_rel}) to {exe_path} "
        f"(sha256 {digest[:12]}...)",
        flush=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui-dir", type=Path, help="Path to gui/ (build unpacked Electron)")
    parser.add_argument("--unpacked", type=Path, help="Existing Electron unpacked directory")
    parser.add_argument("--archive", type=Path, help="Write intermediate tar here")
    parser.add_argument("--exe", type=Path, help="avar executable to append to")
    parser.add_argument("--archive-only", action="store_true", help="Only create the tar archive")
    args = parser.parse_args()

    if args.unpacked is not None:
        unpacked = args.unpacked
    else:
        gui_dir = args.gui_dir or Path(__file__).resolve().parent.parent / "gui"
        unpacked = find_electron_unpacked(gui_dir)

    exe_rel = find_electron_exe_rel(unpacked)
    archive_path = args.archive or (unpacked.parent / "electron-bundle.tar")
    create_archive(unpacked, archive_path)
    print(f"Created archive {archive_path} from {unpacked}", flush=True)

    if args.archive_only:
        return 0

    if args.exe is None:
        raise SystemExit("--exe is required unless --archive-only is set")

    append_archive(args.exe, archive_path, exe_rel)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
