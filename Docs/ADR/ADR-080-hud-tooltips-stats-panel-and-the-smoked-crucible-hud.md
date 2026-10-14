# ADR-080: Tooltips that show their numbers, the Vanguard stats panel, and the Smoked Crucible on the match HUD

**Status:** Proposed. On 2026-10-13 the author reviewed the client's new look (ADR-078) and asked for more polish, then raised two things it had not addressed:

- **Tooltips:** "the way we describe the ability and display damage info needs to change … i think we can find a middle ground", between a compact tooltip and a detailed one.
- **Stats:** "there is no way for us to see our vanguards stats."

From a design board, they approved:
- the structured tooltip;
- a stats panel beside the portrait, with eight stats always shown and more on a click;
- the whole HUD slice: the painter upgrade ADR-078 §6 named, done with these.

This record amends ADR-066 §5 for the player's own Vanguard only: the selected unit's frame still shows no stats. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- [ADR-065](ADR-065-first-match-feedback-waves-spires-rewards-and-patch.md) §7: an ability's numbers beside its text, from tuning. The text table holds no numbers, and that stays.
- [ADR-059](ADR-059-hud-scaling-cooldowns-statuses-and-chat-readability.md): the HUD's component scales, safe area and layout, which this keeps.
- [ADR-078](ADR-078-the-smoked-crucible-client-look.md): the client's type and surfaces, which the HUD now shares.
- Settings & Accessibility Bible:
  - 62: Interface Text Size covers informational tooltips.
  - 63: no pinnable tooltips.
  - 52: damage numbers are colour-coded by type.
  - 97: an item reads stats before effects.

## Context

The canvas HUD had one tooltip shape for everything, always centred over the deck:
- a title, a detail line, a description, and for Q, W, E and R a list of preformatted number lines;
- items showed their description only, which is empty for an item of plain stats;
- cooldowns ignored Ability Haste;
- stuns, slows and other statuses were never named;
- the tooltip ignored Interface Text Size.

Every stat a Vanguard has is an attribute its owner already receives (ADR-006 §6), but nothing showed them. Status chips spelled a status kind's code name, such as "Move Speed Toward Enemy Vanguards".

## Decision

### 1. One look for the HUD

- **Type:** the HUD's font is the client's functional family (ADR-078 §2), read from the shipped files. The HUD's Black weight is that family's Bold.
- **Surfaces:** the painter draws every HUD surface as the client's chamfered, brass-edged surface, nine-sliced from the same generated textures. The deck's flat accent strip is gone.
- **Health bar:** a tick at each `HealthTickAmount` of Health, so its length reads as an amount.

### 2. Tooltips

**Where and how large**
- A hovered tile's tooltip stands over that tile, kept on screen.
- It is drawn at the player's Interface Text Size.

**Its parts**
- **Header:** the icon, the title with its key in a chip, and the detail (an ability's rank or "Not learned", an item's kind). On the right, the cooldown and the cost. The header and the facts carry every number the cast has, so the old line of numbers is gone.
- **Description:** the text table's one line, still without numbers.
- **Facts** (abilities only), each in a row:
  - The role's name, from the `damage_role` rows.
  - The value now, coloured by kind: damage in Combat Text's physical, magic and true colours; crowd control and other harm in `TooltipHarmColor`; shields and other help in Combat Text's healing colour.
  - In grey beside it, how the value is made: "60 + 20% of your 58 Physical Power", "95 + 6% of your 1000 Max Health, for 3 s", or for how long.
- **Stats** (items only): the shop's own lines, before what the item does (Settings Bible 97).
- **Footer** (learned abilities below their last rank): each damage part's whole amount at the next rank with the caster's power now, so it compares with the value above.

**Where the facts come from**
- `VeyraAbilityNumbers::Summarise` builds them from tuning with the formulas gameplay uses: `VeyraAbilityRules::DamageAmount`, and `VeyraHaste::CooldownMultiplier` for the cooldown after the caster's Ability Haste.
- Shields come from `VeyraAbilityRules::ShieldParts`, at `VeyraAbilityRules::ShieldAmount`: the one formula a shield grant is now made with.
- Statuses come from `VeyraAbilityRules::StatusParts`, which reads each effect bundle's status list the way `DamageParts` reads its damage. Each status is named by its kind's `status_kind` row, with its strength as a share and its duration from the `statuses` map.
- The same words name the player's status chips.

**Unchanged:** no tooltip can be pinned (Settings Bible 63), and the text table still holds no numbers (ADR-065 §7).

### 3. The Vanguard stats panel

- **Placement:** a panel left of the deck and level with its foot, beside the portrait. Where the screen leaves no room there, it sits above the deck's left end.
- **What it shows:**
  - Always: Physical Power, Magic Power, Armor, Magic Resist, Attack Speed, Ability Haste, Critical Chance and Move Speed.
  - A click opens a second block above it: Health Regen, the resource's Regen, Physical and Magic Penetration, Tenacity and Attack Range. The open state is the presentation's, for the match.
- **Brass values:** a stat whose value is above its attribute's base (raised by items or effects) reads in brass. Tenacity reads raised when less crowd control is retained.
- **Where the values come from:**
  - The owner's own attributes, which only it receives (ADR-006 §6). Attack Range is the basic attack's, from tuning.
  - The words come from `stat` rows in the text table.
  - The values come from `VeyraHud::DescribePlayer`, as `FVeyraHudStats`.
- **Layout:**
  - The panel is part of the HUD's arrangement (`StatsCore`, `StatsMore`), at the deck's scale.
  - The chat rises above it where they would meet.
  - Clicks on it neither select nor clear.
- **The selected unit's frame still shows no stats** (ADR-066 §5).

### 4. The shop clear of the HUD

- The shop keeps its designed size and stands in the middle of the room the HUD leaves: inside the safe area, above the deck and left of the minimap (`VeyraHudLayout::ShopRoom`).
- It shrinks only where the player's HUD scales leave less room than it needs.
- It no longer covers the deck or the minimap.

### 5. Values

All presentation, in `[/Script/VeyraUI.VeyraGreyboxSettings]`:
- `TooltipWidth`, `TooltipIconSize`, `TooltipLabelWidth`, `TooltipHarmColor`;
- `StatsPanelWidth`, `StatsRowHeight`;
- `HealthTickAmount`.

### 6. Captures

Two development console variables let a capture show these. Neither is in a shipping build.
- `Veyra.Dev.HudHover` shows a deck tile's tooltip as if the cursor rested on it.
- `Veyra.Dev.StatsPanelOpen` holds the stats panel open.

`Smoke.ps1 -Screenshot -ScreenshotCommands "<commands>"` passes them to the rendering client.

## Consequences

- An ability's tooltip now matches its cast at every rank and every power, with no number written twice. A new status kind needs its `status_kind` row; a test fails without one.
- The HUD draws more quads, nine for each chamfered surface, with no new textures beyond the shared few.
- The panel and the tooltip read only what the owner already receives. No new replication.
