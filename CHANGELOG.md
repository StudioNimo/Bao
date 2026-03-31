# Changelog

This project follows semantic versioning (`MAJOR.MINOR.PATCH`).

## Unreleased

- Docs: `man bao` — new `ENVIRONMENT` section (parity with `.env.example`)
- Docs: root [`.env.example`](.env.example) for `BAO_EDITOR`, `EDITOR` / `VISUAL`, `BAO_NO_EDITOR`, `OPENAI_API_KEY`
- Docs: Fedora / RHEL-family build dependencies (`dnf` + `sqlite-devel`, `libcurl-devel`)
- Docs: [`docs/ISSUE_FOOTHOLD.md`](docs/ISSUE_FOOTHOLD.md) and [`docs/sync_protocol.md`](docs/sync_protocol.md) as scaffolding for GitHub issues #5–#7; clearer stub/error strings in `eval` / `push` / `pull` / `run`
- Docs: turn repository file/path references in Markdown into clickable relative links (GitHub-friendly)

## 0.2.0 - 2026-03-31

- **`bao run`**: OpenAI Chat Completions via libcurl; `OPENAI_API_KEY`; `--dry-run` to preview rendered prompts; updates `commits.dataset_hash` for the run file
- **`bao eval`**: terminal RAW mode; 1/2/3 scores stored in SQLite
- **`bao diff --eval`**: regression / improvement report between two commits (matching `dataset_hash`)
- Build: link **libcurl**; CI installs `libcurl4-openssl-dev` on Ubuntu

## 0.1.0 - 2026-03-31

- Initial public release (git-like CLI subset: `init/add/commit/log/status/checkout/switch`, etc.)
- Load settings from `bao.yaml` (flat `key: value`)
- Multi-provider workflow with `prompts_dir` + `profile`
- Internal/external integration tests and scenario tests (`make test`)
- Docs: development concept and naming ([`docs/concept_and_naming.md`](docs/concept_and_naming.md))
- CI: tests on PRs to `main` and pushes to `main`; aggregate job `ci (required)` for branch protection ([`docs/BRANCH_PROTECTION.md`](docs/BRANCH_PROTECTION.md))
- Layout: sample project under [`examples/sample/`](examples/sample/) (no root `bao.yaml` or prompts). Added [`docs/QUICKSTART.md`](docs/QUICKSTART.md)
