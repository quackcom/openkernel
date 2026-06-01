## Summary

<!-- What does this PR change and why? Link issues: Fixes #123, relates to [BUG #N] -->

Write your summary here.

**Optional merge commit** ([details](docs/guides/PR_GOVERNANCE.md#merge-commit-title-and-description)):

- Add `[USE_PR_TITLE]` to the **PR title** and/or a visible line in **Summary** → commit **subject** ( `[Pull Request #N]` is stripped ).
- Add `[USE_PR_DESC]` on a line in **Summary only** → commit **body** is **this section** (not Testing/Checklist below).

The PR summary bot shows a **Planned merge commit** preview. After merge, **Sync merge commit** applies it on `main` if you set secret `OPENKERNEL_REPO_PAT` (admin token — you do **not** need "GitHub Actions" on the bypass list).

## Type of change

- [ ] Bug fix
- [ ] Feature
- [ ] Documentation / wiki
- [ ] Refactor / tooling

## Testing

- [ ] I ran `make all` successfully
- [ ] I ran `make run` (or `make run-iso`) and verified behavior

## Checklist

- [ ] My change is focused (no unrelated edits)
- [ ] I followed existing code style in the touched files
- [ ] I read [CONTRIBUTING.md](../CONTRIBUTING.md)

## Notes for reviewers

<!-- Areas you want extra review on (e.g. memory.c, interrupt handlers) -->
