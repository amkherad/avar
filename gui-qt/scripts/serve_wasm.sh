#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/wasm"
PORT="${PORT:-8765}"
HOST="${HOST:-127.0.0.1}"

if [[ ! -f "${BUILD_DIR}/avar-gui-qt.html" ]]; then
  echo "Build wasm first: QT_ROOT=... ./scripts/build_wasm.sh" >&2
  exit 1
fi

app_url() {
  echo "http://${HOST}:${1}/avar-gui-qt.html"
}

port_listening() {
  ss -tln 2>/dev/null | grep -q ":${1} "
}

if curl -sf -o /dev/null --max-time 1 "$(app_url "${PORT}")"; then
  echo "Already serving wasm at $(app_url "${PORT}")"
  exit 0
fi

if port_listening "${PORT}"; then
  echo "Port ${PORT} is in use by another process (not this wasm app)." >&2
  echo "Use another port: PORT=8770 $0" >&2
  exit 1
fi

if [[ -f "${ROOT}/../.emsdk/emsdk_env.sh" ]]; then
  source "${ROOT}/../.emsdk/emsdk_env.sh"
elif [[ -n "${EMSDK:-}" && -f "${EMSDK}/emsdk_env.sh" ]]; then
  source "${EMSDK}/emsdk_env.sh"
fi

if command -v emrun >/dev/null; then
  echo "Open $(app_url "${PORT}")"
  exec emrun --no_browser --port "${PORT}" --hostname "${HOST}" "${BUILD_DIR}/avar-gui-qt.html"
fi

cd "${BUILD_DIR}"
echo "Open $(app_url "${PORT}") (python fallback)"
exec python3 -m http.server "${PORT}" --bind "${HOST}"
