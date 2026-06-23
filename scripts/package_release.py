#!/usr/bin/env python3
"""Package avar release binaries with platform-standard filenames."""

from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))

from compute_version import compute_version


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


def release_basename(kind: str, os_name: str, arch: str, version: str) -> str:
    ext = {"windows": ".exe", "linux": ".deb", "mac": ".dmg"}[os_name]
    return f"{kind}-{os_name}-{arch}-{version}{ext}"


def install_name(kind: str) -> str:
    return "avar-gui" if kind == "avar-gui" else "avar"


def package_windows(exe: Path, output: Path) -> None:
    shutil.copy2(exe, output)


def package_linux_deb(
    exe: Path,
    output: Path,
    *,
    version: str,
    kind: str,
    arch: str,
) -> None:
    package_name = kind
    bin_name = install_name(kind)
    arch_deb = "amd64" if arch == "x86_64" else "arm64"
    description = (
        "Avar Download Manager with embedded desktop GUI"
        if kind == "avar-gui"
        else "Avar Download Manager"
    )

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / f"{package_name}_{version}_{arch_deb}"
        debian = root / "DEBIAN"
        bindir = root / "usr" / "bin"
        debian.mkdir(parents=True)
        bindir.mkdir(parents=True)

        installed = bindir / bin_name
        shutil.copy2(exe, installed)
        installed.chmod(0o755)

        control = (
            f"Package: {package_name}\n"
            f"Version: {version}\n"
            "Section: utils\n"
            "Priority: optional\n"
            f"Architecture: {arch_deb}\n"
            "Maintainer: Ali Kherad <alimousavikherad@gmail.com>\n"
            f"Description: {description}\n"
        )
        (debian / "control").write_text(control, encoding="utf-8", newline="\n")

        subprocess.run(["dpkg-deb", "--build", "--root-owner-group", str(root), str(output)], check=True)


def package_mac_dmg(exe: Path, output: Path, *, kind: str) -> None:
    label = "Avar GUI" if kind == "avar-gui" else "Avar"
    bin_name = install_name(kind)

    with tempfile.TemporaryDirectory() as tmp:
        staging = Path(tmp) / "staging"
        staging.mkdir()
        dest = staging / bin_name
        shutil.copy2(exe, dest)
        dest.chmod(0o755)

        subprocess.run(
            [
                "hdiutil",
                "create",
                "-volname",
                label,
                "-srcfolder",
                str(staging),
                "-ov",
                "-format",
                "UDZO",
                str(output),
            ],
            check=True,
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="Built executable path")
    parser.add_argument("--kind", choices=("avar", "avar-gui"), required=True)
    parser.add_argument("--version", help="Release version (default: from version.json + git)")
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
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
        version = compute_version(Path(__file__).resolve().parent.parent).version

    exe = args.exe.resolve()
    if not exe.is_file():
        raise SystemExit(f"Executable not found: {exe}")

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / release_basename(args.kind, os_name, arch, version)

    if os_name == "windows":
        package_windows(exe, output)
    elif os_name == "linux":
        package_linux_deb(exe, output, version=version, kind=args.kind, arch=arch)
    else:
        package_mac_dmg(exe, output, kind=args.kind)

    print(f"Created {output}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode) from exc
