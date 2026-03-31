#!/usr/bin/env bash
# External integration: run built bin/bao as an external command via PATH.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/bin:$PATH"
TMP="${TMPDIR:-/tmp}/bao_ext_$$"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/wd"
cd "$TMP/wd"
cp -R "$ROOT/examples/sample/bao.yaml" "$ROOT/examples/sample/prompts" "$ROOT/examples/sample/test_cases.jsonl" .

bao init
bao add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
out=$(BAO_NO_EDITOR=1 bao commit --no-edit 2>&1) || true
case "$out" in
  \[???????\]* ) ;;
  *)
    echo "FAIL: commit output should look like [7hex] message, got: $out"
    exit 1
    ;;
esac

# rev-parse full hash
F=$(bao rev-parse HEAD)
if [ "${#F}" -ne 64 ]; then
  echo "FAIL: external: rev-parse len"
  exit 1
fi

# help and version do not crash
bao version >/dev/null
bao help >/dev/null

echo "OK external integration tests"
