# ADR-022: Veyra Setup, and the launcher's install, update and repair

**Status:** Proposed. On 2026-09-30 the author asked for a customised installer. They chose this record's scope as the first step, and set a fully custom setup window as the final goal (§9). This ADR becomes Accepted when the author merges the M17 pull request that adds it.  
**Date:** 2026-09-30  
**Related:**
- [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md): L1 (the launcher installs, patches and repairs), L2 (Tauri with a signed self-updater), L5 (content-addressed chunks and a published manifest), and build-order step 6.
- [ADR-006](ADR-006-unreal-project-scaffold.md) §6: reviewable text, and generated binaries.
- [ADR-010](ADR-010-play-flow.md) §5: the launcher, its configuration and the build manifest `VeyraBuild.json`.
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md): §1 (the launcher's jobs) and §9, items 87–89 (installed assets and launcher-managed updates).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.3 (values are data) and §12.

## Context

The canon gives the launcher four jobs: log in, install, patch and repair, then launch (Client & Platform §1). ADR-005 L5 fixes how patching works: content-addressed chunks plus a published manifest. The launcher downloads only the chunks it is missing, verify re-hashes the files, and repair re-downloads chunks that fail. L5 leaves the chunking and compression method open until the patcher is built. Build-order step 6 is "launcher install/patch/repair against a local file server", and it is next.

So far the launcher only logs in and launches. It starts whatever `Game/Scripts/Package.ps1` left in `Game/Saved/Packages/`, and nothing puts the launcher itself on a player's machine.

## Decision

### 1. Two programs, one experience

- **Veyra Setup** (`VeyraSetup-<version>.exe`) installs the launcher, for the current Windows user, then opens it. It never carries or places game files.
- **The launcher** installs, updates, repairs and uninstalls the game (ADR-005 L1). It does this in its own window, in the launcher's design language.

A player runs Setup, and the launcher opens on its Install screen.

### 2. Setup is a hand-written NSIS script

`Launcher/setup/VeyraSetup.nsi` is compiled by NSIS 3's `makensis`, which `Launcher/Package.ps1` runs.
- **Per user, no administrator prompt.** Setup installs to `%LOCALAPPDATA%\Programs\Veyra` by default, and the player can choose another folder. It adds a Start menu shortcut, an optional desktop shortcut, and an uninstall entry under `HKCU`. Running Setup over an existing install upgrades it in place.
- **Branded.** Setup has the launcher's icon and the launcher's palette. Raska, Vanguard #1, is on its welcome and finish pages, from her splash art (`ConceptArt/Vanguards/raska/hero.webp`), and her face is in the header of the pages between (author, 2026-09-30).
  - `veyra-setup-art` draws these bitmaps at package time, so no bitmap is committed (ADR-006 §6). The launcher's icon is drawn the same way.
  - `Launcher/setup/art.json` holds which art is used and how each picture is framed: the point of the art it shows, and where that point sits in the picture.
- **WebView2.** The launcher's window needs the WebView2 runtime, which Windows 11 includes. If it is missing, Setup says so and offers Microsoft's download page.
- **Silent.** `/S` installs with no pages, for the self-updater that ADR-005 L2 still owes.
- **Unsigned** until ADR-005's code-signing certificate arrives, so SmartScreen warns.
- **Uninstall** removes the launcher's own files, shortcuts and uninstall entry, and its web view's data folder. It removes Setup's folder only if nothing else is left in it; the web view's folder, named for the launcher, is the only one it deletes whole. It asks whether to remove the game as well; a silent uninstall keeps it. The launcher removes the game (`veyra-launcher --uninstall-game`, §5), and forgets where it was.

**Why not Tauri's bundler:** it needs the Tauri CLI, while the launcher builds with plain `cargo` (ADR-010 §5). Its template's look is also the part a customised installer replaces. **Why NSIS rather than an MSI:** NSIS installs per user without elevation, allows custom pages, and produces a small file. `makensis` also runs on Linux, and Tauri's updater accepts NSIS installers.

### 3. The release store

A release store is a static file tree that any web server can host:

| Path | What it holds |
|---|---|
| `channels/<channel>.json` | The channel's current release: `schemaVersion`, `buildVersion`, and `manifest`, the SHA-256 of the manifest file's bytes. |
| `manifests/<sha256>.json` | A release manifest, addressed by its own hash. |
| `chunks/<first two hex digits>/<sha256>` | One chunk, as a zstd frame, addressed by the SHA-256 of its uncompressed bytes. |

**The manifest** holds `schemaVersion`, `buildVersion`, `executable` and `files`. Each entry in `files` is a `path`, a `size` and an ordered list of `chunks`. Each chunk has a `hash`, a `size` and a `storedSize`. The build's own `VeyraBuild.json` is one of the files, so an installed game launches exactly as a packaged build does. The format carries no file permissions, since the client is Windows only (ADR-005).

**The format's invariants** are protocol limits, not tuning:
- A chunk is 1 byte to 16 MiB, FastCDC's largest.
- A stored chunk is never larger than zstd's bound for its size.
- A manifest is at most 64 MiB.

**Path rules.** A path is relative and `/`-separated. No component is empty, `.` or `..`. No component has a character or name that Windows reserves, or ends in a dot or a space. No two paths are equal ignoring case, and no file sits where another path needs a folder. `VeyraInstall.json` and the `.veyra-part` suffix are the launcher's own names, so no release may use them. The launcher checks every rule before it writes anything, since a manifest decides where files go.

**Trust.** The channel file names the manifest by its hash, and the manifest names every chunk by its hash, so everything the launcher downloads is verified against the channel file. Signing the channel file waits for the CDN and the code-signing decision (§10). Until then the release server is trusted as the backend is: it is local.

### 4. Chunking and compression (ADR-005 L5's open choice)

- **FastCDC (2020)**, content-defined, rather than fixed-size chunks. An Unreal build's pak and IoStore containers are rewritten whenever content changes. With fixed-size chunks, anything inserted shifts every chunk after it, so the whole rest of the file would download again. Content-defined boundaries resynchronise, so only the chunks around a change are new. Epic's patcher scans with a rolling hash for the same reason.
- **SHA-256** names each chunk. It is standard and every tool has it, PowerShell's `Get-FileHash` included.
- **zstd** compresses each chunk on its own.
- **Configuration.** The minimum, average and maximum chunk sizes and the zstd level live in `Launcher/config/publish.json`. The publisher validates them against FastCDC's bounds, and they are provisional: 256 KiB, 1 MiB, 4 MiB and level 9.
- **`veyra-publish`** (`Launcher/publish/`) turns a packaged client into a release. It chunks every file, writes only the chunks the store lacks, writes the manifest, then moves the channel to it. Each file is written under a temporary name and renamed into place, so a web server never serves half a file.

### 5. How the launcher installs, updates and repairs

**The install folder.** A folder qualifies if it does not exist, is empty, or already holds a Veyra install. If the player picks any other folder, the game goes into `<folder>\Veyra`, which must itself qualify. The default comes from configuration, and the player's choice is kept in the launcher's state file.

**The install record, `VeyraInstall.json`,** marks a Veyra install and keeps two things:
- `installed`: the release that is fully installed, with its manifest's hash.
- `target`: the release an unfinished run was installing.

**One engine, two modes.**
- **Update** installs, or updates to, the channel's release. It trusts the record, so a file whose chunks are unchanged since the installed release is left alone. A changed file is rebuilt from:
  1. its partly written `.veyra-part`, kept from an interrupted run (every chunk verified);
  2. chunks found in the installed files at the offsets the record gives (verified as they are read);
  3. downloads, for everything else.
- **Repair** trusts nothing. It re-hashes every file, keeps the chunks that verify, and rebuilds only what fails. A run that finds an unfinished `target` repairs.

**Run and commit.**
- Chunks download in parallel.
- Each download is decompressed with a limit of the chunk's declared size, then verified. A chunk that fails is tried again.
- New files are written as `<path>.veyra-part` beside the old ones, so the old game stays whole until everything verifies.
- **Commit:** with the `target` already recorded, delete the files the old release had and the new one does not, rename each part over its file, then record the release as `installed`.
- If the game is running, the renames fail, and the launcher says to close Veyra.

**Uninstall** deletes only the files the record lists (installed and target), any leftover `.veyra-part` files, and the record. It then removes folders that are left empty. Anything else in the folder stays.

**Configuration.** The number of parallel downloads, the attempts per chunk and the download timeout are launcher configuration, not literals (ADR-005 Consequences).

### 6. The launcher's configuration, schema 2

`game` names where the game comes from, as exactly one of two sources:
- `buildManifest`: a packaged build, for development. This is today's behaviour, and `Play.ps1` and `Smoke.ps1` keep using it.
- `install`: the game from a release store. It holds:
  - `releasesUrl` and `channel`;
  - `defaultFolder` and `stateFile`, relative to the configuration file;
  - `parallelDownloads`, `downloadAttempts` and `downloadTimeoutSeconds`.

Every other rule stands: every field is required, an unknown field is an error, and the values are validated. Setup installs `Launcher/config/installed.json` beside the launcher as `VeyraLauncher.json`. The launcher reads that file when no configuration is named.

### 7. The launcher's screens

- **Install:** the folder, with Change (a native folder picker), the download size, and Install.
- **Progress:** a bar, the bytes done out of the total, and the speed.
- **Updates:** when the channel names a release other than the installed one, the launcher updates as soon as it opens, before sign-in.
- **Offline:** if the release server does not answer, an installed game can still be played; the backend refuses an outdated build (ADR-005 L4).
- **Repair** sits beside the build number while the player signs in.

The window only shows the status the core reports; the core, in Rust, does the work (ADR-010 §5).

### 8. The local file server

The file server is the local stand-in for the CDN:
- **Serving:** `compose.yaml`'s `releases` service (nginx) serves `Game/Saved/Releases` read-only on `127.0.0.1:8090`.
- **Publishing:** `Game/Scripts/Publish.ps1` publishes the packaged client to the `local` channel.
- **Headless install:** `veyra-install` (`Launcher/cli/`) installs, repairs and uninstalls without a window, for scripts and tests, as `veyra-launch-cli` launches.

### 9. The final goal: a fully custom setup window

The author's goal is that everything the player sees while installing is Veyra's own window, not a classic wizard. This record gets there in stages:
1. **Here:**
   - Setup is a branded NSIS wizard that only places the launcher.
   - The game's install, which is the long and visible part, is already a custom window: the launcher's.
2. **Next:** Setup's own pages become one custom window as well. A later ADR decides how: a custom NSIS page that draws Veyra's art and controls, or a small Tauri program that runs this NSIS installer silently. Whichever it chooses must keep this record's contract:
   - the install is per user;
   - there is an uninstall entry and a `/S` switch;
   - Setup never carries the game;
   - the launcher installs the game.

### 10. Deferred

- **Signing:** signing the channel file and manifests, and code-signing Setup and the launcher. These wait for ADR-005's certificate and the CDN.
- **The launcher's self-updater (L2):** it will run Setup with `/S`.
- **Better downloads:**
  - downloading a chunk once when several files share it;
  - a pause between a chunk's attempts;
  - limiting bandwidth;
  - checking free disk space before starting. For now, a full disk fails the run and says so.
- **Patching without the window:** in the background, or while the game runs.
- **Channels:** more than one for players, such as a test realm.
- **The website:** its download page.

## Consequences

- ADR-005's build-order step 6 is implemented: a release is published to a local file server, and the launcher installs, updates and repairs it.
- A player-shaped path exists end to end: Setup → the launcher's Install screen → sign-in → Play. The development path through a packaged build is unchanged.
- Launcher configuration schema 2 replaces schema 1. Only the version number changes for existing files, and the committed ones are updated.
- CI builds Setup on Windows and publishes it as an artifact of the launcher workflow.

## Amendments to earlier records

- **ADR-005 L5:** the chunking and compression method is chosen: FastCDC, SHA-256 and zstd (§4). Build-order step 6 is implemented by this record.
- **ADR-010 §5:** the launcher's configuration is schema 2 (§6). Beside the launcher's executable, `VeyraLauncher.json` comes before the development fallback.

## Alternatives considered

- **A custom setup program now:** this is the final goal (§9). Doing it first means hand-written Windows code for shortcuts, the registry and uninstall, none of it testable before there is an installer to compare against. Staging it keeps each step verifiable.
- **Setup bundling the game:** rejected. It duplicates the launcher's install job (ADR-005 L1), makes Setup as large as the game, and leaves patching to a second mechanism.
- **Fixed-size chunks:** rejected (§4). **BLAKE3:** faster, but a verify is limited by the disk anyway, and SHA-256 is everywhere.
- **Unreal's BuildPatchServices:** Epic's patcher is tied to its own launcher and tooling, and would make the game patch itself, which ADR-005 L2 rules out.
- **A launcher-owned chunk cache:** it would double the disk space the game needs. Rebuilding from the installed files' own chunks gives the same saving.
