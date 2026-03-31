# Bao (Prompt Version Control System)

Baoは、プロンプト・設定・データセット・実行結果・評価をスナップショットとして扱うローカルファーストなCLIです。

- **最短の試し方:** [`docs/QUICKSTART.md`](docs/QUICKSTART.md) または下の「すぐ試す（サンプルプロジェクト）」
- **開発の経緯・命名の由来:** `docs/concept_and_naming.md`
- **概要:** `docs/system_design.md`
- **詳細設計（実装のコア・コンテキスト）:** `docs/architecture.md`

## リポジトリの構成

| 場所 | 内容 |
|------|------|
| `src/`, `Makefile` | **CLI 本体**のソースとビルド |
| `examples/sample/` | **すぐ試せるサンプル**（`bao.yaml`・`prompts/`・`test_cases.jsonl`）。clone 後はここで `bao init` から始められます |
| `tests/` | 結合テスト・シナリオテスト |
| `docs/` | 設計・手順ドキュメント |
| `man/` | `man` 用ページ（プロトタイプ） |

ルート直下には **プロジェクト用の `bao.yaml` は置いていません**（ツールと作業ツリーを分離するため）。実際の作業は `examples/sample/` か、任意のディレクトリで行います。

## ライセンス

- `LICENSE`（MIT）
- 同梱しているサードパーティ: `THIRD_PARTY_NOTICES.md`

## GitHub から取得してローカルで使う

### 必要なもの

- **Git**（リポジトリの取得）
- **C コンパイラ**（`cc` — macOS では Clang、Linux では GCC など）
- **SQLite3 の開発用ライブラリ**（`libsqlite3` とヘッダ `sqlite3.h`）
- ビルド済みバイナリの配布はしていないため、**ソースから `make` でビルド**します。JSON パーサは **cJSON（リポジトリ同梱）**、SHA-256 はデフォルトで **内蔵実装**です。

### 1. リポジトリを取得する

公開先の URL に合わせてください（フォークした場合はフォークの URL）。

```bash
git clone https://github.com/StudioNimo/Bao.git
cd bao
```


### 2. 依存パッケージの例（OS ごと）

- **macOS**  
  - コマンドラインツール: `xcode-select --install`（未導入の場合）  
  - `sqlite3.h` が見つからないとき: `brew install sqlite` など
- **Ubuntu / Debian**

```bash
sudo apt-get update
sudo apt-get install -y build-essential pkg-config libsqlite3-dev
```

### 3. ビルドする

```bash
make
```

成功すると `bin/bao` が生成されます。

### 4. 動作確認する

```bash
./bin/bao version
./bin/bao help
```

### 5. すぐ試す（サンプルプロジェクト）

ビルド後、付属のサンプルで初回コミットまで試せます。

```bash
cd examples/sample
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

詳細は `examples/sample/README.md` および [`docs/QUICKSTART.md`](docs/QUICKSTART.md) を参照してください。

### 6. どこからでも `bao` と打てるようにする（任意）

- **そのシェルだけ**（クローンしたディレクトリで）:

```bash
export PATH="$(pwd)/bin:$PATH"
bao version
```

- **恒久的に**（例: zsh）: `~/.zshrc` に次のように**絶対パス**で追記します。

```bash
echo 'export PATH="/あなたがクローンした場所/bao/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

- **man ページ**を入れる場合は、`make install-man` の表示に従って `man/bao.1` をシステムの `man` ディレクトリへコピーしてください。

### ビルドオプション（任意・上記ステップ 3 の再ビルド）

OpenSSL の `libcrypto` でハッシュ計算を行うビルド:

```bash
make clean
make BAO_USE_OPENSSL=1
```

（Linux では `libssl-dev` などが追加で必要な場合があります。）

## 最小の操作（任意のプロジェクトディレクトリ）

空のディレクトリ、または `bao.yaml` があるディレクトリで作業します（例ではリポジトリルートから `bin/bao` を呼び出しています）。

```bash
mkdir -p ~/my-bao-project && cd ~/my-bao-project
/path/to/bao/bin/bao init
# bao.yaml と prompts/ を編集・配置したあと
/path/to/bao/bin/bao add -A
# または: bao add bao.yaml prompts/ test_cases.jsonl
/path/to/bao/bin/bao commit -m "first snapshot"
/path/to/bao/bin/bao log
```

初めての場合は、上記より **`examples/sample/` の手順**のほうが簡単です。

`commit` 前に `bao add` でステージ（`index`）へ登録する必要があります。`bao add -A` は `bao.yaml` ・設定上のプロンプトファイル・データセット・`prompts/` 以下をまとめて追加します。

`bao.yaml` には `model` と `prompt_file`（または `prompts_dir`）が必要です。`run` / `eval` / `push` / `pull` は今後の拡張です。

## コマンド（抜粋）

- **`add`**: `-A/--all`, `-u/--update`, `-n/--dry-run`, `-v/--verbose`
- **`commit`**: `-m`, `-F/--file`, `-a/--all`, `-q/--quiet`, `--amend`
- **`log`**: `-n/--max-count`, `-1`, `--oneline`
- **`status`**: `-s/--short`, `-b/--branch`, `--porcelain`
- **`checkout`**: `-b`, `-B`, `--detach`
- **`switch`**: `-c`, `-C`, `--detach`
- **`rev-parse`**: `HEAD~n` は **親リンク（parent）** を辿ります（DB の時系列ではありません）

## ドキュメント

| ファイル | 内容 |
|----------|------|
| `docs/QUICKSTART.md` | clone からサンプルで試す最短手順 |
| `examples/README.md` | サンプルディレクトリの説明 |
| `examples/sample/README.md` | `examples/sample/` だけで試す手順 |
| `docs/concept_and_naming.md` | 開発コンセプト・命名の由来（包子メタファー、CLIの手触り） |
| `docs/BRANCH_PROTECTION.md` | `main` 向け PR で CI 必須にする（GitHub ブランチ保護の手順） |
| `docs/system_design.md` | システム設計（概要） |
| `docs/architecture.md` | 詳細システム設計（DB・構造体・コマンドフロー） |
| `docs/RELEASING.md` | リリース作業メモ |
| `man/bao.1` | `man bao` 用マニュアル（プロトタイプ） |
