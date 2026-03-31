#!/usr/bin/env bash
# S21 (extra): multi-provider comparison with fixed result filenames.
# S01–S20 live only in scenarios_20_common.sh; this file sources helpers like write_minimal_repo.
set -euo pipefail

TESTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$TESTDIR/.." && pwd)"
: "${BAO:?set BAO to bao executable (e.g. $ROOT/bin/bao or bao)}"

# shellcheck source=tests/scenarios_20_common.sh
source "$ROOT/tests/scenarios_20_common.sh"

s21_multi_provider_compare_fixed_results() {
  local d
  d=$(mktemp -d)
  (
    cd "$d"
    write_minimal_repo
    mkdir -p results
    run_ok "$BAO" init

    printf '{"provider":"openai","id":"1","out":"ok"}\n' > results/openai.jsonl
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt results/openai.jsonl
    run_ok "$BAO" commit -m "s21: openai results"
    H1=$("$BAO" rev-parse HEAD)
    if ! "$BAO" show --name-only "$H1" | grep -q '^results/openai\.jsonl$'; then
      echo "FAIL: s21 openai commit should include results/openai.jsonl"
      exit 1
    fi

    cat > bao.yaml <<'YAML'
provider: anthropic
profile: p1
model: claude-3-5-sonnet-20241022
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
    printf '{"provider":"anthropic","id":"1","out":"ok"}\n' > results/anthropic.jsonl
    run_ok "$BAO" add bao.yaml test_cases.jsonl prompts/p1/system.txt results/anthropic.jsonl
    run_ok "$BAO" commit -m "s21: anthropic results"
    H2=$("$BAO" rev-parse HEAD)
    [ "$H1" != "$H2" ] || exit 1

    if ! "$BAO" show --name-only "$H2" | grep -q '^results/anthropic\.jsonl$'; then
      echo "FAIL: s21 anthropic commit should include results/anthropic.jsonl"
      exit 1
    fi

    run_ok "$BAO" diff --eval "$H1" "$H2"
  )
  rm -rf "$d"
}

run_scenarios_21_extra() {
  echo "== scenario S21 extra ($BAO) =="
  s21_multi_provider_compare_fixed_results
  echo "OK scenario S21 ($BAO)"
}

if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
  export BAO="${BAO:-$ROOT/bin/bao}"
  run_scenarios_21_extra
fi
