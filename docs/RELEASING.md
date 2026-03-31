# Releasing

このドキュメントは、Bao を公開リリースする手順のメモです。

## リリース前チェック

- `make test` が通る
- 利用者向け手順（`README.md` / `docs/QUICKSTART.md`）が `examples/sample/` のパスと一致している
- `CHANGELOG.md` を更新
- `man/bao.1` のバージョン（ヘッダの `bao X.Y.Z`）を更新
- `src/bao.h` の `BAO_VERSION` を更新

## タグ

```bash
git tag -a vX.Y.Z -m "vX.Y.Z"
git push --tags
```

## GitHub Releases

- リリースノートに `CHANGELOG.md` の該当箇所を貼り付け
