# Pull Request Governance and Automation

This document describes a practical automation strategy for openkernel: what to automate, what to avoid, and how collaborator majority approval can work.

## Goals

| Goal | Recommended approach |
|------|----------------------|
| Summarize PR changes | GitHub Action comment (files + diff stats); optional AI layer |
| Route PRs to reviewers | `CODEOWNERS` + auto-request review |
| Decide when to merge | **Majority of listed collaborators** + branch protection |
| Auto-merge to `main` | Only after CI green + majority approval (optional) |
| Auto-implement without human review | **Not recommended** for a kernel |

## What we suggest (phased)

### Phase 1 — Foundation (do this first)

1. **Protect `main`** — use a **ruleset** (recommended) or classic branch protection (see below).

2. **`CODEOWNERS`** — auto-request review from collaborators on every PR.

3. **PR template** — checklist (`make all`, linked issue, test notes).

4. **`COLLABORATORS` file** — canonical list of usernames who count toward majority votes (see `.github/COLLABORATORS`).

5. **CI build** (add when ready) — run `make all` on Ubuntu with i686-elf toolchain so automation never merges broken kernels.

### Phase 2 — Automation (included in repo)

Workflow **PR governance** (`.github/workflows/pr-governance.yml`):

- **On open/update:** posts a structured summary (files changed, +/− lines).
- **On collaborator approval:** counts approvals from `.github/COLLABORATORS`; when **strict majority** is reached, adds label `ready-to-merge` and comments.

Majority rule: **more than half** of listed collaborators must submit an **Approved** review.

Example: 3 collaborators → need **2** approvals. 5 collaborators → need **3**.

### Phase 3 — AI review (optional)

AI is useful for **summaries and review hints**, not as the sole gate for merging kernel code.

| Option | Pros | Cons |
|--------|------|------|
| **GitHub Copilot code review** | Integrated, no custom workflow | Needs org/repo setting; quality varies on asm |
| **Cursor / Bugbot** on PRs | Good summaries | External product setup |
| **OpenAI / Claude GitHub Action** | Custom prompts (e.g. “summarize risk areas”) | API key secret, cost, may miss hardware bugs |

Suggested AI prompt focus:

- List subsystems touched (boot, memory, fs, …)
- Note missing tests / missing `make` verification
- Flag security-sensitive patterns (unsafe pointers, interrupt context)
- **Do not** auto-merge based only on AI approval

### Phase 4 — Auto-merge (optional, careful)

Enable only when:

1. CI passes (`make all`)
2. Label `ready-to-merge` present (majority met)
3. No `do-not-merge` label

Use GitHub **merge queue** or a workflow with `gh pr merge` and a fine-grained PAT. Start with **manual merge** after the bot comments “majority reached”.

## Majority approval vs GitHub “required reviewers”

| Mechanism | Behavior |
|-----------|----------|
| GitHub branch protection “1 approval” | Any one approved reviewer can satisfy (not majority) |
| **Custom majority workflow** | Counts only users in `COLLABORATORS` who approved |
| Combined | Branch protection blocks merge until someone approves; workflow tells you when **collaborator majority** is met |

Recommended: keep **branch protection** (at least 1 approval) **and** use the workflow label `ready-to-merge` as the social signal for “safe to merge now”.

## Auto-implement when approved?

**Full auto-implementation** (bot merges and deploys without human click) is possible but **not recommended** until:

- CI reliably builds the kernel
- You trust the majority + CI gate
- Rollback process exists

For openkernel, prefer: **bot comments + label → human collaborator clicks Merge**.

## Assigning PRs to people

| Method | Setup |
|--------|--------|
| `CODEOWNERS` | `* @quackcom` or per-directory owners |
| Manual assign | GitHub UI |
| Round-robin bot | Third-party app (e.g. Pull Panda) — optional |
| AI “suggested reviewer” | Custom Action reading `CODEOWNERS` + changed paths — overkill for small teams |

For a small collaborator list, **CODEOWNERS** is enough.

## Ruleset vs classic branch protection

**Recommendation: use a [ruleset](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets)** for new setup on `main`.

| | **Ruleset** (recommended) | **Classic** |
|--|---------------------------|---------------|
| Where | Settings → **Rules** → Rulesets | Settings → **Branches** → Add rule |
| Future | GitHub’s current model, more options over time | Still supported, legacy UI |
| Multiple branches | One ruleset, many patterns (`main`, `release/*`) | One rule per pattern |
| Bypass | Fine-grained (roles, teams, specific actors) | Coarser |
| Works with your PR bot | Yes | Yes |

Both work with `CODEOWNERS`, required reviews, and the `ready-to-merge` label workflow.

**Use classic only if** rulesets are unavailable on your plan or you already maintain classic rules and do not want to migrate.

### Recommended ruleset for openkernel

**Settings → Rules → Rulesets → New ruleset → New branch ruleset**

| Field | Value |
|-------|--------|
| Ruleset name | `Protect main` |
| Enforcement | **Active** |
| Target branches | Include default branch **or** branch name pattern `main` |
| Bypass list | Empty (or only repo admins for emergencies) |

**Rules to enable:**

| Rule | Setting |
|------|---------|
| Restrict deletions | On |
| Block force pushes | On |
| Require a pull request before merging | On |
| ↳ Required approvals | **1** (minimum gate; your Action tracks **collaborator majority** separately) |
| ↳ Dismiss stale approvals when new commits are pushed | On (recommended) |
| ↳ Require review from Code Owners | On (uses `.github/CODEOWNERS`) |
| Require status checks to pass | Off until CI workflow exists, then add e.g. `build` |
| Require conversation resolution | On (optional, good for team review) |
| **Restrict updates** | **On** — blocks `git push` to `main` except bypass list (this is the ruleset equivalent of “no direct pushes”) |

> **Note:** Rulesets do **not** have a rule named “Restrict direct pushes.” Use **Restrict updates** (above). **Require a pull request before merging** stops merging without a PR but does **not** by itself stop `git push origin main`; enable both for full protection.

**How this fits majority approval:**

- **Ruleset:** at least **1** GitHub approval + CODEOWNERS review (hard gate).
- **Workflow:** `ready-to-merge` when **>50%** of `.github/COLLABORATORS` approve (social / process gate).
- **Human:** clicks **Merge** when both look good (and CI when added).

Do **not** enable “Require merge queue” unless you need it — overkill for a small repo.

### Classic equivalent (if you skip rulesets)

**Settings → Branches → Add branch protection rule → Branch name pattern `main`**

Enable the same ideas: require PR, 1 approval, dismiss stale reviews, require code owner review, block force push, do not allow bypass (or limit bypass to admins).

**Classic “restrict pushes” location:** under the same branch rule, expand **Require a pull request before merging** → **Restrict who can push to matching branches** (limits who may push at all). Ruleset equivalent: **Restrict updates**.

## Setup checklist

- [ ] Create **ruleset** on `main` (or classic branch protection rule)
- [ ] Edit `.github/COLLABORATORS` with real GitHub usernames
- [ ] Edit `.github/CODEOWNERS` to match
- [ ] Enable branch protection on `main`
- [ ] Add repo secret `OPENAI_API_KEY` only if you add an AI review workflow later
- [ ] Run a test PR from a fork and confirm summary + vote comments

## Related

- [CONTRIBUTING.md](../../CONTRIBUTING.md) — who can open PRs vs who merges
- [Wiki: Contributing](https://github.com/quackcom/openkernel/wiki/Contributing-and-License)
