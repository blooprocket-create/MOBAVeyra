# Runbook: purge third-party concept sheets from Git history

**Status:** **executed 2026-09-20.** Steps 1–5 complete and verified against the live remote. **Step 6 outstanding** — two GitHub-owned refs still hold the content and only GitHub can remove them.
**Date:** 2026-09-20 (revised twice: after a dry run found the procedure incomplete, and after execution proved where the content survives)

Removes three concept-sheet blobs carrying third-party branding and artwork from the entire history of a **public** repository. See [`Sheet_Canon_Discrepancy_Register_v0.1.md`](../Design/Sheet_Canon_Discrepancy_Register_v0.1.md) §A for what each contains.

## Two findings from the dry run — read these first

**1. The three images each exist at TWO paths in history, not one.** They were uploaded under one name and later renamed, and Git keeps both paths. A path-based filter that lists only the current names removes half of them and then *reports success*, because verifying the same three paths finds nothing either way.

| Image | Paths in history |
|---|---|
| Raska | `ConceptArt/Characters/Raska_The_Redline_Character_Sheet_v2.png` **and** `Raska_The_Redline_Character_Sheet_v2.png` (repo root) |
| Kade | `ConceptArt/Characters/Kade_Dead_Reckoning_Character_Sheet.png` **and** `Kade_Dead_Reckoning_Character_Sheet.png` (repo root) |
| Angeru | `ConceptArt/Characters/Angeru_The_Housebreaker_Character_Sheet.png` **and** `ConceptArt/Characters/98F5AC12-EE7E-4590-9313-5E2887206BE8.png` |

So this procedure strips by **blob ID**, which is a hash of the content and therefore catches every path the content ever occupied, and verifies by blob ID too. Never verify a purge by path alone.

Note also that `git rev-list --all --objects` prints each blob **once**, with only one of its paths. It is not a safe way to enumerate where content lives.

**2. A force-push does NOT remove the PR refs.** GitHub owns `refs/pull/*`, server-side and read-only; no client can delete or rewrite them. **Step 6 is therefore mandatory, not a nicety.**

Execution on 2026-09-20 established exactly which refs survive, and the pattern is more specific than "the PR refs":

| Ref | After the force-push |
|---|---|
| `refs/heads/main` | **clean** |
| `refs/heads/<feature branch>` | **clean** |
| `refs/pull/2/head` | **clean** — an *open* PR's head ref follows its branch when that branch is force-updated |
| `refs/pull/1/head` | **still holds the content** — a *merged* PR's head ref is frozen at its original pre-merge commit and never moves again |
| `refs/pull/2/merge` | **still holds the content** — GitHub's computed merge ref is stale and is not recomputed by a force-push |

So the survivors are the refs of **already-merged PRs**, plus any stale `*/merge` ref. Both are exactly the ones a client cannot touch.

**An ordinary clone is clean.** `git clone` does not fetch `refs/pull/*`, so a normal clone gets 139 commits, 22 sheets, and none of the stripped blobs. The residual exposure is reachable only by deliberately fetching the PR refs or by requesting a known commit SHA on github.com — real, but narrower than "anyone who clones gets them".

For the same reason, do **not** `git push --mirror`: it tries to push `refs/pull/*`, which GitHub rejects. Push the branches explicitly.

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

### 3. Rewrite a fresh mirror clone, stripping by blob ID

`git-filter-repo` refuses to run on a clone with a remote, by design — it wants a fresh one so a half-finished rewrite cannot be pushed by accident.

```bash
git clone --mirror https://github.com/blooprocket-create/veyraMOBA.git veyra-rewrite.git

cat > bad-blobs.txt <<'EOF'
551b67089fce51fd20b528e870e6370c5cb702a2
d4129fbd1604bf7c41208c7a3ce8f0a577d1cb0e
943d0ab144b301e957fdca81b09fe32c3dbe68ef
EOF

cd veyra-rewrite.git
git filter-repo --strip-blobs-with-ids ../bad-blobs.txt
git reflog expire --expire=now --all && git gc --prune=now
```

Those three ids are the Raska, Kade and Angeru sheets respectively. To re-derive them from the backup rather than trusting this file:

```bash
git -C ../veyra-backup.git rev-parse \
  'HEAD~:ConceptArt/Characters/Raska_The_Redline_Character_Sheet_v2.png'
```

### 4. Verify before pushing — by blob ID, then by path

The authoritative check. All three must report `gone`:

```bash
for sha in 551b67089fce51fd20b528e870e6370c5cb702a2 \
           d4129fbd1604bf7c41208c7a3ce8f0a577d1cb0e \
           943d0ab144b301e957fdca81b09fe32c3dbe68ef; do
  git cat-file -e "$sha" 2>/dev/null && echo "STILL PRESENT $sha" || echo "gone $sha"
done
```

Then all six paths, as a secondary check:

```bash
for p in ConceptArt/Characters/Raska_The_Redline_Character_Sheet_v2.png \
         Raska_The_Redline_Character_Sheet_v2.png \
         ConceptArt/Characters/Kade_Dead_Reckoning_Character_Sheet.png \
         Kade_Dead_Reckoning_Character_Sheet.png \
         ConceptArt/Characters/Angeru_The_Housebreaker_Character_Sheet.png \
         ConceptArt/Characters/98F5AC12-EE7E-4590-9313-5E2887206BE8.png; do
  echo "$p: $(git log --all --oneline -- "$p" | wc -l) commits"
done
```

Confirm nothing else was lost:

```bash
git ls-tree -r HEAD --name-only | grep -c 'ConceptArt/Characters/.*\.png'   # expect 22
diff <(git -C ../veyra-backup.git ls-tree -r HEAD --name-only) \
     <(git ls-tree -r HEAD --name-only)                                     # expect no output
```

**Expected, benign differences** from the backup, both confirmed in the dry run:

- one commit fewer (135 → 134). `art: move Raska character sheet into concept art` was a pure rename of a stripped blob, so it became empty and was pruned.
- one commit subject changes, `...Codex review of 288a7f6` → `...of d022b35`. filter-repo rewrites commit SHAs quoted inside commit messages to their new hashes.
- pack size drops about 7.3 MiB, matching the three images.

### 5. Force-push the branches

> **Check the mirror is current first.** A rewrite staged earlier can be missing branches created since. On 2026-09-20 a rewrite staged before a second PR existed would have **deleted that PR's entire branch** had it been pushed. Before pushing, compare `git for-each-ref` in the rewrite against `git ls-remote --heads origin` and confirm every remote branch is present, then confirm the newest commits on each are still there.

```bash
git remote add origin https://github.com/blooprocket-create/veyraMOBA.git
git push --force origin 'refs/heads/*:refs/heads/*'
git push --force --tags origin
```

Do **not** use `--mirror`: it attempts to push `refs/pull/*`, which GitHub owns and rejects.

### 6. Ask GitHub to purge cached views — MANDATORY

The force-push does not garbage-collect GitHub's side, and it cannot touch `refs/pull/1/head`, which still points at the original pre-rewrite commits. **Until GitHub acts, the three images remain publicly reachable** by commit SHA and through that PR ref, regardless of what step 5 achieved locally.

Open a request at <https://support.github.com/contact>, name the repository, state that history was rewritten to remove content that must not remain public, and ask them to garbage-collect unreachable objects **and** drop the stale pull-request refs.

Quote the specifics so they can confirm without a round trip:

```
Repository: blooprocket-create/veyraMOBA
History was rewritten on 2026-09-20 to remove three files. main and all
branches are clean. The following refs still reach the removed content and
cannot be modified from a client:

  refs/pull/1/head
  refs/pull/2/merge

Removed blob ids:
  551b67089fce51fd20b528e870e6370c5cb702a2
  d4129fbd1604bf7c41208c7a3ce8f0a577d1cb0e
  943d0ab144b301e957fdca81b09fe32c3dbe68ef

Please garbage-collect unreachable objects and drop the stale pull-request
refs so these are no longer retrievable.
```

Treat the job as unfinished until they confirm.

### 7. Everyone re-clones

Rewritten history is not a fast-forward. Anyone with a local clone should re-clone rather than pull; pulling will merge the old history straight back in.

```bash
git clone https://github.com/blooprocket-create/veyraMOBA.git
```

## After

- [ ] Regenerate the three sheets with Veyra-owned material only, on the current sheet template (see [`ConceptArt/Characters/README.md`](../../ConceptArt/Characters/README.md)).
- [ ] Set `sheet.status` and `sheet.file` for `raska`, `kade` and `angeru` in [`../Design/Vanguards/`](../Design/Vanguards/), then run the validator.
- [ ] Mark register §A complete.
