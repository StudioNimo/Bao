#!/usr/bin/env bash
# 外部結合: PATH 上の bao（bin を先頭に）で 20 シナリオを検証。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/bin:$PATH"
export BAO=bao
# shellcheck source=tests/scenarios_20_common.sh
source "$ROOT/tests/scenarios_20_common.sh"
run_scenarios_20
