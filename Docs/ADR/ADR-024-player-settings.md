# ADR-024: Player settings, their two stores, and the Settings screen

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M18 pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md): §6 (autosave, resets, Undo, in-match editing), §7 (account versus device), §8 and §12.3 (graphics, display, camera), §13 (the menu).
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §2 step 5 and §1: no Settings in champion select or Reconnect-only.
- [ADR-006](ADR-006-unreal-project-scaffold.md): runtime input and camera settings belong to VeyraMatch.
- [ADR-010](ADR-010-play-flow.md): the match's display mode (amendment of 2026-09-28).
- [ADR-017](ADR-017-match-statistics.md) and [ADR-020](ADR-020-camera-minimap-kill-economy.md): the scoreboard's Toggle mode and the camera's player settings wait for this screen.
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1 (the server decides outcomes), §1.3 (no hardcoded values).

## Context

A survey of the code (2026-09-30) found:

1. **No Settings screen, entry point, client state or intent.** The shell's top bar has Home, Play, Match History and Quit; the in-match menu has Resume, votes and End Match.
2. **Nothing a player chooses is kept.** Every setting-like value is a developer default in `Default*.ini`:
   - `UVeyraInputSettings`: every key;
   - `UVeyraCameraSettings`: mode, speeds, edge scroll;
   - `UVeyraDisplaySettings`: the match's display mode;
   - `UVeyraGreyboxSettings`: minimap scale, icon sizes, click toggles, ping persistence.

   The camera mode changes in a match (Y) but resets every match. No `UGameUserSettings` subclass, SaveGame or user config write exists, and the backend stores no preferences.
3. **The input mapping is built once per controller**, in `AVeyraPlayerController::SetupInputComponent`, and some keys are read directly each tick. Rebinding needs the mapping rebuilt live.
4. **No audio, localisation or chat systems exist**, so the Audio, Language and Communication categories have nothing to act on yet.

## Decision

### 1. Two stores, as SET §7 splits them

- **Device-local** covers display mode, windowed size, frame-rate caps, VSync, graphics quality and render scale: the "hardware-dependent choices". `UVeyraUserSettings`, a `UGameUserSettings` subclass named in `DefaultEngine.ini`'s `GameUserSettingsClassName`, keeps them in the user's `GameUserSettings.ini`, as the engine keeps its own. It never leaves the machine, and two accounts on one machine share it (SET-121: hardware settings stay local).
- **Account-level** covers bindings, camera, HUD and minimap, and the accessibility, chat and language choices once they exist. It is one JSON document per account: `{ "schemaVersion", "revision", "values": { id: value } }`.
  - **The backend** keeps the document opaque, within a size limit, at `GET` and `PUT /v1/account/settings` (migration 0020, `account.settings`). A `PUT` names the revision it was based on. The backend refuses a stale one with `409 settings_conflict` and returns the current document.
  - **The client** caches the document per account under `Saved/VeyraSettings/<account>.json`, so it starts with the last values before the backend answers, and offline edits wait there.
  - **On a conflict**, when the cache has unsent edits and the backend's revision moved on, the client asks which to keep: "This device" or "Your account" (SET §7). It never overwrites either silently.

### 2. Settings are data: a registry

`Game/Settings/Settings.json` (with `Settings.schema.json`) lists every setting. Each entry has:
- its ID, a content ID, as the key of one of three maps: `toggles` (On or Off; the tuning dialect has no booleans), `ranges` and `choices`;
- its `category` from §13;
- `scope`: Device or Account;
- its bounds: a range's `minimum`, `maximum` and `step`, a choice's `options`;
- `default`;
- `availability`: Anywhere, or OutsideMatches (§6.2);
- `applies`: AtOnce, or AfterRestart (§6.3).

Its label and plain-language description (§6.3) are text rows keyed by its ID. Bindings join as a fourth map with the Controls gate. `scripts/check_tuning.py` validates the file against its schema, beside the other documents that use the tuning dialect.

It is presentation data, not gameplay tuning. The server never reads it, and it is outside the tuning hash, so settings never decide a match (Settings Bible §9: "Settings never establish combat truth").

The developer defaults in `Default*.ini` remain the fallback for every system. A setting's `default` is where they come from, so the defaults exist in one place.

### 3. Ownership

- **A new module, VeyraSettings, in a new Preferences layer** between Foundation (VeyraCore) and Rules, holds:
  - the registry and its validation;
  - `FVeyraSettingsStore`, plain C++ with typed reads, a change event and one-step Undo;
  - the account document's reader and writer;
  - `UVeyraSettingsSubsystem`, the GameInstance subsystem that owns the store and keeps each scope;
  - `UVeyraUserSettings`.

  They sit low so that Match's controller, camera and HUD can read values without depending upward, the way they read their developer settings today. VeyraCore itself stays free of the Engine module, which the subsystem and the user settings need, so the new layer amends ADR-006 §3's layer list.
- **VeyraServices** syncs the account document on sign-in and after changes, debounced, and raises the conflict choice through the client flow.
- **VeyraUI** owns the Settings screen and applies display settings, where `UVeyraMatchDisplaySubsystem` already lives.
- **VeyraMatch** applies the camera, input and HUD values live when their settings change.

### 4. Where Settings opens

- **The shell's top bar**, in the Shell, Lobby and Results states.
- **The in-match menu.** The match does not pause and the Vanguard is not protected (§6.2). The Settings screen takes keyboard input only while a binding is being captured, so a key being bound never reaches gameplay.
- **Never in champion select, Match Found or Reconnect-only** (Client & Platform §2, UX-17). A test covers each state, as PROJECT_STRUCTURE requires.

### 5. How it behaves (§6, §13)

- **Autosave**, with no Apply gate. Each change is kept at once.
- **Resets** for one setting, one category, or everything. The last two ask for confirmation.
- **One-step Undo** of the most recent change, in and out of a match (§6.1).
- **Search** by name and related terms (§6.3). Each setting shows its description and a restart mark where one applies.
- **Keep/Revert.** A change to the window size or display mode made *in Settings* applies at once and reverts after 15 seconds unless kept (SET-92, 166, 167). The automatic switch into and out of a match is not a settings change and has no countdown (SET-166 ruling).
- **Only categories with real effects appear.** In M18 these are Controls, Camera, Interface, and Graphics & Display. Accessibility, Audio, Communication, and Language & Account appear when their systems exist. SET-149 prefers options that change something, and SET-168 forbids presenting non-configurable behaviour as toggles.

### 6. What M18 offers

- **Graphics & Display:**
  - Display Mode: Windowed / Borderless Fullscreen / Fullscreen. It is the match's (SET-166), with Borderless Fullscreen by default.
  - The client's windowed size.
  - Foreground frame-rate cap, background cap (30 by default, SET-109), VSync and render scale (165).
  - Quality presets Low / Medium / High / Custom through the engine's scalability groups (§8).
- **Camera:**
  - Default camera mode, kept between matches.
  - Camera Movement Speed, Edge-Scroll Speed and Camera Drag Sensitivity (150).
  - Edge scroll On/Off (85), zone Narrow / Standard / Wide (86) and delay Immediate / Short / Long (87).
  - Return Camera on Respawn (156, On).
  - Free Camera While Dead (157, On).
- **Interface:**
  - HUD scale, minimap scale and minimap icon scale (§3.1, 160).
  - Minimap click moves camera and right-click moves (§3.2).
  - Ping persistence.
  - FPS and ping readouts (§3.6, Off).
  - Scoreboard Hold / Toggle (56, Hold).
- **Controls:**
  - Rebinding every gameplay action on the general profile (§1.1).
  - A conflict dialog with Replace or Cancel (81), and resets for one binding or all (82).
  - An essential-unbound warning (133).
  - Confine Cursor (83, On).
  - The controller rebuilds its mapping when a binding changes.

### 7. Deferred

- The monitor selector (167): its options are the machine's monitors, not data, and the match already takes the monitor its window is on.
- Vanguard-specific profiles and profile copy (§1.1, §12.2).
- Normal Cast and Quick Cast with Indicator (§1.2), and attack-move target preference.
- Manual zoom (155, 158): its limits are still "to test".
- Presets, import and export (79, 122–125).
- The first-launch setup (§4.5).
- Screen reader and keyboard navigation (§4.4).
- Colour-vision palettes.
- Everything in Audio, Communication and Language, until those systems exist.

### 8. Tests

- **Registry validation:** kinds, bounds, scopes and the defaults' sources.
- **Store round trips:** device values through `GameUserSettings.ini`, and the account document through the cache and the backend, including the conflict.
- **Entry-point visibility** in every client state.
- **Rebind conflicts** and live application to the controller.
- **Keep/Revert** timing on world time.
- **A smoke** (`-Flow Settings`): change the display mode and a binding, restart, and find both kept.

### 9. League answers where canon is open (Settings Bible §9, §14.6: "do not silently invent")

Every value here is Provisional data in `Settings.json`.

1. **Speed sliders** show 0–100 and map to 0.5× to 2× the developer default, with 50 as the default, as League's camera speed slider does.
2. **Edge zones** Narrow / Standard / Wide are 6 / 12 / 24 pixels. **Edge delays** Immediate / Short / Long are 0 / 0.15 / 0.3 s.
3. **HUD scales** run 50–150% in steps of 5, with 100% as the default. Minimap scale and icon scale use the same range.
4. **Frame caps** are 30 / 60 / 120 / 144 / 240 / Uncapped, with the foreground default Uncapped and the background default 30 (SET-109). Render scale runs 50–100%, default 100%.
5. **The account document** is at most 64 KiB. The backend keeps the latest revision only.
6. **The conflict choice** is asked once after sign-in, before the shell shows, like League's settings conflict dialog.

## Consequences

- A player's choices outlive a match and a restart for the first time. Account choices follow the player across machines.
- Adding a setting means one data entry, a text row, and the code that applies it. The screen builds itself from the registry.
- The developer ini files stay the source of defaults, so tests and smokes keep deterministic values unless they change a setting on purpose.

## Amendments to earlier records

- **ADR-010:** the match's display mode becomes a device-local player setting, with the same Borderless Fullscreen default.
- **ADR-017:** the scoreboard's Toggle mode arrives.
- **ADR-020:** the camera's speeds, edge scroll and mode become player settings, with the developer values as defaults. ADR-020 also names the key `CameraLockKey`, but the code calls it `CameraModeKey`; that is corrected here.

## Open items

- Everything in §7.
- A backend test for the settings document's size limit, and a migration test on Postgres.
