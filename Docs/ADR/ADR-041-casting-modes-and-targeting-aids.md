# ADR-041: Casting modes, indicators and targeting aids

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md):
  - §1.2 casting modes, §1.3 attack move, §1.4 Target Vanguards Only, §1.5 self-cast, §1.6 continuous input, §1.7 Show Cast Range;
  - §3.3 targeting indicators.
- [ADR-024](ADR-024-player-settings.md): the settings registry, the two stores and rebinding. Its §7 deferred casting modes and the attack-move target preference to this record.
- [ADR-027](ADR-027-mobile-attacks-and-ally-casts.md) §4: a buff that may land on an ally goes to its caster when the cast names no ally.
- [ADR-030](ADR-030-stealth-markers-and-marked-follow-ups.md): what a player may not see, which no indicator or pick may reveal.

## Context

**Every cast is a Quick Cast.** `AVeyraPlayerController::OnAbilityPressed` casts on the key press, at whatever lies under the cursor. Cast actions trigger on press only and no release is bound.

**Nothing shows where a cast will go.** The greybox draws the telegraph of a cast already under way (`VeyraCastTelegraphs::ForCast`). No indicator, range ring or preview exists before the cast.

**The targeting aids the bible approves are missing.**
- There is no self-cast modifier and no Target Vanguards Only.
- Attack move is a single press toward the cursor, which is the bible's Attack Move Click. Its target is always the enemy nearest the Vanguard.

**Ownership.**
- Client input is VeyraMatch's: the controller, `UVeyraInputSettings` and the preference resolvers.
- Drawing is VeyraUI's. UI may call into Match; Match publishes state and hooks for UI, as `SetMinimapHitTest` does.
- Ability shapes and ranges are tuning that every client loads (`UVeyraAbilitiesTuningSubsystem`).

## Decision

### 1. Cast input is a rules unit (VeyraMatch)

`VeyraCastInput` is pure: a state (the pending slot, its mode, whether it only previews) and transitions for a key press, a key release, a confirming click and a cancel. Each transition answers one of: cast now, show the indicator, hide it, or nothing. The controller adapts input events to it and casts through the existing `IssueCastOrder`. Nothing about what a cast does changes; only when and where the player commits it.

**The three modes (§1.2)**, chosen separately for Q, W, E, R, each Flux Spell and item actives:
- **Quick Cast:** the press casts toward the cursor (today's behaviour).
- **Quick Cast with Indicator:** the press shows the indicator; the release casts toward the cursor.
- **Normal Cast:** the press shows the indicator; a left click casts. A right click, Escape, or the ability becoming unavailable cancels. A right click still gives its move or attack order.

Pressing another ability's key while one is pending moves the indicator to it. The rank-up modifier keeps its priority over all of this.

**Show Cast Range (§1.7):** while its modifier is held, an ability's key shows that ability's indicator and never casts. Releasing the modifier, or Escape, hides it. The next cast uses the player's mode.

**Escape** first cancels a pending cast or preview. Only when nothing is pending does it open the match menu: the menu asks the controller before it opens.

### 2. Indicators (VeyraAbilities, VeyraUI)

`VeyraCastTelegraphs::ForAim` gives the shape of an ability not yet cast, from its tuning, the caster's location and the aim:
- the cast-range ring;
- a line of the ability's width for skillshots, dashes and volleys;
- a circle for areas, a cone for cones;
- a ring around a targeted unit.

VeyraUI draws the local player's indicator each frame with the telegraph lines it already owns. Boundaries are Standard or Thick (§3.3). An indicator shows only geometry and what the player can already see: it never tests whether the cast would be valid and never marks a hidden unit.

### 3. Targeting aids (VeyraMatch, VeyraAbilities)

- **Target Vanguards Only (§1.4):** a rebindable key, Hold or Toggle. While active, the cursor picks for direct attack orders and unit-targeted casts consider enemy Vanguards only. Skillshots, areas and attack move are unchanged. The server still validates every target.
- **Self-Cast Modifier (§1.5):** a rebindable key. Held with an ability's key, it names the player's own Vanguard as the target when that ability accepts an allied unit. Otherwise the cast is unchanged: directional, ground-targeted and self-ineligible abilities are never redirected. `VeyraAbilityRules::AcceptsAllyTarget` answers from the archetype data:
  - a buff that may land on an ally;
  - a companion command bound to an ally;
  - the Unanchor order.
- **Smart Self-Cast (§1.5):** per slot, Off by default. With no allied unit under the cursor, an eligible ability targets its caster. A buff that may land on an ally already does so on the server (ADR-027 §4), so the setting changes only the other eligible abilities, which otherwise refuse a cast that names no ally.

### 4. Attack move (§1.3; VeyraMatch)

- **Attack Move:** its key, then a left click on a location. Like a Normal Cast, a right click or Escape cancels it.
- **Attack Move Click:** one press, toward the cursor.
- **Target preference:** Closest to Vanguard or Closest to Cursor. The order carries `{Destination, Preference}`. With Closest to Cursor, the order's first acquisition takes the eligible enemy in the acquisition area nearest the clicked point. Every later acquisition, and every acquisition under Closest to Vanguard, takes the one nearest the Vanguard. Both still obey vision, stealth, range and `CanAcquire`.

### 5. Settings (ADR-024 registry)

- **Choices:**
  - `controls_cast_mode_q`, `_w`, `_e`, `_r`, `_spell1`, `_spell2` and `_items`: Quick / Quick with Indicator / Normal;
  - `controls_attack_move_target`: Closest to Vanguard / Closest to Cursor;
  - `controls_target_vanguards_mode`: Hold / Toggle;
  - `interface_indicator_boundary`: Standard / Thick.
- **Toggles:** `controls_smart_self_cast_q`, `_w`, `_e` and `_r`.
- **Bindings:** Attack Move Click, Show Cast Range, the Self-Cast Modifier, Target Vanguards Only, and the Select Click that casts a waiting cast (§1.8's primary gameplay click). The modifiers and the Select Click are `Shared`, as the rank-up modifier and the ping click are. The controller reads them while it handles a key, so they take no mapping of their own.

### 6. Continuous input (§1.6)

No ability yet needs a sustained key press: a hold in this game is a recast window, not a held key. The Hold/Toggle preference waits for the first such ability, and is not invented here.

### 7. Deferred

- Vanguard-specific control profiles (§1.1, §12.2): their own milestone.
- Indicator interior opacity and high-contrast outlines (§3.3): the line batch draws no fills.
- Cursor appearance and click markers (§3.3).

### 8. Provisional answers where canon is open

1. Every slot defaults to **Quick Cast**, today's behaviour; the bible names no default.
2. **Default keys:** Attack Move A (then a click), Attack Move Click unbound, Show Cast Range Left Shift, Self-Cast Left Alt, Target Vanguards Only the backquote key. All are rebindable.
3. **Normal Cast:** a second press of the pending ability's key keeps the indicator; a refused cast leaves nothing pending.
4. **Target Vanguards Only** defaults to Hold.
5. **Closest to Cursor** decides the order's first acquisition only.
6. **Smart Self-Cast** defaults to Off.
7. **Indicator boundaries:** Standard 2 and Thick 4 units wide, beside a colour of its own, as the greybox's data (`DefaultGame.ini`).

## Consequences

- Every cast still goes through `IssueCastOrder` and the server's rules; the modes change only the moment and aim a player commits.
- The attack-move order gains a preference, so the server's acquisition has one more input.
- The match menu defers to a pending cast on Escape.
- `Veyra.Match.Input`'s mapping count grows with the new actions.

## Amendments to earlier records

- **ADR-024 §7:** Normal Cast, Quick Cast with Indicator and the attack-move target preference are decided here.
