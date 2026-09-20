# Runbook: purge third-party concept sheets from Git history

**Status:** ready to run — **blocked until PR #1 is merged**
**Date:** 2026-09-20

Removes three concept-sheet blobs carrying third-party branding and artwork from the entire history of a **public** repository. See [`Sheet_Canon_Discrepancy_Register_v0.1.md`](../Design/Sheet_Canon_Discrepancy_Register_v0.1.md) §A for what each contains.

## Why a deletion commit is not enough

The files were deleted from the working tree, but Git keeps every blob that any commit ever referenced. On GitHub specifically:

- the blobs stay reachable by commit SHA even after the branch no longer references them;
- they remain in every clone and fork already taken;
- GitHub serves them from cached views until its own garbage collection runs, which a force-push alone does not trigger.

So the job is three steps, not one: rewrite, force-push, then ask GitHub to drop the cache.

## Preconditions

- [ ] **PR #1 is merged.** Rewriting while it is open makes the PR unmergeable and the branch unrecoverable.
- [ ] Every other open branch and PR is merged or closed. Anything left behind must be recreated from the rewritten history afterwards.
- [ ] Fork count is still **0** and no collaborator holds an unpushed clone. Each fork keeps its own copy and must be deleted separately — GitHub does not rewrite forks.
- [ ] A full mirror backup exists (step 1). Do not skip this; the rewrite is not reversible.

## Steps

### 1. Back up

```bash
git clone --mirror https://github.com/blooprocket-create/veyraMOBA.git veyra-backup.git
tar czf veyra-backup-$(date +%Y%m%d).tar.gz veyra-backup.git
```

Keep the tarball off this machine until the rewrite is confirmed good.

### 2. Install git-filter-repo

```bash
pip install git-filter-repo   # or: brew install git-filter-repo
```

`git filter-branch` also works but is slow and easy to get subtly wrong. `git-filter-repo` is the tool the Git project itself recommends.

### 3. Rewrite a fresh mirror clone

`git-filter-repo` refuses to run on a clone with a remote, by design — it wants a fresh one so a half-finished rewrite cannot be pushed by accident.

```bash
git clone --mirror https://github.com/blooprocket-create/veyraMOBA.git veyra-rewrite.git
cd veyra-rewrite.git

git filter-repo --invert-paths \
  --path 'ConceptArt/Characters/Raska_The_Redline_Character_Sheet_v2.png' \
  --path 'ConceptArt/Characters/Kade_Dead_Reckoning_Character_Sheet.png' \
  --path 'ConceptArt/Characters/Angeru_The_Housebreaker_Character_Sheet.png'
```

### 4. Verify before pushing

All three must return nothing:

```bash
git log --all --oneline -- 'ConceptArt/Characters/Raska_The_Redline_Character_Sheet_v2.png'
git log --all --oneline -- 'ConceptArt/Characters/Kade_Dead_Reckoning_Character_Sheet.png'
git log --all --oneline -- 'ConceptArt/Characters/Angeru_The_Housebreaker_Character_Sheet.png'
```

Confirm nothing else was lost — the remaining 22 sheets should all still be present:

```bash
git ls-tree -r HEAD --name-only | grep -c 'ConceptArt/Characters/.*\.png'   # expect 22
```

### 5. Force-push

```bash
git remote add origin https://github.com/blooprocket-create/veyraMOBA.git
git push --force --mirror origin
```

`--mirror` rewrites every ref, which is the point: a branch left un-rewritten keeps the blobs alive.

### 6. Ask GitHub to purge cached views

The force-push does not garbage-collect GitHub's side. Open a request at <https://support.github.com/contact> naming the repository and asking them to run garbage collection to remove unreachable objects and cached blob views. Until they do, the old SHAs may still resolve in the web UI and API.

### 7. Everyone re-clones

Rewritten history is not a fast-forward. Anyone with a local clone should re-clone rather than pull; pulling will merge the old history straight back in.

```bash
git clone https://github.com/blooprocket-create/veyraMOBA.git
```

## After

- [ ] Regenerate the three sheets with Veyra-owned material only, on the current sheet template (see [`ConceptArt/Characters/README.md`](../../ConceptArt/Characters/README.md)).
- [ ] Set `sheet.status` and `sheet.file` for `raska`, `kade` and `angeru` in [`../Design/Vanguards/`](../Design/Vanguards/), then run the validator.
- [ ] Mark register §A complete.
