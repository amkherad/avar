#!/usr/bin/env python3
"""Configure, build, test, and emit Cobertura coverage for the Avar C backend."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def find_gcovr() -> str:
    gcovr = shutil.which("gcovr")
    if gcovr is not None:
        return gcovr

    import importlib.util

    if importlib.util.find_spec("gcovr") is not None:
        return sys.executable

    raise RuntimeError(
        "gcovr is required. Install it with: pip install gcovr"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        default="build-coverage",
        help="CMake build directory (default: build-coverage)",
    )
    parser.add_argument(
        "--output",
        default="coverage.xml",
        help="Cobertura XML output path (default: coverage.xml)",
    )
    parser.add_argument(
        "--min-line-coverage",
        type=float,
        default=85.0,
        help="Fail when line coverage is below this percentage (default: 85)",
    )
    parser.add_argument(
        "--skip-configure",
        action="store_true",
        help="Skip the CMake configure step",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip the CMake build step",
    )
    parser.add_argument(
        "--skip-tests",
        action="store_true",
        help="Skip running ctest",
    )
    parser.add_argument(
        "--generator",
        default=None,
        help="CMake generator (for example Ninja or Unix Makefiles)",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    build_dir = (root / args.build_dir).resolve()
    output = (root / args.output).resolve() if not os.path.isabs(args.output) else Path(args.output)
    src_root = (root / "src").as_posix()
    third_party = (root / "src" / "third_party").as_posix()
    main_c = (root / "src" / "main.c").as_posix()
    daemon_install = (root / "src" / "daemon" / "daemon_install.c").as_posix()
    torrent_c = (root / "src" / "torrent.c").as_posix()
    transport_c = (root / "src" / "transport.c").as_posix()

    configure_cmd = [
        "cmake",
        "-S",
        str(root),
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DAVAR_ENABLE_COVERAGE=ON",
    ]
    if args.generator:
        configure_cmd.extend(["-G", args.generator])
    elif sys.platform == "win32":
        configure_cmd.extend(["-G", "MinGW Makefiles"])

    if not args.skip_configure:
        run(configure_cmd, cwd=root)
    if not args.skip_build:
        run(["cmake", "--build", str(build_dir), "--parallel"], cwd=root)
    if not args.skip_tests:
        run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"], cwd=root)

    gcovr_exe = find_gcovr()
    gcovr_cmd = [gcovr_exe]
    if gcovr_exe == sys.executable:
        gcovr_cmd.extend(["-m", "gcovr"])
    gcovr_cmd.extend(
        [
            "--root",
            root.as_posix(),
            "--object-directory",
            build_dir.as_posix(),
            "--filter",
            src_root,
            "--exclude",
            third_party,
            "--exclude",
            main_c,
            "--exclude",
            daemon_install,
            "--exclude",
            torrent_c,
            "--exclude",
            transport_c,
            "--cobertura",
            str(output),
            "--print-summary",
            "--gcov-ignore-parse-errors",
            "negative_hits.warn_once_per_file",
            "--gcov-ignore-errors",
            "no_working_dir_found",
            "--fail-under-line",
            str(args.min_line_coverage),
        ]
    )

    env = os.environ.copy()
    env.setdefault("PYTHONUTF8", "1")
    run(gcovr_cmd, cwd=root, env=env)

    print(f"Cobertura report written to {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode) from exc
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1) from exc
