# Bao sync protocol (draft)

> **Status:** Outline for [issue #6](https://github.com/StudioNimo/Bao/issues/6). Not a committed protocol yet.

## Goals

- Replicate enough of `.bao/` (objects + SQLite) that two machines can share commits, executions, and evaluations.
- Avoid leaking API keys stored only in environment variables (repo data must not embed secrets).

## Open decisions

- [ ] Transport: HTTPS + token, SSH, or offline bundle (`bao bundle` / `bao unbundle`) first?
- [ ] Exact replication set: full `.bao/index.db` + `objects/` vs. subset.
- [ ] Merge rule for conflicting rows in `executions` / `evaluations`.

## Suggested first milestone

- Document a **file-based** export/import of read-only snapshots before live `push`/`pull`.

## Related code

- [`src/cmd_remote.c`](../src/cmd_remote.c) — current stubs for `push` / `pull`.
