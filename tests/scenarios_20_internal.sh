#!/usr/bin/env bash
# Internal integration: run scenarios 20 with repo bin/bao directly.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export BAO="${BAO:-$ROOT/bin/bao}"
# shellcheck source=tests/scenarios_20_common.sh
source "$ROOT/tests/scenarios_20_common.sh"
run_scenarios_20
