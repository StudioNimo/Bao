# Bao テスト計画（ユーザーシナリオ S01〜S20 + 追加 S21 + コマンド網羅）

## 1. 目的

- 想定される **ユーザーシナリオ S01〜S20**を結合テストで再現し、主要フローが破綻しないことを保証する（**S21** は別スクリプトで追加検証）。
- **すべてのコマンド・サブコマンド・主要オプション**は `tests/cli_smoke.sh` でスモーク実行する。
- **期待出力の厳密アサーション**（`remote -v` の `name<TAB>url`、`status --porcelain` の行形式、`stash` の list / push / pop / apply / drop / clear）は `tests/assert_strict.sh` で検証する。
- **内部結合**（`bin/bao` 直接）と **外部結合**（`PATH` 上の `bao`）の両方で 20 シナリオを実行する。

## 2. テスト資産一覧

| 資産 | 内容 |
|------|------|
| `tests/TEST_PLAN.md` | 本書（シナリオとテストの対応） |
| `examples/sample/` | スモーク・統合テストがコピーする `bao.yaml` / `prompts/` / `test_cases.jsonl` の参照元 |
| `tests/scenarios_20_common.sh` | **S01〜S20** の本体（`BAO` でバイナリを切替） |
| `tests/scenarios_20_internal.sh` | 内部結合ラッパー（S01〜S20） |
| `tests/scenarios_20_external.sh` | 外部結合ラッパー（S01〜S20） |
| `tests/scenarios_21_extra_common.sh` | **S21**（複数プロバイダー比較・結果ファイル固定） |
| `tests/scenarios_21_internal.sh` | 内部結合ラッパー（S21 のみ） |
| `tests/scenarios_21_external.sh` | 外部結合ラッパー（S21 のみ） |
| `tests/cli_smoke.sh` | 全コマンド / サブコマンド / 主要オプションのスモーク |
| `tests/stub_options_fail.sh` | 未実装コマンド（スタブ）の **オプション付き**実行（非0終了を主に期待） |
| `tests/assert_strict.sh` | `remote -v` / `status --porcelain` / `stash` の期待出力・状態遷移の厳密検証 |
| `tests/integration_internal.sh` | init〜commit〜checkout〜restore の流れ |
| `tests/integration_external.sh` | PATH 経由の最小フロー |
| `tests/integration_multi_provider.sh` | 複数プロバイダー同一リポ |

## 3. ユーザーシナリオ 20 と検証内容

| ID | ユーザーシナリオ | 主な検証 | 実装 |
|----|------------------|----------|------|
| S01 | 新規プロジェクトで初回コミット | `init` → `add` → `commit -m` → フル64ハッシュ | `s01_*` |
| S02 | メッセージを自動生成してコミット（CI ではエディタ無効） | `BAO_NO_EDITOR=1` + `commit --no-edit` | `s02_*` |
| S03 | 同一リポで複数プロバイダー（設定切替で別コミット） | `provider`/`profile`/`model`/`prompts_dir` 変更、`HEAD`≠`HEAD~1` | `s03_*` |
| S04 | ブランチを切って作業コミット | `switch -c` → `commit` → `switch main` | `s04_*` |
| S05 | デタッチ HEAD で特定コミットを指す | `checkout --detach` + `rev-parse` | `s05_*` |
| S06 | リリースタグの付与・一覧・削除 | `tag` / `tag -l` / `tag -d` | `s06_*` |
| S07 | 同一ツリーで amend（冪等） | `commit --amend --no-edit` | `s07_*` |
| S08 | 履歴を戻すが索引は保持（soft） | `reset --soft` | `s08_*` |
| S09 | 指定コミットに HEAD を揃える（hard） | `reset --hard` | `s09_*` |
| S10 | 作業を一時退避して戻す | `stash push` → `pop` | `s10_*` |
| S11 | 過去コミットの索引へ戻す | `restore --source <hash>` | `s11_*` |
| S12 | 差分の見方（作業ツリー / 索引 / コミット間） | `diff` / `diff --cached` / `diff H H` | `s12_*` |
| S13 | 短い rev とフル rev の対応 | `rev-parse` / `--short=7` | `s13_*` |
| S14 | リモート設定の追加・変更・削除 | `remote add` / `set-url` / `rename` / `remove` 等 | `s14_*` |
| S15 | 設定ファイルの参照 | `config --list` / `config model` | `s15_*` |
| S16 | ステージング前の確認（dry-run） | `add -n` / `--dry-run` | `s16_*` |
| S17 | 索引とディスクからの削除 | `rm --cached` / `rm -r -f` | `s17_*` |
| S18 | 履歴のワンライン表示（プロバイダ付き） | `log -n 2 --oneline` に `provider=` | `s18_*` |
| S19 | 状態表示の形式切替 | `status -s` / `-b` / `--porcelain` | `s19_*` |
| S20 | 未実装コマンド・無効ヘルプは失敗で返る | `run`/`eval`/`push`/`pull`/`clone`/`merge`/`fetch`/`rebase`/`filter-branch`/`cherry-pick`/`revert`/`blame`/`help` 未知トピック | `s20_*` |
| S21 | 複数プロバイダー比較（結果ファイル名固定） | `bao.yaml` 切替 + `results/openai.jsonl` / `results/anthropic.jsonl` を別コミットに固定 | `scenarios_21_extra_*` |

## 4. 実行方法

```bash
make test
```

個別:

```bash
./tests/scenarios_20_internal.sh
./tests/scenarios_20_external.sh
./tests/scenarios_21_internal.sh
./tests/scenarios_21_external.sh
./tests/cli_smoke.sh
./tests/stub_options_fail.sh
```

## 5. `make test` の構成

1. `integration_internal.sh`
2. `integration_external.sh`
3. `integration_multi_provider.sh`
4. `scenarios_20_internal.sh`（S01〜S20）
5. `scenarios_20_external.sh`（S01〜S20）
6. `scenarios_21_internal.sh`（S21）
7. `scenarios_21_external.sh`（S21）
8. `cli_smoke.sh`
9. `stub_options_fail.sh`
10. `assert_strict.sh`

## 6. 全コマンド・オプション網羅について

- **コマンド列と主要オプション**は `cli_smoke.sh` が担当（スタブは「非0終了」を期待）。
- **20 シナリオ（S01〜S20）**は利用実態に近い **ストーリー単位の回帰**を担当。
- **S21** は `scenarios_21_*` に分離（20 本と切り離して固定）。
- **スタブのオプション付き**は `stub_options_fail.sh` が担当（`clone --abort` 等の **0 終了**と、それ以外の **非0 終了**を区別）。
- これらを `make test` で連続実行し、回帰を一本化している。
