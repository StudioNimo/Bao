#!/usr/bin/env bash
# bao_prompt_versioning_demo_setup.sh で作成したデモ環境を削除してまっさらに戻す。
# 使い方:
#   ./scripts/bao_prompt_versioning_demo_teardown.sh
# 環境変数:
#   DEMO_ROOT  削除対象（既定: <repo>/demo/prompt_versioning_workspace）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEMO_ROOT="${DEMO_ROOT:-$ROOT/demo/prompt_versioning_workspace}"

ROOT_ABS="$(cd "$ROOT" && pwd)"

if [[ ! -e "$DEMO_ROOT" ]]; then
  echo "nothing to remove: $DEMO_ROOT"
  exit 0
fi

DEMO_ABS="$(cd "$DEMO_ROOT" && pwd)"

if [[ "$DEMO_ABS" != "$ROOT_ABS"/* ]]; then
  echo "refusing unsafe DEMO_ROOT (not under repo root): $DEMO_ABS"
  exit 1
fi

if [[ "$DEMO_ABS" == "$ROOT_ABS" ]] || [[ "$DEMO_ABS" == "$ROOT_ABS"/. ]]; then
  echo "refusing to remove repository root"
  exit 1
fi

echo "+ rm -rf $DEMO_ABS"
rm -rf "$DEMO_ABS"
echo "removed demo workspace."
