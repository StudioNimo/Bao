#!/usr/bin/env bash
# External integration: S21 only (bao on PATH).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/bin:$PATH"
export BAO=bao
# shellcheck source=tests/scenarios_21_extra_common.sh
source "$ROOT/tests/scenarios_21_extra_common.sh"
run_scenarios_21_extra
