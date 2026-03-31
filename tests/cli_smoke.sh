#!/usr/bin/env bash
# 目的: 全コマンド/主要オプションを「実行しても落ちない」ことを確認するスモークテスト。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SAMPLE="$ROOT/examples/sample"
BAO="${BAO:-$ROOT/bin/bao}"
TMP="${TMPDIR:-/tmp}/bao_smoke_$$"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/repo"
cd "$ROOT"
[ -x "$BAO" ] || { echo "build bin/bao first (make)"; exit 1; }

cp -R "$SAMPLE/bao.yaml" "$SAMPLE/prompts" "$SAMPLE/test_cases.jsonl" "$TMP/repo/"
cd "$TMP/repo"

run_ok() { echo "+ $*"; "$@" >/dev/null 2>&1 || { echo "FAIL (expected ok): $*"; exit 1; }; }
run_any() { echo "+ $*"; "$@" >/dev/null 2>&1 || true; }

# 1) グローバル
run_ok "$BAO" help
run_ok "$BAO" version
run_any "$BAO" --help
run_any "$BAO" -h

# 2) init / status（repo無いと死ぬ系も確認）
run_ok "$BAO" init
run_ok "$BAO" status
run_ok "$BAO" status -s
run_ok "$BAO" status -b
run_ok "$BAO" status --porcelain

# 3) add（全オプション経路）
run_ok "$BAO" add -n bao.yaml
run_ok "$BAO" add --dry-run bao.yaml
run_ok "$BAO" add -A
run_ok "$BAO" add -u
run_ok "$BAO" add bao.yaml
run_ok "$BAO" add -v bao.yaml
run_any "$BAO" add -A -n   # warns + ok

# 4) commit（全オプション経路）
run_ok "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
run_ok "$BAO" commit -m "smoke: first"
run_ok "$BAO" log -1
run_ok "$BAO" log -1 --oneline

# 自動メッセージ生成（エディタ禁止）
run_ok "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
run_ok env BAO_NO_EDITOR=1 "$BAO" commit --no-edit

# -F
printf "file message\n" > "$TMP/msg.txt"
run_ok "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
run_ok "$BAO" commit -F "$TMP/msg.txt"

# --amend（エディタ禁止）
run_ok "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
run_ok env BAO_NO_EDITOR=1 "$BAO" commit --amend --no-edit

# --edit（EDITORが無い場合は実質 no-op でOK）
run_ok "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl
run_ok env BAO_NO_EDITOR=1 "$BAO" commit --edit --no-edit

# 5) rev-parse / show / diff
H=$("$BAO" rev-parse HEAD)
run_ok "$BAO" rev-parse --short HEAD
run_ok "$BAO" rev-parse --short=12 HEAD
run_ok "$BAO" show --stat "$H"
run_ok "$BAO" show --name-only "$H"
run_ok "$BAO" diff
run_ok "$BAO" diff --cached
run_ok "$BAO" diff "$H" "$H"

# 6) branch / tag
run_ok "$BAO" branch
run_ok "$BAO" branch -v
run_ok "$BAO" branch feat "$H"
run_ok "$BAO" branch
run_ok "$BAO" tag -l
run_ok "$BAO" tag v0
run_ok "$BAO" tag -l
run_ok "$BAO" tag -d v0

# 7) checkout / switch / reset / restore
run_ok "$BAO" checkout --detach "$H"
run_ok "$BAO" switch --detach "$H"
run_ok "$BAO" checkout -b newbranch "$H"
run_ok "$BAO" switch main
run_ok "$BAO" reset --soft "$H"
run_ok "$BAO" reset --mixed "$H"
run_ok "$BAO" reset --hard "$H"
run_ok "$BAO" restore --staged
run_ok "$BAO" restore --source "$H" --staged

# 8) stash
run_ok "$BAO" stash list
run_ok "$BAO" add bao.yaml
run_ok "$BAO" stash push -m "m"
run_ok "$BAO" stash list
run_ok "$BAO" stash apply
run_ok "$BAO" stash pop
run_ok "$BAO" stash clear

# 9) rm（--cached / -r / -f 経路）
mkdir -p tmpdir && echo hi > tmpdir/a.txt
run_ok "$BAO" add tmpdir/a.txt
run_ok "$BAO" rm --cached tmpdir/a.txt
run_ok "$BAO" rm -r -f tmpdir

# 10) remote（サブコマンド網羅）
run_ok "$BAO" remote
run_ok "$BAO" remote -v
run_ok "$BAO" remote add origin https://example.invalid/repo
run_ok "$BAO" remote -v
run_ok "$BAO" remote get-url origin
run_ok "$BAO" remote show origin
run_ok "$BAO" remote set-url origin https://example.invalid/repo2
run_ok "$BAO" remote rename origin upstream
run_ok "$BAO" remote remove upstream

# 11) config（現在アクティブのみ / profile 一覧 / profile 指定）
run_ok "$BAO" config --list
run_ok "$BAO" config --profiles
run_ok "$BAO" config --list --profile p0
run_ok "$BAO" config --list --profile p1
run_ok "$BAO" config --list --raw
run_ok "$BAO" config model

# 12) stubs（落ち方/メッセージ確認）
run_any "$BAO" run
run_any "$BAO" eval
run_any "$BAO" push
run_any "$BAO" pull
run_any "$BAO" clone
run_any "$BAO" merge
run_any "$BAO" fetch
run_any "$BAO" rebase
run_any "$BAO" filter-branch
run_any "$BAO" cherry-pick
run_any "$BAO" revert
run_any "$BAO" blame

echo "OK cli smoke"

