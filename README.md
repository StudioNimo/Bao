# Bao (Prompt Version Control System)

[![CI](https://github.com/StudioNimo/Bao/actions/workflows/ci.yml/badge.svg)](https://github.com/StudioNimo/Bao/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Bao is a local-first CLI that treats prompts, configuration, datasets, run outputs, and evaluations as versioned snapshots.

- **Fastest path to try it:** [`docs/QUICKSTART.md`](docs/QUICKSTART.md) or the “Try it now (sample project)” section below
- **Background and naming:** [`docs/concept_and_naming.md`](docs/concept_and_naming.md)
- **Overview:** [`docs/system_design.md`](docs/system_design.md)
- **Detailed design (implementation context):** [`docs/architecture.md`](docs/architecture.md)

## Repository layout

| Location | Contents |
|----------|----------|
| [`src/`](src/), [`Makefile`](Makefile) | **CLI** source and build |
| [`examples/sample/`](examples/sample/) | **Runnable sample** (`bao.yaml`, `prompts/`, `test_cases.jsonl`). After cloning, start here with `bao init` |
| [`tests/`](tests/) | Integration and scenario tests |
| [`docs/`](docs/) | Design and how-to docs |
| [`man/`](man/) | `man` page (prototype) |

There is **no project `bao.yaml` at the repository root** (the tool stays separate from a working tree). Use [`examples/sample/`](examples/sample/) or any directory you choose.

## License

- [`LICENSE`](LICENSE) (MIT)
- Bundled third-party: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)

## Clone from GitHub and run locally

### Prerequisites

- **Git** (to clone)
- **C compiler** (`cc` — Clang on macOS, GCC on Linux, etc.)
- **SQLite3 development library** (`libsqlite3` and `sqlite3.h`)
- **libcurl** (development headers) for `bao run` against the OpenAI HTTP API
- There are **no prebuilt binaries**; **build from source with `make`**. JSON parsing uses **cJSON (vendored)**; SHA-256 defaults to a **built-in implementation**.
- Optional: see **[`.env.example`](.env.example)** in the repo root for environment variables (`BAO_EDITOR`, `OPENAI_API_KEY`, etc.); copy to `.env` if your shell loads it.

### 1. Clone the repository

Use the URL of your fork or this repo.

```bash
git clone https://github.com/StudioNimo/Bao.git
cd Bao
```

### 2. Install dependencies (examples)

- **macOS**  
  - Command-line tools: `xcode-select --install` if needed  
  - If `sqlite3.h` is missing: `brew install sqlite`, etc.
- **Ubuntu / Debian**

```bash
sudo apt-get update
sudo apt-get install -y build-essential pkg-config libsqlite3-dev libcurl4-openssl-dev
```

- **Fedora / RHEL / AlmaLinux / Rocky Linux** (dnf; on older RHEL use `yum` where appropriate)

```bash
sudo dnf install -y gcc make sqlite-devel libcurl-devel
```

Package names may differ slightly by major version; if the build fails on missing headers, install the `-devel` package that provides `sqlite3.h` and libcurl headers.

### 3. Build

```bash
make
```

On success, `bin/bao` is created.

### 4. Smoke test

```bash
./bin/bao version
./bin/bao help
```

### 5. Try it now (sample project)

After building, you can reach a first commit with the bundled sample:

```bash
cd examples/sample
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

See [`examples/sample/README.md`](examples/sample/README.md) and [`docs/QUICKSTART.md`](docs/QUICKSTART.md) for details.

### 6. Run `bao` from anywhere (optional)

- **Current shell only** (from the clone directory):

```bash
export PATH="$(pwd)/bin:$PATH"
bao version
```

- **Persistently** (e.g. zsh): append **an absolute path** to `~/.zshrc`:

```bash
echo 'export PATH="/path/to/your/clone/bao/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

- For **`man`**, follow the output of `make install-man` to copy [`man/bao.1`](man/bao.1) into your system `man` directory.

### Build options (optional; rebuild after step 3)

Build with OpenSSL `libcrypto` for hashing:

```bash
make clean
make BAO_USE_OPENSSL=1
```

(On Linux you may need `libssl-dev`, etc.)

## Minimal workflow (any project directory)

Work in an empty directory or one that already has `bao.yaml` (the examples below call `bin/bao` from the repo root).

```bash
mkdir -p ~/my-bao-project && cd ~/my-bao-project
/path/to/bao/bin/bao init
# After editing bao.yaml and prompts/
/path/to/bao/bin/bao add -A
# or: bao add bao.yaml prompts/ test_cases.jsonl
/path/to/bao/bin/bao commit -m "first snapshot"
/path/to/bao/bin/bao log
```

If you are new to Bao, **[`examples/sample/`](examples/sample/)** is easier than the snippet above.

You must **`bao add`** to stage the index before **`bao commit`**. **`bao add -A`** stages `bao.yaml`, configured prompt files, the dataset, and everything under `prompts/`.

`bao.yaml` needs `model` and `prompt_file` (or `prompts_dir`).

- **`bao run`**: runs the JSONL dataset through the current HEAD config; **`--dry-run`** prints rendered prompts only. **`provider: openai`** uses **`OPENAI_API_KEY`** and `https://api.openai.com/v1/chat/completions`.
- **`bao eval`**: TTY-only; scores unevaluated rows with **1=BAD / 2=NEUTRAL / 3=GOOD** after a `run`.
- **`bao diff --eval A B`**: compares saved scores between two commits (same `dataset_hash` required).
- **`push` / `pull`**: not implemented yet ([issue #6](https://github.com/StudioNimo/Bao/issues/6); draft [`docs/sync_protocol.md`](docs/sync_protocol.md)).

## Commands (excerpt)

- **`add`**: `-A/--all`, `-u/--update`, `-n/--dry-run`, `-v/--verbose`
- **`commit`**: `-m`, `-F/--file`, `-a/--all`, `-q/--quiet`, `--amend`
- **`log`**: `-n/--max-count`, `-1`, `--oneline`
- **`status`**: `-s/--short`, `-b/--branch`, `--porcelain`
- **`checkout`**: `-b`, `-B`, `--detach`
- **`switch`**: `-c`, `-C`, `--detach`
- **`rev-parse`**: `HEAD~n` follows **parent links** (not DB timeline order)
- **`run`**: `-d/--dataset`, `--dry-run`
- **`diff`**: `--eval` with two commit hashes for evaluation deltas

## Documentation

| File | Description |
|------|-------------|
| [`docs/QUICKSTART.md`](docs/QUICKSTART.md) | Shortest path from clone to sample |
| [`examples/README.md`](examples/README.md) | Sample directory overview |
| [`examples/sample/README.md`](examples/sample/README.md) | Try only `examples/sample/` |
| [`docs/concept_and_naming.md`](docs/concept_and_naming.md) | Concept and naming (steamed-bun metaphor, CLI feel) |
| [`docs/BRANCH_PROTECTION.md`](docs/BRANCH_PROTECTION.md) | Require CI on `main` (GitHub branch protection) |
| [`docs/system_design.md`](docs/system_design.md) | System design (overview) |
| [`docs/architecture.md`](docs/architecture.md) | Detailed design (DB, structs, command flow) |
| [`docs/RELEASING.md`](docs/RELEASING.md) | Release checklist |
| [`docs/ISSUE_FOOTHOLD.md`](docs/ISSUE_FOOTHOLD.md) | Map of open feature issues (#5–#7) to source files |
| [`docs/sync_protocol.md`](docs/sync_protocol.md) | Draft notes for future `push`/`pull` sync |
| [`man/bao.1`](man/bao.1) | `man bao` (prototype) |
