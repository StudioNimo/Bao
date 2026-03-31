#!/usr/bin/env bash
# 未実装コマンド（スタブ）の主要オプションを実行し、終了コードを検証する。
# - 多くは非0終了を期待する。
# - clone/merge/rebase/cherry-pick の一部オプションは実装上 0 終了（メッセージのみ）のため expect_ok。
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

expect_fail() {
  set +e
  "$@" >/dev/null 2>&1
  local rc=$?
  set -e
  if [ "$rc" -eq 0 ]; then
    echo "FAIL: expected non-zero exit: $*"
    exit 1
  fi
}

expect_ok() {
  if ! "$@" >/dev/null 2>&1; then
    echo "FAIL: expected zero exit: $*"
    exit 1
  fi
}

run_stub_suite() {
  local BAO="$1"
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    mkdir -p prompts/p0 && printf 'x\n' > prompts/p0/system.txt
    printf '{"id":1}\n' > test_cases.jsonl
    cat > bao.yaml <<'YAML'
provider: openai
profile: p0
model: gpt-4.1-mini
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
    "$BAO" init

    # 実装上、未実装でも 0 を返す「noop」系（cmd_misc.c）
    expect_ok "$BAO" clone --abort
    expect_ok "$BAO" clone --continue
    expect_ok "$BAO" merge --abort
    expect_ok "$BAO" merge --continue
    expect_ok "$BAO" rebase --abort
    expect_ok "$BAO" rebase --continue
    expect_ok "$BAO" rebase --skip
    expect_ok "$BAO" cherry-pick --abort
    expect_ok "$BAO" cherry-pick --continue

    # 非0終了を期待
    expect_fail "$BAO" run
    expect_fail "$BAO" run --dry-run
    expect_fail "$BAO" run -n 1
    expect_fail "$BAO" eval
    expect_fail "$BAO" eval --json
    expect_fail "$BAO" push
    expect_fail "$BAO" push -u origin main
    expect_fail "$BAO" push --force-with-lease
    expect_fail "$BAO" pull
    expect_fail "$BAO" pull --rebase
    expect_fail "$BAO" clone https://example.invalid/r.git
    expect_fail "$BAO" clone --depth 1 https://example.invalid/r.git
    expect_fail "$BAO" merge main
    expect_fail "$BAO" merge --no-ff main
    expect_fail "$BAO" fetch
    expect_fail "$BAO" fetch origin
    expect_fail "$BAO" rebase main
    expect_fail "$BAO" rebase -i HEAD~1
    expect_fail "$BAO" filter-branch
    expect_fail "$BAO" filter-branch --tree-filter 'true' HEAD
    expect_fail "$BAO" cherry-pick abcdef01
    expect_fail "$BAO" cherry-pick --no-commit abcdef01
    expect_fail "$BAO" revert HEAD
    expect_fail "$BAO" revert -m 1 abcdef01
    expect_fail "$BAO" blame README
    expect_fail "$BAO" blame -L 1,10 README
  )
  rm -rf "$d"
}

echo "== stub options (internal: $ROOT/bin/bao) =="
run_stub_suite "$ROOT/bin/bao"

echo "== stub options (external: bao on PATH) =="
export PATH="$ROOT/bin:$PATH"
run_stub_suite bao

echo "OK stub_options_fail"
