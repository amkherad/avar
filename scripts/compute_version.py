#!/usr/bin/env python3
"""Compute Avar version from version.json and git commit height (NBGV-style).

major.minor come from version.json; revision is the number of commits on the
current branch since the last commit that changed version.json.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from datetime import UTC, datetime
from pathlib import Path


@dataclass(frozen=True)
class VersionInfo:
    major: int
    minor: int
    revision: int
    version: str
    commit: str
    commit_full: str


def _run_git(args: list[str], *, cwd: Path) -> str | None:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=cwd,
            check=True,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


def _parse_major_minor(raw: str) -> tuple[int, int]:
    match = re.fullmatch(r"(\d+)\.(\d+)(?:\.\d+)?", raw.strip())
    if not match:
        raise ValueError(f"version.json 'version' must be major.minor, got {raw!r}")
    return int(match.group(1)), int(match.group(2))


def compute_version(root: Path) -> VersionInfo:
    version_file = root / "version.json"
    if not version_file.is_file():
        return VersionInfo(0, 0, 0, "0.0.0", "", "")

    data = json.loads(version_file.read_text(encoding="utf-8-sig"))
    major, minor = _parse_major_minor(str(data.get("version", "0.0")))

    revision = 0
    commit = ""
    commit_full = ""

    if (root / ".git").exists():
        version_commit = _run_git(
            ["log", "-1", "--format=%H", "--", "version.json"],
            cwd=root,
        )
        if version_commit:
            count = _run_git(
                ["rev-list", "--count", f"{version_commit}..HEAD"],
                cwd=root,
            )
            if count is not None:
                revision = int(count)

        commit_full = _run_git(["rev-parse", "HEAD"], cwd=root) or ""
        commit = _run_git(["rev-parse", "--short=7", "HEAD"], cwd=root) or ""

    version = f"{major}.{minor}.{revision}"
    return VersionInfo(major, minor, revision, version, commit, commit_full)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root (default: parent of scripts/)",
    )
    parser.add_argument(
        "--format",
        choices=("json", "version", "cmake"),
        default="json",
        help="Output format (default: json)",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    try:
        info = compute_version(root)
    except (ValueError, json.JSONDecodeError) as exc:
        print(exc, file=sys.stderr)
        return 1

    if args.format == "version":
        print(info.version)
    elif args.format == "cmake":
        print(f"set(AVAR_VERSION_MAJOR {info.major})")
        print(f"set(AVAR_VERSION_MINOR {info.minor})")
        print(f"set(AVAR_VERSION_REVISION {info.revision})")
        print(f'set(AVAR_VERSION_STRING "{info.version}")')
        print(f'set(AVAR_VERSION_COMMIT "{info.commit}")')
        print(f'set(AVAR_VERSION_COMMIT_FULL "{info.commit_full}")')
    else:
        payload = asdict(info)
        payload["date"] = datetime.now(UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")
        print(json.dumps(payload, indent=2))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
