> Repository Markdown edition derived from Veyra Item Bible v0.3. Exact prices, stat values, cooldowns, and ratios remain prototype tuning values.

# Veyra Item Bible

**Version:** 0.3 — Mage Foundation + Consumables + Crit Capstone

**Revision 2026-09-30:** Added the Warforged Grip and Titansteel Grip Physical Power components, Killstring Assembly, and Doombringer Bow. Doombringer's exact stack counts, damage ratio, and stat values are prototype tuning; its no-cooldown Doom cycle is the intended mechanic.

## 1. Core philosophy

Build backward from useful finished items. Design the Tier 2 and Tier 3 outcomes players actually need, then let those recipes reveal the Tier 1 component pool.

Recipes should explain the finished item's stats and purpose.

**Roster-synergy rule:** Items should be designed around mechanics and needs that recur across multiple current Vanguards and their kits. An item may naturally be especially attractive to some Vanguards, but it should not be designed as a disguised named item for one specific Vanguard. Synergy should emerge from shared mechanics such as shielding, sustained combat, crowd control, basic attacks, mobility, protection, Health scaling, or spell patterns.

**Lore-naming principle:** Item names should draw from Veyra's established regions, peoples, materials, technologies, weapons, artifacts, events, and institutions where that connection feels natural. Some shop items may reference or derive from equipment used by Vanguards, provided the item remains a general systemic item rather than a mechanically exclusive item for that Vanguard. Functional names remain valid when a lore reference would be forced.

**External-inspiration rule:** Existing games may be used to identify useful item roles, counterplay needs, or broad strategic niches, but they are inspiration only. Veyra items should not be direct mechanical copies or simple renamed equivalents. Their recipes, Attunements, interactions, and lore identity should be designed around Veyra's own combat rules, current Vanguard roster, and worldbuilding.

Items do **not** auto-combine. Owning the components is not enough; the player must purchase the item's recipe/completion cost.

## 2. Tier architecture

| Tier | Role | Rule |
|---|---|---|
| **Tier 1** | Components | Cheap, readable base stats. No Attunements. |
| **Tier 2** | Assemblies | Improved stat combinations or specialized stats. No Attunements. Simple Active abilities are allowed. |
| **Tier 3** | Masterworks | Finished build-defining items. Exactly **one Attunement** per item. A distinct Active may coexist with that Attunement. |
| **Tier 4** | Mythicals | Apex items. A player may own exactly one Tier 4 per match; purchasing one permanently locks all other Tier 4s for that player. Tier 4 Mythicals may contain exactly **two Attunements** and may combine or evolve mechanics established by their prerequisite items. |

Tier 3 recipes are not locked to one formula. A Masterwork can reasonably use two Tier 2s, three Tier 2s, Tier 2s plus Tier 1 components, or another logical combination.

## 2.5 Quest Items

Quest Items are purchased normally but evolve through match-specific gameplay objectives rather than by paying an upgrade recipe cost.

- A Vanguard may own **only one copy of a given Quest Item** at a time.
- Quest progress is bound to that Vanguard's owned item and cannot be duplicated by purchasing additional copies.
- Completing the quest automatically evolves the item at no additional Gold cost.
- Evolution is permanent for that owned item.
- Exact quest thresholds, charge rules, Gold payouts, progress values, and similar numbers are prototype tuning data.
- Quest Items must not enforce predefined lanes or roles; they should reward recognizable play patterns while remaining open purchases.
- Evolved Quest Items may be used as ingredients in later item recipes.
- Selling, rebuying, undoing, or otherwise manipulating shop state must not duplicate, reset, or farm completion rewards unless a future rule explicitly permits it.


## 3. Current stat language

Working stat names include:

- Health
- Health Regeneration
- Physical Power
- Magic Power
- Armor
- Magic Resistance
- Attack Speed
- Ability Haste
- Movement Speed
- Critical Strike Chance
- Physical Penetration
- Magic Penetration
- Critical Damage
- Lifesteal
- Omnivamp
- Tenacity
- Slow Resistance

**Penetration comes in two forms for both damage types.** Physical Penetration and Magic Penetration each exist as a **percentage** and a **flat** stat, applied in the Combat Bible §3 order (percentage before flat). Each item's data states which form it grants; the stat names above cover both.

Critical Damage, Lifesteal, Omnivamp, Tenacity and Slow Resistance are defined by the Combat Bible, which uses them; they are listed here so item data can grant them.

**Prices are data, not bible canon.** Item prices, recipe completion costs and exact stat values are set in validated item data during the prototype and tuned by playtest. The bible does not need them before implementation starts. Economy's starting-Gold rule sizes against whatever the Tier 1 prices are in data.

Final public-facing terminology and exact formulas remain open where not otherwise specified.

## 4. Tier 1 components

| Item | Stat identity |
|---|---|
| **Basic Boots** | Movement Speed |
| **Vital Plate** | Health |
| **Renewal Mesh** | Health Regeneration |
| **Timing Coil** | Ability Haste |
| **Iron Grip** | Physical Power |
| **Warforged Grip** | Physical Power; stronger, more slot-efficient raw component than Iron Grip |
| **Titansteel Grip** | Physical Power; strongest current raw Physical Power component |
| **Quickcoil** | Attack Speed |
| **Keensteel** | Critical Strike Chance |
| **Arc Crystal** | Magic Power |
| **Marchplate** | Armor |
| **Shatterdeep Crystal** | Magic Resistance |

Future Tier 1 components for Armor, Magic Resistance, and other raw stats should be added only as finished-item recipes require them.

## 5. Boots

Boots stop at Tier 2 in the initial item system.

### Swift Boots
**Tier 2**  
Movement Speed specialization.

### War Boots
**Tier 2**  
Movement Speed + Attack Speed.

### Arcane Boots
**Tier 2**  
Movement Speed + Magic Power amplification.

The exact Magic Power amplification model remains open until playtesting; it is not needed before implementation starts.

## 6. Current Tier 2 assemblies

### Reinforced Chassis
**Recipe:** Vital Plate + Renewal Mesh + recipe  
**Stats:** Health + Health Regeneration.

### Picket Plating
**Recipe:** Vital Plate + Marchplate + recipe  
**Stats:** Health + Armor.

Picket Plating takes its name from the heavy defensive construction associated with Eudora Blackbridge's Picket machine and the Iron March engineering tradition around it. It is a general defensive Assembly, not equipment exclusive to Eudora.

### Canyonward
**Recipe:** Vital Plate + Shatterdeep Crystal + recipe  
**Stats:** Health + Magic Resistance.

Canyonward takes its name from protective gear and traditions associated with Shatterdeep's dangerous canyon crossings. It is a general anti-magic defensive Assembly, not equipment exclusive to Aurelisse or any one Vanguard.

### Breaker Aegis
**Recipe:** Marchplate + Shatterdeep Crystal + Timing Coil + recipe  
**Stats:** Armor + Magic Resistance + Ability Haste.

Breaker Aegis draws its name from the Breaker Fluxborn tradition associated with Torr's origin. Its identity is mixed defense plus faster ability cycling; it has no Active or Attunement.

### Resonant Wardstone
**Recipe:** Shatterdeep Crystal + Shatterdeep Crystal + recipe  
**Stats:** High Magic Resistance.

Resonant Wardstone is the concentrated anti-magic Assembly: the Tier 2 raw-Magic-Resistance specialization built from two Shatterdeep Crystals. It has no Active or Attunement and exists to feed heavier anti-magic Masterworks.

### Foundation Plate
**Recipe:** Marchplate + Marchplate + recipe  
**Stats:** High Armor.

Foundation Plate is the concentrated anti-physical Assembly: the Tier 2 raw-Armor specialization built from two Marchplates. Its name draws from the buried bridge foundations, masonry, and ancient iron of the Buried Riverlands. It has no Active or Attunement.

### Waymark Weave
**Recipe:** Renewal Mesh + Renewal Mesh + recipe  
**Stats:** High Health Regeneration.

Waymark Weave is the concentrated Health Regeneration Assembly. Its name draws from the regenerative safe-harbor identity of the Drowned Cantons' wayfinding traditions. It has no Active or Attunement.

### Rescue Rig
**Recipe:** Vital Plate + Quickcoil + recipe  
**Stats:** Health + Attack Speed.

Rescue Rig draws from the practical harnesses, lines, and working rescue equipment used around the Drowned Cantons. It is a general Health/Attack-Speed Assembly and has no Active or Attunement.

### Siege Frame
**Recipe:** Vital Plate + Timing Coil + recipe  
**Stats:** Health + Ability Haste.

### War Harness
**Recipe:** Vital Plate + Iron Grip + recipe  
**Stats:** Health + Physical Power.

### Bloodforged Bracer
**Recipe:** Renewal Mesh + Iron Grip + recipe  
**Stats:** Health Regeneration + Physical Power.

### Accelerant Gear
**Recipe:** Quickcoil + Timing Coil + recipe  
**Stats:** Attack Speed + Ability Haste.

### War Mechanism
**Recipe:** Quickcoil + Iron Grip + recipe  
**Stats:** Attack Speed + Physical Power.

### Striker Assembly
**Recipe:** Iron Grip + Timing Coil + recipe  
**Stats:** Physical Power + Ability Haste.

### Razorwheel
**Recipe:** Iron Grip + Iron Grip + recipe  
**Stats:** High Physical Power.

**Active — Cleave:** Perform a short-range physical cleave around the user.

Razorwheel is the explicit example proving that Tier 2 may contain a simple Active without containing an Attunement.

### Deadeye Edge
**Recipe:** Iron Grip + Keensteel + recipe  
**Stats:** Physical Power + Critical Strike Chance.

### Veil Needle
**Recipe:** Iron Grip + recipe  
**Stats:** Physical Power + flat Physical Penetration.

Veil Needle draws its name from the thin throwing needles used by the Veil House tradition of the Reed Provinces. It is a general physical burst/assassin Assembly rather than equipment exclusive to Angeru. It has no Active or Attunement.

### Killstring Assembly
**Recipe:** Quickcoil + Keensteel + recipe  
**Stats:** Attack Speed + Critical Strike Chance.

Killstring Assembly is the clean Attack-Speed/Crit Assembly. It has no Attunement; its purpose is to communicate a sustained basic-attack/crit build direction and feed later Masterworks such as Doombringer Bow.

## 7. Mage Tier 2 assemblies

### Grand Prism
**Recipe:** Arc Crystal + Arc Crystal + recipe  
**Stats:** High Magic Power.

### Catalyst Coil
**Recipe:** Arc Crystal + Timing Coil + recipe  
**Stats:** Magic Power + Ability Haste.

### Nullglass Shard
**Recipe:** Arc Crystal + recipe  
**Stats:** Magic Power + Magic Penetration.

This is a specialized Tier 2 that upgrades a single Tier 1 component through a recipe.

### Spellguard Plate
**Recipe:** Arc Crystal + Vital Plate + recipe  
**Stats:** Magic Power + Health.

## 8. Tier 3 Masterworks

Each Masterwork has exactly **one Attunement**.

### Siegeheart Core

**Recipe:** Reinforced Chassis + Siege Frame + Tier 3 recipe  
**Stat identity:** Very high Health + Health Regeneration + Ability Haste.

**Attunement — Tempered by Conflict**

After sustained proximity/combat with an enemy Vanguard, that enemy becomes Tempered. The next basic attack against that Vanguard consumes Tempered, deals bonus damage based partly on maximum Health, and permanently increases the user's maximum Health.

Each enemy Vanguard has an individual cooldown.

Exact timing, damage ratio, and permanent-health amount remain tuning values.

### Colossus Temper

**Recipe:** War Harness + Bloodforged Bracer + Vital Plate + Tier 3 recipe  
**Stat identity:** High Health + Physical Power + Health Regeneration.

**Attunement — Weight of War**

Gain additional Physical Power equal to a percentage of bonus Health.

No second low-Health Attunement. The item's identity is simply: build Health, gain damage.

### Cycler Core

**Recipe:** Accelerant Gear + War Mechanism + Tier 3 recipe  
**Stat identity:** High Attack Speed + Physical Power + Ability Haste.

**Attunement — Spool Up**

Basic attacks against enemy Vanguards grant stacking Attack Speed for a short duration, up to a cap. Further attacks refresh the duration.

No max-stack explosion or additional proc is required.

### Impact Aegis

**Recipe:** War Harness + Striker Assembly + Tier 3 recipe  
**Stat identity:** Health + Physical Power + Ability Haste.

**Attunement — Reprisal Guard**

Damaging an enemy Vanguard grants a shield equal to a percentage of the damage dealt by the triggering attack or ability, then the Attunement goes on cooldown.

A maximum shield cap may be used as a tuning safety valve.

### Razorwheel Prime

**Recipe:** Razorwheel + War Harness + Quickcoil + Tier 3 recipe  
**Stat identity:** High Physical Power + Health + Attack Speed.

When upgraded, Razorwheel's original manual Cleave Active is removed.

**Attunement — Endless Cleave**

Every basic attack deals reduced physical damage to nearby enemies around the primary target.

**Active — Seize Momentum**

Perform a wide cleaving strike. Enemy Vanguards hit lose a percentage of Movement Speed for a short duration. Razorwheel Prime gains the stolen Movement Speed, pooled across enemies hit up to a cap, for the same duration.

The Active is distinct from the item's single Attunement.

### Sovereign Edge

**Recipe:** Deadeye Edge + Iron Grip + Keensteel + Tier 3 recipe  
**Stat identity:** Very high Physical Power + high Critical Strike Chance.

**Attunement — Perfect Cut**

Critical strikes deal increased damage.

This is intentionally a simple crit capstone rather than a proc-heavy item.

### Riverhold Bastion

**Recipe:** Picket Plating + Foundation Plate + Marchplate + Tier 3 recipe  
**Stat identity:** Very high Armor + high Health.

**Attunement — Drag the Tempo**

When an enemy Vanguard hits the holder with a basic attack, that enemy's Attack Speed is reduced for a short duration. Further basic attacks from that Vanguard refresh the duration rather than stacking the reduction.

Riverhold Bastion is the dedicated anti-basic-attack / anti-Attack-Speed defensive Masterwork. Its name draws from the Buried Riverlands' ancient bridge foundations, buried iron, and hold-the-line identity. Exact Attack Speed reduction, duration, Health, and Armor values are prototype tuning values.

### Blackreef Bell

**Recipe:** Canyonward + Resonant Wardstone + Tier 3 recipe  
**Stat identity:** High Health + very high Magic Resistance.

**Attunement — Quieting Chime**

After avoiding enemy-Vanguard damage for a short period, gain **Spellward**. The next hostile enemy ability that would successfully affect the holder is negated, breaking Spellward. Spellward reforms after another period without taking enemy-Vanguard damage.

Quieting Chime blocks the qualifying ability impact as a whole, including its damage and attached crowd control or status from that impact. It does not block basic attacks, ordinary item procs by themselves, or damage/effects that were already successfully applied before Spellward formed.

Exact reform timing and stat values are prototype tuning values.

Blackreef Bell draws from the Drowned Cantons' black reefs, tuned bells, sea-glass, resonance, and fog-guiding traditions. It is a general anti-magic defensive Masterwork rather than equipment exclusive to any one Vanguard.

### Blank Sigil

**Recipe:** Veil Needle + Striker Assembly + Warforged Grip + Tier 3 recipe  
**Stat identity:** High Physical Power + flat Physical Penetration + Ability Haste.

**Attunement — No Allegiance**

After going a short period without damaging an enemy Vanguard, the holder's next damaging attack or ability against an enemy Vanguard creates a brief **Opening** on that target.

The holder's next **different** damaging attack or ability against the same Vanguard consumes Opening, gains additional Physical Penetration for that hit, and deals bonus physical damage.

The different-action requirement is deliberate: Blank Sigil rewards a setup → commit → payoff sequence rather than repetitive poke with one attack or spell. Exact out-of-combat timing, Opening duration, penetration amount, bonus-damage ratio, and stat values are prototype tuning values.

Blank Sigil draws its name from the cut-out insignia patches associated with Angeru's rejection of both Reed Provinces martial houses. It is a general assassin/burst Masterwork and is not mechanically exclusive to Angeru.

### Harborline Harness

**Recipe:** Rescue Rig + Waymark Weave + Warforged Grip + Tier 3 recipe  
**Stat identity:** Physical Power + Attack Speed + Health + high Health Regeneration.

**Attunement — Safe Harbor**

Damaging enemy Vanguards with attacks or abilities stores a percentage of the actual post-mitigation damage dealt as **Reserve**, up to a cap based on the holder's maximum Health.

After leaving Vanguard combat for a short period, stored Reserve begins converting into Health over time.

Re-entering Vanguard combat stops the recovery. Remaining Reserve is retained, subject to its cap.

Damage against minions, Fluxborn, jungle wildlife, structures, and other non-Vanguard targets does not generate Reserve. Item damage does not recursively generate Reserve. AoE and DoT Reserve generation may use reduced effectiveness if required by playtesting.

Harborline Harness is a cross-class physical sustain Masterwork rather than a tank-only item. It trades some pure offensive specialization for the ability to bank combat contribution as later recovery, creating a fight → disengage → recover → re-enter loop for bruisers, frontliners, and carries.

Exact Reserve percentage, cap, out-of-combat delay, conversion rate, and stat values are prototype tuning values.

### Doombringer Bow

**Recipe:** Killstring Assembly + Titansteel Grip + Tier 3 recipe  
**Stat identity:** High Physical Power + Attack Speed + Critical Strike Chance.

**Attunement — Marked for Doom**

Basic attacks against an enemy Vanguard apply Doom to that target. The prototype starting behavior is:

- a normal basic attack applies **1 Doom**;
- a critical basic attack applies **2 Doom**;
- at **4 Doom**, the target becomes **Doomed**;
- the user's next basic attack against a Doomed target consumes the Doom and deals bonus physical damage based on that target's missing Health.

There is **no internal or per-target cooldown** after Marked for Doom is consumed. The user may immediately begin applying Doom again, so the item's intended loop is sustained attacks → Doom payoff → immediately rebuild Doom.

The Doom amounts, threshold, bonus-damage ratio, and item stat values are prototype tuning values. Balance should adjust those levers rather than adding a cooldown unless a later design ruling explicitly changes the mechanic.

## 9. Mage Masterworks

### Starfall Prism

**Recipe:** Grand Prism + Catalyst Coil + Arc Crystal + Tier 3 recipe  
**Stat identity:** Very high Magic Power + Ability Haste.

**Attunement — Convergence**

Damaging an enemy Vanguard with one ability primes them. The next damaging ability against that target within a short window consumes the prime and deals bonus magic damage.

### Arc Reactor

**Recipe:** Catalyst Coil + Catalyst Coil + Timing Coil + Tier 3 recipe  
**Stat identity:** High Magic Power + very high Ability Haste.

**Attunement — Overcycle**

Damaging enemy Vanguards with abilities grants stacking Ability Haste for a short duration, up to a cap. Further ability hits refresh the duration.

### Nullglass Lens

**Recipe:** Nullglass Shard + Grand Prism + Tier 3 recipe  
**Stat identity:** High Magic Power + Magic Penetration.

**Attunement — Fracture**

Repeated magic damage against the same enemy Vanguard progressively reduces that Vanguard's Magic Resistance, up to a cap.

### Gravitic Seal

**Recipe:** Spellguard Plate + Catalyst Coil + Tier 3 recipe  
**Stat identity:** Magic Power + Health + Ability Haste.

**Attunement — Drag**

Damaging abilities briefly slow enemy Vanguards.

### Crown of the Crucible

**Recipe:** Grand Prism + Grand Prism + Arc Crystal + Tier 3 recipe  
**Stat identity:** Massive Magic Power.

**Attunement — Overcharge**

Increase total Magic Power by a percentage.

This is the intentionally straightforward raw-Magic-Power capstone.

## 10. Quest Items

Quest Items are purchased normally but evolve through match-specific gameplay objectives rather than by paying an upgrade recipe cost. The general Quest Item rules are defined in §2.5.

### Flux Reclaimer

**Quest Item — base form**  
**Stat identity:** Health + Health Regeneration.

**Quest — Reclamation**

Personally last-hit enemy lane Fluxborn to gain quest progress. Only credited killing blows against enemy lane Fluxborn count; jungle wildlife, structures, and other targets do not advance Reclamation.

At a data-driven completion threshold, Flux Reclaimer automatically evolves at no additional Gold cost into **Wayline Reservoir**.

### Wayline Reservoir

**Quest Item — evolved form**  
**Stat identity:** Improved Health + improved Health Regeneration.

**Passive — Residual Current**

Last-hitting an enemy lane Fluxborn stores a small amount of **Current**, up to a cap.

After avoiding enemy-Vanguard damage for a short period, stored Current is gradually consumed to substantially increase Health Regeneration.

Taking enemy-Vanguard damage suspends the enhanced regeneration but does not delete stored Current.

Exact quest threshold, Current gain, Current cap, combat-exit delay, regeneration amplification, and stat values are prototype tuning values.

Wayline Reservoir may be used as an ingredient in later item recipes.

## 11. Tier 4 Mythicals

A player may purchase exactly **one** Tier 4 Mythical per match. Purchasing one permanently locks every other Tier 4 Mythical for that player for the remainder of the match.

Tier 4 Mythicals may contain exactly **two Attunements**. They should represent an apex build commitment rather than a generic numerical upgrade and may preserve, combine, or evolve mechanics established by prerequisite items.

### The Last Harbor

**Recipe:** Harborline Harness + Wayline Reservoir + Tier 4 completion cost  
**Stat identity:** High Physical Power + Attack Speed + very high Health + very high Health Regeneration.

**Attunement I — Safe Harbor**

The Last Harbor retains and carries forward Harborline Harness's **Safe Harbor** Attunement: damaging enemy Vanguards with attacks or abilities stores a percentage of actual post-mitigation damage dealt as **Reserve**, up to a cap based on maximum Health. After leaving Vanguard combat for a short period, Reserve converts into Health over time. Re-entering Vanguard combat stops the recovery while preserving remaining Reserve, subject to its cap.

**Attunement II — High Tide**

Last-hitting enemy lane Fluxborn stores **Current**, up to a cap. While out of Vanguard combat, stored Current is gradually consumed to amplify Health Regeneration and accelerate Safe Harbor's Reserve conversion.

If **High Tide** and **Safe Harbor** restore the holder to full Health while both still have stored energy remaining, a portion of the remaining recovery is converted into **Temporary Health**, up to a cap based on maximum Health. Temporary Health follows the Combat Bible's ordinary Temporary Health rules.

The Last Harbor therefore combines two progression paths: lane farming builds Current through Wayline Reservoir, while Vanguard combat builds Reserve through Harborline Harness. The intended apex loop is farm → bank Current → fight → bank Reserve → disengage → recover rapidly → potentially re-enter with a limited Temporary Health buffer.

Exact Current gain, caps, conversion rates, acceleration, Temporary Health conversion, delays, and stat values are prototype tuning values.

## 12. Consumables

### Field Tonic

Single-use purchased consumable.

Restores a flat amount of Health over several seconds.

The heal is stronger than Flux Flask. Exact price, duration, and damage-interruption behavior remain tuning decisions.

### Flux Flask

Reusable healing consumable.

Restores a smaller flat amount of Health over several seconds.

Flux Flask refills when:

- the player recalls/returns to base; or
- the player's team secures a jungle Flux Well.

The Flask should be non-stackable. Its purpose is weaker reusable sustain that connects personal map endurance to Flux objective control.

## 13. Locked item-system rules

- Tier 1 is intentionally boring and readable.
- Tier 2 communicates build direction through stats and may contain simple Actives.
- Tier 2 never contains an Attunement.
- Tier 3 contains exactly one Attunement.
- A Tier 3 may also contain a separate Active.
- Tier 3 recipe complexity may vary.
- Items never auto-combine; completion/recipe gold must be purchased.
- Boots currently stop at Tier 2.
- Tier 4 Mythicals are apex items and may contain exactly two Attunements.
- A player may own exactly one Tier 4 Mythical per match; purchasing one permanently locks all other Tier 4s for that player during that match.
- Work backward from useful completed items instead of inventing a giant component catalog in isolation.
- A Vanguard may own only one copy of a given Quest Item at a time; Quest completion cannot be duplicated through multiple copies or shop manipulation.

## 14. Current gaps

The initial shop still needs significant expansion. Likely future families include:

- expanded Armor and anti-physical defense beyond the new Marchplate foundation;
- expanded Magic Resistance and anti-magic defense beyond the new Shatterdeep Crystal foundation;
- healing reduction;
- universal damage-based sustain / omnivamp;
- support/enchanter utility;
- displacement actives;
- optional ordinary-inventory vision interactions or counter-items; the actual Persistent Ward, Sweeper, and Quick Sight tools are already defined by the Vision & Reconnaissance Bible and occupy the dedicated vision-tool slot rather than ordinary inventory;
- anti-Attack-Speed and anti-basic-attack defense;
- additional crit/on-hit branches;
- hybrid and niche counter-items.

The current catalog is a prototype foundation, not a launch-complete shop.
