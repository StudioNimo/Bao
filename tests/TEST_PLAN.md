# Bao test plan (user scenarios S01–S20, extra S21, command coverage)

## 1. Goals

- Reproduce **user scenarios S01–S20** in integration tests so main flows stay healthy (**S21** is covered by a separate script).
- Run **every command, subcommand, and major option** through `tests/cli_smoke.sh`.
- **Strict output checks** (`remote -v` as `name<TAB>url`, `status --porcelain` line format, `stash` list/push/pop/apply/drop/clear) live in `tests/assert_strict.sh`.
- Run the 20 scenarios in both **internal** (`bin/bao` directly) and **external** (`bao` on `PATH`) modes.

## 2. Test assets

| Asset | Role |
|-------|------|
| `tests/TEST_PLAN.md` | This document (scenario ↔ test mapping) |
| `examples/sample/` | Source for `bao.yaml` / `prompts/` / `test_cases.jsonl` used by smoke and integration tests |
| `tests/scenarios_20_common.sh` | **S01–S20** core (binary chosen via `BAO`) |
| `tests/scenarios_20_internal.sh` | Internal wrapper (S01–S20) |
| `tests/scenarios_20_external.sh` | External wrapper (S01–S20) |
| `tests/scenarios_21_extra_common.sh` | **S21** (multi-provider comparison, fixed result filenames) |
| `tests/scenarios_21_internal.sh` | Internal wrapper (S21 only) |
| `tests/scenarios_21_external.sh` | External wrapper (S21 only) |
| `tests/cli_smoke.sh` | Smoke all commands / subcommands / major options |
| `tests/stub_options_fail.sh` | Stub commands **with options** (mostly non-zero exit) |
| `tests/assert_strict.sh` | Strict checks for `remote -v`, `status --porcelain`, `stash` transitions |
| `tests/integration_internal.sh` | init → commit → checkout → restore |
| `tests/integration_external.sh` | Minimal flow via `PATH` |
| `tests/integration_multi_provider.sh` | Multiple providers in one repo |

## 3. Scenarios S01–S20

| ID | Scenario | Main checks | Impl prefix |
|----|----------|-------------|-------------|
| S01 | First commit on new project | `init` → `add` → `commit -m` → full 64-char hash | `s01_*` |
| S02 | Auto-generated commit message (no editor in CI) | `BAO_NO_EDITOR=1` + `commit --no-edit` | `s02_*` |
| S03 | Multiple providers in one repo (config switch → new commit) | change `provider`/`profile`/`model`/`prompts_dir`, `HEAD`≠`HEAD~1` | `s03_*` |
| S04 | Branch and commit | `switch -c` → `commit` → `switch main` | `s04_*` |
| S05 | Detached HEAD | `checkout --detach` + `rev-parse` | `s05_*` |
| S06 | Release tags | `tag` / `tag -l` / `tag -d` | `s06_*` |
| S07 | Amend (idempotent) | `commit --amend --no-edit` | `s07_*` |
| S08 | Reset soft (keep index) | `reset --soft` | `s08_*` |
| S09 | Reset hard | `reset --hard` | `s09_*` |
| S10 | Stash push/pop | `stash push` → `pop` | `s10_*` |
| S11 | Restore index from past commit | `restore --source <hash>` | `s11_*` |
| S12 | Diff working / index / commits | `diff` / `diff --cached` / `diff H H` | `s12_*` |
| S13 | Short vs full rev | `rev-parse` / `--short=7` | `s13_*` |
| S14 | Remote add/set-url/rename/remove | `remote add` / `set-url` / `rename` / `remove` | `s14_*` |
| S15 | Config queries | `config --list` / `config model` | `s15_*` |
| S16 | Dry-run staging | `add -n` / `--dry-run` | `s16_*` |
| S17 | Remove from index and disk | `rm --cached` / `rm -r -f` | `s17_*` |
| S18 | One-line log with provider | `log -n 2 --oneline` contains `provider=` | `s18_*` |
| S19 | Status formats | `status -s` / `-b` / `--porcelain` | `s19_*` |
| S20 | Stub / failure paths | bare `bao run` (no key) and `bao eval` fail; `push`/`pull`/`clone`/… stubs | `s20_*` |
| S21 | Multi-provider comparison (fixed result files) | toggle `bao.yaml` + pin `results/openai.jsonl` / `results/anthropic.jsonl` | `scenarios_21_extra_*` |

## 4. How to run

```bash
make test
```

Individual scripts:

```bash
./tests/scenarios_20_internal.sh
./tests/scenarios_20_external.sh
./tests/scenarios_21_internal.sh
./tests/scenarios_21_external.sh
./tests/cli_smoke.sh
./tests/stub_options_fail.sh
```

## 5. `make test` order

1. `integration_internal.sh`
2. `integration_external.sh`
3. `integration_multi_provider.sh`
4. `scenarios_20_internal.sh` (S01–S20)
5. `scenarios_20_external.sh` (S01–S20)
6. `scenarios_21_internal.sh` (S21)
7. `scenarios_21_external.sh` (S21)
8. `cli_smoke.sh`
9. `stub_options_fail.sh`
10. `assert_strict.sh`

## 6. Coverage notes

- **`cli_smoke.sh`** covers command lists and major options (stubs expect non-zero exit).
- **S01–S20** are story-style regression tests.
- **S21** is isolated in `scenarios_21_*`.
- **`stub_options_fail.sh`** distinguishes **zero exit** cases (e.g. some no-op stubs) from **non-zero** cases.
- `make test` runs all of the above in one pass.
