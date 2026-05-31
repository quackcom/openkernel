# Publishing the Wiki

The `wiki/` folder in the main repository contains the same pages used on **GitHub Wiki**. GitHub stores wiki content in a separate git repository.

## One-time setup

```bash
git clone https://github.com/quackcom/openkernel.wiki.git
```

## Publish updated pages

From your openkernel checkout (after editing files under `wiki/`):

```bash
# Copy all wiki pages into the wiki repo
cp wiki/*.md /path/to/openkernel.wiki/

cd /path/to/openkernel.wiki
git add .
git commit -m "Update wiki documentation"
git push
```

On Windows (PowerShell), adjust paths:

```powershell
Copy-Item -Path wiki\*.md -Destination C:\path\to\openkernel.wiki\ -Force
cd C:\path\to\openkernel.wiki
git add .
git commit -m "Update wiki documentation"
git push
```

## Enable the wiki on GitHub

1. Open **Settings → General → Features** for the repository.
2. Ensure **Wikis** is enabled.
3. Browse to the **Wiki** tab — `Home` should appear after the first push.

## Editing online

You can also edit pages in the GitHub web UI. To keep the repo in sync, periodically copy changes back into `wiki/` in the main project.

## Sidebar

The file `_Sidebar.md` controls the left navigation on GitHub Wiki.
