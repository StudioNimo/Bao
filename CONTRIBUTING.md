# Contributing to Bao

ありがとうございます。Bao はまだプロトタイプ段階ですが、バグ報告・改善提案・PR を歓迎します。

プロジェクトの背景と名前の由来は [`docs/concept_and_naming.md`](docs/concept_and_naming.md) にあります。

**リポジトリ構成:** ツール本体は `src/` と `Makefile` です。ユーザー向けの最小サンプルは **`examples/sample/`** にあり、`cli_smoke.sh` 等のテストはここをコピー元にしています。ルート直下に `bao.yaml` はありません。

## 開発環境

- macOS / Linux を想定
- 依存: `sqlite3`（開発用ヘッダ含む）, C コンパイラ（`cc`）
- 任意: OpenSSL（`make BAO_USE_OPENSSL=1`）

## ビルド / テスト

```bash
make
make test
```

## 変更の方針

- **動作互換を重視**: 既存のコマンド出力・終了コードを壊す変更は、テスト更新と合わせて理由を明記してください。
- **安全性**: パス操作（`..`/絶対パス/`.bao` 直下）などは常に安全側に倒します。
- **依存追加は慎重に**: 追加が必要なら、理由と代替案も記載してください。

## Issue / PR

- **バグ**: 再現手順（最小）、期待結果、実際の結果、OS/環境を添えてください。
- **機能提案**: 目的、ユーザー価値、想定 UX、互換性影響を記載してください。
- **PR**: テスト追加/更新（または不要である根拠）を含めてください。`main` 向け PR では GitHub Actions（`.github/workflows/ci.yml`）が走ります。マージ前に CI を必須にする設定は [`docs/BRANCH_PROTECTION.md`](docs/BRANCH_PROTECTION.md) を参照してください。

## コードスタイル（目安）

- C11、警告ゼロを目標（`-Wall -Wextra -Wpedantic`）
- 早期 return + 明確なエラーメッセージ

## ライセンス

コントリビューションは `LICENSE`（MIT）に基づき提供されたものとみなします。
