#!/usr/bin/env bash
# Safe parity loop: re-runs audit only (no agent injection). Default: every 10 minutes.
set -euo pipefail
INTERVAL="${1:-600}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Parity audit loop every ${INTERVAL}s (Ctrl+C to stop)"
while true; do
  python3 "${ROOT}/scripts/parity_audit.py"
  sleep "${INTERVAL}"
done
