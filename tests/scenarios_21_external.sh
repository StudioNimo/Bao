#!/usr/bin/env bash
# 外部結合: S21 のみ（PATH 上の bao）。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/bin:$PATH"
export BAO=bao
# shellcheck source=tests/scenarios_21_extra_common.sh
source "$ROOT/tests/scenarios_21_extra_common.sh"
run_scenarios_21_extra
