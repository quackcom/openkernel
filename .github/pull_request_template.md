## Summary

<!-- What does this PR change and why? Link issues: Fixes #123, relates to [BUG #N] -->

Write your summary here.

**Optional merge commit** ([details](docs/guides/PR_GOVERNANCE.md#merge-commit-title-and-description)):

- Add `[USE_PR_TITLE]` to the **PR title** and/or **Summary** → workflow saves the **subject** (title text, or the first Summary line after the flag), then **removes the flag**.
- Add `[USE_PR_DESC]` on a line in **Summary only** → workflow saves **Summary text** as the commit body (not the flag line alone), then **removes the flag**.

After the PR is opened, flags disappear from the title/description; the saved text is in **Merge commit message (saved)**. On merge, **Sync merge commit** writes it to `main` (use secret `OPENKERNEL_REPO_PAT` if `main` is protected).

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
