#!/usr/bin/env python3
"""Package the experimental Tiny desktop shell for release."""

from __future__ import annotations

import argparse
import platform
import shutil
import sys
import zipfile
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from compute_version import compute_version
from paths import ROOT, rel


DIST_TINY_DIR = ROOT / "output" / "dist-tiny"


def detect_platform() -> tuple[str, str]:
    system = platform.system()
    machine = platform.machine().lower()
    if machine in ("amd64", "x86_64"):
        arch = "x86_64"
    elif machine in ("arm64", "aarch64"):
        arch = "arm64"
    else:
        raise SystemExit(f"Unsupported architecture: {machine}")

    if system == "Windows":
        os_name = "windows"
    elif system == "Darwin":
        os_name = "mac"
    elif system == "Linux":
        os_name = "linux"
    else:
        raise SystemExit(f"Unsupported OS: {system}")
    return os_name, arch


def release_basename(os_name: str, arch: str, version: str) -> str:
    return f"avar-tiny-{os_name}-{arch}-{version}.zip"


def write_launcher(staging: Path, os_name: str) -> None:
    readme = staging / "README.txt"
    readme.write_text(
        "\n".join(
            [
                "Avar Tiny desktop shell (experimental)",
                "",
                "Requires Node.js 20+ and a running Avar daemon (HTTP on port 8000).",
                "Configure the daemon URL in GUI session settings.",
                "",
                "Linux/macOS: ./run-avar-tiny.sh",
                "Windows: run-avar-tiny.cmd",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )

    if os_name == "windows":
        launcher = staging / "run-avar-tiny.cmd"
        launcher.write_text(
            "@echo off\r\n"
            "cd /d %~dp0\r\n"
            "node tiny\\main.cjs\r\n",
            encoding="utf-8",
            newline="\r\n",
        )
        return

    launcher = staging / "run-avar-tiny.sh"
    launcher.write_text(
        "#!/bin/sh\n"
        'cd "$(dirname "$0")" || exit 1\n'
        "exec node tiny/main.cjs\n",
        encoding="utf-8",
        newline="\n",
    )
    launcher.chmod(0o755)


def copy_tree(src: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest)


def package_tiny(gui_dir: Path, output: Path, os_name: str) -> None:
    dist_dir = gui_dir / "dist"
    tiny_dir = gui_dir / "tiny"
    desktop_dir = gui_dir / "desktop"
    tinytron_dir = gui_dir / "node_modules" / "tinytron"
    addon = tinytron_dir / "build" / "Release" / "addon.node"

    for path in (dist_dir, tiny_dir, desktop_dir, addon):
        if not path.exists():
            raise SystemExit(f"Missing Tiny build input: {path}")

    staging = output.parent / f".staging-{output.stem}"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    try:
        copy_tree(dist_dir, staging / "dist")
        copy_tree(tiny_dir, staging / "tiny")
        copy_tree(desktop_dir, staging / "desktop")
        copy_tree(tinytron_dir, staging / "node_modules" / "tinytron")
        write_launcher(staging, os_name)

        if output.exists():
            output.unlink()

        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for file_path in sorted(staging.rglob("*")):
                if file_path.is_file():
                    archive.write(file_path, file_path.relative_to(staging).as_posix())
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui-dir", type=Path, default=ROOT / "gui")
    parser.add_argument("--version", help="Release version (default: from version.json + git)")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DIST_TINY_DIR,
        help=f"Packaged release output directory (default: {rel(DIST_TINY_DIR)})",
    )
    parser.add_argument("--os", dest="os_name", choices=("windows", "linux", "mac"))
    parser.add_argument("--arch", choices=("x86_64", "arm64"))
    args = parser.parse_args()

    os_name, arch = detect_platform()
    if args.os_name:
        os_name = args.os_name
    if args.arch:
        arch = args.arch

    version = args.version
    if not version:
        version = compute_version(ROOT).version

    gui_dir = args.gui_dir.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / release_basename(os_name, arch, version)

    package_tiny(gui_dir, output, os_name)
    print(f"Created {output}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
