#!/usr/bin/env python3
"""Create a portable GNU tar.xz bundle with avar + the unpacked desktop app."""

from __future__ import annotations

import argparse
import platform
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import textwrap
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from paths import DIST_DIR, ROOT, rel


def find_electron_unpacked(gui_dir: Path) -> Path:
    release = gui_dir / "release"
    if not release.is_dir():
        raise SystemExit(f"Electron release directory not found: {release}")

    system = platform.system()
    if system == "Windows":
        patterns = ["win-unpacked", "win-*-unpacked"]
    elif system == "Darwin":
        patterns = ["mac-*-unpacked", "mac", "mac-arm64", "mac-x64"]
    else:
        patterns = ["linux-unpacked", "linux-*-unpacked"]

    for pattern in patterns:
        for path in sorted(release.glob(pattern)):
            if path.is_dir():
                return path

    raise SystemExit(f"No Electron unpacked directory found under {release}")


def find_desktop_entry(unpacked: Path) -> Path:
    system = platform.system()
    if system == "Windows":
        for name in ("Avar.exe", "avar.exe"):
            candidate = unpacked / name
            if candidate.is_file():
                return candidate
        raise SystemExit(f"No desktop executable found in {unpacked}")

    if system == "Darwin":
        for app in sorted(unpacked.glob("*.app")):
            macos_dir = app / "Contents" / "MacOS"
            if not macos_dir.is_dir():
                continue
            for candidate in macos_dir.iterdir():
                if candidate.is_file() and candidate.stat().st_mode & stat.S_IXUSR:
                    return candidate
        raise SystemExit(f"No macOS desktop executable found in {unpacked}")

    for name in ("avar", "Avar"):
        candidate = unpacked / name
        if candidate.is_file() and candidate.stat().st_mode & stat.S_IXUSR:
            return candidate

    for candidate in sorted(unpacked.iterdir()):
        if candidate.is_file() and candidate.stat().st_mode & stat.S_IXUSR:
            return candidate

    raise SystemExit(f"No Linux desktop executable found in {unpacked}")


def copy_tree(src: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest, symlinks=True)


def write_launcher(root: Path, desktop_rel: Path) -> None:
    bin_dir = root / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    if platform.system() == "Windows":
        launcher = bin_dir / "avar-gui.cmd"
        launcher.write_text(
            textwrap.dedent(
                f"""\
                @echo off
                setlocal
                set "ROOT=%~dp0.."
                start "" "%ROOT%\\{desktop_rel.as_posix().replace('/', '\\\\')}"
                """
            ),
            encoding="utf-8",
            newline="\r\n",
        )
        return

    launcher = bin_dir / "avar-gui"
    launcher.write_text(
        textwrap.dedent(
            f"""\
            #!/bin/sh
            set -eu
            ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
            exec "$ROOT/{desktop_rel.as_posix()}" "$@"
            """
        ),
        encoding="utf-8",
        newline="\n",
    )
    launcher.chmod(0o755)


def write_readme(root: Path, version: str) -> None:
    readme = root / "README.txt"
    readme.write_text(
        textwrap.dedent(
            f"""\
            Avar {version} — portable desktop bundle
            ========================================

            Contents:
              bin/avar       CLI and daemon backend
              bin/avar-gui   Desktop GUI launcher
              share/avar/desktop/   Electron desktop application files

            Quick start:
              1. Run ./bin/avar daemon start --http
              . Or launch ./bin/avar-gui (starts the desktop shell)

            The desktop shell bundles the backend and starts it automatically when needed.
            """
        ),
        encoding="utf-8",
        newline="\n",
    )


def gnu_bundle_name(os_name: str, arch: str, version: str) -> str:
    return f"avar-desktop-{os_name}-{arch}-{version}.tar.xz"


def create_gnu_bundle(
    *,
    avar_exe: Path,
    unpacked: Path,
    output: Path,
    version: str,
    os_name: str,
    arch: str,
) -> None:
    desktop_entry = find_desktop_entry(unpacked)
    desktop_rel = Path("share/avar/desktop") / desktop_entry.relative_to(unpacked)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / f"avar-desktop-{os_name}-{arch}-{version}"
        bin_dir = root / "bin"
        share_desktop = root / "share" / "avar" / "desktop"
        bin_dir.mkdir(parents=True)
        share_desktop.parent.mkdir(parents=True)

        backend_name = "avar.exe" if platform.system() == "Windows" else "avar"
        shutil.copy2(avar_exe, bin_dir / backend_name)
        (bin_dir / backend_name).chmod(0o755)
        copy_tree(unpacked, share_desktop)
        write_launcher(root, desktop_rel)
        write_readme(root, version)

        output.parent.mkdir(parents=True, exist_ok=True)
        temp_output = output.with_suffix(output.suffix + ".tmp")
        if temp_output.exists():
            temp_output.unlink()

        if shutil.which("tar"):
            subprocess.run(
                ["tar", "-cJf", str(temp_output), "-C", str(root.parent), root.name],
                check=True,
            )
        else:
            with tarfile.open(temp_output, "w:xz") as tar:
                tar.add(root, arcname=root.name)

        if output.exists():
            output.unlink()
        temp_output.replace(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="Built avar executable")
    parser.add_argument("--gui-dir", type=Path, default=ROOT / "gui")
    parser.add_argument("--unpacked", type=Path, help="Electron unpacked directory")
    parser.add_argument("--version", required=True)
    parser.add_argument("--os", dest="os_name", choices=("windows", "linux", "mac"), required=True)
    parser.add_argument("--arch", choices=("x86_64", "arm64"), required=True)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DIST_DIR,
        help=f"Output directory (default: {rel(DIST_DIR)})",
    )
    args = parser.parse_args()

    exe = args.exe.resolve()
    if not exe.is_file():
        raise SystemExit(f"Executable not found: {exe}")

    gui_dir = args.gui_dir.resolve()
    unpacked = args.unpacked.resolve() if args.unpacked else find_electron_unpacked(gui_dir)

    output_dir = args.output_dir.resolve()
    output = output_dir / gnu_bundle_name(args.os_name, args.arch, args.version)
    create_gnu_bundle(
        avar_exe=exe,
        unpacked=unpacked,
        output=output,
        version=args.version,
        os_name=args.os_name,
        arch=args.arch,
    )
    print(f"Created {output}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode) from exc
