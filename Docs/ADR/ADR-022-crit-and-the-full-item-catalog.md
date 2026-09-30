# ADR-022: Critical strikes and the rest of the Item Bible's catalog

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M17 pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [ADR-009](ADR-009-runtime-combat-primitives.md): statuses, shields and the hostile-damage event.
- [ADR-012](ADR-012-items-and-shop.md): items and the shop. This ADR carries out ADR-012 §1's deferred list.
- [ADR-014](ADR-014-jungle-and-flux-wells.md): Flux Wells.
- [ADR-015](ADR-015-flux-spells.md): item and spell slots.
- [Item Bible](../Design/Veyra_Item_Bible_v0.3.md): §3–§10.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md): §5 (critical strikes), §7 (shields), §17 (riders), §33 (structures), §41 (percentage stacking).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1, §1.3 and §1.5.

## Context

ADR-012 shipped 26 of the Item Bible's 37 items. Its §1 deferred:
- critical strikes and their items (Keensteel, Deadeye Edge, Sovereign Edge);
- Arcane Boots, whose amplification model is open;
- Flux Flask, which needed Flux Wells (ADR-014 added them since);
- the six Masterworks whose Attunements need combat hooks: Siegeheart Core, Impact Aegis, Razorwheel Prime, Starfall Prism, Nullglass Lens and Gravitic Seal.

The author has since supplied icons for all 37, and three of the ten Vanguards are marksmen with nothing to crit. A survey of the code (2026-09-30) found:

1. **Crit does not exist.** There is no Crit Chance or Crit Damage attribute, no crit tuning, and no random source anywhere in combat. The only random stream is the bots'.
2. **Attunements are typed maps in `Items.json`**, one per kind: `weightOfWar`, `overcharge`, `spoolUp` and `overcycle`. Static ones fold into `VeyraEquipment::StatsFor`. Stacking ones live in `UVeyraShopSubsystem`, on Combat's hostile-damage event, with their stacks on the inventory component.
3. **No event carries both how much damage was dealt and how it was delivered.** `FVeyraHostileDamageEvent` has the delivery but no amount; `FVeyraDamageResolution` has the amount but no delivery. Reprisal Guard, Convergence, Fracture and Drag all need both.
4. **No status reduces Magic Resistance**, though the attributes for it exist.
5. **Items sits below World and Match.** A Flux Well secured must reach the shop through Match, as fountain arrival already does.

## Decision

### 1. Critical strikes (Combat §5)

- **Stats.** The Offence set gains **Crit Chance** and **Crit Damage Bonus**, both fractions floored at 0. Items grant Crit Chance; Perfect Cut grants Crit Damage Bonus.
- **Tuning.** `Combat.json` gains `crit`: `damage` 1.75, `chanceCap` 1.0, and `overflowDamagePerChance` 0.5, all Canon.
- **The pure rule.** `VeyraCrit::Resolve(Chance, DamageBonus, Tuning, Roll)` gives whether the attack crits and its multiplier:
  - The effective chance is `min(Chance, chanceCap)`, and the attack crits when `Roll < effective chance`.
  - The multiplier is `damage + DamageBonus + max(0, Chance − chanceCap) × overflowDamagePerChance`.
  - For example, 120% Crit Chance gives certain crits at 185%, and 200% gives 225%, as §5's examples do.
- **What crits.** Only basic attacks, rolled once when the attack is planned, on the server. Clients never roll.
  - A crit adds a bonus rider to the attack: the base damage × (multiplier − 1). On-hit riders and empowerments keep their own values (§5: "On-Hit effects are not automatically multiplied by Crit Damage"; §17).
  - Crits apply to every target the attack can damage. A structure takes the bonus at Structure Effectiveness, as §33 lists the crit bonus among the riders.
  - `FVeyraAttackPlan` and `FVeyraAttackEvent` carry `bCritical`, which items read.
- **The roll** comes from a per-match `FRandomStream` that Combat's event subsystem owns. It is seeded when the world begins play, and tests may set the seed. A roll is a uniform draw in [0, 1).

### 2. Stats items may grant

`Items.json`'s `stats` gains `critChance` and `magicPowerFraction`. Every item states them, as the schema requires every stat today (Item §3: "each item's data states which form it grants"). `FVeyraEquipmentStats` carries them to `SetEquipmentStats`:
- Crit Chance adds across items.
- `magicPowerFraction` multiplies total Magic Power (§41), beside Overcharge's.

### 3. The eleven items

Every value below is Provisional data in `Items.json` and its Attunement maps; §8 has the table.

- **Keensteel (T1):** Crit Chance.
- **Deadeye Edge (T2):** Iron Grip + Keensteel; Physical Power and Crit Chance.
- **Sovereign Edge (T3):** Deadeye Edge + Iron Grip + Keensteel; very high Physical Power and high Crit Chance.
  - Attunement **Perfect Cut** (`perfectCut`): a static Crit Damage Bonus, folded in by `StatsFor` as Weight of War is.
- **Arcane Boots (T2 Boots):** Basic Boots; Movement Speed and `magicPowerFraction` (§9.3).
- **Flux Flask (consumable):** reusable. Its state and use are §6's.
- **Impact Aegis (T3):** War Harness + Striker Assembly.
  - Attunement **Reprisal Guard** (`reprisalGuard`): a basic attack or ability that damages an enemy Vanguard grants a shield of a fraction of the damage it dealt (Item §8: "the triggering attack or ability"; a proc or a tick does not trigger it). The fraction is taken after mitigation, and damage a shield absorbed counts.
  - A cap bounds the shield, which lasts `shieldSeconds`. The Attunement then cools down.
- **Gravitic Seal (T3):** Spellguard Plate + Catalyst Coil.
  - Attunement **Drag** (`drag`): ability damage to an enemy Vanguard slows it by a fraction for a short time, as a Slow status with the Attunement's ID.
- **Starfall Prism (T3):** Grand Prism + Catalyst Coil + Arc Crystal.
  - Attunement **Convergence** (`convergence`): ability damage to an enemy Vanguard primes it for a window.
  - The next ability damage from the holder within the window consumes the prime, dealing bonus magic damage (flat + a Magic Power ratio) as Proc delivery, which neither primes nor consumes.
- **Nullglass Lens (T3):** Nullglass Shard + Grand Prism.
  - Attunement **Fracture** (`fracture`): each instance of magic damage the holder deals to an enemy Vanguard, of any delivery, adds a stack reducing its Magic Resistance by a fraction, up to a cap (Item §9: "repeated magic damage"). The stacks refresh together and expire together.
  - A new status kind, Magic Resist Reduction, carries it. Validation refuses stacks that together would remove all of a Magic Resistance.
- **Razorwheel Prime (T3):** Razorwheel + War Harness + Quickcoil.
  - Upgrading from Razorwheel removes Cleave, since an item's Active is its own (ADR-012 §3).
  - Attunement **Endless Cleave** (`endlessCleave`): each basic attack also deals a fraction of its damage as Physical damage to other enemies around the primary target. A ranged holder's fraction is lower. It never hits structures.
  - Active **Seize Momentum** (`seize_momentum` in `Abilities.json`, an area archetype):
    - it deals Physical damage around the user;
    - enemy Vanguards hit are slowed;
    - the user gains Movement Speed for each Vanguard hit, up to a cap, for the same time.
- **Siegeheart Core (T3):** Reinforced Chassis + Siege Frame.
  - Attunement **Tempered by Conflict** (`temperedByConflict`):
    - an enemy Vanguard that stays within a radius of the holder for a time becomes Tempered;
    - the holder's next basic attack against it consumes Tempered, dealing bonus Physical damage (flat + a fraction of the holder's Max Health);
    - the holder permanently gains Max Health equal to a fraction of that damage;
    - each enemy has its own cooldown.
  - The permanent Health belongs to that item in its slot: sold, it is gone.

### 4. The dealt-damage event (amends ADR-009)

`UVeyraCombatEventSubsystem` gains `OnDamageDealt(FVeyraDamageDealtEvent)`, broadcast on the server once per damage instance that reaches an enemy, after `OnHostileDamage`. It carries:
- source, target and delivery. A damage-over-time tick is its own instance with the `Periodic` delivery, so no separate flag is needed;
- what each damage type cost the target after mitigation: Health, Temporary Health and shields, never overkill.

The damage pipeline opens an instance around its application, and the event subsystem sums each component's resolution into it (`ResolveDamage`, which also broadcasts `OnDamageResolved`). Instances nest, as when a hit's death deals damage of its own.

`OnHostileDamage` stays for the aggro router and recall interruption. Reprisal Guard, Drag, Convergence and Fracture live in a new **`UVeyraAttunementSubsystem`** (VeyraItems), which subscribes to `OnDamageDealt` and acts through Combat's verbs. They are not in the shop subsystem, which already owns transactions, undo, consumables and the stacking buffs. Endless Cleave and Tempered by Conflict read the attack plan, as Vanguard passives do (`OnModifyAttack`).

### 5. Magic Resistance reduction as a status (amends ADR-009)

`EVeyraStatusKind::MagicResistReduction`: a percentage status on the defence set's retained Magic Resistance, applied through the existing status verbs, with source, stacks and duration as other statuses have. Combat §3's order is unchanged: percentage reduction, then flat.

### 6. Flux Flask

- **State.** A slot's `FVeyraInventorySlot` gains `Charges`. The rule is generic: a consumable whose `charges` is above 0 is refillable, and nothing names the Flask. It is bought full and never stacks, at most one per inventory, queued ones included (Item §10: "non-stackable"). Validation refuses a refillable consumable whose `stackLimit` is not 1.
- **Use.** Using it spends a charge and restores Health over time through the same restoration as Field Tonic. One restoration runs at a time (ADR-012 §9.3): while one runs, the key does nothing and spends nothing. An empty one keeps its slot and refuses with `NoCharges`; the HUD shows its charges, even none.
- **Refill to full** (`UVeyraShopSubsystem::RefillCharges(Participant)`):
  - on arrival at the side's fountain, respawn and death included (the dead shop as at the fountain), through the shop's `SetAtFountain`. Standing there does not refill again; arriving does;
  - when the holder's side secures a Flux Well. Match's battleground link calls it for each of the side's participants from the Well-secured event.

### 7. Bots

Bots do not use Actives or consumables (ADR-013, unchanged). Their builds (`Bots.json`):
- **Marksmen** (Kade, Vera, Bryn) build Deadeye Edge and then Sovereign Edge in place of their second Physical Masterwork.
- **Mages** keep Crown of the Crucible first, then take Starfall Prism or Nullglass Lens where Arc Reactor stood second.
- **Fighters** swap one Masterwork for Razorwheel Prime or Impact Aegis.

The rule that a build never lists an item another consumes still holds.

### 8. Values (all Provisional; the shop's prices follow ADR-012 §10's scale)

| Item or rule | Values |
|---|---|
| Keensteel | 400 Gold; Crit Chance 0.15 |
| Deadeye Edge | + 350 recipe; Physical Power 20, Crit Chance 0.2 |
| Sovereign Edge | + 700 recipe; Physical Power 60, Crit Chance 0.25; Perfect Cut +0.4 Crit Damage |
| Arcane Boots | + 700 recipe; Movement Speed 45, Magic Power fraction 0.08 |
| Flux Flask | 150 Gold; 2 charges; 90 Health over 12 s each; resale 0.4 |
| Impact Aegis | + 800 recipe; Health 300, Physical Power 30, Ability Haste 15; Reprisal Guard 0.2 of the hit, cap 250, lasting 3 s, cooldown 8 s |
| Gravitic Seal | + 750 recipe; Magic Power 60, Health 250, Ability Haste 15; Drag 30% for 1 s |
| Starfall Prism | + 800 recipe; Magic Power 90, Ability Haste 20; Convergence window 4 s, 50 + 0.15 Magic Power |
| Nullglass Lens | + 750 recipe; Magic Power 80, Magic Penetration 15; Fracture 5% per stack, 5 stacks, 4 s |
| Razorwheel Prime | + 800 recipe; Physical Power 45, Health 250, Attack Speed 0.2; Endless Cleave 0.4 melee, 0.2 ranged, radius 300; Seize Momentum radius 450, 1.0 Physical Power, 30% slow and 8% Movement Speed a Vanguard (cap 24%) for 3 s, cooldown 25 s |
| Siegeheart Core | + 800 recipe; Health 650, Health Regeneration 3, Ability Haste 15; Tempered after 3 s within 700; 50 + 0.08 of Max Health; 0.1 of it as permanent Max Health; 30 s per enemy |
| Crit | damage 1.75, cap 1.0, overflow 0.5 (Canon, Combat §5) |

### 9. League answers where canon is silent (for the author to overturn)

1. **Crit randomness:** a plain roll per attack from a per-match server stream. League's pseudo-random distribution, which evens out streaks, stays open.
2. **Crit and structures:** crits apply to towers and the Prime Well, as League's do, with their bonus at Structure Effectiveness (Combat §33, canon).
3. **Arcane Boots' amplification** is a percentage of Magic Power, the model Item §5 leaves open. League's Sorcerer's Shoes give flat Magic Penetration instead. The percentage keeps the bible's words; playtest decides.
4. **Flux Flask** as League's Refillable Potion: 2 charges, refilled at the fountain (respawn included) and, per canon, by a secured Flux Well.
5. **Tempered by Conflict** as League's Heartsteel: proximity for 3 s, a charged hit, permanent Max Health lost on selling, 30 s per enemy.
6. **Endless Cleave** as League's Hydra: centred on the target, reduced for ranged holders, never on structures.
7. **Seize Momentum** as League's Stridebreaker: only Vanguards are slowed, and minions take its damage.
8. **Reprisal Guard** counts damage absorbed by shields and never counts structures; its cooldown is global.
9. **Fracture** as League's Abyssal Mask: percentage stacks refreshing together.
10. **Drag** as League's Rylai's Crystal Scepter: every damaging ability slows, damage over time and areas included.
11. **Convergence** is primed and consumed only by direct ability damage, never by damage over time or Proc.

## Consequences

- The Item Bible's catalog is complete (37 items), and marksmen have their crit identity.
- **One random source in combat.** A replay reproduces crits only with the stream's seed, which the match records in its log line.
- `Items.json` grows two stats on every item, a `charges` field for the Flask, and six Attunement maps. `check_tuning.py` and the items rules learn the new maps.
- The dealt-damage event gives later on-damage effects (Lifesteal, Omnivamp, healing reduction) the hook they need.

## Amendments to earlier records

- **ADR-009:** the `OnDamageDealt` event, and the Magic Resist Reduction status.
- **ADR-012:** §1's deferred items arrive. Lifesteal, Omnivamp, Item Haste and Tier 4 remain deferred.
- **ADR-013:** the bots' builds include the new items (§7).

## Open items

- A pseudo-random distribution for crits (League's).
- Lifesteal, Omnivamp and healing reduction items: the Item Bible's §12 families.
- Armor and Magic Resist items (§12).
- Damage numbers and a crit statistic on the scoreboard.
