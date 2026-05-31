# Contributing to openkernel

Thank you for your interest in openkernel.

## How contributions work

openkernel separates **submitting ideas** from **landing changes in the project**.

### Open to everyone

You do **not** need write access to participate:

- **Issues** — bug reports, feature requests, wiki suggestions (use the templates)
- **Pull requests** — you may fork the repo, open a PR, and propose code changes for **review**

### Limited to approved collaborators

These actions require maintainer/collaborator access on the repository or wiki:

- **Merging** pull requests into `main`
- **Pushing** commits directly to the upstream repo
- **Editing** the GitHub Wiki directly

### What “implementation is limited to review” means

1. **Anyone can open a PR** — we welcome patches for discussion and review.
2. **Nothing is merged automatically** — a collaborator reviews every PR for quality, scope, and fit.
3. **Only collaborators merge** — if a PR is accepted, a collaborator merges it (or re-implements the same fix with attribution).
4. **Issues are the main entry point** — if you are unsure whether a change is wanted, open a **Feature request** or **Bug report** first; maintainers may implement accepted work themselves instead of merging an external PR.

```text
Anyone                         Maintainers / collaborators
   │                                      │
   ├─ Issue (bug / feature / wiki) ──────► Review → implement or close
   │
   └─ Pull request (from fork) ─────────► Review → merge OR re-implement
                                              (only collaborators merge)
```

### Summary table

| Action | Anyone | Collaborator |
|--------|:------:|:------------:|
| Open bug / feature / wiki issue | ✓ | ✓ |
| Open pull request (from fork) | ✓ | ✓ |
| Review and comment on PRs | ✓ | ✓ |
| Merge pull request | | ✓ |
| Push directly to `main` | | ✓ |
| Edit wiki directly | | ✓ |

To request collaborator access, contact a maintainer via issue or discussion.

### Approving and merging pull requests

**Anyone** can comment on a PR. **Approving** (for the project’s majority rule) is done on GitHub by users listed in [`.github/COLLABORATORS`](.github/COLLABORATORS):

1. Open the PR → **Files changed** → **Review changes** → **Approve** → **Submit review**.

When enough listed collaborators have approved, the [PR governance workflow](docs/guides/PR_GOVERNANCE.md) adds the `ready-to-merge` label. **Only collaborators** with merge permission should then click **Merge pull request**.

Full steps, majority math, and `gh pr review` usage: **[PR governance guide](docs/guides/PR_GOVERNANCE.md#how-to-approve-a-pull-request)**.

## Our values

- **Open collaboration** — we grow the project together.
- **Respect** — treat contributors and their work with care. Critique code and ideas, not people.
- **Attribution** — honor existing authors, licenses, and prior art. Do not copy code without compatible licensing and credit where required.
- **Quality over quantity** — small, focused changes are easier to review and maintain.

## Before you start

1. Search [existing issues](https://github.com/quackcom/openkernel/issues) to avoid duplicates.
2. For large changes, open a **Feature request** issue first to discuss design.
3. Read the [README](README.md), [wiki](https://github.com/quackcom/openkernel/wiki), and relevant docs under `docs/`.

## Issue templates

| Template | Label | Title format |
|----------|-------|--------------|
| **Bug report** | `bug` | `[BUG #N]` |
| **Feature request** | `feature-request` | `[Feature Request #N]` |
| **Wiki suggestion** | `wiki-suggestion` | `[Wiki Suggestion #N]` |

GitHub Actions assign sequential IDs (one at a time per label if many issues arrive together).

### Bug reports

Include steps to reproduce, expected vs actual behavior, and your environment (OS, toolchain, QEMU version).

### Feature requests

Describe the problem, proposed solution, and why it helps the project.

### Wiki suggestions

Name the wiki page(s), what should change, and why. Collaborators apply accepted suggestions to the wiki.

## Code contributions

### If you are not a collaborator

1. Prefer opening an issue first (especially for large features).
2. Fork the repository and create a branch.
3. Open a **pull request** against `main` with a clear description and test notes.
4. Respond to review feedback — the PR may be **merged**, **closed with guidance**, or **re-implemented** by a collaborator.

You do not need merge rights for your contribution to matter; reviewed PRs help even when maintainers land the final commit.

### If you are an approved collaborator

#### Setup

See [Setup Guide](docs/setup/SETUP_WINDOWS.md) and run:

```bash
make all
make run
```

#### Guidelines

- Match the style of surrounding code (naming, indentation, comment level).
- Keep changes scoped to one topic per pull request.
- Do not commit secrets, personal paths, or unrelated refactors.
- Prefer clear commit messages that explain **why**, not only **what**.

#### Pull request process

Automated helpers (see [PR governance guide](docs/guides/PR_GOVERNANCE.md)):

- A bot posts a **change summary** on each PR.
- **Majority approval** from usernames in [`.github/COLLABORATORS`](.github/COLLABORATORS) adds the `ready-to-merge` label.
- Merging remains **manual** — automation does not auto-implement to `main`.

1. Create a branch from `main` on the upstream repository.
2. Make your changes and verify `make all` succeeds.
3. Open a pull request with a short summary and test notes.
4. Respond to review feedback constructively.
5. A collaborator merges when the change is ready and CI (if applicable) passes.

## Respect for others' work

- **Do not** submit others' code as your own.
- **Do not** remove copyright or license notices from files you did not create.
- If you adapt external code, ensure the license is compatible (this project uses the [MIT License](LICENSE)) and document the source.
- If you build on another contributor's patch, acknowledge it in the PR description.

## Code of conduct

Be professional and inclusive. Harassment, discrimination, and personal attacks are not tolerated. Maintainers may moderate issues and contributions to keep the community healthy.

## Questions

Open a [Discussion](https://github.com/quackcom/openkernel/discussions) or an issue if you are unsure how to proceed.

We appreciate every suggestion that helps make openkernel a better learning resource.
