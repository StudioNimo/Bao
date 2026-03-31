#!/usr/bin/env bash
# Internal integration: state transitions, objects, and refs in one process.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BAO="${BAO:-$ROOT/bin/bao}"
TMP="${TMPDIR:-/tmp}/bao_test_$$"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/repo"
cd "$ROOT"
[ -x "$BAO" ] || { echo "build bin/bao first (make)"; exit 1; }

cp -R "$ROOT/examples/sample/bao.yaml" "$ROOT/examples/sample/prompts" "$ROOT/examples/sample/test_cases.jsonl" "$TMP/repo/"
cd "$TMP/repo"

"$BAO" init

# 1) Commit object stored as full64 filename
"$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
"$BAO" commit -m "first"
FULL1=$("$BAO" rev-parse HEAD)
if [ "${#FULL1}" -ne 64 ]; then
  echo "FAIL: HEAD should be 64 hex, got len=${#FULL1} '$FULL1'"
  exit 1
fi
OBJ=".bao/objects/commits/${FULL1}.json"
if [ ! -f "$OBJ" ]; then
  echo "FAIL: missing commit object $OBJ"
  exit 1
fi
FH=$(grep -o '"full_hash"[[:space:]]*:[[:space:]]*"[^"]*"' "$OBJ" | sed 's/.*"\([^"]*\)"/\1/')
if [ "$FH" != "$FULL1" ]; then
  echo "FAIL: full_hash in JSON mismatch"
  exit 1
fi

# 2) rev-parse matches full hash
RP=$("$BAO" rev-parse HEAD)
if [ "$RP" != "$FULL1" ]; then
  echo "FAIL: rev-parse HEAD != HEAD file"
  exit 1
fi

# 3) Short display (--short=7)
S7=$("$BAO" rev-parse --short=7 HEAD)
if [ "${#S7}" -ne 7 ] || [ "${FULL1:0:7}" != "$S7" ]; then
  echo "FAIL: short prefix expected ${FULL1:0:7} got $S7"
  exit 1
fi

# 4) Second commit + HEAD~1
"$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
"$BAO" commit -m "second"
FULL2=$("$BAO" rev-parse HEAD)
P1=$("$BAO" rev-parse HEAD~1)
if [ "$P1" != "$FULL1" ]; then
  echo "FAIL: HEAD~1 should be first commit"
  exit 1
fi

# 5) Prefix resolution (unique)
PF=$("$BAO" rev-parse "${FULL2:0:12}")
if [ "$PF" != "$FULL2" ]; then
  echo "FAIL: 12-char prefix should resolve to $FULL2"
  exit 1
fi

# 6) log --oneline starts with ~7 hex
LOG1=$("$BAO" log -n 1 --oneline | head -1 | awk '{print $1}')
if [ "${#LOG1}" -ne 7 ]; then
  echo "FAIL: log oneline should start with 7-char short, got '$LOG1'"
  exit 1
fi

# 7) Detached checkout stores full 64-char ref
"$BAO" checkout --detach "$FULL1"
HD=$("$BAO" rev-parse HEAD)
if [ "$HD" != "$FULL1" ] || [ "${#HD}" -ne 64 ]; then
  echo "FAIL: detached HEAD should store full 64"
  exit 1
fi

# 8) restore --source (align index to commit)
"$BAO" restore --source "$FULL2" --staged

echo "OK internal integration tests"
