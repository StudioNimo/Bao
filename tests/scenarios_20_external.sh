#!/usr/bin/env bash
# External integration: scenarios 20 with bao on PATH (bin first).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/bin:$PATH"
export BAO=bao
# shellcheck source=tests/scenarios_20_common.sh
source "$ROOT/tests/scenarios_20_common.sh"
run_scenarios_20
