# Issue foothold (implementation map)

Short pointers for open GitHub work items. Use these paths when starting a PR.

## [#5](https://github.com/StudioNimo/Bao/issues/5) — Non-interactive `bao eval`

- **Goal:** Scripting / CI without a TTY (`--json`, stdin protocol, or similar).
- **Code:** `src/cmd_eval.c` (option parsing today rejects `--json` before DB open), `src/db.h` / `src/db.c` (`bao_uneval_exec_t`, `bao_db_insert_evaluation`).
- **Tests:** Extend `tests/stub_options_fail.sh` or add a small integration test that feeds fake stdin once `--json` exists.

## [#6](https://github.com/StudioNimo/Bao/issues/6) — `bao push` / `bao pull`

- **Goal:** Local-first sync between clones or a small server.
- **Design draft:** [`docs/sync_protocol.md`](sync_protocol.md) (outline only; update as decisions land).
- **Code:** `src/cmd_remote.c` (`stub_push`, `stub_pull`), `.bao/` layout in `docs/architecture.md`.

## [#7](https://github.com/StudioNimo/Bao/issues/7) — Additional `bao run` providers (e.g. Anthropic)

- **Goal:** Dispatch on `provider:` in `bao.yaml` beyond `openai`.
- **Code:** `src/cmd_run.c` (provider check ~line 207; OpenAI HTTP in `openai_chat_completion`), `src/config.c` / `bao.yaml` parsing.
- **Docs / env:** `README.md`, `.env.example` (placeholder `ANTHROPIC_API_KEY`).

When an issue is fully implemented, update this file or remove the corresponding section so it stays accurate.
