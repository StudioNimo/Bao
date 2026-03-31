# Changelog

このプロジェクトはセマンティックバージョニング（`MAJOR.MINOR.PATCH`）を想定しています。

## 0.1.0 - 2026-03-31

- 初期公開（git-like CLI subset: `init/add/commit/log/status/checkout/switch` ほか）
- `bao.yaml`（フラット `key: value`）による設定ロード
- `prompts_dir` + `profile` による複数プロバイダー運用パターン
- 内部/外部の統合テストとシナリオテスト（`make test`）
- ドキュメント: 開発コンセプトと命名の由来（`docs/concept_and_naming.md`）
- CI: `main` 向け PR / `main` への push でテスト。集約ジョブ `ci (required)` でブランチ保護と連携可能（`docs/BRANCH_PROTECTION.md`）
- レイアウト: サンプルプロジェクトを `examples/sample/` に分離（ルート直下の `bao.yaml` とプロンプトを廃止）。`docs/QUICKSTART.md` 追加
