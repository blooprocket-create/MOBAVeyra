# ADR-078: The Smoked Crucible: the client's type and surfaces

**Status:** Proposed. On 2026-10-13 the author asked for "a focused pass on bringing the UI into the modern era with high quality work". From a direction board of Home, champion select and the match HUD, they approved the **Smoked Crucible** direction, **Home and the social shell** as the first slice, and an **upgraded canvas painter** for the match HUD rather than a move to UMG. They first chose an inscriptional pairing (Marcellus and Barlow); seeing it in engine, they asked for type that is "modern and clean" and chose **Sora** for headlines and **Inter** for the interface. This record settles two of the Visual Production Standard's open choices (§13): the fonts and the UI's visual tokens. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- Art Bible v0.1 §4 (the author's working document): smoked translucency, subtle metallic or mineral cues, two tiers of type, calm motion; no filigree, carved borders or generic futuristic glass. §9 left fonts, UI geometry and the HUD open.
- [Visual Production Standard](../Production/VEYRA_VISUAL_PRODUCTION_STANDARD.md) §3–§5, §11–§13 and the [Visual Upgrade Roadmap](../Production/VEYRA_VISUAL_UPGRADE_ROADMAP.md) Stages 0–2.
- [ADR-010](ADR-010-play-flow.md) §4: the shell's UMG widgets are built in C++ and styled by `VeyraShellStyle`.
- [ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md) §2–§3: Interface Text Size, Reduce Interface Transparency and the enhanced focus outline still apply to everything here.
- [ADR-059](ADR-059-hud-scaling-cooldowns-statuses-and-chat-readability.md): the match HUD's scales and layout, which the HUD slice keeps.

## Context

The Stage 0 review of the client (smoke captures of 2026-10-06, a match capture of 2026-10-12) found a functional prototype. Every surface was a flat dark rectangle with a one-pixel outline, every word was set in the engine's default Roboto, and the match HUD was grey boxes with plain bars. The shell's look was already centralised: every screen asks `VeyraShellStyle` for its text, surfaces and buttons, from values in `[/Script/VeyraUI.VeyraShellStyleSettings]`. So a new look could reach every screen through one place, without touching any screen's behaviour.

## Decision

### 1. The direction

The **Smoked Crucible**: premium, dimensional and calm.

- Smoked translucent surfaces that shade towards their foot.
- A brass hairline taken from the Crucible's metalwork.
- One ornament: a cut across two opposite corners (top-left and bottom-right), finished by a brass tick.
- Brass fills only the one primary action of a screen. Flux teal marks live state, focus and selection.
- Ally blue and enemy red differ in lightness as well as hue.

The approved direction board is the reference. The values below are presentation settings, not tuning.

### 2. Type

- **Display** (identity: headlines, Vanguard names, the wordmark): Sora, in Regular, SemiBold and Bold.
- **Interface** (function: labels, body text, buttons, chat): Inter, in Regular, Medium, SemiBold and Bold.
- **Numeric** (timers, counts, currencies): Inter, in SemiBold and Bold. It is its own family in the settings, so a narrower cut can replace it without touching the other roles.
- Both are under the SIL Open Font License 1.1, with each family's licence as `OFL.txt` beside its files in `Game/Content/Veyra/Fonts/`. They were downloaded on 2026-10-13 from their upstream projects: Sora's static weights from `github.com/sora-xor/sora-font` (`fonts/ttf/`), and Inter's from its v4.1 release (`github.com/rsms/inter`, `extras/ttf/` of `Inter-4.1.zip`).
- The game ships **static** weights, one file per typeface. Slate draws only a variable font's default instance, so the variable files Google Fonts publishes would give every weight the same letters.
- The game reads them as **files**, not imported assets. `VeyraShellFonts` builds one composite font per family the first time it is asked, from `DisplayFontFaces`, `InterfaceFontFaces` and `NumericFontFaces`. Each family keeps the engine default font's other scripts and fallback, so a player's name in another alphabet still draws. Packaging stages the folder with `+DirectoriesToAlwaysStageAsUFS=(Path="Veyra/Fonts")`.
- Each role of text names its family and typeface (`TitleFace`, `BodyFace`, `ButtonFace` and the rest). Sizes, tracking and Interface Text Size are as before.
- The display tier is no longer forced into capitals (`bDisplayInCapitals`): headlines read as written. The wordmark, buttons, eyebrows and column headings stay in capitals.
- The shell's validation fails when a face's file is missing or a role names a typeface its family lacks.

### 3. Surfaces and buttons

- `VeyraShellBrushes` makes each chamfered shape once, as a small texture from its parameters, rooted and shared. It is drawn as a nine-sliced box so one texture fits any size. Textures are drawn at two texels per slate unit and supersampled, so the cut stays smooth.
- **A surface** (`MakeSurface`, now a `UVeyraShellSurface`) is a white mask tinted the surface's colour, with the brass outline drawn over it by its own Slate widget. The tint stays the surface's colour, so Reduce Interface Transparency still makes it opaque, and tests still read it.
- **A button state** is one face with its fill and edge baked in.
  - An ordinary button has a brass hairline, which takes the accent under the pointer.
  - The primary action is brass from a lit head to a shaded foot, with a thin shine.
  - A tab is underlined in the accent while shown.
  - A quiet action is text until hovered.
- **The enhanced focus outline** (ADR-055 §3) draws the same face with its edge in the focus colour at the focus width.
- Hand-built rounded boxes elsewhere (the player's own seat, the scoreboard's own row, banners, swatches, shop marks) use the same helpers.
- Values: `PanelChamfer`, `ButtonChamfer`, `EdgeColor`, `EdgeTickColor`, `ButtonEdgeColor`, `SurfaceShade`, `PrimaryHighlightColor`, `PrimaryShadeColor`, `PrimaryShineColor`, `TabUnderlineHeight`, and the colour values updated to the direction.

### 4. What does not change

- No screen's behaviour, label, navigation or permission changes. Tests and smokes find buttons by the same labels.
- The approved champion-select arrangement, the friends sidebar, the party bar and Match Found's priority stay as they are.
- No new player setting, and no client authority.
- Interface Text Size, Reduce Interface Transparency, Reduce Interface Animation and the enhanced focus outline apply to the new look as to the old.

### 5. The first slice: Home, the social shell, champion select and the results

The author asked for the layouts to change with the look, so the slice recomposes these screens. Their arrangements, labels and actions stay as approved.

- **The shell's frame:** the friends rail runs the whole right side, and the party bar sits under the page beside it. The two persistent surfaces never cover each other.
- **The top bar:**
  - The Flux mark beside the wordmark, in tracked capitals.
  - An account cluster: the level's badge (`LevelBadgeSize`), the player's name over a line filling toward the next level (`ProgressionBarWidth`, `ProgressionBarHeight`) and the level and XP, and each account currency's amount beside its own mark (`CurrencyGlyphSize`).
  - The progression model now carries the level's number and each balance alone.
- **Home:**
  - The welcome sits high on the left over the art's darkened side (`HomeHeroHeight`).
  - Play is the clear way in (`HomePlayWidth`, `HomePlayHeight`, `HomePlayFontSize`).
  - The featured Vanguard is named at the foot of the same column.
- **The friends rail:**
  - The heading says how many friends are online (`OnlineCount`, `OnlineSummary`).
  - The player's own status is one line with its toggle.
  - The add field and its button share a line; the button shows a plus and is still named Add Friend.
  - Friends are grouped under Online and Offline headings, each with its count, once any status is known.
  - Each friend has an initial tile (`AvatarSize`) with a status dot on its corner (`Presence*Color`, `PresenceDotSize`), beside the words that already say the status, so colour is never the only signal.
- **The party bar:** members are chips side by side, each with an initial tile. When it shows and what it offers are unchanged.
- **Champion select:**
  - The shown Vanguard's art is the centrepiece, full bleed (`SelectArtTint`), darkened only at the edges where the bench, columns and loadout sit. A right-hand scrim joins the left, top and bottom ones.
  - The name is in the display tier, its title in brass, and the kit's icons (`SelectKitIconSize`) sit beside View Abilities.
  - Lock In is the primary action.
  - The approved arrangement stands: bench on top, countdown beneath, teams at the sides, art in the centre, loadout and Lock In at the bottom.
- **The results' and Match History's scoreboard:**
  - Each player is one line: the Vanguard's portrait (`ReportPortraitSize`), its name over the player's, the figures, and the build as item and Flux Spell icons (`ReportIconSize`). The words are in tooltips.
  - Report lines carry the Vanguard, item and Flux Spell IDs.

### 6. What comes next, each after the author reviews the slice in engine

1. **The match HUD:** upgrade the canvas painter.
   - Keep the HUD's models, layout, scales and safe area (ADR-059).
   - Give it the Numeric and Interface fonts, chamfered frames, gradient bars with segment ticks, and material cooldown sweeps.
   - Fix the shop's overlap with the deck and minimap.
2. **The friends rail's collapse** (Visual Production Standard §5.1: the sidebar and party panel independent and collapsible).
3. **The remaining screens:** Collection, Profile, Match History's list, Settings and the shop.
4. **Background blur** (Art Bible §4.2: selective blur): measure its Slate cost first, and turn it off under Reduce Interface Transparency.

## Consequences

- Every shell screen takes the new type and surfaces at once, through `VeyraShellStyle`. Screens built from it need no change.
- The client now ships seven font files (about 0.7 MB) and their licences. Packaged builds must stage `Veyra/Fonts`, or validation fails and the shell shows nothing.
- A few dozen small textures are made at start-up, kept for the life of the process.
- The look is reviewed in engine, on packaged builds, before it spreads to the HUD and the remaining screens (Visual Production Standard §12: generated ≠ integrated ≠ reviewed ≠ approved).
