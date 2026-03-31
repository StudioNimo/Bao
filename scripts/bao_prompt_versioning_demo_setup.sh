#!/usr/bin/env bash
# Recreates a project where prompt versioning has progressed for a while, in one directory.
# Usage:
#   ./scripts/bao_prompt_versioning_demo_setup.sh
#   FORCE=1 ./scripts/bao_prompt_versioning_demo_setup.sh   # overwrite existing demo
# Environment:
#   BAO        path to bao binary (default: repo bin/bao)
#   DEMO_ROOT  workspace root (default: <repo>/demo/prompt_versioning_workspace)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BAO="${BAO:-$ROOT/bin/bao}"
DEMO_ROOT="${DEMO_ROOT:-$ROOT/demo/prompt_versioning_workspace}"

if [[ ! -x "$BAO" ]]; then
  echo "bao not found: $BAO (hint: run 'make' in $ROOT)"
  exit 1
fi

if [[ -e "$DEMO_ROOT" ]]; then
  if [[ "${FORCE:-0}" == "1" ]]; then
    rm -rf "$DEMO_ROOT"
  else
    echo "already exists: $DEMO_ROOT"
    echo "Remove it or rerun with FORCE=1"
    exit 1
  fi
fi

mkdir -p "$DEMO_ROOT"
cd "$DEMO_ROOT"

run() { echo "+ $*"; "$@"; }

# --- File skeleton (before init) ---
mkdir -p prompts/p0 prompts/p1 providers/notes
cat > test_cases.jsonl <<'JSONL'
{"id":"tc-001","prompt":"hello"}
{"id":"tc-002","prompt":"world"}
JSONL

cat > prompts/p0/system.txt <<'TXT'
You are a helpful assistant (profile p0, revision 1).
Follow instructions precisely.
TXT

cat > prompts/p1/system.txt <<'TXT'
You are a helpful assistant (profile p1, initial).
Be concise.
TXT

printf 'Vendor-specific memo for experiments.\n' > providers/notes/vendor.txt

write_bao_openai_p0() {
  cat > bao.yaml <<'YAML'
# Demo: active = OpenAI + profile p0
provider: openai
profile: p0
model: gpt-4.1-mini
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
}

write_bao_anthropic_p1() {
  cat > bao.yaml <<'YAML'
# Demo: active = Anthropic + profile p1
provider: anthropic
profile: p1
model: claude-3-5-sonnet-20241022
temperature: 0.2
max_tokens: 4096
prompts_dir: prompts
dataset: test_cases.jsonl
YAML
}

run "$BAO" init

# --- Commit 1: initial snapshot (p0 active) ---
write_bao_openai_p0
run "$BAO" add bao.yaml test_cases.jsonl prompts/p0/system.txt prompts/p1/system.txt providers/notes/vendor.txt
run env BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "bootstrap: dataset + prompts/p0 v1 + vendor notes"

# --- Commit 2: same profile, prompt-only revision ---
cat > prompts/p0/system.txt <<'TXT'
You are a helpful assistant (profile p0, revision 2).
Follow instructions precisely. Prefer structured output when asked.
TXT
run "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl providers/notes/vendor.txt
run env BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "prompt: refine p0 instructions"

run "$BAO" tag v0.1-baseline

# --- Commit 3: switch provider + profile (version config and prompts) ---
write_bao_anthropic_p1
cat > prompts/p1/system.txt <<'TXT'
You are a helpful assistant (profile p1, revision 2).
Be concise. Match the user's language in your replies.
TXT
run "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl providers/notes/vendor.txt
run env BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "experiment: switch to anthropic + p1 prompt"

# --- Commit 4: add evaluation snapshot (artifacts in commit) ---
mkdir -p results
cat > results/run01.jsonl <<'JSONL'
{"case_id":"tc-001","model_profile":"p1","score":0.92,"note":"demo snapshot"}
JSONL
run "$BAO" add bao.yaml prompts/p0/system.txt prompts/p1/system.txt test_cases.jsonl providers/notes/vendor.txt results/run01.jsonl
run env BAO_NO_EDITOR=1 "$BAO" commit --no-edit -m "eval: add results/run01.jsonl snapshot"

echo ""
echo "Demo workspace ready:"
echo "  $DEMO_ROOT"
echo "Try: cd \"$DEMO_ROOT\" && $BAO log -n 4 --oneline && $BAO tag -l"
