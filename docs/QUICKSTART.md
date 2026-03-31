# Quickstart

Clone from GitHub and build `bao` with minimal steps. See the root [`README.md`](../README.md) for full details.

## 1. Clone and build

Use the URL of your fork or this repo (see [README “Clone the repository”](../README.md)).

```bash
git clone https://github.com/StudioNimo/Bao.git
cd Bao
# Dependencies: SQLite3 dev library, C compiler (see README “Prerequisites”)
make
```

## 2. First commit with the sample

The sample lives under [`examples/sample/`](../examples/sample/) so it stays separate from the tool sources.

```bash
cd examples/sample
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

## 3. Use your own project

In an empty directory:

```bash
/path/to/Bao/bin/bao init
# Edit bao.yaml and prompts/
/path/to/Bao/bin/bao add -A
/path/to/Bao/bin/bao commit -m "..."
```

You can also copy [`examples/sample/`](../examples/sample/) and edit from there.
