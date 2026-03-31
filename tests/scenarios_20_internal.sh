#!/usr/bin/env bash
# 内部結合: リポジトリ内の bin/bao を直接実行して 20 シナリオを検証。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export BAO="${BAO:-$ROOT/bin/bao}"
# shellcheck source=tests/scenarios_20_common.sh
source "$ROOT/tests/scenarios_20_common.sh"
run_scenarios_20
