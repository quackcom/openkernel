# Contributing and License

## License

openkernel is released under the **[MIT License](https://github.com/quackcom/openkernel/blob/main/LICENSE)**.

You may use, modify, and distribute the code with attribution and the license notice preserved.

## How contributions work

### Everyone can submit

- **Issues** — bugs, feature requests, wiki suggestions ([[Home#Quick links|templates below]])
- **Pull requests** — fork the repo, propose code, and ask for **review**

You do **not** need write access to the repository or wiki to help.

### Collaborators implement changes

**Merging PRs, pushing to `main`, and editing the wiki directly** are limited to **approved collaborators**.

That does **not** mean PRs are closed to the public — it means:

| Step | Who |
|------|-----|
| Open an issue or PR | **Anyone** |
| Review, discuss, request changes | **Maintainers / collaborators** |
| Merge PR or commit to `main` / wiki | **Collaborators only** |

Maintainers may **merge your PR**, **ask for revisions**, or **land the same fix themselves** after review. Opening a PR is still valuable for review and discussion.

```text
Anyone                    Collaborators
   │                            │
   ├─ Issue ───────────────────► Review → fix / close
   └─ PR (from fork) ──────────► Review → merge or re-implement
```

**Wiki:** non-collaborators should use the **Wiki suggestion** issue template; collaborators apply accepted edits.

Full policy: **[CONTRIBUTING.md](https://github.com/quackcom/openkernel/blob/main/CONTRIBUTING.md)**

### Pull request automation

| Automation | What it does |
|------------|----------------|
| **PR summary bot** | Lists files and line counts on each PR |
| **Majority approval** | When more than half of [`.github/COLLABORATORS`](https://github.com/quackcom/openkernel/blob/main/.github/COLLABORATORS) approve, label `ready-to-merge` is added |
| **Merge** | Still done **manually** by a collaborator after review (no auto-merge to `main`) |

Optional later: CI (`make all`) + AI review hints. See [PR governance guide](https://github.com/quackcom/openkernel/blob/main/docs/guides/PR_GOVERNANCE.md).

## Issue templates

| Template | Label | Auto title |
|----------|-------|------------|
| Bug report | `bug` | `[BUG #N]` |
| Feature request | `feature-request` (exact) | `[Feature Request #N]` |
| Wiki suggestion | `wiki-suggestion` (exact) | `[Wiki Suggestion #N]` |
| Question | `question` (exact) | `[Question #N]` |
| Pull request | _(any PR opened)_ | `[Pull Request #N]` _(title, automatic)_ |

Label names are defined in the repo [`.github/issue-labels.json`](https://github.com/quackcom/openkernel/blob/main/.github/issue-labels.json).

Each type has its own numbering queue — parallel submissions are processed one at a time per category.

### Wiki suggestions (non-collaborators)

1. Open a **Wiki suggestion** issue (do not edit the wiki directly).
2. Name the page(s) (e.g. [[Component-Memory]], [[Getting-Started]]).
3. Describe what to add, fix, or clarify.

Collaborators update the wiki when the suggestion is accepted.

## Documentation

| Resource | Location |
|----------|----------|
| This wiki | GitHub Wiki + `wiki/` in repo |
| Architecture | `docs/architecture/` |
| Windows setup | `docs/setup/SETUP_WINDOWS.md` |
| Quick reference | `docs/guides/QUICK_REFERENCE.md` |

Collaborators: sync wiki changes via [[Publishing-the-Wiki]].

## Code of conduct

Be respectful in issues and reviews. Maintainers may moderate harmful behavior.

## Contact

- **Issues:** [github.com/quackcom/openkernel/issues](https://github.com/quackcom/openkernel/issues)  
- **Wiki:** [github.com/quackcom/openkernel/wiki](https://github.com/quackcom/openkernel/wiki)
