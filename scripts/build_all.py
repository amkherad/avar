#!/usr/bin/env python3
"""Build the all-in-one avar binary with embedded web UI and Electron runtime."""

from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def run(cmd: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, cwd=cwd or ROOT)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-all", help="CMake build directory")
    parser.add_argument(
        "--config",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        help="CMake build type",
    )
    parser.add_argument("--output-dir", default="dist", help="Output directory for artifacts")
    parser.add_argument(
        "--skip-electron",
        action="store_true",
        help="Skip Electron desktop build and embedding (web UI only)",
    )
    parser.add_argument("--generator", default="Ninja", help="CMake generator (Ninja, Unix Makefiles, ...)")
    args = parser.parse_args()

    build_dir = ROOT / args.build_dir
    output_dir = ROOT / args.output_dir
    gui_dir = ROOT / "gui"
    output_dir.mkdir(parents=True, exist_ok=True)

    run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_version.py"),
            "--root",
            str(ROOT),
            "--out-env",
            str(gui_dir / ".avar-build.env"),
        ]
    )

    if not args.skip_electron:
        run(["npm", "ci"], cwd=gui_dir)
        run(["npm", "run", "build:desktop:current"], cwd=gui_dir)

    run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(build_dir),
            f"-DCMAKE_BUILD_TYPE={args.config}",
            "-DAVAR_BUILD_ALL=ON",
            "-G",
            args.generator,
        ]
    )
    run(["cmake", "--build", str(build_dir), "--target", "avar", "--parallel"])

    exe_name = "avar.exe" if platform.system() == "Windows" else "avar"
    built_exe = build_dir / exe_name
    if not built_exe.is_file():
        built_exe = build_dir / "avar"
    if not built_exe.is_file():
        raise SystemExit(f"Built executable not found under {build_dir}")

    if not args.skip_electron:
        run(
            [
                sys.executable,
                str(ROOT / "scripts" / "embed_electron.py"),
                "--gui-dir",
                str(gui_dir),
                "--archive",
                str(build_dir / "electron-bundle.tar"),
                "--exe",
                str(built_exe),
            ]
        )

    shutil.copy2(built_exe, output_dir / exe_name)
    print(f"All-in-one binary: {output_dir / exe_name}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode) from exc
