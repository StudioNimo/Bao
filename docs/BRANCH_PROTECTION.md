# Protect `main` (require CI before merging PRs)

How to require **green checks** on pull requests before merging into `main`.  
(This repo’s workflow is `ci` in `.github/workflows/ci.yml`.)

## Current ruleset (repository)

A **branch ruleset** named **Protect main** is configured on this repo (targets the default branch via `~DEFAULT_BRANCH`). It enforces:

- **Pull request required** before merging (0 approving reviews required; merge / squash / rebase allowed)
- **Required status check:** `ci (required)` (with **strict** policy: branch must be up to date)
- **No force-push** (`non_fast_forward`)

Manage it under **Settings → Rules → Rulesets**, or via the API:  
`GET /repos/StudioNimo/Bao/rulesets` and `GET /repos/StudioNimo/Bao/rules/branches/main`.

## Prerequisites

- **Settings → Actions → General:** Actions enabled
- Default branch is `main` (adjust branch names in rules if yours differs)

## Steps (GitHub web UI)

1. Open the repository **Settings**
2. Open **Rules** → **Rulesets** (or **Branches** → **Branch protection rules**)
3. **New ruleset** / **Add branch protection rule** targeting `main`
4. Enable (recommended):
   - **Require a pull request before merging**  
     (blocks direct pushes to `main`; use PRs)
   - **Require status checks to pass before merging**
   - Under **Status checks that are required**, add at least:
     - **`ci (required)`**  
       (Aggregate job from workflow `ci`; succeeds only when both Ubuntu and macOS pass)
5. Optionally:
   - **Require branches to be up to date before merging**
   - **Require conversation resolution before merging**

On first use, open a PR so CI runs once; then `ci (required)` appears in the required checks list. If it does not, confirm the workflow completed on a PR targeting `main`.

## References

- Workflow: `.github/workflows/ci.yml`
- `pull_request` is limited to `main` (other branches do not run this CI)
