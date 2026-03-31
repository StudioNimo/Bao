# Security Policy

## Scope

- `bao` itself (`src/`)
- Build and packaging (`Makefile`)

## Reporting vulnerabilities

We do not publish a private contact channel (email, etc.) at this time. For issues that can be discussed in public, please use **GitHub Issues**.

If your repository has GitHub **Private vulnerability reporting** enabled, you may be able to report through the **Security** tab.

## Design notes

- `bao` accepts repository-relative paths only (path traversal and direct `.bao/` manipulation are rejected).
