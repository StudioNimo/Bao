#!/usr/bin/env bash
# S01〜S20 のユーザーシナリオに基づく結合テスト（共通ロジック）。S21 は scenarios_21_extra_common.sh。
# 使用法: BAO を設定し、このファイルを source するか、*_internal.sh / *_external.sh から呼ぶ。
# 前提: リポジトリルートで make 済み bin/bao、または PATH 上の bao。
set -euo pipefail

TESTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$TESTDIR/.." && pwd)"
: "${BAO:?set BAO to bao executable (e.g. $ROOT/bin/bao or bao)}"

if ! "$BAO" version >/dev/null 2>&1; then
  echo "FAIL: cannot execute: $BAO"
  exit 1
fi

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

run_ok() {
  if ! "$@" >/dev/null 2>&1; then
    echo "FAIL (expected ok): $*"
    exit 1
  fi
}

write_minimal_repo() {
  mkdir -p prompts/p0 prompts/p1 providers/x
  printf 'p0\n' > prompts/p0/system.txt
  printf 'p1\n' > prompts/p1/system.txt
  printf 'prov\n' > providers/x/note.txt
  printf '{"id":"1"}\n' > test_cases.jsonl
  cat > bao.yaml <<'YAML'
provider: openai
profile: p0
model: gpt-4.1-mini
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
}

# --- scenarios ---

s01_new_project_first_commit() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "s01: first"
    H=$("$BAO" rev-parse HEAD)
    [ "${#H}" -eq 64 ] || exit 1
  )
  rm -rf "$d"
}

s02_auto_commit_message() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok env BAO_NO_EDITOR=1 "$BAO" commit --no-edit
  )
  rm -rf "$d"
}

s03_multi_provider_same_repo() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt prompts/p1/system.txt providers/x/note.txt
    run_ok "$BAO" commit -m "s03: openai"
    H1=$("$BAO" rev-parse HEAD)
    cat > bao.yaml <<'YAML'
provider: anthropic
profile: p1
model: claude-3-5-sonnet-20241022
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt prompts/p1/system.txt providers/x/note.txt
    run_ok "$BAO" commit -m "s03: anthropic"
    H2=$("$BAO" rev-parse HEAD)
    [ "$H1" != "$H2" ] || exit 1
    [ "$("$BAO" rev-parse HEAD~1)" = "$H1" ] || exit 1
  )
  rm -rf "$d"
}

s04_create_branch_and_commit() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "base"
    H=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" switch -c feature
    printf 'x\n' >> prompts/p0/system.txt
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "on-feature"
    run_ok "$BAO" switch main
    M=$("$BAO" rev-parse HEAD)
    [ "$M" = "$H" ] || exit 1
  )
  rm -rf "$d"
}

s05_detached_head() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c1"
    H=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" checkout --detach "$H"
    [ "$("$BAO" rev-parse HEAD)" = "$H" ] || exit 1
  )
  rm -rf "$d"
}

s06_tag_lifecycle() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "t"
    H=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" tag v-s06
    run_ok "$BAO" tag -l
    run_ok "$BAO" tag -d v-s06
  )
  rm -rf "$d"
}

s07_amend_noop_same_tree() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "a"
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok env BAO_NO_EDITOR=1 "$BAO" commit --amend --no-edit
  )
  rm -rf "$d"
}

s08_reset_soft() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c1"
    C1=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    printf 'change\n' >> prompts/p0/system.txt
    run_ok "$BAO" add prompts/p0/system.txt
    run_ok "$BAO" commit -m "c2"
    C2=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" reset --soft "$C1"
    [ "$("$BAO" rev-parse HEAD)" = "$C1" ] || exit 1
  )
  rm -rf "$d"
}

s09_reset_hard() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c1"
    C1=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" reset --hard "$C1"
    [ "$("$BAO" rev-parse HEAD)" = "$C1" ] || exit 1
  )
  rm -rf "$d"
}

s10_stash_push_pop() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml
    run_ok "$BAO" stash push -m "s10"
    run_ok "$BAO" stash list
    run_ok "$BAO" stash pop
  )
  rm -rf "$d"
}

s11_restore_from_commit() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "snap"
    H=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" restore --source "$H" --staged
  )
  rm -rf "$d"
}

s12_diff_modes() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c1"
    H=$("$BAO" rev-parse HEAD)
    run_ok "$BAO" diff
    run_ok "$BAO" diff --cached
    run_ok "$BAO" diff "$H" "$H"
  )
  rm -rf "$d"
}

s13_rev_parse_short() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c"
    F=$("$BAO" rev-parse HEAD)
    S=$("$BAO" rev-parse --short=7 HEAD)
    [ "${#F}" -eq 64 ] && [ "${#S}" -eq 7 ] && [ "${F:0:7}" = "$S" ] || exit 1
  )
  rm -rf "$d"
}

s14_remote_crud() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c"
    run_ok "$BAO" remote add origin https://example.invalid/r
    run_ok "$BAO" remote -v
    run_ok "$BAO" remote get-url origin
    run_ok "$BAO" remote set-url origin https://example.invalid/r2
    run_ok "$BAO" remote rename origin upstream
    run_ok "$BAO" remote remove upstream
  )
  rm -rf "$d"
}

s15_config_list_get() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" config --list
    run_ok "$BAO" config model
  )
  rm -rf "$d"
}

s16_add_dry_run() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add -n bao.yaml
    run_ok "$BAO" add --dry-run bao.yaml
  )
  rm -rf "$d"
}

s17_rm_paths() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    mkdir -p tmpdir
    echo z > tmpdir/z.txt
    run_ok "$BAO" init
    run_ok "$BAO" add tmpdir/z.txt
    run_ok "$BAO" rm --cached tmpdir/z.txt
    run_ok "$BAO" rm -r -f tmpdir
  )
  rm -rf "$d"
}

s18_log_oneline_provider() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "a"
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "b"
    if ! "$BAO" log -n 2 --oneline | grep -q 'provider='; then
      echo "FAIL: log should include provider="
      exit 1
    fi
  )
  rm -rf "$d"
}

s19_status_formats() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    run_ok "$BAO" init
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt
    run_ok "$BAO" commit -m "c"
    run_ok "$BAO" status -s
    run_ok "$BAO" status -b
    run_ok "$BAO" status --porcelain
  )
  rm -rf "$d"
}

s20_stubs_and_unknown_help_topic() {
  expect_fail "$BAO" run
  expect_fail "$BAO" eval
  expect_fail "$BAO" push
  expect_fail "$BAO" pull
  expect_fail "$BAO" clone
  expect_fail "$BAO" merge
  expect_fail "$BAO" fetch
  expect_fail "$BAO" rebase
  expect_fail "$BAO" filter-branch
  expect_fail "$BAO" cherry-pick
  expect_fail "$BAO" revert
  expect_fail "$BAO" blame
  expect_fail "$BAO" help not-a-topic
}

run_scenarios_20() {
  echo "== scenarios 01-20 ($BAO) =="
  s01_new_project_first_commit
  s02_auto_commit_message
  s03_multi_provider_same_repo
  s04_create_branch_and_commit
  s05_detached_head
  s06_tag_lifecycle
  s07_amend_noop_same_tree
  s08_reset_soft
  s09_reset_hard
  s10_stash_push_pop
  s11_restore_from_commit
  s12_diff_modes
  s13_rev_parse_short
  s14_remote_crud
  s15_config_list_get
  s16_add_dry_run
  s17_rm_paths
  s18_log_oneline_provider
  s19_status_formats
  s20_stubs_and_unknown_help_topic
  echo "OK scenarios 20 ($BAO)"
}

# 直接実行された場合
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
  export BAO="${BAO:-$ROOT/bin/bao}"
  run_scenarios_20
fi
