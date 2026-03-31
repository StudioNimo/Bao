# Sample (try it now)

Assumes you have already run `make` at the repository root. From **this directory** you can reach a first commit:

```bash
# From repository root
cd examples/sample

# Point to the binary with a relative path (or put bin/ on PATH first)
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

Use `bao add` to stage and `bao commit` to snapshot. `.bao/` is created next to this directory.

See comments in [`bao.yaml`](bao.yaml) for configuration (example `profile` / `prompts_dir` layout for multiple providers).

More context: [repository `README.md`](../../README.md), [`docs/QUICKSTART.md`](../../docs/QUICKSTART.md).
