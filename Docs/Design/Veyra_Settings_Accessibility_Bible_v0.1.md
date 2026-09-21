# Veyra Settings & Accessibility Bible

**Version:** 0.1 — Approved player-facing decisions, Proposals 1–120 (2026-09-21)  
**Status:** Working design canon for the approved choices below; this bible is not a complete implementation specification. Discussion paused after Proposal 120; resume at Proposal 121 only when the author says “continue”.  
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
- Select audio output device or **System Default** in-game, including during a live match. When the selected device becomes unavailable, apply the fallback preference approved in Proposal 70 (§11.3), rather than unconditionally redirecting sound. Save device choice **locally**, not across computers.
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

**Proposal 120 is approved as modified and documented: 8 seconds minimum for automatically rotating loading tips and lore facts, with longer entries receiving more time; manual navigation and match entry are unaffected. Discussion is paused here at the author's request.** On the author's next **“continue”**, resume the one-proposal-at-a-time Yes / No / Modify discussion at **Proposal 121**. Do not re-propose an accepted/rejected item or imply that Proposal 121 has been approved.


## 11. Approved decisions and explicit rejections — Proposals 46–120

This checkpoint supersedes any earlier shorthand in §§1–9 where a later proposal adds specificity (particularly audio-device fallback, chat text/background, display change recovery, and camera edge scrolling). Decisions are product rules, not unreviewed implementation details. A proposal marked rejected must not be reintroduced as an implicit prerequisite of another accepted feature.

### 11.1 Controls, shop interaction, camera and HUD

- **46 — Mono Audio, approved:** Optional, **Off by default**; combine left and right content without losing audible events. Compatible with normal volume and Night Mode.
- **47 — Left/Right Audio Balance, approved:** Centered by default and device-local. Mono combines original channels *before* applying output balance; moving the balance never suppresses a source channel. Adjustable during matches.
- **48 — Tooltip Hover Delay, rejected:** No Instant/Short/Long hover-delay preference. Keyboard and screen-reader descriptions remain accessible through their approved behavior.
- **52 — Damage Number Appearance, approved:** **Color-Coded default** distinguishes physical, magic and true damage. **Uniform** is available; optional high-contrast non-color labels/symbols identify damage types. Healing/shielding retain their own presentation. Respects combat-text visibility (Proposal 39), vision, and actual damage calculations.
- **53 — Own-HUD Status-Effect Sorting, approved:** **By Category default**, **By Remaining Duration**, or **By Application Order**; keep beneficial and harmful effects in separate groups, with CC distinct within the harmful group for category sorting. Only rearranges permitted own-HUD information.
- **54 — Purchase Confirmation, rejected:** No optional extra confirmation for ordinary in-match Gold item purchases. This does not set account-purchase confirmation rules.
- **55 — Team Status Panel, modified/approved in principle:** Team-panel customization is accepted, but **Expanded/Compact modes, exact contents, placement within panel, and visibility rules are NOT approved** and must be discussed in future HUD/UI work. Retain its designed anchor and previously approved independent scaling.
- **56 — Scoreboard Open Mode, approved:** Rebindable **Hold default** or **Toggle**; Escape closes. Scoreboard layout and contents remain for UI/UX design.
- **57 — Enhanced Enemy Selection Feedback, rejected:** No additional selectable high-contrast enemy outline or health-bar marker setting; keep standard selection and approved general indicator options.
- **58 — Focus Shop Search, approved:** Rebindable shortcut opens in-match shop and focuses search. Typed characters cannot trigger gameplay. Escape first exits search, then closes shop on a second press.
- **59 — Hold/Toggle Open Shop, rejected:** No separate Hold vs Toggle preference for the ordinary Open Shop action; Focus Shop Search is unchanged.
- **61 — Expanded Minimap, rejected:** No expanded minimap view or its Hold/Toggle controls; keep existing fixed minimap and approved controls.
- **63 — Tooltip Pinning, rejected:** No player-pinnable ability, item or Flux Spell tooltips.
- **64 — Controller Menu Navigation, rejected:** No controller-navigation feature for pre-game client/settings; approved keyboard/mouse access remains.
- **69 — Scroll to Adjust Settings, rejected:** No option for scroll wheel to adjust hovered settings sliders/dropdowns; wheel scrolls the settings page.
- **76 — Confirm Leave Match, approved:** **On by default**, optional Off. Choosing Leave Match opens Stay in Match / Leave Match confirmation with Stay initially keyboard-focused. Does not pause live match or change leave/rejoin rules.
- **81 — Binding Conflict Warning, approved:** On same-context assignment conflict identify both actions; **Replace Existing Binding** clears old assignment and makes new one, **Cancel** preserves both. Safe reuse in separate input contexts is allowed.
- **82 — Reset One Keybinding, approved:** Restore only one action's default; apply Proposal 81 conflict handling; preserve other bindings and Vanguard-specific profiles.
- **83 — Confine Cursor to Game Window, approved:** **On by default** in focused live matches, Off optional. Automatically release on loss of focus, restore when focused. Cursor boundary only, not camera/targeting or background gameplay.
- **84 — Window Focus Indicator, rejected:** No optional “Veyra Not in Focus” in-window label.
- **85 — Edge Scrolling, approved:** **On by default** in Free/Semi-Locked camera, Off optional; retain camera drag and centering; no effect on Locked camera.
- **86 — Edge-Scroll Activation Zone, approved:** **Narrow / Standard default / Wide** cursor-edge threshold, active only with edge scrolling enabled in Free/Semi-Locked; does not change speed.
- **87 — Edge-Scroll Delay, approved:** **Immediate default / Short Delay / Long Delay** for cursor dwelling in zone before camera starts moving; leaving zone cancels pending scroll. Same Free/Semi-Locked and On-only scope.
- **88 — Minimap Camera Click, approved:** **Direct Click default** or rebindable **Modifier + Click** to move camera via minimap. In modifier mode ordinary click does not move camera. Does not change minimap info or movement command rules.
- **89 — Suspend Camera While Typing, rejected:** No optional camera/edge-scroll suspension setting tied to text focus. Proposal 58's prohibition on typing triggering gameplay remains; whether edge scrolling continues during text focus is not fixed by this rejection and must be resolved during UI/input implementation.
- **90 — Hold to Pan Semi-Locked Camera, rejected:** No separate temporary free-drag binding for Semi-Locked.
- **91 — Accessibility Summary Page, rejected:** No read-only summary page; previously approved settings search remains.
- **98 — Screen Reader Shop Section-Jump Shortcuts, rejected:** No dedicated shortcuts for jumping among shop regions.
- **99 — Screen Reader Shop Announcement History, rejected:** No announcement-history panel; approved Repeat Last Announcement remains.
- **101 — Screen Reader Pause/Resume Shop Announcement, rejected:** No dedicated in-game pause/resume shortcut; external accessibility controls unaffected.
- **103 — Keyboard Label Style, rejected:** No player-selectable Physical Key vs Keyboard Layout label style; rebindable input remains.

### 11.2 Text, visuals, UI accessibility and settings navigation

- **62 — Interface Text Size, approved:** **Standard default / Large / Extra Large** for pre-game client, settings, shop and informational tooltips; reflow without clipping. Separate from individual HUD scaling and combat numbers; fixed HUD anchors remain.
- **65 — Reduce Ambient UI Animation, approved:** Optional **Off by default** for pre-game client and in-match menus. Static alternatives for decorative motion; preserve essential feedback and all gameplay animations/effects. Distinct from Reduce HUD Motion.
- **66 — Chat Text Size, approved:** Independent **Standard default / Large / Extra Large** for pre-game and in-match chat including entry fields; wrap, retain all messages, permit scrolling, keep designed chat-panel position.
- **67 — Chat Message Background, approved:** In-match backdrop **Transparent / Standard default / High Contrast**; chat content, position and panel footprint unchanged. This specifies the chat-opacity option in Proposal 23.
- **68 — Ping Text Labels, approved:** Optional **Off by default** short type labels on visible map/world-space pings, supplementing icon/color/sound. Preserve allowed information and ping lifespan; avoid illegible overlap or hiding essential gameplay.
- **74 — Enhanced Keyboard Focus Indicator, approved:** **Standard default / Enhanced** (thicker, high-contrast, distinct from hover/selection/disabled) in pre-game client, settings and in-match shop; no change to navigation or targeting.
- **75 — Reduce Interface Transparency, approved:** Optional **Off by default**; increase opacity of pre-game, settings and in-match shop panels without changing their size/position. In-match chat backdrop remains separate under Proposal 67.
- **77 — Visual Settings Live Preview, approved:** Where useful, settings menu shows immediate sample-content preview of current visual preference; does not pause gameplay, add confirmation, or replace Undo.
- **79 — Named Settings Presets, approved:** Up to **three** player-managed named presets for **account-level preferences only**, with save/rename/overwrite/delete. Loading applies saved preferences with one-step Undo; excludes local resolution, device output, hardware graphics, etc.; obeys ordinary setting restrictions.
- **80 — Preset Modified Label, approved:** When current account-level preferences differ from loaded named preset, show **Modified**; clear when values match/reload/overwrite. Normal current-setting autosave continues and never auto-overwrites preset.
- **92 — Disruptive Display Change Recovery, approved:** For resolution, display mode or monitor choice that could impair visibility, show **Keep Changes / Revert** with **15-second countdown**; Revert/timeout restores last working display configuration. Non-disruptive settings need no prompt; no live-match pause. This is separate from one-step Undo.
- **100 — Photosensitivity Preview Safeguard, approved:** Visual accessibility previews **static by default**; animated demonstrations need explicit Play Preview, immediately stoppable, respecting Reduce Flashing. No animation is required to configure preferences.
- **102 — Settings Search Keywords, approved:** Match common alternative and supported localized terms, show actual setting name and navigate to its existing control; no duplicate settings.
- **96 — Screen Reader Speech Rate, rejected:** No Veyra-specific speech-rate option; rely on system/screen-reader preferences.

### 11.3 Audio, chat and pre-game notifications

- **49 — Background Audio, approved:** **Full Audio default / Mute Nonessential Audio / Mute All Audio** while minimized/unfocused. Nonessential mode mutes music and ambience but preserves ordinary gameplay sounds, pings and announcements; Mute All silences game output. Does not pause live matches or change connection state.
- **50 — Background Match-Ready Notification, modified/approved:** **On by default**, privacy-conscious desktop notification and supported taskbar attention when matchmaking finds a match while pre-game client is in background. Offer **Accept / Decline directly in OS notification where supported**; actions require explicit player click and must arrive before normal acceptance deadline. Clicking notification otherwise focuses client; platform/OS permissions and support are respected. Never auto-accept or disclose party/player/DM information. Visual notification independent of background audio, including Mute All.
- **51 — Do Not Disturb During Matches, rejected:** No separate optional social-notification suppression setting. Existing non-obtrusive DM rules still apply.
- **60 — UI Sound Volume, approved:** Independent **0–100%, default 100%** for ordinary menu and shop UI sounds in and out of match. Gameplay-critical sounds, pings, combat and announcements remain with their own audio categories.
- **70 — Audio Device Fallback, approved:** **Automatically Switch default** to OS current default output when selected device unavailable, or **Wait for Selected Device** (no automatic redirect; resume on return/manual selection). Audio-routing only; compatible with background audio, volume, gameplay.
- **71 — Match-Ready Sound, approved:** Separate **On by default** toggle for matchmaking-found alert in pre-game client. Turning it Off retains visual prompt and Proposal 50 notification; background Mute All silences it regardless.
- **72 — Push-to-Talk Release Tone, rejected:** **Veyra has NO built-in voice chat.** No push-to-talk, microphone-transmission UI or voice-chat settings; companion Chat Bible owns text-only channels. Spoken screen-reader output and localized Vanguard/announcer audio are *not* multiplayer voice chat.
- **73 — Per-Message-Category Chat Sound, rejected:** No All Messages/DM Only/Off classification setting. Proposal 23's general incoming-message sound toggle remains; no recipient or delivery changes.
- **78 — Audio Settings Test Buttons, approved:** Local short category samples (e.g. UI/pings/match ready) through chosen output under volume, balance, Mono and device preferences. No transmitted pings or map markers; no match pause or setting changes.
- **104 — Audio Output Connection Test, approved:** Test Output plays brief sample via selected output; report whether sound was successfully **sent** (not whether user physically heard it). On unavailable device show reason and shortcut to output selector. Respect active routing/volume/balance/Mono and Proposal 70 fallback; do not silently change preferences.
- **105 — Automatic Output Switch Visual Notification, rejected:** No separate setting or extra on-screen notification for automatic audio-device fallback; device status remains inspectable in audio settings.

### 11.4 In-match shop screen readers

- **93 — In-Match Shop Screen Reader, approved:** Extend client screen-reader accessibility into live-match shop: keyboard-navigable categories, search, results and Buy controls; local accessible names, item cost/stats/effects/purchase availability and result/error feedback. Do not automate purchases, narrate combat, reveal hidden info or add voice chat.
- **94 — Announcement Priority, approved:** Purchase results and errors take priority over routine focused-shop descriptions; superseded descriptions are replaced rather than queued without bound.
- **95 — Repeat Last Announcement, approved:** Rebindable shortcut repeats most recent **completed** shop screen-reader announcement; repeating “purchase succeeded” never repeats the purchase. Honor binding conflicts under Proposal 81.
- **97 — Item Reading Order, approved:** **Name → Gold cost → purchase availability (and existing reason if unavailable) → stats → effects**; omit inapplicable sections, start new item at its name. No new item data or purchase mechanics.

### 11.5 Graphics, rendering and diagnostics

- **106 — Detect Recommended Graphics, approved:** Player-initiated hardware-dependent recommendation with proposed changes shown before **Apply / Cancel**. Cancel preserves current settings; Apply never overwrites accessibility preferences or adjusts graphics automatically mid-match. Normal Undo available.
- **107 — Graphics Benchmark, approved:** Player-initiated **outside live matches** in dedicated representative scene; show average FPS, minimum observed FPS and tested configuration. No automatic settings change; results are estimates, not guarantees of in-match FPS.
- **108 — Benchmark Comparison, approved:** Compare two latest runs side by side (average/minimum observed FPS, configurations and numerical differences); keep locally, permit clear, replace older run on next test. No automatic recommendation/application or match guarantee.
- **109 — Background FPS Limit, approved:** **30 FPS default / 15 FPS / Use Foreground Limit** when minimized/unfocused; restore normal cap on focus. Affects rendering only, never server-authoritative live gameplay, match connection, audio preferences or match-ready notifications.
- **110 — Low-Performance Warning, approved:** Optional **On by default**, small nonblocking/nonflashing/inaudible warning for sustained foreground FPS substantially below chosen cap; clears on recovery and links to graphics settings. No automatic adjustment or network diagnosis; do not trigger from deliberate background FPS cap alone.

### 11.6 Loading screen, progress, accessibility and optional content

- **111 — Graphics Preparation Progress, modified/approved:** Show required shader/graphics preparation status **on the match loading screen**, not in a new dedicated pre-game screen: Preparing/Complete/Failed and readable error plus Retry on failure. Player cannot enter live match before their required preparation finishes. Existing loading timeouts and match-start/connection handling prevent holding all other players indefinitely. Keyboard/screen-reader accessibility retained.
- **112 — Loading Screen Accessibility, approved:** Honor applicable Reduce Flashing, Reduce Ambient UI Animation, Interface Text Size and Reduce Interface Transparency; essential progress/status/errors remain legible without color or motion alone. No extra toggle, no changes to load/match timing.
- **113 — Loading Completion/Error Audio Cues, rejected:** No separate additional audio cues for these loading events; visual status/error feedback remains.
- **114 — Loading Stage Labels, approved:** Plain-language stage alongside progress where known (Loading Match/Preparing Graphics/Waiting for Players). For stages with no reliable progress measure show status/activity **without fabricated percentage or time estimate**. Do not reveal other players' device/connectivity details.
- **115 — Loading Error Automatic Focus Rule, rejected:** Do not require proposed automatic focus movement/restoration; existing keyboard/screen-reader accessibility and visible retry actions remain.
- **116 — Loading Tips & Lore, modified/approved:** Optional loading-screen content includes **both gameplay tips and lore fun facts** from established mechanics, controls, world, regions and Vanguard canon. No hidden information, reading gate, interference with essential status/errors or added loading delay. Original simple On/Off selection is superseded by Proposal 118.
- **117 — Manual Tip/Fact Navigation, approved:** Keyboard-accessible Previous/Next when content enabled; any manual navigation pauses automatic rotation for remainder of loading screen. Never delay match start or steal focus from actionable loading errors.
- **118 — Content Categories, approved:** **Both default / Gameplay Tips Only / Lore Facts Only / Off** replaces Proposal 116's On/Off; Previous/Next browses enabled categories only. Essential loading information remains in all modes.
- **119 — Non-Repeating Automatic Rotation, approved:** Show each eligible entry once before automatic repetition when enough are available, vary starting entry across matches, allow manual revisit, respect categories. Do not demand tracking lifetime read history.
- **120 — Minimum Tip/Fact Display Time, modified/approved:** **8 seconds** minimum per automatically displayed entry (not 10); longer entries receive additional time. Manual Previous/Next is immediate and suspends rotation under Proposal 117. Never keep completed loading screen open for a fact; loading status/errors take priority.

### 11.7 Explicitly rejected settings and implementation guardrails

Rejections are enumerated above to distinguish a rejected optional feature from ordinary usability/accessibility requirements that still apply. In particular: **no built-in voice chat**, no automatic or custom live-combat screen-reader narration, no implicit approval of full shop/HUD layouts, no additive setting based on an unapproved gameplay mechanic, and no change to server-authoritative or vision/communication boundaries.

All prior approved decisions from Proposals 1–45 remain in force except where a later approved proposal explicitly refines them. The design is **paused after Proposal 120** by author request; resume at **Proposal 121** only after “continue”.
