#!/usr/bin/env bash
# 内部結合: S21 のみ（bin/bao 直接）。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export BAO="${BAO:-$ROOT/bin/bao}"
# shellcheck source=tests/scenarios_21_extra_common.sh
source "$ROOT/tests/scenarios_21_extra_common.sh"
run_scenarios_21_extra
