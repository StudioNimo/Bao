# Contributing to Bao

Thanks for your interest. Bao is still a prototype, but bug reports, suggestions, and pull requests are welcome.

Background and naming are described in [`docs/concept_and_naming.md`](docs/concept_and_naming.md).

**Layout:** The tool lives under `src/` and `Makefile`. The minimal user sample is **`examples/sample/`**; tests such as `cli_smoke.sh` copy from there. There is no `bao.yaml` at the repository root.

## Development setup

- macOS / Linux expected
- Dependencies: `sqlite3` (dev headers), **libcurl** (dev headers), C compiler (`cc`)
- Optional: OpenSSL (`make BAO_USE_OPENSSL=1`)

## Build / test

```bash
make
make test
```

## Change guidelines

- **Compatibility:** If you change command output or exit codes, update tests and explain why.
- **Safety:** Path handling (`..`, absolute paths, `.bao/`) should stay on the safe side.
- **Dependencies:** Propose new deps sparingly; include rationale and alternatives.

## Issues / PRs

- **Bugs:** Minimal repro steps, expected vs actual, OS/environment.
- **Features:** Goal, user value, intended UX, compatibility impact.
- **PRs:** Include test updates (or why not). PRs to `main` run GitHub Actions (`.github/workflows/ci.yml`). To require CI before merge, see [`docs/BRANCH_PROTECTION.md`](docs/BRANCH_PROTECTION.md).

## Code style (guidelines)

- C11; aim for zero warnings (`-Wall -Wextra -Wpedantic`)
- Early returns and clear error messages

## License

Contributions are accepted under the terms of `LICENSE` (MIT).
