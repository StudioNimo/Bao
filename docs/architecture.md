# 🥟 Bao (Prompt Version Control System) 詳細システム設計書

## 1. システム・アーキテクチャ概要

Baoは、C言語（C99/C11準拠）で実装されたローカルファーストのLLMOps用CLIツールである。外部ライブラリを最小限に抑え、静的/動的リンクにより単一の高速なバイナリとして動作する。Gitのコンテンツアドレス・ストレージの概念を応用し、プロンプト設定・実行結果・評価スコアの完全なスナップショットを管理する。

---

## 2. ディレクトリ・ファイルシステム仕様

Baoはカレントディレクトリ直下の `.bao/` 隠しディレクトリ内に全データを格納する。

### 2.1. プロジェクトルート層（ユーザー操作領域）

- `bao.yaml`: プロンプトのメタデータ、モデル設定、パラメータを定義するYAMLファイル。
- `*.txt / *.md`: 外部参照されるプロンプト本文。
- `*.jsonl`: テストデータセット（入力変数群）。

### 2.2. `.bao/` 内部構造（システム管理領域）

- `index.db`: 実行結果、評価、メタデータを管理するSQLite3データベース。
- `HEAD`: 現在のチェックアウト状態を示すテキストファイル（例: `ref: refs/heads/main` またはハッシュ値）。
- `refs/heads/<branch_name>`: 各ブランチの最新コミットハッシュを格納するテキストファイル。
- `objects/commits/<hash>.json`: コミット時の `bao.yaml` とプロンプト本文を完全にフリーズしたJSONファイル。
- `objects/results/<hash>.jsonl`: 当該コミットで実行された推論結果の生ログ（バックアップ用）。

---

## 3. データベース設計 (SQLite Schema)

`.bao/index.db` に構築されるテーブル群。C言語からはプリペアドステートメントを用いて安全にアクセスする。

### 3.1. `commits` テーブル

スナップショットのメタデータを管理。

```sql
CREATE TABLE IF NOT EXISTS commits (
    hash TEXT PRIMARY KEY,           -- SHA-256ショートハッシュ(7桁)
    model TEXT NOT NULL,             -- 例: gpt-4-turbo
    temperature REAL,                -- 推論パラメータ
    prompt_text TEXT NOT NULL,       -- 展開前のプロンプト本文
    dataset_hash TEXT,               -- 実行時のデータセットのハッシュ（一貫性検証用）
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    synced INTEGER DEFAULT 0         -- リモートpush済みフラグ (0:未, 1:済)
);
```

### 3.2. `executions` テーブル

`bao run` によるLLMの推論結果を管理。

```sql
CREATE TABLE IF NOT EXISTS executions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    commit_hash TEXT NOT NULL,       -- 紐づくコミット
    dataset_id TEXT,                 -- テストデータの識別子 (例: "test_001")
    input_text TEXT NOT NULL,        -- プロンプトに埋め込まれた変数(JSON文字列)
    output_text TEXT NOT NULL,       -- LLMからのレスポンス本文
    latency_ms INTEGER,              -- API応答時間
    tokens_used INTEGER,             -- 消費トークン数
    FOREIGN KEY(commit_hash) REFERENCES commits(hash)
);
```

### 3.3. `evaluations` テーブル

`bao eval` による人間の評価結果を管理。

```sql
CREATE TABLE IF NOT EXISTS evaluations (
    execution_id INTEGER PRIMARY KEY,
    score TEXT CHECK(score IN ('GOOD', 'NEUTRAL', 'BAD')),
    note TEXT,                       -- 自由記述のメモ（オプション）
    evaluated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(execution_id) REFERENCES executions(id)
);
```

---

## 4. メモリデータ構造 (C Struct Definitions: `bao.h`)

C言語内でデータを引き回すためのコア構造体。各モジュールはこれらのポインタを受け渡し、使用後は専用の `free_*` 関数でメモリを解放する責務を持つ。

```c
// プロンプト設定を保持する構造体 (config.c でパース)
typedef struct {
    char *model;
    double temperature;
    int max_tokens;
    char *prompt_filepath;
    char *prompt_text;
} BaoConfig;

// テストデータ1行分を保持する構造体 (template.c で展開)
typedef struct {
    char *test_id;
    char *raw_json; // input変数のJSON文字列表現
} BaoTestCase;

// LLMの実行結果を保持する構造体 (cmd_run.c で生成)
typedef struct {
    char *output_text;
    int latency_ms;
    int prompt_tokens;
    int completion_tokens;
} BaoLLMResponse;
```

---

## 5. モジュール分割と内部API仕様

### 5.1. `src/hash.c` (暗号化・ハッシュモジュール)

- **依存:** `openssl/evp.h`（オプション; デフォルトは内蔵SHA-256）
- **責務:** 文字列およびファイルからSHA-256ハッシュを計算する。
- **関数:** `char* generate_sha256(const char* data, size_t len);`

### 5.2. `src/config.c` (YAMLパーサーモジュール)

- **依存:** `yaml.h` (libyaml)
- **責務:** `bao.yaml` を読み込み、`BaoConfig` 構造体を動的にメモリ確保して返す。
- **関数:** `BaoConfig* load_config(const char* path);`

### 5.3. `src/template.c` (テンプレートエンジンモジュール)

- **依存:** `cJSON.h`
- **責務:** プロンプト内の `{{key}}` を、JSONオブジェクトの値で置換し、新たな文字列を生成する。バッファオーバーランを防ぐため、動的に `realloc` を行う。
- **関数:** `char* render_template(const char* tmpl, cJSON* variables);`

### 5.4. `src/db.c` (データ永続化モジュール)

- **依存:** `sqlite3.h`
- **責務:** SQLiteとのコネクション確立、トランザクション処理、SQLクエリの実行をカプセル化する。
- **関数:** `int db_insert_execution(sqlite3* db, const char* hash, const char* input, const char* output, int latency);`

---

## 6. コマンド仕様とシーケンス (CLI Commands)

### 6.1. `bao commit -m "<message>"`

1. **Read:** `load_config("bao.yaml")` を呼び出し設定を読み込む。
2. **Hash:** 設定内容（モデル、パラメータ、プロンプト）を連結し、SHA-256ハッシュを生成。
3. **Save Object:** `.bao/objects/commits/<hash>.json` を生成し、設定の完全なJSONダンプを書き込む。
4. **Save DB:** `commits` テーブルに INSERT。
5. **Update Ref:** `.bao/HEAD` が指すブランチのポインタを新しいハッシュに更新。

### 6.2. `bao run -d <dataset.jsonl>`

1. **Resolve HEAD:** 現在のコミットハッシュと設定を取得。
2. **Hash Dataset:** `-d` で指定されたファイルのハッシュを計算し、`commits.dataset_hash` を更新（一貫性担保）。
3. **Stream Read:** `.jsonl` を1行ずつ読み込み、`cJSON_Parse` で解析。
4. **Render:** `render_template` でプロンプトを構築。
5. **API Call:** `libcurl` を用いてLLMプロバイダへHTTP POSTリクエストを送信。
   - *Payload Example:* `"content": "明日のAM10時にブラウザを開いて"`
6. **Parse & Save:** レスポンスから出力テキスト（構造化JSON等）を抽出し、`db_insert_execution` でSQLiteへ保存。

### 6.3. `bao eval`

1. **Fetch:** `evaluations` テーブルに存在しない `executions` レコードを1件 SELECT。
2. **Raw Mode:** `termios` でターミナルをカノニカルモードからRAWモードに変更。
3. **Display:** 入力プロンプトと出力結果をANSIカラーコードでハイライト表示。
4. **Input:** `1`, `2`, `3` のキー入力を `getchar()` で即時捕捉。
5. **Update:** 捕捉したスコアを `evaluations` テーブルに INSERT し、次のレコードへ進む。

### 6.4. `bao diff --eval <hash_A> <hash_B>`

1. **Validate:** 双方のコミットが同一の `dataset_hash` を持っているか検証（異なれば警告表示）。
2. **Join Query:** `executions` と `evaluations` を自己結合し、同一 `dataset_id` でスコアが変化したレコードを抽出。
3. **Report:** `GOOD -> BAD` になったもの（デグレ）を赤色で、`BAD -> GOOD` になったものを緑色でターミナルに出力。

---

## 7. 外部連携仕様 (Remote Sync)

### 7.1. Push Payload (`bao push`)

未同期（`synced = 0`）のデータをリモートサーバーに送信する際のJSON構造。

```json
{
  "project": "my-project",
  "commits": [
    {
      "hash": "8f3a2b1",
      "model": "gpt-4-turbo",
      "prompt_text": "あなたはエージェントです...",
      "executions": [
        {
          "input_text": "{\"utterance\": \"設定を開く\"}",
          "output_text": "{\"intent\": \"open_settings\"}",
          "evaluation": "GOOD"
        }
      ]
    }
  ]
}
```

---

## 8. ビルド・コンパイル仕様 (Makefile)

- **コンパイラ:** `gcc` または `clang`
- **最適化:** `-O2` (リリースビルド用)
- **警告レベル:** `-Wall -Wextra -Werror` (厳格な品質管理)
- **リンクフラグ (LDFLAGS):**
  - `-lcrypto` (Macの場合は `/opt/homebrew/opt/openssl/lib` などのパス指定が必要な場合あり)
  - `-lcurl`
  - `-lsqlite3`
  - `-lyaml`

```makefile
# Makefile 抜粋
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

## 9. エラーハンドリングと例外処理

- **OOM (Out of Memory):** `malloc`, `realloc` の戻り値が `NULL` の場合は即座にエラーメッセージを出力し、`exit(1)` で安全に終了する。
- **Network Failure:** `libcurl` の戻り値が `CURLE_OK` 以外（タイムアウト等）の場合、リトライは行わずエラーをログ出力し、当該テストケースの推論をスキップする。
- **DB Lock:** SQLiteが `SQLITE_BUSY` を返した場合（他プロセスとの競合）、数ミリ秒のバックオフを挟んで最大3回リトライする。

---

## 10. 実装メモ（本リポジトリ）

- **`load_config`:** 設計書では `libyaml` を想定しているが、現行実装は **`bao.yaml` のフラットな `key: value` 行**のみを解釈する軽量パーサである。ネストや複雑なYAMLが必要になった段階で `libyaml` への差し替えが可能。
- **`cJSON`:** `third_party/cJSON/` に同梱し、`template.c` およびコミットJSONの生成で利用する。
- **SHA-256:** デフォルトは `hash.c` の内蔵実装。`make BAO_USE_OPENSSL=1` で OpenSSL の EVP を使うビルドも可能。
- **スキーマ移行:** `index.db` の `commits` テーブルが旧形式の場合、`user_version` に基づきテーブルを再作成する（開発初期の破壊的移行）。

---

*(End of System Design Document)*
