# Releasing

Checklist for publishing a Bao release.

## Pre-release

- `make test` passes
- User-facing steps in `README.md` / `docs/QUICKSTART.md` match paths under `examples/sample/`
- Update `CHANGELOG.md`
- Update `man/bao.1` version (header line `bao X.Y.Z`)
- Update `BAO_VERSION` in `src/bao.h`

## Tags

```bash
git tag -a vX.Y.Z -m "vX.Y.Z"
git push --tags
```

## GitHub Releases

- Paste the relevant section from `CHANGELOG.md` into the release notes
