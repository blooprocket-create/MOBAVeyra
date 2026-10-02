# ADR-050: Stasis, Echoes, Echo Lens and The Second Self

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §10 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §10: Untargetable, Invulnerable and Stasis, with the controllable-proxy exception.
  - §14: effects over time.
  - §32: what an owned unit causes is its owner's.
- [Item Bible](../Design/Veyra_Item_Bible_v0.3.md):
  - §9: Echo Lens, Project Echo and Reverberation.
  - §11: The Second Self and the one-Mythical rule.
- [ADR-009](ADR-009-runtime-combat-primitives.md): statuses, action blocks and the combat verbs.
- [ADR-012](ADR-012-items-and-shop.md): item Actives and the shop.
- [ADR-023](ADR-023-crit-and-the-full-item-catalog.md): Attunements, and why a companion's damage does not set off its owner's.
- [ADR-034](ADR-034-companions-for-marek-and-nix.md): owned units, attribution and inheritance.

## Context

The Item Bible's 2026-10-02 revision adds an item line built on a new combat state.

**Echo Lens** is a Tier 3 Mage Masterwork: Grand Prism + Catalyst Coil + Spellguard Plate.
- Its Active, **Project Echo**, manifests a short-lived Flux Echo.
- Its Attunement, **Reverberation**, has the Echo repeat the holder's next eligible ability at reduced damage.
- Nothing an Echo causes may create another Echo or set off Reverberation again.

**The Second Self** is a Tier 4 Mythical: Echo Lens + Gravitic Seal.
- It keeps Reverberation and Drag.
- Its Active puts the holder in **Stasis** where they stand and forms a controllable Echo at a chosen point within a tether radius.
- The Echo forms in 0.5 s, inside a 2.0 s window when enemies cannot reduce its Integrity. Control then passes to it.
- It has **Echo Integrity** in place of Health. Integrity decays from activation, enemy hits remove set amounts, and nothing restores it.
- It deals 25% of the holder's damage. It cannot use item Actives or Flux Spells, and may use only the one ability Reverberation allows.
- Its tether radius shrinks with its Integrity. Leaving the circle, or reaching 0 Integrity, ends the Echo and the Stasis, and control returns to the holder where they stand.

**What exists today:**
- No Stasis: Dormant blocks actions but is not invulnerable, and Untargetable leaves damage over time running.
- Item Actives are ordinary abilities in item slots, cast through the one cast path.
- Companions are owned units whose damage and kills are their owner's. Each owner has at most one, and their AI moves them.
- Orders reach only the participant's own Vanguard controller, and the camera follows only the Vanguard.

## Decision

### 1. Stasis is a status kind

`EVeyraStatusKind::Stasis` (Combat) has magnitude 0. While a unit holds it:
- **It cannot act:** it cannot move, basic attack or cast. Item Actives and Flux Spells cast through the same path, so Cast blocks them too.
- **Entering Stasis interrupts** a windup or channel in progress, as a Knockup does.
- **It is Untargetable.** `VeyraTargeting::IsUntargetable` answers true for Stasis, so acquisition, skillshots, areas, cleaves, lingering areas, tethers and targeted projectiles already treat it as Untargetable.
- **It takes no damage, True Damage included.** The vitals treat Stasis as Invulnerability at §25 step 7, so a damage-over-time tick deals 0 and consumes no shield, while the status stays attached and keeps its time.
- **New statuses from enemies are refused**, crowd control or not. Displacement from anyone is refused.
- **Nothing restores its Health or grants it a shield** while it lasts. Health Regeneration, which is not a new heal, continues.
- **It is not crowd control:** Tenacity does not shorten it, and Cleanse does not remove it.
- **It does not end early** unless the effect that applied it removes it. Combat State is unchanged.

### 2. The Echo is a unit of its own kind

`EVeyraUnitKind::Echo` (Core) is a projection of a Vanguard.

`AVeyraEcho` (Abilities, `Echoes/`) is a character on the Veyra movement component. It has:
- its own Ability System Component (Minimal replication) and the combat components;
- the holder's basic attack profile;
- `IVeyraOwnedUnit`: everything it causes is its holder's (Combat Bible §32). Its damage therefore sets off none of the holder's Attunements, as with a companion (ADR-023), which keeps Reverberation from recursing.

At formation it takes a snapshot of the holder's offence (powers, penetration, crit, Attack Speed). Its Outgoing Damage Multiplier is the holder's multiplied by the Echo's **damage coefficient**.

Vision treats an Echo as it treats a companion: a sight source for its side. The minimap and the Fluxborn rules treat it in the same way.

### 3. Integrity is a sealed Health meter

A new verb, `VeyraCombat::SealHealth`:
- makes damage reduce a unit's Health by 0, as Invulnerability does, while every hit is still announced (`OnHostileDamage`);
- refuses any heal or shield.

Its owner moves Health only through `SetSealedHealth`. An Echo's Health **is** its Integrity, so health bars, death and the HUD read it with no special case.

`UVeyraEchoSubsystem` (Abilities, server) keeps each Echo. On its timer it:
- removes the natural decay;
- removes the set amount for each enemy hit, by its delivery (basic attack, ability, structure attack, effect over time). It does this only after the immunity window, and only for hits from the Echo's enemies;
- ends the Echo at 0.

### 4. The Echo ability archetype

`UVeyraEchoAbility` reads a new `echo` section in `Abilities.json`, schema-validated like every archetype. Each entry has a **mode**.

- **`Manifest`** (Project Echo):
  - The Echo forms at once at the cast's ground point, within its cast range.
  - It is Untargetable, takes up no space, and has no Integrity to lose.
  - It lasts `windowSeconds`, or until it repeats an ability.
- **`Projection`** (Second Self):
  - The cast's point must lie within the current maximum tether radius.
  - The holder takes the entry's Stasis status, and the Echo begins forming at the point.
  - After `formationSeconds` control passes to it. Enemies cannot reduce its Integrity until `immunitySeconds` after the cast.
  - Its Integrity starts at `integrity` and decays at `decayPerSecond`.
  - Its tether radius is `minRadius + (maxRadius − minRadius) × (Integrity / integrity)^radiusExponent`.
  - It ends when it leaves the radius, at 0 Integrity, or when the holder dies or leaves. Its end removes the holder's Stasis.

Every Echo entry also carries:
- the **damage coefficient**;
- the **slots** whose abilities it may repeat;
- the number of **repeats** it allows (one, for both items).

### 5. Reverberation: one repeat of one eligible ability

An ability is eligible when:
- its slot is in the Echo's slots;
- its archetype can repeat it. Areas and skillshots deliver from a caster's position, can be repeated, and do not channel. An archetype that moves, attaches, rides or buffs its caster cannot be repeated.

**How the repeat is delivered:** the archetype's own delivery runs again with the Echo as caster.
- It starts from where the Echo stands, aimed at the original ground point, or at the original unit if the Echo may target it.
- A point beyond the ability's range from the Echo is brought within it, as any cast's is.
- The repeat has no windup, cost or cooldown, and is never announced as a cast. Nothing listening for casts sees it, so it cannot set off another repeat.
- Its damage uses the Echo's coefficient. Its other effects (statuses, displacement) are the ability's own.

**When the repeat happens:**
- **Manifest:** when the holder's next eligible cast commits within the window, the base ability delivers it a second time with the Echo as caster, then the Echo ends.
- **Projection:** a cast order for an eligible slot, given while controlling the Echo, delivers that ability once from the Echo. It uses the holder's rank and ignores the holder's cooldown and resource. Any other cast order is refused with the Echo's reason, item Actives and Flux Spells included.

A level-scaled amount reads the caster's Level. An owned unit's Level is its holder's.

**Items data:** `Items.json` gains a `reverberation` Attunement section. Validation requires every item holding it to carry an Echo ability as its Active. Echo Lens and The Second Self are added as items. The Second Self is a Mythical under the existing one-per-match rule.

### 6. Control passes through Match

When a Projection Echo forms, `UVeyraEchoSubsystem` announces control passing to the Echo. When it ends, it announces control returning. **Match** routes this, as it routes every peer:
- **Orders:** the GameMode keeps a **commanded unit** for each participant: its Vanguard, or its Echo while one is controlled.
  - Move, attack, attack-move and stop orders reach a controller possessing the commanded unit. Match spawns one when control passes and releases it when control returns.
  - Cast orders go to the Echo subsystem while an Echo is commanded.
  - The Vanguard's own controller keeps nothing from the Echo's orders.
- **Camera:** the PlayerController replicates the commanded unit to its owner. The camera follows it, and the HUD's ability bar shows only the slots the Echo may still repeat.
- **Bots:** bots do not buy the Echo items until their brains can play a projection. Their builds are data, and the Echo items are left out.

### 7. Presentation reads replicated state only

The Echo replicates:
- its holder and anchor (the Stasis position);
- its Integrity and maximum;
- its immunity end, its formation end and its current radius.

The grey-box draws:
- the Echo as a translucent body in its side's colour;
- the **tether circle** around the anchor at the current radius;
- the **stream** from the anchor to the Echo. It reads strained once Integrity falls below a set share, and warns near the boundary.

None of this is authority. Effects come later, from the same state.

### 8. Tuning

**Abilities.json:**
- The `echo` section holds `project_echo` and `second_self`.
- The `statuses` section holds `second_self_stasis`. Its duration must cover the Echo's longest life, `integrity ÷ decayPerSecond`, so only the Echo's end removes it.

**Items.json:** `echo_lens`, `the_second_self` and the `reverberation` section.

Every value is **Provisional** unless the bible fixes it. The bible fixes:
- formation 0.5 s;
- immunity 2.0 s;
- the 25% coefficient;
- the recipes.

### 9. Tests

- **`Veyra.Combat.Stasis`:**
  - Stasis blocks moving, attacking and casting;
  - it is Untargetable;
  - it takes 0 damage, True Damage included;
  - a damage-over-time tick deals 0 while the status stays attached;
  - an enemy status is refused, and so is any displacement;
  - healing and shields are refused, while regeneration continues;
  - Tenacity does not shorten it;
  - removing it ends all of this.
- **`Veyra.Combat.SealedHealth`:** damage changes nothing but is announced; heals and shields are refused; `SetSealedHealth` moves Health and kills at 0.
- **`Veyra.Abilities.EchoRules`:** the radius curve, eligibility, and the repeat's aim and clamping.
- **`Veyra.Net.Echo`:**
  - Project Echo repeats the next eligible cast once, at the coefficient, and then ends;
  - an ineligible cast is not repeated;
  - the window expires;
  - nothing recurses: no second repeat, and no Attunement from the Echo;
  - kill credit goes to the holder.
- **`Veyra.Net.SecondSelf`:**
  - Stasis on cast;
  - formation, then control;
  - immunity, then hits removing Integrity;
  - decay ending the Echo;
  - leaving the tether and a shrinking tether both ending it;
  - the end removing Stasis and returning control;
  - orders moving the Echo, its attack and its one repeat;
  - item Actives and Flux Spells refused;
  - the camera's commanded unit.
- **Items:** recipes, the one-Mythical rule, and Reverberation's validation.

## 10. Provisional answers where canon is open

1. **Project Echo** is ground-targeted within 700, forms at once, and waits 4 s for the repeat. Its coefficient is 0.35 and its cooldown 30 s. The Tier 3 identity is "one deliberate Echo-assisted ability", so the Tier 3 coefficient sits above the Mythical's 25%.
2. **Eligible slots** are Q, W and E for both items. Ultimates are left out so that the repeat stays a "deliberate" assist, not a second ultimate.
3. **A repeat carries the ability's statuses and displacement unchanged.** Only damage is scaled; the bible scales only damage. Reducing crowd control would need a per-archetype reduced delivery. The author may ask for it.
4. **The controlled Echo's repeat** ignores the holder's cooldown and resource and uses the holder's rank, once per activation. It is a repeat, not a second kit.
5. **Second Self:**
   - Integrity 100, decaying 12.5 per second, which gives at most 8 s.
   - Tether radius from 1100 down to 450, linear (exponent 1).
   - After immunity, an enemy Vanguard's basic attack removes 20, an ability hit 25, a structure attack 50, a Fluxborn's or wildlife's attack 10, and an effect-over-time tick 5. Five basic attacks therefore break a healthy Echo.
   - Cooldown 90 s; its cast range equals the maximum radius.
6. **Stasis** refuses enemy statuses of every kind, not only crowd control, because nothing hostile can target the unit anyway. It refuses displacement from allies too.
7. **Health Regeneration continues in Stasis.** The bible forbids only new healing and shields.
8. **The Echo is seen by both sides,** as a unit is, and gives its side sight as a companion does.
9. **Stats:**
   - Echo Lens: 70 Magic Power, 200 Health, 15 Ability Haste, 750 to complete.
   - The Second Self: 110 Magic Power, 400 Health, 25 Ability Haste, 750 to complete.

## Out of scope

- Echo VFX beyond the grey-box circle and stream, and Echo audio.
- Bots buying or playing the Echo items.
- A version of a repeat with reduced crowd control.
