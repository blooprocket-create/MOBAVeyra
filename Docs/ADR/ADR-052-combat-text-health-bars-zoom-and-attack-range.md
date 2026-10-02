# ADR-052: Combat text, health-bar visibility, camera zoom and Show Attack Range

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md):
  - §3.4: health bars and combat text (Proposals 38 and 39).
  - Proposal 52: damage number appearance.
  - Proposals 155 and 158: manual camera zoom.
  - Proposals 162 and 163: Show Attack Range.
- [ADR-024](ADR-024-player-settings.md): the settings registry, stores and categories, and §7's deferrals, which this ADR takes up.
- [ADR-041](ADR-041-casting-modes-and-targeting-aids.md): indicators and the boundary appearance Show Attack Range inherits.
- [ADR-016](ADR-016-vision.md): what each client may receive.

## Context

ADR-024 deferred four approved, Locked settings features, and none exists today:
- **Floating combat text.** The game shows no damage, healing or shield numbers, so a player cannot read their own impact.
- **Health-bar visibility.** Every unit's bar shows at all times, which crowds a lane of ten Fluxborn.
- **Manual camera zoom.** The distance is fixed.
- **Show Attack Range.** There is no way to preview the player's own basic-attack reach.

## Decision

### 1. Floating combat text: the server tells only the player it concerns

Combat's events are server-only. **Match** answers them with a combat text link the game mode owns (`Feedback/FVeyraCombatTextLink`), as it owns its other links:
- **Damage dealt:** each damage instance dealt to an enemy (`OnDamageDealt`) whose source answers to the player's participant: its Vanguard, or a unit it owns. Each damage type is its own number at the target, with crit emphasis when a basic attack crit.
- **Damage received:** each damage instance dealt to the player's Vanguard, by type, whatever it cost: Health, Temporary Health or shields. It is shown at the player's Vanguard.
- **Healing:** Health a unit restored (`OnHealthRestored` with a provider), when the player gave or received it, at the unit healed. Regeneration, the fountain and a Well heal no one in particular and show nothing.
- **Shielding:** a shield the player granted or received, at the unit shielded. Combat gains `OnShieldGranted`, raised by the absorption component with what the grant added.

**Who receives it:**
- Each line goes only to the player it concerns, through an unreliable client RPC on that player's controller. It carries the unit, the unit at the other end, the kind, the damage type, the crit and the amount. Nothing is broadcast, and a bot's participant has no controller to send to.
- A number about a unit the player's side cannot see is never sent, and the other end travels only while the side sees it: the server asks Vision's contract (`VeyraVisibility::IsVisibleToTeam`) first, so combat text never reveals a unit in the fog.

**Showing it (presentation only):**
- The controller raises each line it receives as a client event; the UI keeps them and draws each number rising and fading above its unit, in the HUD canvas, from replicated positions.
- **Density** is Standard or Reduced. Reduced merges quick successive numbers of the same kind and type between the same two units into one running total, as the bible says. The merge window is a presentation setting.
- **Damage number appearance** (Proposal 52): Color-Coded by default, so Physical, Magic and True damage differ; Uniform shows every damage number in one colour. Healing and shielding keep their own colours.

**Settings (Interface):**
- `interface_combat_text_damage_dealt`, `_damage_received`, `_healing` and `_shielding`, each On or Off. Every one defaults On.
- `interface_combat_text_crits`, On by default.
- `interface_combat_text_density`: Standard or Reduced, Standard by default.
- `interface_damage_number_colors`: Color-Coded or Uniform, Color-Coded by default.

The client skips a turned-off kind. The server sends regardless, since the setting is the client's.

### 2. Health-bar visibility for Fluxborn and jungle creatures

Interface settings:
- `interface_fluxborn_bars_allied` and `interface_fluxborn_bars_enemy`: **Always**, **When Damaged** or **When Targeted**.
- `interface_jungle_bars`: **Always** or **When Engaged**.

What each condition means:
- *When Damaged:* below full Health.
- *When Targeted:* the player's attack target, or the unit under the cursor.
- *When Engaged:* below full Health or fighting someone.

Vanguards and structures keep their bars whenever they are seen. The setting only hides bars: it never shows an unseen unit, and it changes no target's validity.

### 3. Manual camera zoom

- **The range:** `UVeyraCameraSettings` gains `MinDistance` and `MaxDistance` around today's `Distance`, the standard zoom. The range is shipped configuration, the same for every player, and no zoom changes what a player may see or target.
- **Input:** the bindings `controls_bind_camera_zoom_in` and `controls_bind_camera_zoom_out` (Mouse Wheel Up and Down by default) move the arm by `ZoomStep`, smoothed by `ZoomSmoothing`. The wheel over a scrollable UI scrolls that UI instead.
- **The level persists:** the account setting `camera_zoom`, from 0 at the nearest to 100 at the farthest, holds the player's level and persists. Its reset returns to the standard zoom.

### 4. Show Attack Range

- The binding `controls_bind_show_attack_range` holds a ring at the player's own current basic-attack reach, statuses included, around the commanded body (ADR-050 §6).
- The ring uses the indicator boundary appearance (Standard or Thick, ADR-041). It is a guide only: no target acquisition or attack follows.

### 5. Ownership

| Piece | Owner |
|---|---|
| Who receives which number | Match: the combat text link, which listens to Combat's events on the server and sends through each player's controller |
| What a shield grant added | Combat: `OnShieldGranted` |
| Keeping, merging and drawing numbers; health-bar visibility | VeyraUI |
| Zoom input and the camera arm | Match: the PlayerController and the camera rig |
| Setting definitions and text | Settings.json and the text table |

### 6. Tests

- **Match:**
  - a hit reaches its dealer and its receiver, and no one else;
  - a hit on a unit in the fog sends nothing;
  - healing and shields reach both ends.
- **UI:**
  - Reduced density merges quick numbers and Standard does not;
  - turned-off kinds are not drawn;
  - each health-bar condition shows and hides as it should;
  - zoom stays within the range and resets to the standard zoom;
  - the attack-range ring's radius follows the player's reach.
- **Settings:** every new entry has its text, and every binding names a key of the input settings.

## 7. Provisional answers where canon is open

1. **The zoom range** is 1200 to 2200 around the standard 1600, in steps of 100 and smoothed over 0.12 s. The bible says the exact limits remain to test.
2. **When Targeted** means the player's attack target or the unit under the cursor.
3. **When Engaged** means below full Health or fighting someone.
4. **Presentation values:** combat text rises 60 units over 1.2 s, and Reduced density merges within 0.4 s.
5. **Show Attack Range has no default key.** The bible names none, and every free key near the hand is spoken for.
6. **Damage dealt by the player's companion or Echo** counts as the player's. It shows at the target like their own.
7. **Only a unit's healing shows.** Regeneration, the fountain and a Well show no healing number.
8. **Temporary Health shows no shielding number.** Only shields do.
9. **One number per damage type.** A hit dealing Physical and Magic damage shows two numbers, each in its type's colour.

## Out of scope

- Per-component HUD scales and safe-area margins (§3.1).
- Status-icon options and cooldown display options (§3.4).
- Accessibility (§4).
