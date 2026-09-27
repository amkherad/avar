#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/desktop"
CONFIG="Release"
GENERATOR="Ninja"
EXTRA_CMAKE_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config) CONFIG="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --generator) GENERATOR="$2"; shift 2 ;;
    --) shift; EXTRA_CMAKE_ARGS+=("$@"); break ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -DAVAR_GUI_QT_WASM=OFF \
  "${EXTRA_CMAKE_ARGS[@]}"

cmake --build "${BUILD_DIR}" --parallel
echo "Built: ${BUILD_DIR}/avar-gui-qt"
