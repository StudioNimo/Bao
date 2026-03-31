# クイックスタート

GitHub から clone して、最短で `bao` を触る手順です。詳細はルートの [`README.md`](../README.md) も参照してください。

## 1. 取得とビルド

```bash
git clone https://github.com/OWNER/bao.git
cd bao
# 依存: SQLite3 開発用ライブラリ、C コンパイラ（README の「必要なもの」を参照）
make
```

## 2. サンプルで初回コミット

ツールのソースと混ざらないよう、サンプルは `examples/sample/` にあります。

```bash
cd examples/sample
../../bin/bao init
../../bin/bao add -A
../../bin/bao commit -m "first snapshot"
../../bin/bao log
```

## 3. 自分のプロジェクトで使う

空のディレクトリで:

```bash
/path/to/bao/bin/bao init
# bao.yaml と prompts/ を編集
/path/to/bao/bin/bao add -A
/path/to/bao/bin/bao commit -m "..."
```

または `examples/sample/` をコピーしてから編集しても構いません。
