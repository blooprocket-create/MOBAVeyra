# ADR-081: The launcher reads the servers' hours and what's new from the website

**Status:** Proposed. On 2026-10-13 the author asked for a launcher worth keeping open: the servers' hours with their live state, a plain word when Play cannot find a match, the patch notes, and every Vanguard's art. The author chose (2026-10-13) patch notes for the build being installed or played rather than a news feed, the website as their source, and all 25 Vanguards' art. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- [ADR-070](ADR-070-the-website-on-vercel.md): the website and `site.json`, which already holds the servers' daily hours; the build now also publishes the launcher's feed.
- [ADR-057](ADR-057-public-test-hosting-from-the-authors-pc.md): the public address answers 503 while its host is offline; the launcher reads that as the servers being down.
- [ADR-022](ADR-022-installer-and-game-patching.md): §6, the launcher's configuration Setup installs beside it, which gains an optional `website` section; §11, the launcher updating itself, which carries the new configuration with it.
- [ADR-010](ADR-010-play-flow.md): §5, all of the launcher's HTTP is the core's, in Rust; the web view makes no requests.
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §1: the launcher owns login, install, patching, repair and launch, and is not a second pre-game feature suite; the client's Home owns news.

## Context

The public servers run on the author's PC for a daily session (10:00–11:30 PM Eastern Time). Outside it, the launcher could only fail: a sign-in answered "refused (HTTP 503)", an install could not reach the release store, and nothing said when to come back. The website already shows the hours and a live state from `site.json`. The launcher had no patch notes, and carried art for only 10 of the 25 Vanguards.

## Decision

### 1. The website publishes the launcher's feed

- `node Website/site/build.mjs` writes `launcher.json` at the site's root: `{ schemaVersion: 1, serverHours, patchNotes }`.
- **`serverHours`** is `site.json`'s, unchanged: the one source of the hours stays where ADR-070 put it.
- **`patchNotes`** comes from **`Website/site/patch-notes.json`**, a new file of what each game build changed, in words a player reads, newest first: `{ version, date, title, changes[] }`.
  - The build validates it strictly, as it does `site.json`: unique build versions, dates newest first, a title of at most 60 characters, and 1 to 8 changes of at most 160 characters each.
  - Notes are written with each release, and keyed by the game build's version, so the launcher can match them.
- **The website stays up when the host is down**, so the feed answers when the servers do not.
- Feed schema 1 only grows: the launcher ignores fields it does not know, so the website can add to it without breaking older launchers. A change that would break them is schema 2.

### 2. The launcher reads it through its core

- **Configuration:** a new optional `website.feedUrl`, an http(s) URL of a `.json` file (ADR-022 §6, schema 2 unchanged). Absent, the launcher shows neither hours nor notes, as development launchers do. `config/public.json` names the production website's `launcher.json`. A website test checks that the URL names the file the build writes.
- **`launcher_feed`**, in Rust: fetches and checks the feed (schema 1, an IANA-shaped zone, two "HH:MM" times, notes with a version, a title and changes). It answers nothing when the website does not: the window then shows neither, and nothing else waits on it.
- **`server_status`**, in Rust: the backend's readiness check (`/readyz`) through `backend.baseUrl`. "online" for success, "offline" for any other answer, and "unknown" when nothing answered, as when this computer is offline.
- **An offline answer is named plainly** wherever a request meets one: 502, 503 and 504 read "Veyra's servers are offline right now", not "refused".

### 3. One reading of the hours, the website's

- The window words the hours, the live session and the wait until the next exactly as the website does. It uses the website's own `Website/site/src/js/hours.js`, which `Launcher/app/build.rs` copies into the git-ignored `Launcher/ui/generated/` on every build. The two never diverge, and the window's script becomes an ES module to import it.
- Launcher CI watches that file, since the launcher's build reads it.

### 4. What the window shows

- **Beside the art,** at its foot, two small surfaces, each hidden until it has something to say:
  - **What's new in `<version>`:** the patch note for the build being installed or updated, or else the build that would launch. With no note for that build, nothing is shown.
  - **The servers:** online, offline or unknown, with the daily hours (and the player's own time where it differs) and the website's line about the session. The window asks every minute and rewords the countdown every 30 seconds.
- **On the sign-in, Play and problem cards,** while the servers are offline: one sentence saying that signing in, downloading and playing wait until they open, and when that is.
- **Play is not disabled.** The servers may come up between checks, and the launch says why it failed if they have not.
- This is the launcher's patching and launch job (Client & Platform Bible §1), not a news feed: news stays the client's Home.

### 5. Every Vanguard's art

- All 25 heroes are in `Launcher/ui/art/`, kept current from `ConceptArt/Vanguards/<id>/hero.webp` as before.
- `build.rs` lists them into the git-ignored `ui/generated/art.js`, so the art folder alone says which Vanguards can fill the window.
- The art is 16:9 and the window 1.6:1, so one framing (left, centred) serves all 25.
- The launcher grows by about 6 MB, to about 14 MB (author's choice, 2026-10-13).

## Consequences

- Hours change in one place, `site.json`, and every launcher shows them once the website is deployed. No launcher release is needed.
- A release now has one more step: a patch note in `Website/site/patch-notes.json` for the build being published. A build with no note simply shows none.
- The launcher depends on the website at run time for these extras only. Sign-in, install and launch never wait on it.
- The window's script is an ES module, and the launcher's build reads one file from `Website/`.

## Alternatives considered

- **A news feed in the launcher:** declined (author, 2026-10-13). The Client & Platform Bible keeps news in the client's Home and the launcher out of a second feature suite.
- **Hours and notes built into the launcher's configuration:** no run-time dependency, but every new note or change of hours would need a launcher release, and the hours would have two sources.
- **Patch notes in the release store:** the store is behind the host's tunnel, so the notes would vanish exactly when the servers are down.
- **The hours computed in Rust:** would need a time-zone database in the launcher and a second implementation to keep in step with the website's.
- **Shrinking the art at build time:** keeps the launcher near its old size, but needs an image encoder in the build; declined for now (author, 2026-10-13).
