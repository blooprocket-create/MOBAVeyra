# Veyra Replay, Saved Matches & Live Spectating Bible

**Version:** 0.1 — First-class replay and spectator feature design checkpoint  
**Status:** Locked product/mechanics rules where stated; recording format, compatibility and infrastructure remain **unresolved architecture work**  
**Scope:** Per-match gameplay replay recording, frame-step/scrub viewing, public share links, account saves, retention, moderation evidence, delayed live friend spectating and configurable invite-only custom-match spectating.  
**Related:** [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns state transitions within the one Unreal application; [Moderation & Telemetry](Veyra_Moderation_Telemetry_Bible_v0.1.md) owns evidence access and retention holds; [Profiles & Identity](Veyra_Profiles_Identity_Bible_v0.1.md) owns Match History/profile entry points; [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns block restrictions and friend presence. [Custom Matches](Veyra_Custom_Matches_Bible_v0.1.md) owns invite-only, AI-filled and solo 1v0 match configurations.

> **Recording intent:** Store authoritative gameplay recording data, **not rendered video footage**. Client-submitted commands alone are **insufficient** to guarantee faithful playback: record/derive authenticated player inputs **and** authoritative simulation events/outcomes, timing, relevant state and periodic checkpoints as required by the eventual replay protocol. A design for deterministic reconstruction, patch/version compatibility and seeking must precede implementation. Do not promise that every engine frame can be reproduced merely by logging inputs.

## 1. Locked — replay for every match

- **Every game has a replay recording**: matchmade PvP, Co-op vs AI, future Ranked and custom/private games, including games resolved by remake: **remakes are always recorded** (ruled 2026-09-23). Custom/private replay support does **not** mean those games grant account XP or Vanguard Mastery.
- Custom match replay coverage also includes **single-human 1v0 solo practice, uneven teams, mixed human/AI sides and individually configured bot difficulties**. **Host-ended open-ended practice is a completed custom session**, with an explicit **host-ended/no competitive winner** result and its gameplay recording finalized for the existing completed-replay availability, saving and retention rules. Exact replay protocol, early-exit/crash recovery and completion metadata remain architecture work; never drop the recording merely because the match is nonstandard.
- A completed replay can be opened from the player's Match History or by anyone who has an **available replay link**. A player **does not have to be a participant** to watch or save a linked replay.
- Public replay access confers **viewing** permission, not participant status, account ownership, moderation privileges, reporting rights, party membership or eligibility to spectate an unrelated live match.
- Replay data is produced by trusted match authority, then retained/served by an authoritative storage/index service. The pre-game or Unreal clients cannot submit an arbitrary fabricated authoritative recording.
- Gameplay replay, event indexes and confidential moderation evidence must have separate access policies even when they share match IDs/time markers.

## 2. Locked — Replay Mode in the Unreal client

- Opening an available replay link goes through the pre-game/client platform flow and **enters dedicated Replay Mode in the same installed Unreal application**, with **a replay-specific UI**, rather than joining the dedicated server as an active player.
- Viewer can **pause, resume, rewind, scrub to a match timestamp, change playback speed, step frame by frame, switch between any participant's Vanguard, and use a free camera**.
- Viewer can follow an individual player perspective or observe the entire battlefield. Implement per-perspective information honestly; what a participant could see at a time must be distinguishable from omniscient viewing if used as moderator evidence.
- Replays do **not** alter live gameplay, player controls, match outcomes, permanent rewards, parties or queue state.
- **Public replay/spectator UI never displays match, team or party chat evidence.** Only authorized moderators may access relevant archived match-associated chat through restricted investigative tools; live in-match communication is a separate feature.
- Replay control and seek responsiveness, camera permissions, exact player-perspective reconstruction, frame-step semantics and replay-specific UI layout need engineering/design specification.

## 3. Locked — normal public replay availability

- An ordinary replay is available for **at least three days from match completion**, **or while it remains among a participant's latest 15 games**. The intended policy is that either condition can keep the recording available; it does not expire merely because one condition ends.
- Match History's last-15 calculation is **per participating account**, so the underlying shared recording should remain available while at least one participating player's last-15 retention still applies. Exact global index/eviction behavior for accounts that stop playing is a backend/storage policy to validate against this requirement.
- A public link is usable while the shared recording is ordinary-eligible or **at least one account has saved that recording**. Otherwise public access may expire, even if moderators can still access it.
- Do **not** assume unlimited permanent public retention merely because a moderator can access the match, or treat a saved replay as a separate video file.
- Ordinary retention duration and last-N threshold are **editable, validated configuration**, initially three days and 15 games.

## 4. Locked — saved replays, quota and deduplication

- A user may save **any replay they can access**, including their own match or another player's shared match. Each account initially has **10 saved-replay slots**; the limit is data-driven and may be adjusted using actual storage/usage metrics.
- **Save Replay persists the gameplay recording on Veyra servers**, not a rendered video and not automatically a download to the player's computer. A later optional local-export feature remains undecided.
- Multiple accounts saving the same match must reference **one shared underlying replay recording**. The save consumes **one slot per account**, not one physical replay recording per saver.
- Deleting a saved replay removes **only that account's save reference** and frees one of their slots. It cannot remove another account's save, truncate an active moderator evidence hold or remove an ordinary-eligible replay.
- The underlying recording is retained while **any** account still has it saved, ordinary retention applies, or authorized moderator retention/evidence hold applies. **The public replay link works only while ordinary or saved-replay eligibility remains**; moderator-only retention does not make an expired public link viewable.
- Save/delete and quota allocation must be atomic and authorized; concurrent saves/un-saves cannot create duplicate recordings or delete an object that another saver/hold still references.
- Behavior for a saved replay becoming unplayable after a major compatibility update, quota migration, account deletion and abuse/illegal content removal remains open.

## 5. Locked — moderator access and preservation

- Authorized moderators can open replay evidence for **up to one month (30 days) from match completion**, even after normal public access expires.
- An active report investigation/appeal must be able to **preserve the relevant recording and evidence beyond expiration** while the case is open. Retention/hold removal after closure remains to be designed.
- Public spectators/replay viewers receive **no archived chat**, no private moderation notes and no exclusive moderator evidence access; a shared link cannot disclose it.
- Match events and replay seek timestamps are aligned with case report timestamps and authoritative participation/AFK records.

## 6. Locked — delayed live friend spectating

- A player may **watch a friend's in-progress match**, entered through the **ordinary client state into Spectator Mode in the same Unreal application** using the same underlying recording/playback infrastructure. Spectator Mode is not a playable slot, a substitute for matchmaking, a way to alter a match, or a vehicle for party management.
- **Normal friend spectating of matchmade games** is subject to an initial **three-minute delay** measured against the **authoritative live match timeline**. The viewer must **never** receive events, camera state, player positions, fog-of-war information, event metadata or any seekable replay portion newer than the permitted delayed frontier. The delay is configurable/validated data, not a literal buried in playback code. The only approved no-delay exception is an **invited spectator-only seat in a custom lobby whose host explicitly selects zero delay before launch** (see §7).
- The spectator can use replay-like camera/participant-switching, pause/rewind/scrub and playback controls **only within the already released delayed portion**. Seeking to "live" means seeking no further than the delayed frontier.
- Spectator Mode may show a player's viewpoint or the full map **only within that delay**. The external player can still talk to their friend through unrelated services; the delay exists to reduce real-time information leakage, not to claim prevention of all outside communication.
- **No direct spectator-to-player communication through the spectator client** and no client-side effect on the active match. Live spectating friend access must respect social block restrictions.
- After the match ends, its recording becomes the normal completed replay under the same link/access and retention rules; exact transition/session behavior remains an implementation design decision.
- Three minutes is a design starting value, not a guarantee that a full buffer is available three minutes after a match starts. Before sufficient delayed history exists, presentation should wait or only release eligible events; do not leak a more recent snapshot to "catch up."

## 7. Locked — private custom-match spectator guests and host delay

- The host of an **invite-only custom match** may invite spectator-only guests separately from Vanguard-bearing participants; those guests do **not** occupy either team's player slots, control a Vanguard or influence match results. The ordinary Unreal client state admits authorized guests into that application's **Spectator Mode** for that specific custom session.
- **Before match launch**, the custom host may select **the usual three-minute delayed feed or zero-delay live viewing** for the lobby's invited spectator-only guests. A zero-delay feed is permitted for coaching, practice and community commentary **only inside this authorized custom session**. The host's selected delay is shown to participants before launch and enforced by trusted session/replay-stream services. Other intermediate delays, midmatch delay switching, and spectator-seat limits are not yet approved or specified.
- Standard friend spectating of matchmade games **always retains its three-minute delay**; a friend relationship, public completed-replay link or normal friend-spectate action never upgrades its viewer to a zero-delay custom spectator. Only an authorized invited custom spectator seat receives the custom session's chosen feed.
- Cameras, replay seeking and alternate viewpoints remain bounded by whichever frontier the invited custom spectator is permitted to receive. When zero delay is selected, that frontier may reach the match's live authoritative timeline; when delayed, the normal three-minute embargo applies to all state and metadata. No spectator control can alter live gameplay, and Spectator Mode supplies no spectator-to-player chat.
- **Public replay/spectator UI still does not show archived in-match, team or party chat.** The zero-delay custom option changes playback timing, not moderation evidence permissions, block rules, account reporting rights, recording retention or completed replay-link policy.


## 8. Unified Unreal replay/spectator state checkpoint — Client UX-63, UX-90–92 (2026-09-23)

**Watch Replay** is available from eligible Match History records, showing actual unavailable/processing/retention state. Starting replay or authorized live spectating **transitions within the same installed Unreal application** to a distinct Replay or Spectator mode; it does **not** launch a second Unreal executable. Leaving an opened history replay returns to the originating Match History entry without changing party readiness. Normal replay-link authorization, recording availability/retention, three-minute matchmade spectator delay, eligible custom spectator delay, saved slots, confidentiality, and public no-chat rules are **unchanged**. The app must not allow replay/spectator state to bypass a live assigned match's **Reconnect-only** restriction or turn spectator into an active Vanguard slot.

See [ADR-004](../ADR/ADR-004-unified-unreal-client-states.md). Mode map/input/asset cleanup and return/navigation lifecycle remain implementation details, not a new replay or spectator permission. The pre-game client design is paused after **UX-92**.


## 9. Architecture work required before implementation

1. Choose replay event/input/schema design, authoritative snapshots/checkpoints, deterministic/faithful playback and ability to seek/frame-step, including non-deterministic physics, RNG, AI actions and GAS/network prediction corrections.
2. Define Unreal replay/spectator mode lifecycle, camera and UI separation, recorded visibility/fog semantics, versioned assets/content, and compatibility when live game patches change rules/geometry/Vanguard kits.
3. Define a storage/index/retention system for ordinary three-day/last-15 eligibility, ten-per-account saves, shared references, one-month moderator access and case legal/evidence holds, with safety against premature deletion.
4. Protect public links from exposing moderation records or unreleased live events; determine URL/token semantics, replay-opening authentication/install flow, caching and invalidation.
5. Define replay coverage for exceptionally long, corrupted/abandoned, open-ended custom/private and remade matches; resilience to crash, process restart, disconnect, storage outage and incomplete checkpoint data. Open-ended custom practice intentionally closed by its host finalizes a **noncompetitive completed replay**, not a win/loss; abandoned/crashed session finalization remains open.
6. Decide detailed spectate permissions for public non-friend matches, custom guest admission/revocation/late joining, and players who Appear Offline. **Live friend spectating with its fixed three-minute delay and host-invited custom spectator seats with a pre-match three-minute/zero-delay choice are approved**; a public *completed* replay link does not automatically grant live spectating.
7. Establish telemetry for recording availability/size, seek performance, retention, storage cost and compatibility failures without recording video by default.
8. Test **delay enforcement at the data boundary**, not just hidden UI controls: no later events via free camera, timeline indexes, cached snapshots, API metadata, alternative player viewpoints or reconnect/seek.

**This is an agreed high-value, large-scope feature; implementation must wait for a dedicated replay architecture/design pass.**
