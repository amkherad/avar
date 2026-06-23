"""Shared build output paths for Avar scripts and automation."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "output"

BUILD_DIR = OUTPUT_DIR / "build"
BUILD_GUI_DIR = OUTPUT_DIR / "build-gui"
BUILD_ALL_DIR = OUTPUT_DIR / "build-all"
BUILD_COVERAGE_DIR = OUTPUT_DIR / "build-coverage"
DIST_DIR = OUTPUT_DIR / "dist"
DIST_ALL_DIR = OUTPUT_DIR / "dist-all"
COVERAGE_XML = OUTPUT_DIR / "coverage.xml"


def rel(path: Path) -> str:
    """Path relative to the repository root (POSIX separators)."""
    return path.relative_to(ROOT).as_posix()
