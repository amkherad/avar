#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

find_test_bin() {
    local name=$1
    find "$BUILD_DIR" \( -name "$name" -o -name "$name.exe" \) -type f 2>/dev/null | head -1
}

run_test() {
    local name=$1
    local bin
    bin="$(find_test_bin "$name")"
    if [[ -z "$bin" ]]; then
        echo "Test binary not found: $name" >&2
        exit 1
    fi
    echo "==> $name"
    "$bin"
}

run_test test_utils
run_test test_http
run_test test_download_state
run_test test_config_array
run_test test_download_integration

echo "All tests passed"
