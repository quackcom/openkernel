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
- **On open/update and on each review:** posts **Collaborator approval status** (vote count + how to approve).
- **On collaborator approval:** counts **Approved** reviews from `.github/COLLABORATORS`; when **strict majority** is reached, adds label `ready-to-merge`.
Majority rule: more than half of listed collaborators must count as approved.

| Who counts | How |
|------------|-----|
| **PR author** | Counts as approved **automatically** if their GitHub user is in `.github/COLLABORATORS` (GitHub does not offer self-Approve on your own PR) |
| **Other collaborators** | Must submit **Approve** on GitHub |

Examples with `quackcom` + `RX4-WQ` (need **2**):

- @quackcom opens PR → quackcom counted + **@RX4-WQ must Approve** → `ready-to-merge`
- External contributor opens PR → **both** collaborators must **Approve**

The workflow runs on PR open/update and on each review, and applies `ready-to-merge` when the threshold is met.

## How to approve a pull request

Approvals are done **on GitHub**, not in git or the terminal. Only users in [`.github/COLLABORATORS`](../../.github/COLLABORATORS) count toward the bot’s majority tally (branch protection may still require a separate GitHub approval).

### Steps (web UI)

1. Open the pull request.
2. Open the **Files changed** tab (or use the review controls on **Conversation**).
3. Click **Review changes** (top right).
4. Choose **Approve** — **not** “Comment” only (a comment alone does **not** count).
5. Click **Submit review**.

**Shortcut:** on the right sidebar, under **Reviewers**, click your name → **Approve**.

### What happens next

| Step | Who | What |
|------|-----|------|
| 1 | Listed collaborators | Submit **Approve** reviews until majority is met |
| 2 | Bot | Adds label `ready-to-merge` and updates the approval comment |
| 3 | Collaborator with merge rights | Clicks **Merge pull request** on GitHub |

### Single collaborator (solo maintainer)

GitHub **does not allow** the PR author to click **Approve** on their own pull request. That message is from GitHub, not openkernel — and **no ruleset setting turns self-approval on**.

With only one name in `COLLABORATORS` (e.g. you open and review your own PR), pick one of these:

| Approach | What to do |
|----------|------------|
| **A. Bypass for admins** (common for solo repos) | Ruleset on `main`: add your user (or **Repository admins**) to **Bypass list**. You still open PRs for history, but you **merge** without needing an **Approve** review. The bot’s `ready-to-merge` label may never appear until you add workflow logic for solo maintainers (below). |
| **B. Second reviewer** | Add another GitHub user to the repo and to `COLLABORATORS`; they submit **Approve** on your PRs. |
| **C. Lower GitHub’s approval gate** | Ruleset: **Required approvals** = `0` (or disable required review). Use the bot comment + `ready-to-merge` as your process signal only. |
| **D. Solo-maintainer workflow** | Extend `pr-governance.yml` so when `COLLABORATORS` has one user and they authored the PR, the workflow adds `ready-to-merge` on open (GitHub approval still impossible; process gate moves to “reviewed by you” + merge). |

**Recommended for a one-person project:** **A** or **D**, plus optional **C** if the merge button stays blocked.

### Approving from the command line (optional)

```bash
gh pr review 42 --approve
```

Replace `42` with the PR number. Your GitHub user must be listed in `COLLABORATORS`.

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

### Merge commit title and description

Put flags on the **PR before merging** (they are **not** kept in the final commit).

| Flag | Where | Merge commit gets |
|------|--------|-------------------|
| `[USE_PR_TITLE]` | PR **title** or visible **Summary** line | **Subject** (markers and `[Pull Request #N]` removed) |
| `[USE_PR_DESC]` | **Summary** section only (not inside `<!-- comments -->`) | **Body** = Summary text only |

The **PR governance** comment shows **Planned merge commit** while the PR is open. After merge, **Sync merge commit** tries to update `main`.

#### If you cannot add "GitHub Actions" to the ruleset bypass list

`GITHUB_TOKEN` often cannot push to protected `main`. Use one of these:

| Option | What to do |
|--------|------------|
| **A. Admin PAT (recommended for automation)** | 1. GitHub → **Settings → Developer settings → Fine-grained personal access tokens** → token scoped to `openkernel` with **Contents: Read and write**.<br>2. Token owner must be on the ruleset **Bypass list** as **Repository admin** (your user — **not** the "GitHub Actions" app).<br>3. Repo → **Settings → Secrets and variables → Actions** → `OPENKERNEL_REPO_PAT` = that token.<br>4. After merge, **Sync merge commit** runs; or re-run it with the PR number. |
| **B. Manual merge dialog** | On **Squash and merge**, edit the message and paste **Subject** / **Body** from **Planned merge commit** in the PR comment. No PAT, no bypass for Actions. |
| **C. AI hint only** | Set `NVIDIA_NIM_KEY` for an extra suggested title/body in the PR summary (optional; still paste manually when merging). |

You do **not** need to add the **GitHub Actions** app to bypass if you use **A** or **B**.

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
| Bypass list | **Repository admins** (required if you are the only maintainer — see below) |

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

### Solo maintainer ruleset (one person on the repo)

If you are the only merger and reviewer, use this instead of the strict table above:

| Rule | Solo-friendly setting |
|------|------------------------|
| Bypass list | **Repository admins** (your account must be admin on the repo) |
| Required approvals | **0**, *or* keep **1** but rely on **bypass** to merge (you still cannot self-**Approve**) |
| Require review from Code Owners | **Off** (with one person, you are the author *and* the code owner — GitHub blocks self-approval) |
| Require conversation resolution | **Off** (optional; can block merge if bot threads are open) |
| Restrict updates | On (still blocks direct `git push` to `main` unless bypassed) |

You open PRs for history and review; you **merge** using admin bypass, not by approving your own PR.

### “Cannot update this protected ref” when merging

GitHub shows this when merging would **update `main`** but the ruleset (or branch protection) **does not allow it**. Common causes on openkernel:

| Cause | What you see | Fix |
|-------|----------------|-----|
| **Missing approval** | Merge button disabled or error on merge; “review required” | Add a second approver, **or** set required approvals to **0**, **or** add **Repository admins** to the ruleset **Bypass list** and merge as admin |
| **Code owner review required** | Waiting on `@quackcom` (you) | Turn off **Require review from Code Owners** for solo work, or get another user to approve |
| **Unresolved conversations** | “Conversation must be resolved” | On the PR **Conversation** tab, resolve threads (including bot comments if GitHub treats them as blocking) |
| **Failed / missing CI** | Required check stuck | Ruleset: turn off required status checks until a `build` workflow exists, or fix CI |
| **Not allowed to bypass** | You are not admin / not on bypass list | Repo **Settings → Collaborators**: confirm you are **Admin**; ruleset **Bypass list** → add **Repository admins** |


**Quick fix (most solo repos):**  
**Settings → Rules → Rulesets** → edit `Protect main` → **Bypass list** → add **Repository admins** → save.  
Then open the PR → **Merge pull request** (you do not need a self-approval if bypass applies).

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
- [ ] Test approval: submit **Approve** review as a user in `COLLABORATORS`; confirm `ready-to-merge` label

## Related

- [CONTRIBUTING.md](../../CONTRIBUTING.md) — who can open PRs vs who merges
- [Wiki: Contributing](https://github.com/quackcom/openkernel/wiki/Contributing-and-License)
