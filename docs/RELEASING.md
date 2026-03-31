# Releasing

Checklist for publishing a Bao release.

## Pre-release

- `make test` passes
- User-facing steps in [`README.md`](../README.md) / [`QUICKSTART.md`](QUICKSTART.md) match paths under [`examples/sample/`](../examples/sample/)
- Update [`CHANGELOG.md`](../CHANGELOG.md)
- Update [`man/bao.1`](../man/bao.1) version (header line `bao X.Y.Z`)
- Update `BAO_VERSION` in [`src/bao.h`](../src/bao.h)

## Tags

```bash
git tag -a vX.Y.Z -m "vX.Y.Z"
git push --tags
```

## GitHub Releases

- Paste the relevant section from [`CHANGELOG.md`](../CHANGELOG.md) into the release notes
