#!/usr/bin/env bash
# Stricter than smoke: remote -v / status --porcelain / stash transitions
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BAO="${BAO:-$ROOT/bin/bao}"
TMP="${TMPDIR:-/tmp}/bao_assert_$$"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/wd"
cd "$TMP/wd"

[ -x "$BAO" ] || { echo "build bin/bao first (make)"; exit 1; }

die() { echo "FAIL: $*"; exit 1; }

# --- minimal repo ---
mkdir -p prompts
printf 'p\n' > prompts/system.txt
printf '{"t":1}\n' > test_cases.jsonl
cat > bao.yaml <<'YAML'
provider: openai
model: gpt-4.1-mini
temperature: 0.2
max_tokens: 4096
prompt_file: prompts/system.txt
dataset: test_cases.jsonl
YAML

"$BAO" init
"$BAO" add bao.yaml test_cases.jsonl prompts/system.txt
"$BAO" commit -m "base" >/dev/null

SHORT=$("$BAO" rev-parse --short=7 HEAD)
[ "${#SHORT}" -eq 7 ] || die "short hash length"

# 1) status --porcelain lines (cmd_status)
#    # branch.oid <7hex>
#    # branch.head <branch>
#    M  <path>  … staged
OUT=$("$BAO" status --porcelain)
echo "$OUT" | grep -q '^# branch.oid '"$SHORT"'$' || die "porcelain: branch.oid line should be 7-hex HEAD"
echo "$OUT" | grep -q '^# branch.head main$' || die "porcelain: branch.head should be main on init"
if echo "$OUT" | grep -q '^M  '; then echo "$OUT"; die "porcelain: unexpected staged lines before add"; fi

"$BAO" add bao.yaml test_cases.jsonl prompts/system.txt
OUT2=$("$BAO" status --porcelain)
echo "$OUT2" | grep -q '^# branch.oid '"$SHORT"'$' || die "porcelain: branch.oid after add"
echo "$OUT2" | grep -q '^M  bao\.yaml$' || die "porcelain: expected 'M  bao.yaml'"
echo "$OUT2" | grep -q '^M  test_cases\.jsonl$' || die "porcelain: expected 'M  test_cases.jsonl'"
echo "$OUT2" | grep -q '^M  prompts/system\.txt$' || die "porcelain: expected 'M  prompts/system.txt'"

# 2) remote -v (remote_list verbose → name<TAB>url)
R0=$("$BAO" remote -v)
[ "$R0" = "(no remotes configured)" ] || die "remote -v empty: got [$R0]"

"$BAO" remote add origin https://example.invalid/repo
R1=$("$BAO" remote -v)
[ "$R1" = $'origin\thttps://example.invalid/repo' ] || die "remote -v: expected name<TAB>url, got [$R1]"

"$BAO" remote add upstream https://example.invalid/up
R2=$("$BAO" remote -v)
echo "$R2" | grep -q $'^origin\thttps://example.invalid/repo$' || die "remote -v line 1"
echo "$R2" | grep -q $'^upstream\thttps://example.invalid/up$' || die "remote -v line 2"

# 3) stash transitions (list / push / list / pop / list)
L0=$("$BAO" stash list)
[ "$L0" = "stash@{0}: empty" ] || die "stash list before: [$L0]"

"$BAO" restore --staged 2>/dev/null || true
"$BAO" add bao.yaml
[ -f .bao/stash ] && die "stash file should not exist yet"
L1=$("$BAO" stash list)
[ "$L1" = "stash@{0}: empty" ] || die "stash list still empty before push"

OUT_PUSH=$("$BAO" stash push -m "m" 2>&1)
echo "$OUT_PUSH" | grep -q '^Stashed [0-9][0-9]* path(s); index cleared\.$' || die "stash push message: $OUT_PUSH"
[ -f .bao/stash ] || die "stash file should exist after push"
L2=$("$BAO" stash list)
[ "$L2" = "stash@{0}: present (see .bao/stash)" ] || die "stash list after push: [$L2]"

L3=$("$BAO" stash pop 2>&1)
[ "$L3" = "Applied stash to staging area." ] || die "stash pop message: [$L3]"
[ ! -f .bao/stash ] || die "stash file should be removed after pop"
L4=$("$BAO" stash list)
[ "$L4" = "stash@{0}: empty" ] || die "stash list after pop: [$L4]"

# stash apply keeps stash file (push → apply → file still present)
"$BAO" add bao.yaml
"$BAO" stash push -m "x" >/dev/null
[ -f .bao/stash ] || die "stash file after second push"
"$BAO" stash apply >/dev/null
[ -f .bao/stash ] || die "stash file should remain after apply"
L5=$("$BAO" stash list)
[ "$L5" = "stash@{0}: present (see .bao/stash)" ] || die "stash list after apply"
"$BAO" stash drop >/dev/null
[ ! -f .bao/stash ] || die "stash file should be gone after drop"

"$BAO" stash clear
L6=$("$BAO" stash list)
[ "$L6" = "stash@{0}: empty" ] || die "stash list after clear"

echo "OK assert_strict"
