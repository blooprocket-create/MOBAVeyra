# ADR-059: HUD scaling, cooldown display, own statuses and chat readability

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md): §3.1 (Proposals 10, 41), §3.4 (Proposals 38, 43, 44), §11.1 (Proposal 53), §11.2 (Proposals 62, 66, 67).
- [ADR-024](ADR-024-player-settings.md): the settings registry.
- [ADR-052](ADR-052-combat-text-health-bars-zoom-and-attack-range.md): combat text and bar visibility.
- [ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md): text size and the HUD warnings.
- [ADR-029](ADR-029-in-match-chat.md): chat and its settings.

## Context

The in-match HUD is drawn on the canvas every frame:
- One HUD Scale multiplies every size in the bottom-centre deck: portrait, abilities, health and resource, Flux Spells, items and gold.
- The minimap has its own scale and ignores HUD Scale.
- Nothing keeps the two apart: at 1080p, both at their largest overlap.
- A cooldown shades the whole tile and counts down. There is no sweep, and spells and items show no ready cue.
- The player's own statuses show only as text above their unit.
- Chat Text Size reaches only the in-match chat log, not pre-game chat or the entry fields.

The canon approves individual component scales, safe-area margins, cooldown display options, an own-HUD status display with sorting, high-contrast status marks, and a chat text size for every chat. It rejects a free-form layout editor.

## Decision

### 1. Component scales (Proposal 10)
- **The scales:** HUD Scale stays as the overall scale. Each component has its own scale, multiplying HUD Scale:
  - **Ability Bar:** portrait, passive and abilities.
  - **Health and Resource.**
  - **Items:** items and gold.
  - **Flux Spells:** Flux Spells and the vision tool.
  - **Team Panels:** the kill and clock strip, and the Team Flux panel.
  - **Chat:** panel dimensions.
  - **Combat Text.**
  - **Overhead Bars.**
- **The minimap** keeps its scale and icon size.
- **Range:** each is 75–150 % in steps of 5, default 100 %. These are the tested size limits.
- **Layout:** components keep their designed anchors. A scale changes a component's size, never where it is anchored.

### 2. Safe area (Proposal 41)
- **Margins:** Safe Area Horizontal and Safe Area Vertical are each 0–10 % of the viewport, default 0. They move edge-anchored components inward along their axis: the deck, minimap, chat, strip, Team Flux panel and readouts.
- **Collision:** a pure `VeyraHudLayout` arranges the components inside the safe area.
  - The deck centres, then moves left as far as it must to clear the minimap.
  - If it still doesn't fit between the safe area's left edge and the minimap, the deck shrinks uniformly until it does. No component hides another.

### 3. Cooldowns (Proposal 44)
The player's own abilities, Flux Spells and items get three cooldown options:
- **Cooldown Numbers** (On by default).
- **Cooldown Sweep** (On by default): a radial sweep of the time left.
- **Cooldown Precision:** **Tenths** below ten seconds (the default) or **Whole** seconds.

Every slot shows a clear ready state whatever the options: a ready ability, Flux Spell or item Active gets the accent outline. No enemy cooldown is ever shown.

### 4. Own statuses (Proposals 38, 43, 53)
- **The row:** a status row above the deck shows the player's own Vanguard's statuses.
  - Beneficial and harmful effects form separate groups.
  - Crowd control stands apart within the harmful group.
- **Harmful:** `VeyraStatuses::IsHarmful(Kind, Magnitude)` in Combat decides whether a status harms its bearer: crowd control, the reductions, damage over time, and the stat kinds with negative magnitudes. It is a description of a status, never a change to one.
- **Status Sorting:** **By Category** (the default), **By Remaining Duration** or **By Application Order**. Sorting only rearranges the row.
- **Marks:** every chip carries a mark that needs no colour: **+** for beneficial, **-** for harmful, **!** for crowd control.
- **High-Contrast Statuses** (Off by default): opaque chips with a strong outline.
- **Status Durations** (On by default): the seconds left on each of the player's own statuses. Other units keep their overhead lines as they are; no setting reveals a hidden status.

### 5. Chat text size everywhere (Proposal 66)
Chat Text Size (Standard, Large or Extra Large; ADR-029) already sizes the in-match chat log. It now sizes every chat the player reads or types in:
- the shell's social sidebar, both party and direct conversations;
- champion select's chat panel;
- the results screen's post-match chat;
- the in-match composer and every chat entry field.

Lines wrap and keep the panel's position. The in-match Chat Background (Proposal 67) is unchanged.
### 6. Ownership
- Every option is an account-scoped setting in `Settings.json`, resolved by `VeyraInterfacePreferences`.
- The HUD reads the resolved preferences each frame, as it does today.
- The tuning that bounds the layout (the minimum gap to the minimap, the smallest deck scale) lives in `UVeyraGreyboxSettings` with the HUD's other dimensions.

### 7. Provisional answers where canon is open
1. Component scales are 75–150 % and multiply HUD Scale.
2. Safe-area margins are 0–10 % of the viewport on each axis.
3. When the deck can't clear the minimap within the safe area, it shrinks rather than overlapping.
4. Status Durations is On by default, matching the overhead lines players already see.
5. The status row shows text chips until status icons exist.

## Out of scope
- Cursor options and click markers (Proposal 15), indicator outline and fill (36), ping labels (68), Reduce HUD Motion and Auto-Hide (37, 42), the edge-scroll zone and delay (86, 87), the minimap modifier-click (88) and the background FPS limit (109). These are planned for the next milestone.
- The team status panel's modes (Proposal 55), which canon leaves open.
