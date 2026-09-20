# Veyra Settings & Accessibility Bible

**Version:** 0.1 — Approved player-facing decisions, Proposals 1–45 (2026-09-21)  
**Status:** Working design canon for the approved choices below; this bible is not a complete implementation specification. Discussion paused after Proposal 45.  
**Scope:** Personal controls, casting, camera, interface, accessibility, audio, graphics, language, settings persistence and in-match access.  
**Related:** [Combat Bible](Veyra_Combat_Bible_v0.5.md) owns actual attack, ability, targeting and status rules; [Vision Bible](Veyra_Vision_Bible_v0.1.md) owns permitted information; [Chat & Communication Bible](Veyra_Chat_Communication_Bible_v0.1.md) owns recipients, All Chat, mute and communication permissions; [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) owns application handoffs; [Match Flow Bible](Veyra_Match_Flow_Bible_v0.1.md) owns match phases and outcomes. [Architecture](../../ARCHITECTURE.md) still governs implementation.

> **Design intent:** Player preferences may change how permitted information and inputs are presented, not the authoritative rules or what a player is allowed to know. Every setting that affects a visible gameplay cue must preserve its essential meaning. These rules are approved product intent, not permission to invent gameplay timing, input automation, networking, storage, or UI architecture.

## 1. Controls and ability interaction

### 1.1 Fully customizable bindings (Proposal 1 — approved)
- Players may rebind gameplay actions, including movement, abilities, basic attacks, item actives, Flux Spells, camera and communication. Save a default binding profile and optional Vanguard-specific profiles which override it where configured.
- Offer individual and whole-profile resets, and clearly identify conflicting bindings. Profile behavior must remain compatible with safe mouse rebinding, in-match changes and cross-device persistence below.

### 1.2 Casting modes (Proposal 2 — approved)
- Independently configure **Normal Cast** (press to display indicator, click to commit), **Quick Cast** (press to cast toward current cursor), and **Quick Cast with Indicator** (hold to preview, release to cast) for Q/W/E/R, each Flux Spell and eligible targeted item actives.
- Vanguard-specific control profiles can override these preferences. Unique charge, stance and multi-stage mechanics retain their intended ability behavior; a setting changes compatible initiation input, not the underlying gameplay.

### 1.3 Attack move (Proposal 3 — approved)
- **Attack Move** uses a configured key plus location click: move toward that location and attack eligible enemies encountered. **Attack Move Click** issues the command toward the cursor with one keypress.
- Independently choose target preference **Closest to Vanguard** or **Closest to Cursor** within the ordinary eligible acquisition area.
- Both obey normal vision, stealth, range and target-validity rules. They cannot acquire unseen or hidden enemies.

### 1.4 Target Vanguards Only (Proposal 4 — approved)
- Offer configurable **Hold** and **Toggle** activation for a modifier that restricts direct attack commands and eligible targeted abilities to enemy Vanguards, ignoring minions/Fluxborn, jungle creatures and structures as direct targets.
- Untargeted projectile collision, area damage and ordinary skillshot behavior are unchanged. It neither reveals hidden enemies nor changes attack-move into automatic Vanguard acquisition.

### 1.5 Self-cast (Proposal 6 — approved)
- A rebindable **Self-Cast Modifier** casts eligible self-targetable abilities on the player's Vanguard. Optional **Smart Self-Cast** is configurable per eligible ability: if no valid target is under the cursor, self-target rather than fail.
- Both honor each ability's actual target rules; they do not redirect directional, ground-targeted, or self-ineligible abilities. Vanguard-specific profiles may hold the preferences.

### 1.6 Continuous input (Proposal 25 — approved)
- Eligible actions that already require a sustained key press may use **Hold** (default) or optional per-ability **Toggle**. Toggle starts on press and ends on a second press, ordinary maximum duration, or interruption.
- No extension of duration, repeated casts, or bypass of ability restrictions results from this option. It does not apply to automatic basic-attack repetition.

### 1.7 Show Cast Range (Proposal 45 — approved)
- Bind a **Show Cast Range** modifier to preview the ordinary indicator for an eligible ability, Flux Spell or targeted item active **without casting**. While holding the modifier, pressing that action's key displays its indicator; release the modifier or press Escape to dismiss. A subsequent normal cast uses the player's selected casting mode.
- Respect unique ability mechanisms and ordinary information restrictions; preview cannot reveal a hidden target, guarantee a hit or change range, shape, timing or validity.

### 1.8 Mouse input and accessibility (Proposals 32–33 — approved)
- Optional raw/unaccelerated mouse input where supported; independently adjustable middle-mouse camera-drag sensitivity; no extra gameplay-cursor-speed slider (pointer speed follows the OS).
- Rebind primary and secondary gameplay mouse commands, with a one-click **Swap Mouse Buttons** convenience option. Update displayed prompts and maintain a working path through Settings even when bindings change or are cleared.
- Store hardware-dependent mouse preferences locally and sync general camera preferences according to §7.

### 1.9 Explicitly rejected input proposals
- **Proposal 7 — rejected:** Do **not** add an optional ability-input-buffering/one-pending-ability setting. General input processing remains for a later Controls & Gameplay Interaction design; this rejection does not silently specify every low-level input rule.
- **Proposal 13 — rejected:** Do **not** add hold-to-repeat basic attacks. Ordinary attack and approved attack-move inputs remain.

## 2. Camera (Proposals 5 and 16)

- **Proposal 5 — approved:** Free Camera (default; screen-edge scrolling or rebindable movement keys), Locked Camera (toggle follows and centers on Vanguard), Semi-Locked Camera (follows with limited player-directed offset), and rebindable **Hold to Center**. Include adjustable camera movement speed, edge-scroll sensitivity and disable-edge-scroll option, camera smoothing, and optional middle-mouse dragging with its own sensitivity (§1.8).
- **Proposal 16 — rejected and clarified:** No auto-pan toward objectives/events or cinematic ability-zoom behavior during live gameplay. The game uses the player's selected camera behavior. **The only intended game-driven pan discussed is the match-ending pan to the destroyed Prime Well.** Do not create pointless switches for non-existent auto-pans or zooms.
- Camera controls do not grant sight outside normal vision. Replay and spectator camera behavior belongs to their own design, not to an assumed live-game cinematic system.

## 3. HUD, minimap and battlefield readability

### 3.1 Scaling and layout (Proposals 10, 11, 41 — approved/rejected as noted)
- Independently scale **each individual HUD component** (rather than only a single grouped HUD scale), including ability bar, health/resource display, items, Flux Spell slots, minimap, team panels and other supported components. Separately scale chat text/panel dimensions, combat text, and interface/menu/tooltip text. Provide live preview, per-element reset and tested size limits.
- **Proposal 11 — rejected:** No draggable HUD layout editor or free-form repositioning. Components retain their designed anchors.
- **Proposal 41 — modified and approved:** Offer **separate horizontal and vertical UI Safe Area margin sliders**. Each moves applicable edge-anchored elements inward along its axis, preserves the designed relative layout, provides preview and individual reset, and respects collision/visibility limits. Safe-area adjustments do not constitute free-form HUD editing.

### 3.2 Minimap (Proposal 14 — approved)
- Independently adjust minimap icon size versus minimap scale; configure whether minimap click/drag moves the camera; configure whether right-click on minimap issues movement or is ignored; adjust minimap-ping persistence within tested bounds.
- Do not change fog-of-war, stealth or objective-information permissions. Minimap remains in its designed fixed location subject to scale and safe-area margins.

### 3.3 Cursor and targeting indicators (Proposals 15 and 36 — approved)
- Independently scale gameplay cursor; offer high-contrast cursor appearances, optional extra outline, and optional brief movement/attack click markers. Standard cursor is default.
- Offer **Standard/Thick** targeting-indicator boundaries, optional high-contrast outlines, adjustable interior opacity while boundaries remain readable, and preview of range, line and area guides. Indicator styling must not exaggerate gameplay hitboxes or display information not otherwise visible.

### 3.4 Health bars, status, cooldown and combat text (Proposals 38, 39, 43, 44 — approved)
- Independently configure allied and enemy **Fluxborn** health bars as **Always / When Damaged / When Targeted**. Jungle-creature health bars are **Always / When Engaged**. Vanguard and structure health bars remain visible **when the player has vision**; no setting reveals unseen units or changes actual target validity.
- Independently toggle floating **damage dealt**, **damage received**, **healing** and **shielding** amounts; optional critical-hit visual emphasis. **Standard** or **Reduced** combat-text density: Reduced combines quick successive numbers for the same source and target into a running total. Existing health and essential combat feedback remain readable; game calculations are unchanged.
- Offer high-contrast status icons and non-color distinctions between buffs and debuffs; optionally show numeric duration alongside the usual indicator **only where the player is already entitled to that duration**. Include preview examples; no hidden statuses or private enemy timers.
- For the player's own eligible ability, item and Flux Spell cooldown displays, independently toggle numeric countdown and radial sweep. Choose whole seconds or tenths below ten seconds. A clear ready state remains visible regardless. No enemy-cooldown tracking is introduced.

### 3.5 Nonessential HUD motion/visibility (Proposals 37 and 42 — approved)
- **Reduce HUD Motion** is optional and **Off by default**. Replace decorative HUD/menu pulses, bounce, slide and scale animations with brief fades or static states, while retaining health changes, cooldown progress, casts/channels, statuses and important alerts.
- **Auto-Hide Nonessential HUD** is optional and **Off by default**. Inactive secondary panels may fade/collapse and reappear on interaction or relevant updates. Health/resources, abilities, items, Flux Spells, minimap and important notifications **never** auto-hide. Auto-hide does not add a draggable layout.

### 3.6 Performance/network indicators (Proposal 21 — approved)
- Independently toggle FPS and round-trip server ping numeric displays; **both Off by default**. Connection-quality/packet-loss warnings are **On by default**. Indicators use designed HUD anchors with independent scale; they report telemetry, not combat authority.

## 4. Accessibility, subtitles and visuals

### 4.1 Color and vision (Proposal 8 — approved)
- Provide color-vision accessibility presets and independently customizable ally/enemy/neutral indicator colors for relevant health bars, outlines, minimap and targeting cues; include preview and default reset.
- Important distinctions must also use shapes, icons, patterns or labels where suitable; color alone is insufficient. This changes UI/gameplay indicators, not Vanguard skins or map-world color schemes.

### 4.2 Visual effects and photosensitivity (Proposals 9 and 18 — approved)
- Screen shake **Full / Reduced / Off**, reduced intense flashes/bloom and **Full / Reduced** nonessential decorative particles. All options retain clearly readable danger zones, projectiles, hitbox boundaries, CC and stealth-reveal cues.
- Separate **Reduce Flashing** option replaces rapid repeated pulses/strobes with steadier alternatives; available from the pre-match main menu. Preserve the same permitted gameplay meaning. The default visual design should itself avoid unnecessary rapid flashing; do not promise complete medical safety for all photosensitive players.

### 4.3 Subtitles and accessible audio cues (Proposal 12 — modified and approved)
- **Subtitles and visual gameplay-audio indicators default Off**, independently opt-in. Subtitles cover dialogue, announcer and other voiced information with size/background-opacity controls. Visual cues for important audible gameplay events may represent **only events the player would already be entitled to perceive**, never hidden sources or private information.
- Spoken-dialogue, announcer, music, ambience and gameplay-effect volumes are independent; preview and reset as appropriate. Off-by-default applies to optional accessibility displays, **not** to regular game-audio volume.

### 4.4 Screen-reader and keyboard accessibility (Proposals 17 and 34 — approved)
- Out-of-match menu controls have accessible names, descriptions and states; keyboard-only navigation across pre-game/client menus, collection, shop, matchmaking and Settings. Important client messages (e.g. match confirmation or purchase error) are available to screen readers. **No live-combat narration or automated gameplay assistance** is approved.
- Consistent menu conventions: **Tab / Shift+Tab** focus order; **Enter / Space** activation; **arrows** for appropriate lists/tabs/dropdowns/sliders; **Escape** dismiss/back. Show focus outline. Popup focus enters the popup and returns to its opener on dismissal. Menu conventions do not lock the player's in-match bindings.

### 4.5 First-launch entry (Proposal 28 — approved)
- Offer a **skippable, one-time accessibility setup** before tutorial/first match: text language, interface text size, screen reader, color-vision palette, Reduce Flashing, subtitles and visual gameplay-audio cues. Reuse existing Settings values, not a second preference system. Skip retains all approved defaults; options remain available later.

## 5. Audio, chat and language

### 5.1 Audio presentation (Proposals 24, 31 and 40 — approved)
- Independently control teammate-ping volume and game-generated announcement volume; preview ping sounds. Visual notifications remain independent of audio sliders.
- Select audio output device or **System Default** in-game, including during a live match. If the device disconnects, fall back to System Default. Save device choice **locally**, not across computers.
- **Audio Dynamic Range:** **Standard** default; optional **Night Mode** reduces loud peaks and makes softer events audible at lower overall volume. Same permitted audible events in both modes.

### 5.2 Chat appearance (Proposal 23 — approved)
- Adjust chat background opacity, time until new messages fade, optional message timestamps and independently toggle incoming-chat-message sound. Chat text scale and panel dimensions follow §3.1. These options do **not** modify Team/All/Party/DM recipient scope, mute, moderation, or the Chat Bible's **default-on All Chat with individual Off option**.

### 5.3 Language (Proposal 22 — approved)
- Select **text** language (menus, tooltips, descriptions, announcements and subtitles) separately from **voice** language (Vanguard and announcer recordings where available). Show which locales have text and voice; if selected localized recording is missing, play the original voice audio instead of silence.
- No specific launch-language list is approved here; localized descriptions/search must track the chosen text language.

## 6. Settings navigation and safeguards

### 6.1 Persistence, reset and Undo (Proposals 19 and 30 — approved)
- Applied settings automatically persist between sessions. Allow individual-setting and category resets (Controls, Camera, Audio, Accessibility, Interface, etc.) and whole-settings reset. Confirm category-wide or whole-settings resets.
- Resetting general controls must not silently erase Vanguard-specific bindings. Offer **one-step Undo for the most recent change**, both out of match and in match; it restores that setting's previous value, not a full edit history. Resolution and display-mode changes use their dedicated confirm/revert behavior instead.

### 6.2 In-match editing (Proposal 27 — approved)
- Most settings are available during live matches, including controls, casting, camera, audio, accessibility and HUD scale. Rebinding captures input rather than sending the same keypress to gameplay. Opening Settings **does not pause** a live match or make the Vanguard invulnerable.
- Changes persist under §6.1/§7; risky display changes use confirmation and automatic reversion.

### 6.3 Find and understand a setting (Proposals 29 and 35 — approved)
- Search Settings by name and related term, display result category and navigate directly to an option; clearing restores normal navigation.
- Every setting has a concise plain-language explanation, practical example where useful, and indication if a restart is required. Descriptions are accessible through mouse hover, keyboard focus and screen readers, translated into the selected text language.

## 7. Cross-device preferences (Proposal 26 — approved)

- **Account-synced:** bindings and Vanguard-specific controls, casting, general camera, accessibility, HUD/minimap, chat and language preferences, subject to any hardware-specific exception.
- **Device-local:** resolution, display mode, graphics/frame-rate and other hardware-dependent choices, plus selected audio output and hardware-dependent mouse input.
- If the device's newer local account-level preferences conflict with the saved account version, ask which to keep rather than silently overwriting either. Actual storage and conflict-resolution architecture remain engineering work.

## 8. Graphics and display (Proposal 20 — approved)

- **Low / Medium / High / Custom** presets; resolution, display mode, frame-rate limit and VSync; individual texture, shadow, environmental-detail and nonessential-effect quality.
- Resolution scaling can reduce scene-render resolution while keeping HUD/interface text clear. Allow preview; confirm disruptive resolution/display-mode changes and automatically revert if unconfirmed.
- Even the lowest graphics quality must retain gameplay-essential indicators and the same permitted information.

## 9. Decisions explicitly not included / cross-bible boundaries

| Proposal | Ruling |
|---|---|
| 7 — optional ability-input buffer | **Rejected.** No special queued one-ability settings feature; general input semantics need their own design. |
| 11 — draggable HUD layout editor | **Rejected.** Fixed layout, independent component scaling and two-axis safe-area margins only. |
| 13 — hold to repeat basic attacks | **Rejected.** No such repeated-attack setting. |
| 16 — automatic camera-pan/zoom comfort toggles | **Rejected.** No such ordinary live-match effects; match-ending pan to destroyed Prime Well is the specified exception. |

Settings never establish combat truth, server authority, hidden information, chat recipient permissions, match outcomes or paid entitlements. Proposed defaults in this document apply only where specifically approved. Unspecified defaults, slider limits, exact buffer timings for other game systems, available launch locales and technical implementation remain open; do not silently invent them.

## 10. Resume point

**Proposal 45 is approved and documented. Pause here.** On the author's next **“continue”**, resume the one-proposal-at-a-time Yes / No / Modify discussion at **Proposal 46**, without re-proposing any accepted/rejected feature or adding unapproved decisions. No Proposal 46 has been accepted yet.
