# ADR-074: The skill showcase recorder

**Status:** Proposed. On 2026-10-12 the author asked for "a different way to capture the abilities and their animations". They want a separate map, or a separate instance of one, with only the Vanguard being recorded and a dummy. The Vanguard uses a skill while every other frame is captured, and the frames are combined into a GIF or video. The dummy is reset, the next skill is cast and recorded, and so on until every skill has a recording. The clips might later show a Vanguard's abilities in the client while a player browses the shop. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-12
**Related:**
- [ADR-072](ADR-072-skill-animations-and-staged-skill-effects.md): §8, verification in motion; the skills whose motion and effects these clips show.
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): §5, the frame budget, whose `Smoke.ps1 -PerfSeconds` method this record leaves unchanged.
- [ADR-016](ADR-016-vision.md): fog of war, which hides an enemy outside sight from a client.
- `PROJECT_STRUCTURE.md`, "VeyraDeveloper": developer tools that never ship.

## Context

Checking skills in motion has been done in two ways so far, and both fall short:
- **Frozen captures:** `CapturePresentation.ps1` spawns each effect in an editor world and pauses it at one moment. It shows effects, but no casts, bodies or timing.
- **Kit smoke shots:** `Smoke.ps1 -KitShots` saves about six screenshots per skill from a networked match. Those are a timed handful rather than motion, and each full-resolution screenshot stalls a frame.
  - The camera is free, so it stays where the Vanguard started. In ADR-072's round 2, Cairn walked out of every shot.
  - The target is whichever enemy is nearest, so the result depends on where the opponent client wandered.
  - Nothing is reset between casts.

What exists that a recorder can use:
- **Match setup:** a match is a dedicated server plus networked clients. The editor build's `-server` mode starts one in seconds (`Play.ps1 -Direct`), and fog of war only works through that path.
- **Maps:** the grey-box map is generated from a layout file by the `VeyraGreyboxMap` commandlet.
- **Bots:** a brainless bot Vanguard can be seated on a side (`AVeyraGameMode::AddBotParticipant`), and it stands still unless something orders it.
- **Dev commands:** the developer commands heal, reset cooldowns, grant levels and teleport, but only for the Vanguard of the player who types them.
- **Camera:** the camera rig can lock onto its Vanguard, and edge scrolling can be turned off.
- **Frame capture:** the engine's `FFrameGrabber` reads frames back from the GPU asynchronously, so capturing every other frame does not stall the game.

## Decision

### 1. The stage
A small map, `/Game/Veyra/Developer/Maps/L_Showcase`, is generated from `Source/VeyraDeveloper/Greybox/Showcase.json` by the same commandlet as the grey box. The commandlet takes the layout file and output map as arguments, and the grey box stays its default.

The stage is a plain floor with two team starts facing each other. Between resets the bodies stand at marks inside the starts, on the line between them: close enough that the dummy is always within the caster's sight, and so in the same shot, with the start markers behind them. The starts are the layout file's data and the marks the settings'. The marks' line is turned from the starts' line, 90 degrees as shipped, so the recorded Vanguard stands on the left of the screen and the dummy on the right, the camera looking across the line between them (the author's ruling, 2026-10-12).

### 2. The dummy (server)
`UVeyraShowcaseSubsystem`, in VeyraDeveloper, exists only on a dedicated server whose map URL carries `?VeyraShowcase`. Once preparation begins it seats one brainless bot Vanguard on the side opposite the player: the Vanguard `?VeyraShowcaseDummy=<id>` names, or the settings' default. That bot has no brain, does not wander and does not fight back.

`Veyra.Dev.ResetShowcase` is a server developer command that works only on such a server. It puts the stage back as it began, through the owners' own verbs:
- the caster's Health and resource are refilled and its cooldowns cleared;
- the dummy respawns if it died, through a participant-based form of the game mode's developer respawn, and its Health and resource are refilled;
- a ride either body is still on is ended through Combat, as if its time ran out;
- both bodies return to their marks, facing each other.

The recorder numbers each reset it asks for, and the reply carries that number and whether the stage is set. It is set only when both bodies stand at their marks. A body that cannot be stood there yet, such as one held on to another, leaves the stage unset, and the recorder asks again.

Crowd control and other statuses are not stripped: the recorder waits for them to run out (§3).

### 3. The recorder (client)
`UVeyraShowcaseRecorderSubsystem`, in VeyraDeveloper, runs only on a networked client started with `-VeyraShowcase=<directory>`.
1. **Setup:** it takes developer levels up to the first ultimate rank and ranks every ability, as the kit smoke does.
2. **View:** it hides the HUD and the engine's on-screen messages, locks the camera onto its Vanguard at the game's own zoom, as a player sees the fight, and turns edge scrolling off.
3. **Rehearsal:** with the settings' `bRehearse`, it first casts the whole kit once without capturing. An effect a client first plays may still be compiling, and would be missing from its first clip.
4. **Each ability, in slot order:**
   - it asks for `ResetShowcase` and, once the stage is set, waits the settle time and until the caster carries no status, within the patience. A mode the last clip turned on would otherwise change what its abilities are;
   - it starts capturing and casts at the dummy, or at itself for an ability aimed at an ally;
   - a cast counts as taken when its cooldown starts, its own or a shared one, or when its caster is seen in a new cast of it. An order that goes unanswered while the caster is free is sent again. A retry refused as cooling down shows that the first order was taken;
   - if the server refuses the cast because the target is not prepared, for example an ability that needs a target hurt a moment ago, it drops the clip, hurts the dummy with a basic attack and casts again at once, as a new clip;
   - it keeps capturing until the cast has ended and the tail time has passed, or the clip limit is reached.
5. **The basic attack, after the kit:** from a stage set again, it steps within the Vanguard's basic-attack range (Vanguards.json) times the approach share and orders one attack at the dummy, as the clip `basic_attack`. The clip runs from the order until the first swing has passed its backswing and the tail time has passed, or the clip limit is reached. An attack that never commits is listed with that reason. This shows how a basic attack looks in flight (ADR-072 §9) beside the kit.
6. **Frames:** every Nth frame is captured through `FFrameGrabber` at the settings' resolution. Each is written as a PNG off the game thread, as `<directory>/<ability>/<index>.png`.
7. **Manifest:** `manifest.json` records each clip's frames and their world times, so the encoder plays them back at the speed they happened.
8. **Exit:** once every ability and the basic attack are recorded it quits. An ability it cannot record leaves the rest unaffected. Examples are a refusal it cannot outlast or prime away, or a stage it cannot reset. That ability is listed in the manifest with the reason, and the recording fails. A timeout fails it outright. The encoder encodes what was recorded, and fails unless every listed clip was encoded.

The kit smoke and the recorder share their kit orders: the nearest enemy body, the target an ability aims at, which refusals pass, and the test that a cast committed. These live in `VeyraKitOrders`, not duplicated in each.

### 4. Settings
`UVeyraShowcaseSettings` (`[/Script/VeyraDeveloper.VeyraShowcaseSettings]` in `DefaultGame.ini`) holds harness settings, not gameplay tuning:
- the frame stride, the client's frame rate limit while it records, and the capture resolution;
- the settle time, the tail time and the clip limit;
- how long a refused cast may be retried;
- where the bodies stand, and how far their line is turned;
- whether the HUD shows;
- the default dummy Vanguard.

They are validated at start.

### 5. Encoding
`Game/Scripts/Showcase/encode_showcase.py` turns each clip's frames into a GIF (Pillow) and an MP4 (H.264 through the ffmpeg build `imageio-ffmpeg` bundles), timed from the manifest. Its dependencies are pinned in `Game/Scripts/Showcase/requirements.txt`.

### 6. Running it
`Game/Scripts/RecordShowcase.ps1 -Vanguard <id> [-Dummy <id>]` runs the whole loop in the worktree's editor build:
1. starts the editor server on the stage;
2. starts one rendering client with the recorder;
3. waits for the client to finish and stops the server;
4. encodes the clips.

Frames and clips go to `Game/Saved/Showcase/<vanguard>/`, never into the repository.

### 7. Never shipped
Everything here is in VeyraDeveloper, a developer-tool module absent from Shipping builds. That includes the server subsystem, the developer command, the recorder and the settings. The stage map is not added to `MapsToCook`, so no cook carries it. The one gameplay-module change is the participant-based form of the game mode's existing developer respawn, which Shipping already refuses.

### 8. For the author: clips in the shop
Showing these clips in the client is a separate product decision, left open. The questions it raises:
- **Format:** Unreal plays video through its Media Framework (MP4 or WebM), not GIF. A flipbook texture is the alternative for short loops.
- **Size:** 26 Vanguards with four or five clips each is a hundred-odd clips. Each would be cooked into the client and stored in LFS.
- **Freshness:** clips go stale whenever a skill's motion or effects change, so the recorder would run in the release gate.
- **Where they show:** the shop's item view is not a Vanguard view today. Champion select, or a Vanguard details screen, may fit better.

Until the author decides, clips stay in `Saved/` for review.

## Consequences
- **Every skill in motion, the same way each time:** the same stage, framing, dummy and timing for every Vanguard, so a change shows as a difference between two recordings.
- **Reviews from clips:** a pilot's review can start from the clips, before the author plays.
- **One more map to regenerate:** `L_Showcase` is a generated LFS asset; its layout file is its source.
- **Real-time capture:** the server runs at real time, so a frame that takes longer than the stride drops the capture's frame rate rather than slowing the clip. The manifest's times keep playback at true speed.

## Provisional answers for the author
1. The dummy is another Vanguard, the settings' default unless named, so crowd control, damage numbers and hit reactions show on a real body.
2. Each clip runs from the cast order until the cast ends plus the tail. Lingering effects longer than the tail are cut at the clip limit.
3. GIF and MP4 are both written; the shop decision (§8) picks what ships, if anything.
