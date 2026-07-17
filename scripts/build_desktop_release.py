#!/usr/bin/env python3
"""Build desktop release packages bundling the avar backend and Electron GUI."""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from compute_version import compute_version
from paths import BUILD_DIR, DIST_DIR, ROOT, rel


def resolve_npm() -> str:
    for name in ("npm.cmd", "npm") if platform.system() == "Windows" else ("npm",):
        path = shutil.which(name)
        if path:
            return path
    raise SystemExit("npm not found on PATH")


def run(cmd: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(
        cmd,
        check=True,
        cwd=cwd or ROOT,
        env=env,
        shell=platform.system() == "Windows",
    )


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
        return "windows", arch
    if system == "Darwin":
        return "mac", arch
    if system == "Linux":
        return "linux", arch
    raise SystemExit(f"Unsupported OS: {system}")


def electron_builder_flag(os_name: str) -> str:
    return {"windows": "--win", "mac": "--mac", "linux": "--linux"}[os_name]


def resolve_avar_exe(build_dir: Path) -> Path:
    for name in ("avar.exe", "avar"):
        candidate = build_dir / name
        if candidate.is_file():
            return candidate
    raise SystemExit(f"Built avar executable not found under {build_dir}")


def collect_installer_artifacts(release_dir: Path) -> list[Path]:
    patterns = (
        "*-setup.exe",
        "*-portable.exe",
        "*.dmg",
        "*.deb",
        "*.AppImage",
        "*.zip",
    )
    artifacts: list[Path] = []
    for pattern in patterns:
        artifacts.extend(sorted(release_dir.glob(pattern)))
    return artifacts


def rename_artifact(source: Path, output_dir: Path, os_name: str, arch: str, version: str) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    name = source.name.lower()

    if name.endswith(".exe") and "portable" in name:
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}-portable.exe"
    elif name.endswith(".exe"):
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}-setup.exe"
    elif name.endswith(".dmg"):
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}.dmg"
    elif name.endswith(".deb"):
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}.deb"
    elif name.endswith(".AppImage"):
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}.AppImage"
    elif name.endswith(".zip"):
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}.zip"
    else:
        dest = output_dir / f"avar-desktop-{os_name}-{arch}-{version}{source.suffix}"

    shutil.copy2(source, dest)
    return dest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=BUILD_DIR,
        help=f"CMake build directory with avar (default: {rel(BUILD_DIR)})",
    )
    parser.add_argument("--version", help="Release version (default: from version.json + git)")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DIST_DIR,
        help=f"Packaged release output directory (default: {rel(DIST_DIR)})",
    )
    parser.add_argument("--os", dest="os_name", choices=("windows", "linux", "mac"))
    parser.add_argument("--arch", choices=("x86_64", "arm64"))
    parser.add_argument(
        "--skip-cmake",
        action="store_true",
        help="Skip cmake configure/build (avar must already exist in --build-dir)",
    )
    parser.add_argument(
        "--skip-installers",
        action="store_true",
        help="Only build the unpacked bundle and GNU archive",
    )
    parser.add_argument("--generator", default="Ninja", help="CMake generator")
    args = parser.parse_args()

    os_name, arch = detect_platform()
    if args.os_name:
        os_name = args.os_name
    if args.arch:
        arch = args.arch

    version = args.version or compute_version(ROOT).version
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    gui_dir = ROOT / "gui"
    release_dir = gui_dir / "release"

    if not args.skip_cmake:
        run(
            [
                "cmake",
                "-S",
                str(ROOT),
                "-B",
                str(build_dir),
                "-DCMAKE_BUILD_TYPE=Release",
                "-G",
                args.generator,
            ]
        )
        run(["cmake", "--build", str(build_dir), "--target", "avar", "--parallel"])

    avar_exe = resolve_avar_exe(build_dir)

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

    run(
        [
            sys.executable,
            str(ROOT / "scripts" / "stage_desktop_bundle.py"),
            "--exe",
            str(avar_exe),
        ]
    )

    npm = resolve_npm()
    run([npm, "ci"], cwd=gui_dir)
    run([npm, "run", "build"], cwd=gui_dir)

    builder_bin = "electron-builder.cmd" if platform.system() == "Windows" else "electron-builder"
    builder_path = gui_dir / "node_modules" / ".bin" / builder_bin
    builder_env = os.environ.copy()
    builder_env["CSC_IDENTITY_AUTO_DISCOVERY"] = "false"

    if not args.skip_installers:
        installer_args = [
            str(builder_path),
            "--config",
            str(gui_dir / "electron-builder.config.cjs"),
            "--publish",
            "never",
            electron_builder_flag(os_name),
        ]
        run(installer_args, cwd=gui_dir, env=builder_env)
    else:
        dir_args = [
            str(builder_path),
            "--config",
            str(gui_dir / "electron-builder.config.cjs"),
            "--publish",
            "never",
            electron_builder_flag(os_name),
            "--dir",
        ]
        run(dir_args, cwd=gui_dir, env=builder_env)

    created: list[Path] = []

    run(
        [
            sys.executable,
            str(ROOT / "scripts" / "package_gnu_bundle.py"),
            "--exe",
            str(avar_exe),
            "--version",
            version,
            "--os",
            os_name,
            "--arch",
            arch,
            "--output-dir",
            str(output_dir),
        ]
    )
    created.append(output_dir / f"avar-desktop-{os_name}-{arch}-{version}.tar.xz")

    if not args.skip_installers:
        for artifact in collect_installer_artifacts(release_dir):
            created.append(rename_artifact(artifact, output_dir, os_name, arch, version))

    print("\nDesktop release artifacts:", flush=True)
    for path in created:
        if path.is_file():
            size_mib = path.stat().st_size / (1024 * 1024)
            print(f"  {path.name}: {size_mib:.1f} MiB", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode) from exc
