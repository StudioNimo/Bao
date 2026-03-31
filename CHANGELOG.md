# Changelog

This project follows semantic versioning (`MAJOR.MINOR.PATCH`).

## 0.1.0 - 2026-03-31

- Initial public release (git-like CLI subset: `init/add/commit/log/status/checkout/switch`, etc.)
- Load settings from `bao.yaml` (flat `key: value`)
- Multi-provider workflow with `prompts_dir` + `profile`
- Internal/external integration tests and scenario tests (`make test`)
- Docs: development concept and naming (`docs/concept_and_naming.md`)
- CI: tests on PRs to `main` and pushes to `main`; aggregate job `ci (required)` for branch protection (`docs/BRANCH_PROTECTION.md`)
- Layout: sample project under `examples/sample/` (no root `bao.yaml` or prompts). Added `docs/QUICKSTART.md`
