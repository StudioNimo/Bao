# main ブランチ保護（PR マージ前に CI を必須にする）

リポジトリ上で **プルリクエストが緑のチェックを通過するまで `main` にマージできない**ようにする手順です。  
（このリポジトリの CI は `.github/workflows/ci.yml` の `ci` ワークフローです。）

## 前提

- GitHub の **Settings → Actions → General** で Actions が有効であること
- `main` ブランチがデフォルトブランチであること（別名の場合はルールの対象ブランチを読み替えてください）

## 手順（GitHub の Web UI）

1. リポジトリの **Settings** を開く
2. 左メニュー **Rules** → **Rulesets**（または従来の **Branches** → **Branch protection rules**）を開く
3. **New ruleset** / **Add branch protection rule** で `main` を対象にする
4. 次を有効にする（推奨）:
   - **Require a pull request before merging**  
     （直接 `main` への push を防ぎ、PR 経由にする）
   - **Require status checks to pass before merging**
   - **Status checks that are required** で、少なくとも次を追加:
     - **`ci (required)`**  
       （ワークフロー `ci` の集約ジョブ。Ubuntu / macOS の両方が成功したときだけ成功します）
5. 必要に応じて:
   - **Require branches to be up to date before merging**（ベースとの同期を強制）
   - **Require conversation resolution before merging**（レビューコメントの解決）

初回は、一度 PR を開いて CI が走ったあとで「必須チェック」の候補に `ci (required)` が表示されます。表示されない場合は、ワークフローが `main` 向き PR で完了しているか確認してください。

## 参考

- ワークフロー定義: `.github/workflows/ci.yml`
- `pull_request` は `main` 向けに限定してあります（無関係なブランチでは CI を走らせません）
