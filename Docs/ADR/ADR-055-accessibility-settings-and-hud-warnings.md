# ADR-055: Accessibility settings and HUD warnings

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md) §3.6 (SET-21), §4.1 (SET-8), §4.2 (SET-18), and the approved Proposals 62, 65, 74, 75, 110 and 112.
- [ADR-024](ADR-024-player-settings.md): the settings registry, which already has an empty **Accessibility** category.
- [ADR-052](ADR-052-combat-text-health-bars-zoom-and-attack-range.md) and [ADR-053](ADR-053-leave-match-match-found-alert-loading-screen-and-break-reminder.md): the interface settings and the loading screen this ADR extends.

## Context

The Settings Bible approves visual-accessibility options and two HUD warnings that are On by default. None exist yet:
- the registry's Accessibility category has no settings;
- side colours are fixed developer values;
- text, focus outlines and panel opacity have one look;
- nothing tells a player that their connection or frame rate is struggling.

## Decision

### 1. Colour vision (SET-8)

- `accessibility_color_vision`: **Standard** (the default), **Protanopia**, **Deuteranopia**, **Tritanopia** or **Custom**.
- Under Custom, `accessibility_ally_color`, `accessibility_enemy_color` and `accessibility_neutral_color` each choose from a named palette, so the registry needs no new kind of setting. The player's own colour follows their ally colour's family.
- **One owner:** VeyraUI resolves the player's side colours (own, ally, enemy, neutral) once. Every side-coloured cue in a match reads them: bodies and bars, telegraphs, outlines, the minimap, the HUD's kill and chat colours, and the scoreboard. The presets and the named palette are validated presentation data.
- Colour stays a second cue: sides already differ by position, icon and label. The preset changes indicators only, never Vanguard art or the map's own colours.
- Settings shows a swatch of each side's colour beside the choice, and Reset returns Standard.

### 2. Interface Text Size (SET-62)

- `accessibility_text_size`: **Standard** (the default), **Large** or **Extra Large**.
- It scales the text of the client, Settings, the shop and tooltips by a factor from data, and layouts reflow.
- It never scales the HUD's own text, which keeps its own scale, nor combat numbers.

### 3. Focus, transparency and motion (SET-74, SET-75, SET-65)

- `accessibility_focus_indicator`: **Standard** or **Enhanced**. Enhanced draws a thicker, high-contrast focus outline, distinct from hover, selection and disabled.
- `accessibility_reduce_transparency`, Off by default: the client, Settings and shop panels draw fully opaque.
- `accessibility_reduce_ui_animation`, Off by default: decorative motion becomes static. Today that is the loading screen's spinning indicator, which becomes a steady "working" label. Gameplay motion and essential feedback stay.

### 4. Reduce Flashing (SET-18)

- `accessibility_reduce_flashing`, Off by default, reachable before a match like every setting.
- Rapid pulses become steady. Today that is the Match Found taskbar attention, which becomes a single steady highlight instead of repeated flashes. The default design itself avoids flashing, and nothing claims medical safety.

### 5. The HUD's warnings (SET-21, SET-110)

- `interface_connection_warning`, On by default: a small steady warning while the client's connection loses packets or lags beyond thresholds over a window.
- `interface_performance_warning`, On by default: a small steady warning while foreground frames stay well below the chosen cap over a window. It never fires from the background cap, and it names the Graphics settings.
- Both are silent, non-blocking and drawn at a designed HUD anchor. They report what the client measures and decide nothing.
- Pure rules decide when each starts and clears, with hysteresis. The thresholds and windows are presentation data.

### 6. Ownership

| Piece | Owner |
|---|---|
| The settings, their text and defaults | Settings.json and the text table |
| Side colours, text scale, focus and opacity, motion and flashing | VeyraUI (`VeyraInterfacePreferences` and the shell style) |
| The warnings' rules | VeyraUI (pure rules beside the HUD) |
| Presets, palette, scales and thresholds | Validated presentation settings |

### 7. Provisional answers where canon is open

1. **Presets:** a Protanopia, a Deuteranopia and a Tritanopia palette (blue/orange, blue/yellow and teal/red families), each keeping own, ally, enemy and neutral apart.
2. **Custom colours** come from a named palette of ten (blue, teal, green, yellow, orange, red, magenta, purple, white and grey), not a free picker. The player's own colour is their ally colour lightened by a set amount.
3. **Text sizes:** Large is 1.15× and Extra Large is 1.3×.
4. **Warnings:** connection when over 5% of packets are lost or the round trip passes 200 ms; performance when frames stay under 70% of the cap, or of 60 frames a second when uncapped. Each starts after 3 s past its threshold and clears after 3 s back within it.
5. **Screen shake and particle options** (SET-9) wait until the game has shake or decorative particles to reduce.

## Out of scope

- The screen reader (SET-17, SET-93–97) and first-launch accessibility setup (SET-28).
- Subtitles and visual audio cues (SET-12), which wait for audio.
- Reduced Background Detail (SET-152).
- Per-component HUD scales (SET-10/41).
