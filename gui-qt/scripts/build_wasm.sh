#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/wasm"
CONFIG="Release"

if [[ -z "${QT_ROOT:-}" ]]; then
  echo "Set QT_ROOT to your Qt for WebAssembly install (e.g. ~/Qt/6.8.0/wasm_singlethread)" >&2
  exit 1
fi

TOOLCHAIN="${QT_ROOT}/lib/cmake/Qt6/qt.toolchain.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "Qt wasm toolchain not found: ${TOOLCHAIN}" >&2
  exit 1
fi

HOST_QT="${QT_HOST_PATH:-${QT_ROOT%/wasm_*}/gcc_64}"
if [[ ! -f "${HOST_QT}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "Host Qt not found at ${HOST_QT}. Install gcc_64 (aqt install-qt linux desktop 6.4.2 gcc_64) or set QT_HOST_PATH." >&2
  exit 1
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
  -DQT_HOST_PATH="${HOST_QT}" \
  -DAVAR_GUI_QT_WASM=ON \
  -DAVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS=OFF

cmake --build "${BUILD_DIR}" --parallel
echo "Wasm artifacts: ${BUILD_DIR}/avar-gui-qt.html"
