#!/usr/bin/env bash
# Multiple providers in one repo:
# - prompts/<profile>/… alongside providers/…; switch provider/model/profile and commit.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BAO="${BAO:-$ROOT/bin/bao}"
TMP="${TMPDIR:-/tmp}/bao_mpv_$$"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/repo"
cd "$ROOT"
[ -x "$BAO" ] || { echo "build bin/bao first (make)"; exit 1; }

cd "$TMP/repo"

mkdir -p prompts/p0 prompts/p1 providers/gemini
printf 'You are OpenAI.\n' > prompts/p0/system.txt
printf 'You are Anthropic.\n' > prompts/p1/system.txt
printf 'gemini hint\n' > providers/gemini/hint.txt
printf '{"test_id":"m1","utterance":"x"}\n' > test_cases.jsonl

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
"$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt prompts/p1/system.txt providers/gemini/hint.txt
BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "snapshot: openai active + multi-tree"

H1=$("$BAO" rev-parse HEAD)
if [ "${#H1}" -ne 64 ]; then echo "FAIL: H1 not full hash"; exit 1; fi

if ! "$BAO" show --stat "$H1" 2>/dev/null | grep -q '^provider openai'; then
  echo "FAIL: first commit should record provider openai"
  "$BAO" show --stat "$H1" || true
  exit 1
fi

# Switch provider (same repo, prompts from another profile dir)
cat > bao.yaml <<'YAML'
provider: anthropic
profile: p1
model: claude-3-5-sonnet-20241022
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML

"$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt prompts/p1/system.txt providers/gemini/hint.txt
BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "snapshot: anthropic active + multi-tree"

H2=$("$BAO" rev-parse HEAD)
if [ "$H1" = "$H2" ]; then
  echo "FAIL: commit hash unchanged after provider/model/prompt switch"
  exit 1
fi

if ! "$BAO" show --stat "$H2" 2>/dev/null | grep -q '^provider anthropic'; then
  echo "FAIL: second commit should record provider anthropic"
  "$BAO" show --stat "$H2" || true
  exit 1
fi

P1=$("$BAO" rev-parse HEAD~1)
if [ "$P1" != "$H1" ]; then
  echo "FAIL: HEAD~1 should be first commit"
  exit 1
fi

# log oneline should mention provider (after DB migration)
if ! "$BAO" log -n 2 --oneline 2>/dev/null | grep -q 'provider='; then
  echo "FAIL: log oneline should mention provider="
  "$BAO" log -n 2 --oneline || true
  exit 1
fi

echo "OK multi-provider integration"
