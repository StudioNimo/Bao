# 🥟 Bao (Prompt Version Control System) — detailed design

## 1. Architecture overview

Bao is a local-first LLMOps CLI in C (C99/C11). External dependencies are kept small; the binary can be linked statically or dynamically. It applies Git-style content-addressed ideas to prompt configuration, run outputs, and evaluation scores.

---

## 2. Directory and filesystem layout

Everything lives under a hidden `.bao/` directory next to the current working directory.

### 2.1. Project root (user-visible)

- `bao.yaml`: model, parameters, and prompt metadata.
- `*.txt` / `*.md`: prompt bodies referenced from config.
- `*.jsonl`: test dataset (input variables).

### 2.2. Inside `.bao/` (managed)

- `index.db`: SQLite for runs, evaluations, and metadata.
- `HEAD`: current branch (symbolic ref) or detached hash.
- `refs/heads/<branch>`: branch tip hashes.
- `objects/commits/<hash>.json`: frozen `bao.yaml` + prompt text at commit time.
- `objects/results/<hash>.jsonl`: optional raw run logs (backup).

---

## 3. Database design (SQLite)

Tables in `.bao/index.db`; access via prepared statements from C.

### 3.1. `commits`

Snapshot metadata.

```sql
CREATE TABLE IF NOT EXISTS commits (
    hash TEXT PRIMARY KEY,           -- SHA-256 short hash (7 hex)
    model TEXT NOT NULL,             -- e.g. gpt-4-turbo
    temperature REAL,                -- sampling parameter
    prompt_text TEXT NOT NULL,       -- prompt body before expansion
    dataset_hash TEXT,               -- dataset hash for consistency checks
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    synced INTEGER DEFAULT 0         -- remote push flag (0 = not synced)
);
```

### 3.2. `executions`

LLM runs from `bao run`.

```sql
CREATE TABLE IF NOT EXISTS executions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    commit_hash TEXT NOT NULL,
    dataset_id TEXT,                 -- test id (e.g. "test_001")
    input_text TEXT NOT NULL,        -- variables embedded in prompt (JSON)
    output_text TEXT NOT NULL,       -- model response
    latency_ms INTEGER,
    tokens_used INTEGER,
    FOREIGN KEY(commit_hash) REFERENCES commits(hash)
);
```

### 3.3. `evaluations`

Human scores from `bao eval`.

```sql
CREATE TABLE IF NOT EXISTS evaluations (
    execution_id INTEGER PRIMARY KEY,
    score TEXT CHECK(score IN ('GOOD', 'NEUTRAL', 'BAD')),
    note TEXT,
    evaluated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(execution_id) REFERENCES executions(id)
);
```

---

## 4. In-memory structures (`bao.h`)

Core structs passed between modules; callers free with dedicated helpers.

```c
typedef struct {
    char *model;
    double temperature;
    int max_tokens;
    char *prompt_filepath;
    char *prompt_text;
} BaoConfig;

typedef struct {
    char *test_id;
    char *raw_json;
} BaoTestCase;

typedef struct {
    char *output_text;
    int latency_ms;
    int prompt_tokens;
    int completion_tokens;
} BaoLLMResponse;
```

---

## 5. Modules and internal APIs

### 5.1. `src/hash.c`

- **Optional:** `openssl/evp.h` when `BAO_USE_OPENSSL=1`; default is built-in SHA-256.
- **Role:** SHA-256 over strings and files.
- **Example:** `char* generate_sha256(const char* data, size_t len);`

### 5.2. `src/config.c`

- **Design note:** full `libyaml` parsing is a future option.
- **Role:** load `bao.yaml` into `BaoConfig`.
- **Example:** `BaoConfig* load_config(const char* path);`

### 5.3. `src/template.c`

- **Depends:** `cJSON.h`
- **Role:** replace `{{key}}` in prompts with JSON values; dynamic `realloc` for safety.
- **Example:** `char* render_template(const char* tmpl, cJSON* variables);`

### 5.4. `src/db.c`

- **Depends:** `sqlite3.h`
- **Role:** connections, transactions, queries.
- **Example:** `int db_insert_execution(sqlite3* db, ...);`

---

## 6. Commands and sequences

### 6.1. `bao commit -m "<message>"`

1. **Read:** `load_config("bao.yaml")`.
2. **Hash:** concatenate model, parameters, prompts → SHA-256.
3. **Save object:** write `.bao/objects/commits/<hash>.json`.
4. **Save DB:** INSERT into `commits`.
5. **Update ref:** move branch pointer at HEAD.

### 6.2. `bao run -d <dataset.jsonl>`

1. **Resolve HEAD:** current commit + config.
2. **Hash dataset:** `-d` file hash → `commits.dataset_hash`.
3. **Stream:** read JSONL line by line, `cJSON_Parse`.
4. **Render:** `render_template`.
5. **API:** HTTP POST via `libcurl` to the LLM provider.
   - *Payload example:* `"content": "Open the browser tomorrow at 10am"`
6. **Save:** `db_insert_execution` into SQLite.

### 6.3. `bao eval`

1. **Fetch:** one `executions` row without an `evaluations` row.
2. **RAW mode:** `termios` off canonical mode.
3. **Display:** prompt + output with ANSI colors.
4. **Input:** `1`/`2`/`3` via `getchar()`.
5. **Update:** INSERT into `evaluations`, advance.

### 6.4. `bao diff --eval <hash_A> <hash_B>`

1. **Validate:** same `dataset_hash` (warn if not).
2. **Query:** join `executions` + `evaluations` for score changes per `dataset_id`.
3. **Report:** regressions in red, improvements in green.

---

## 7. Remote sync

### 7.1. Push payload (`bao push`)

JSON for unsynced (`synced = 0`) data:

```json
{
  "project": "my-project",
  "commits": [
    {
      "hash": "8f3a2b1",
      "model": "gpt-4-turbo",
      "prompt_text": "You are an agent...",
      "executions": [
        {
          "input_text": "{\"utterance\": \"open settings\"}",
          "output_text": "{\"intent\": \"open_settings\"}",
          "evaluation": "GOOD"
        }
      ]
    }
  ]
}
```

---

## 8. Build and Makefile

- **Compiler:** `gcc` or `clang`
- **Optimization:** `-O2` for release
- **Warnings:** `-Wall -Wextra` (`-Werror` in `RELEASE=1` CI)
- **Typical links:** `-lsqlite3` (and optionally `-lcrypto`, `-lcurl`)

The snippet below is illustrative; see the real `Makefile` for flags.

```makefile
CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -I./include
LDFLAGS = -lcrypto -lcurl -lsqlite3 -lyaml

TARGET = bao
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)
```

---

## 9. Error handling

- **OOM:** `malloc`/`realloc` failure → message and `exit(1)`.
- **Network:** `libcurl` ≠ `CURLE_OK` → log and skip the case (no retry loop in design).
- **DB lock:** `SQLITE_BUSY` → short backoff, retry up to N times.

---

## 10. Implementation notes (this repository)

- **`load_config`:** The design doc mentions `libyaml`; the current code parses **flat `key: value` lines** in `bao.yaml`. Nested YAML can be introduced later with `libyaml`.
- **`cJSON`:** Vendored under `third_party/cJSON/`; used in `template.c` and JSON commit objects.
- **SHA-256:** Default implementation in `hash.c`; optional OpenSSL with `make BAO_USE_OPENSSL=1`.
- **Schema migration:** Older `commits` layouts may be recreated based on `user_version` during early development.

---

*(End of document)*
