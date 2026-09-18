> Repository Markdown edition derived from Veyra Item Bible v0.3. Exact prices, stat values, cooldowns, and ratios remain prototype tuning values.

# Veyra Item Bible

**Version:** 0.3 — Mage Foundation + Consumables + Crit Capstone

## 1. Core philosophy

Build backward from useful finished items. Design the Tier 2 and Tier 3 outcomes players actually need, then let those recipes reveal the Tier 1 component pool.

Recipes should explain the finished item's stats and purpose.

Items do **not** auto-combine. Owning the components is not enough; the player must purchase the item's recipe/completion cost.

## 2. Tier architecture

| Tier | Role | Rule |
|---|---|---|
| **Tier 1** | Components | Cheap, readable base stats. No Attunements. |
| **Tier 2** | Assemblies | Improved stat combinations or specialized stats. No Attunements. Simple Active abilities are allowed. |
| **Tier 3** | Masterworks | Finished build-defining items. Exactly **one Attunement** per item. A distinct Active may coexist with that Attunement. |
| **Tier 4** | Future Ascension | Not designed yet. A player may eventually own exactly one Tier 4; purchasing one permanently locks all other Tier 4s for that player for the match. |

Tier 3 recipes are not locked to one formula. A Masterwork can reasonably use two Tier 2s, three Tier 2s, Tier 2s plus Tier 1 components, or another logical combination.

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

Final public-facing terminology and exact formulas remain open where not otherwise specified.

## 4. Tier 1 components

| Item | Stat identity |
|---|---|
| **Basic Boots** | Movement Speed |
| **Vital Plate** | Health |
| **Renewal Mesh** | Health Regeneration |
| **Timing Coil** | Ability Haste |
| **Iron Grip** | Physical Power |
| **Quickcoil** | Attack Speed |
| **Keensteel** | Critical Strike Chance |
| **Arc Crystal** | Magic Power |

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

The exact Magic Power amplification model remains open.

## 6. Current Tier 2 assemblies

### Reinforced Chassis
**Recipe:** Vital Plate + Renewal Mesh + recipe  
**Stats:** Health + Health Regeneration.

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

## 10. Consumables

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

## 11. Locked item-system rules

- Tier 1 is intentionally boring and readable.
- Tier 2 communicates build direction through stats and may contain simple Actives.
- Tier 2 never contains an Attunement.
- Tier 3 contains exactly one Attunement.
- A Tier 3 may also contain a separate Active.
- Tier 3 recipe complexity may vary.
- Items never auto-combine; completion/recipe gold must be purchased.
- Boots currently stop at Tier 2.
- Tier 4 is future-facing and has no designed items yet.
- If Tier 4 is introduced, one purchased Tier 4 permanently locks all other Tier 4s for that player during that match.
- Work backward from useful completed items instead of inventing a giant component catalog in isolation.

## 12. Current gaps

The initial shop still needs significant expansion. Likely future families include:

- Armor and anti-physical defense;
- Magic Resistance and anti-magic defense;
- healing reduction;
- universal damage-based sustain / omnivamp;
- support/enchanter utility;
- displacement actives;
- vision tools;
- anti-Attack-Speed and anti-basic-attack defense;
- additional crit/on-hit branches;
- hybrid and niche counter-items.

The current catalog is a prototype foundation, not a launch-complete shop.
