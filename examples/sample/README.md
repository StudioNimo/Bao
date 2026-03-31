# サンプル（すぐ試す）

リポジトリルートで `make` 済みであることを前提に、**このディレクトリで**初回コミットまで試せます。

```bash
# リポジトリルートから
cd examples/sample

# バイナリを相対パスで指定（または事前に PATH に bin を通す）
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

作業のたびに `bao add` でステージし、`bao commit` でスナップショットを切ります。`.bao/` はこのディレクトリ直下に作成されます。

設定の意味はルートの `bao.yaml` 内コメントを参照してください（複数プロバイダー用の `profile` / `prompts_dir` 構成の例です）。
