# 🥟 Bao (Prompt Version Control System) — system design

For motivation, naming, and design philosophy, see [`concept_and_naming.md`](concept_and_naming.md).

## 1. Background

In LLM application development, small prompt changes (parameters or wording) produce non-deterministic outputs and often “this case got better while another regressed.” When building agents (e.g. voice input to structured data and OS control), if prompt history is not tied to run outputs and human scores, objective comparison and reproducibility suffer.

This project applies Git-style **content-addressed storage** to LLMOps: prompts, parameters, test data, runs, and evaluations are wrapped in one immutable **snapshot** (`Bao`) so teams can iterate quickly and safely.

## 2. System requirements

- **Fast startup:** Native C (C99/C11) to avoid script interpreter overhead.
- **Local-first:** Only a hidden `.bao/` directory and a local SQLite database—no heavy server.
- **Git-like UX:** Familiar commands (`init`, `commit`, `checkout`, `log`) and a branch-oriented model.
- **Clear separation:** Human-edited declarative config (`bao.yaml`) vs system-generated JSON/JSONL.
- **Collaboration:** `push` / `pull` over HTTP(S) to sync with a remote.

## 3. Architecture overview

### 3.1. Directory layout

Under the project root, `.bao/` stores objects and index data.

```text
my_project/
├── bao.yaml              # User-edited declarative config
├── prompts/              # Prompt text files
├── test_cases.jsonl      # Evaluation dataset
└── .bao/                 # Bao metadata
    ├── objects/
    │   ├── commits/      # Snapshots (e.g. 8f3a2b1.json)
    │   └── results/      # Run log backups (.jsonl)
    ├── refs/
    │   └── heads/        # Branch pointers (e.g. main)
    ├── HEAD              # Current checkout
    └── index.db          # SQLite for fast queries
```

This repository keeps CLI sources separate from a working tree; equivalent `bao.yaml` / `prompts/` / `test_cases.jsonl` live under **[`examples/sample/`](../examples/sample/)** for cloning and demos.

### 3.2. Database schema (`index.db`)

SQLite backs fast aggregation.

- `commits`: hash, model, parameters, prompt text, sync flag.
- `executions`: run id, commit hash, input/output text, latency.
- `evaluations`: run id, score (GOOD/BAD/NEUTRAL).

## 4. Dependencies (target)

Native C with small, mature libraries:

1. **`libcrypto` (OpenSSL):** SHA-256 for commit IDs (optional; built-in hash available).
2. **`libsqlite3`:** Store and aggregate runs and scores.
3. **`libcurl`:** LLM HTTP calls and `push`/`pull` transport.
4. **`libyaml`:** Parse `bao.yaml` (design doc; current code uses a flat parser—see [`architecture.md`](architecture.md)).
5. **`cJSON` (vendored):** JSON for API payloads and template rendering.

## 5. Use cases

1. **A/B and regression:** Branch from `main`, try a new prompt, `bao run` 50 cases, `bao eval`, then `bao diff --eval` vs the old commit to see which utterances regressed.
2. **Fast manual annotation:** In the terminal, `1`/`2`/`3` for BAD/NEUTRAL/GOOD without extra Enter presses.
3. **Team sync:** `bao push` high-scoring configs and evaluations; teammates `bao pull` and `checkout` locally.

## 6. Design considerations

- **CLI UX:** RAW `termios` mode for eval reduces cognitive load.
- **Dataset integrity:** Hash the JSONL dataset and bind it to commits/runs to avoid bogus score comparisons.
- **Conflict-free merge:** `INSERT OR IGNORE` style merges on `pull` to avoid merge conflicts in evaluation data.

## 7. Command overview

- `bao init`: create `.bao/`.
- `bao commit -m "..."`: hash `bao.yaml` + prompts and save a snapshot.
- `bao run [-d dataset.jsonl]`: call the LLM API with the current commit and store results.
- `bao eval`: interactive 3-way scoring for unevaluated runs.
- `bao log`: history plus GOOD/BAD summaries per commit.
- `bao diff --eval <A> <B>`: cases where scores improved or worsened.
- `bao checkout`: restore `bao.yaml` etc. to a branch or commit.
- `bao push` / `bao pull`: sync with a remote server.

## 8. Intended source layout (design target)

```text
src/
├── bao.c           # Entrypoint + command routing
├── cmd_init.c      # Create .bao/
├── cmd_commit.c    # Hashing + snapshot storage
├── cmd_run.c       # libcurl + response handling
├── cmd_eval.c      # RAW-mode eval UI
├── cmd_log.c       # SQLite + diff display
├── cmd_checkout.c  # Restore working files
├── cmd_remote.c    # push/pull HTTP + JSON
├── hash.c          # SHA-256 (OpenSSL or built-in)
├── db.c            # SQLite helpers
├── config.c        # bao.yaml parsing
└── template.c      # {{variable}} rendering
```

*(The `Makefile` uses `-O2` and links `-lcrypto -lcurl -lsqlite3` as needed; the live tree may differ—see [`architecture.md`](architecture.md).)*

## 9. Additional roadmap (OSS sustainability)

1. **Unit tests:** Lightweight C test frameworks for hashing, templates, and JSON safety.
2. **Distribution:** Static binaries where possible; Homebrew tap / GitHub Releases.
3. **CI:** Already partially met (GitHub Actions on multiple OSes).
4. **Provider abstraction:** `cmd_run` behind a small interface for OpenAI, Anthropic, Gemini, Ollama, etc.
5. **Contribution guide:** [`CONTRIBUTING.md`](../CONTRIBUTING.md), coding style (`clang-format`), issue/PR norms.
