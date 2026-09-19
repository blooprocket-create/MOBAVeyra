# Veyra Combat Bible

**Version:** 0.3  
**Status:** Working combat canon for the first playable prototype.  
**Scope:** Vanguard combat rules, damage resolution, targeting, control, movement interactions, statuses, timing, and structure combat.  
**Tuning rule:** Numerical values identified as prototype placeholders must remain data-driven.

## 1. Core combat principles

- Veyra uses familiar competitive-MOBA combat language where that improves readability.
- Rules must be explicit in Veyra documentation; implementation must never depend on "works like another game" as a specification.
- Server-authoritative gameplay owns final combat outcomes.
- Combat effects use reusable primitives rather than champion-specific copies of core logic.
- Full internal precision is retained through calculations. UI may round for presentation.
- There is no universal minimum damage of 1. A final resolved damage value may legitimately be 0.
- Visual telegraphs and VFX must closely represent real gameplay hitboxes, ranges, and danger areas.

## 2. Damage types and tags

Veyra has three primary damage types:

- **Physical Damage** — mitigated by Armor.
- **Magic Damage** — mitigated by Magic Resistance.
- **True Damage** — ignores Armor and Magic Resistance.

True Damage does **not** bypass shields.

Damage events may also carry descriptive tags such as:

- Basic Attack
- Ability
- Item
- DoT
- AoE
- Projectile
- Reflected
- Redirected

These tags do not create new damage types. A damage event may simultaneously be Magic + Ability + DoT + AoE, for example.

A single ability may deal multiple damage types when explicitly designed to do so. Each component is resolved independently.

## 3. Armor and Magic Resistance

Positive Armor and Magic Resistance use the same mitigation formula:

```text
Damage Taken = Raw Damage × 100 / (100 + Resistance)
```

Examples:

- 0 resistance → 100% damage taken
- 50 → ~66.7%
- 100 → 50%
- 200 → ~33.3%
- 300 → 25%

Negative Armor/MR is allowed and increases damage taken using:

```text
Damage Taken = Raw Damage × (2 - 100 / (100 - Resistance))
```

Examples:

- -25 → 120%
- -50 → ~133.3%
- -100 → 150%
- -200 → ~166.7%

Negative resistance approaches, but does not exceed, 200% damage taken from resistance alone.

Armor and MR are fully separate channels. Armor effects never modify MR unless an effect explicitly says so, and vice versa.

### Reduction and penetration order

The universal order is:

1. Flat Resistance Reduction
2. Percentage Resistance Reduction
3. Percentage Penetration
4. Flat Penetration
5. Final Armor/MR mitigation

Reduction changes the target's actual defensive stat and may push Armor/MR below 0.

Penetration is attacker-specific and cannot by itself push effective Armor/MR below 0. Penetration only bypasses existing positive resistance.

## 4. Basic attacks

A basic attack has:

1. Windup
2. Commit point
3. Recovery / backswing
4. Attack cooldown governed by Attack Speed

A target must be valid and in attack range when the attack begins and again at commit.

If the target leaves range before commit, the attack cancels.

Once a ranged basic attack commits and launches its projectile, leaving attack range does not cause the projectile to miss or disappear.

Ranged attacks normally use projectiles. Melee attacks resolve at the appropriate committed melee hit point.

Basic attacks deal Physical Damage by default unless the Vanguard explicitly overrides that rule.

Different Vanguards may have different base Attack Speed, windup proportions, attack ranges, and projectile speeds.

## 5. Critical strikes

Basic attacks are normally crit-eligible.

Prototype baseline:

- Normal Crit Damage: **175%**
- Effective Crit Chance caps at **100%**

Abilities do not crit unless explicitly allowed.

On-Hit effects are not automatically multiplied by Crit Damage unless explicitly permitted.

### Crit overflow

Crit Chance above 100% converts into additional Crit Damage at:

**1% overflow Crit Chance → +0.5% Crit Damage**

Examples using the current 175% baseline:

- 120% listed Crit → 100% effective chance, 185% Crit Damage
- 150% → 200% Crit Damage
- 200% → 225% Crit Damage

## 6. Healing, regeneration, Lifesteal, and Omnivamp

### Healing

Normal healing restores current Health up to Max Health.

Normal healing never overheals. Excess healing is wasted.

Overheal must be an explicit mechanic, such as a Vanguard skill that converts excess healing into another resource or shield.

Healing, Health Regeneration, Lifesteal, and Omnivamp are distinct source categories even though all may restore Health.

### Lifesteal

Lifesteal heals from actual post-mitigation damage dealt by basic attacks.

Damage dealt to shields counts for Lifesteal.

Damage that is fully prevented generates no Lifesteal.

Overkill damage that was never actually dealt does not generate Lifesteal.

### Omnivamp

Omnivamp heals from actual damage dealt by attacks, abilities, DoTs, item damage, and other qualifying sources unless explicitly excluded.

Current prototype rule:

- Single-target direct damage → 100% Omnivamp effectiveness
- AoE damage → **44% effectiveness**
- DoT damage → **44% effectiveness**
- Damage that is both AoE and DoT applies the 44% modifier once, not twice

True Damage can generate Lifesteal/Omnivamp normally.

Normal Lifesteal/Omnivamp cannot overheal.

### Healing reduction

Standard Healing Reduction prototype value: **40%**

Healing Reduction affects:

- direct healing
- Health Regeneration
- Lifesteal
- Omnivamp
- other Health restoration unless explicitly excluded

Healing Reduction does not reduce shields.

Multiple ordinary Healing Reduction effects do not stack; the strongest active value applies.

## 7. Shields

Veyra supports:

- **Physical Shields** — absorb Physical Damage only
- **Magic Shields** — absorb Magic Damage only
- **Universal Shields** — absorb Physical, Magic, and True Damage

True Damage always goes through shields before Health.

### Shield priority

Use the most specialized eligible shield first:

- Physical Damage → Physical Shield before Universal
- Magic Damage → Magic Shield before Universal
- True Damage → Universal only

If multiple eligible shields of the same category exist, consume the oldest first unless explicitly overridden.

Different shields may coexist.

Reapplying the same named shield from the same source follows that effect's explicit refresh/replace rule rather than automatically stacking.

## 8. Crowd control

### Core CC types

- **Stun** — cannot move, basic attack, or cast.
- **Root** — cannot move or dash; may attack/cast if otherwise able.
- **Silence** — may move/basic attack; cannot cast abilities.
- **Disarm** — may move/cast; cannot basic attack.
- **Slow** — reduces Movement Speed.
- **Knockup** — forced airborne displacement/control.
- **Knockback** — forced movement away from a source/direction.
- **Pull** — forced movement toward a source/destination.
- **Polymorph** — may move, but cannot basic attack, cast abilities, use item actives, or use Flux Spells.
- **Suppression** — ultimate lockdown category.
- **Fear** — loses normal control and moves away from the fear source; cannot attack/cast.
- **Taunt** — loses normal control, moves toward the taunter if needed, and is forced to basic attack the taunter when able.
- **Charm** — loses normal control and moves toward the charm source; cannot attack/cast.
- **Sleep** — cannot move, attack, or cast once active; a specific Sleep may allow qualifying damage to wake the target early.
- **Blind** — combat impairment; basic attacks miss while active.

### Tenacity

Tenacity reduces the duration of:

- Stun
- Root
- Silence
- Disarm
- Polymorph
- Slow
- Fear
- Taunt
- Charm
- Sleep
- Blind

Tenacity does not reduce:

- Knockup
- Knockback
- Pull
- Suppression

Tenacity reduces Slow duration, not Slow magnitude.

Slow magnitude reduction belongs to a separate **Slow Resistance** mechanic.

Tenacity sources stack multiplicatively.

Prototype minimum duration for Tenacity-reducible CC: **0.3 seconds**.

### Cleanse

A normal Cleanse can remove:

- Stun
- Root
- Silence
- Disarm
- Slow
- Fear
- Taunt
- Charm
- Sleep
- Blind

A normal Cleanse cannot remove:

- Polymorph
- Knockup
- Knockback
- Pull
- Suppression

Polymorph requires an effect that explicitly says it can remove Polymorph.

Suppression is the strongest CC category. By default:

- Tenacity does not shorten it.
- Cleanse does not remove it.
- Ordinary CC immunity does not prevent it.
- Becoming immune after it lands does not break it.

Only a specifically designated mechanic may prevent or remove Suppression.

### CC immunity

Normal CC immunity prevents new ordinary CC from applying while active.

CC immunity does not automatically remove effects already present unless it also explicitly Cleanses.

Suppression ignores ordinary CC immunity unless a mechanic explicitly says it grants Suppression Immunity.

### No global CC diminishing returns

Veyra has **no universal diminishing-return system for repeated crowd control**.

Correctly chained CC receives full value.

If a team times Root → Stun → Polymorph → Knockup correctly and kills the target before they can act, that is a legitimate reward for coordination.

Overlapping duplicate CC may waste duration according to the stacking rules below, but there is no hidden anti-chain protection.

### Overlapping CC

Multiple instances of the same CC track independently. The target remains controlled until the longest remaining applicable instance expires.

Durations are not automatically added end-to-end.

Different CC types may coexist.

For Slows, the strongest active Slow controls Movement Speed; weaker Slows remain tracked and can take over after the stronger one expires.

## 9. Forced movement and mobility

### Unstoppable, Uninterruptible, Guaranteed Resolution

These are separate concepts.

- **Unstoppable** — ordinary CC cannot affect the Vanguard during the protected state. It does not guarantee the current action completes.
- **Uninterruptible** — the current action/cast cannot be cancelled by ordinary interruption.
- **Guaranteed Resolution** — once an ability reaches its defined committed point, its payoff resolves unless a specifically stronger exception prevents it.

Movement-heavy abilities should explicitly declare phases such as:

Windup → Commit → Movement/Carry → Impact

### Displacement

Forced displacement temporarily owns the target's movement.

Normal movement commands do not override it.

If a second valid displacement lands while another is resolving, the newer displacement replaces the remaining movement of the previous displacement unless the active displacement is explicitly uninterruptible.

Terrain stops ordinary displacement at the nearest legal point.

Displacement can never place a Vanguard inside impassable terrain.

Displacement does not inherently deal damage.

### Dashes

A Dash moves through space over time.

Dashes may be interrupted by valid displacement unless protected.

A Dash explicitly declares whether terrain blocks it, whether it may cross terrain, and where it may legally end.

### Blinks

A Blink is instantaneous relocation with no travel path once resolved.

The destination must be legal or the ability must define how invalid destinations are corrected.

### Lunges / advances

Short attack- or ability-linked advances use Dash rules unless explicitly overridden.

Movement abilities do not inherently grant Untargetability or Invulnerability.

## 10. Targetability, Invulnerability, and Stasis

### Untargetable

While Untargetable:

- enemies cannot acquire the Vanguard as a new target;
- new attacks and abilities cannot hit them;
- targeted projectiles already traveling toward them fail if the Vanguard is still Untargetable when the projectile would connect;
- skillshots and AoEs do not hit during the Untargetable window.

Untargetability does not automatically Cleanse existing effects.

Existing DoTs may continue ticking unless the specific Untargetable state also prevents their damage.

### Invulnerable

An Invulnerable Vanguard may remain targetable and may still receive non-damage interactions unless otherwise stated.

Physical, Magic, and True Damage reduce Health by 0 during Invulnerability.

### Stasis

While in Stasis, the Vanguard:

- cannot move;
- cannot basic attack;
- cannot cast abilities, item actives, or Flux Spells;
- is Untargetable;
- cannot take damage, including True Damage;
- cannot be affected by new CC or displacement;
- cannot receive new healing or shields unless explicitly allowed.

Existing status durations continue ticking normally during Stasis.

Existing DoTs remain attached, but their damage ticks deal 0 while Stasis is active.

Stasis cannot normally be cancelled early unless explicitly allowed.

## 11. Stealth and vision interaction

### Camouflage

Camouflaged Vanguards are hidden beyond a detection radius.

Enemies sufficiently close may reveal Camouflage according to the effect's rules.

### Invisibility

Invisible Vanguards remain hidden regardless of ordinary proximity unless revealed by True Sight or another specifically valid reveal mechanic.

### General stealth rules

- Taking damage does not automatically break stealth unless the stealth effect says so.
- Attacking or casting an offensive ability normally breaks stealth unless explicitly allowed.
- A reveal may expose a stealthed Vanguard without cancelling the stealth buff itself.
- Ordinary wards do not automatically reveal Invisibility.
- True Sight reveals valid Camouflaged/Invisible targets within its area.
- Stealth does not make a Vanguard Untargetable.
- Targeted projectiles already launched do not disappear because the target enters stealth.
- AoEs, skillshots, and other non-targeted effects may hit stealthed/camouflaged targets without seeing them.
- Being hit by a non-targeted effect does not automatically reveal the target unless that effect says so.

### Dense Fog hierarchy

Dense Fog and stealth are separate systems.

Dense Fog's direct-vision rules still apply even against reveal/stealth systems unless an effect explicitly overrides Dense Fog.

Non-targeted attacks may be blind-cast into Dense Fog and can hit enemies inside.

## 12. Ability targeting classes

Common targeting categories:

- **Targeted** — requires a valid target.
- **Skillshot** — aimed path/shape; may hit unseen targets by collision.
- **Ground-targeted** — resolves at a chosen valid location.
- **Self-centered / Aura** — originates from the caster and affects valid units in its area.

Each ability separately declares:

- projectile vs instant;
- collision rules;
- eligible unit classes;
- stop/pierce behavior;
- special targeting exceptions.

## 13. Projectiles, AoEs, and hit detection

A projectile defines speed, width/radius, collision profile, and target eligibility.

It explicitly declares whether it:

- stops on the first valid unit;
- pierces a fixed number;
- pierces indefinitely;
- ignores specific unit classes.

Targeted projectiles follow their already-acquired target.

Entering stealth/camouflage after launch does not break them.

Untargetability or Stasis may cause a targeted projectile to fail when it would connect.

Skillshot projectiles use collision and do not require vision.

AoEs evaluate gameplay hitboxes against the effect's defined shape at each resolution point.

### Gameplay hitboxes

Gameplay collision/hurtboxes are authoritative, not the visual model.

Weapons, capes, horns, tails, animation poses, and decorative geometry do not arbitrarily enlarge the gameplay hurtbox.

### Visual fidelity rule

The visible danger area must closely match the actual gameplay collision area.

If a skillshot has intentionally forgiving collision, its VFX/telegraph should communicate that forgiveness.

A replay viewed frame-by-frame should still make the hit look believable.

Development tooling should support visible collision/hurtbox debugging.

## 14. Damage-over-time and status effects

A DoT defines:

- source
- damage type
- duration
- tick interval
- stacking policy
- refresh policy

Rules:

- DoTs do not crit unless explicitly allowed.
- Reapplying the same DoT from the same source normally refreshes duration unless the effect explicitly stacks.
- Different sources may maintain independent instances.
- A DoT does not automatically gain a free instant tick on application.
- No partial expiration tick exists unless explicitly designed.
- Untargetability does not stop an already-attached DoT.
- Invulnerability reduces DoT damage to 0 while active.
- Stasis keeps the DoT attached and duration running, but ticks deal 0.
- Ordinary temporary DoTs are removed on death.

### Snapshot rule

By default, DoTs snapshot the attacker's relevant offensive values when applied.

Each tick uses the target's **current** defenses when the tick resolves.

An effect may explicitly opt into dynamic scaling.

## 15. Generic damage amplification and reduction

Generic Damage Amplification and generic Damage Reduction are separate from Armor/MR.

Multiple percentage modifiers stack multiplicatively.

Ordinary generic Damage Amplification and Damage Reduction affect Physical and Magic Damage.

They do **not** affect True Damage unless explicitly stated.

## 16. Combat trigger language

### On Attack

Triggers when a basic attack is successfully committed.

### On Hit

Triggers when a basic attack successfully connects with a valid target.

### On Damage

Triggers when a damage event successfully deals damage to Health or a shield.

### On Health Damage

Triggers only when damage actually removes Health.

### On Ability Hit

Triggers when an ability successfully connects with a valid target even if the ability's damage is prevented.

Proc-generated damage does not automatically recursively trigger the proc that created it.

## 17. Empowered attacks and attack resets

An Empowered Basic Attack remains a basic attack unless explicitly stated otherwise.

It may normally:

- crit;
- trigger On Attack;
- trigger On Hit;
- apply Lifesteal;
- use normal attack targeting rules.

Added effects do not automatically inherit Crit or Lifesteal unless explicitly permitted.

A full Attack Reset clears the remaining basic-attack cooldown and allows the next attack to begin immediately. It does not skip the next attack's windup.

A Partial Attack Reset reduces the remaining attack cooldown by a defined amount/percentage.

Activating an Attack Reset before the prior attack commits normally cancels that prior attack unless explicitly preserved.

## 18. Death, kills, assists, executes, and revives

A Vanguard actually **dies** only when the lethal result is finalized into the game's death/respawn state. A lethal hit intercepted by Death Prevention or a revival-style save is **not a death**, even if the effect briefly plays a collapse, downed, or reconstitution animation. A Vanguard who is saved never enters the death timer.

### Kill credit

Kill credit normally goes to the source of the lethal damage event **when it results in a finalized, actual death**. Merely reaching a would-be lethal threshold before a save effect intercepts it does not grant kill credit.

DoTs, items, summons, companions, and other owned entities trace attribution back to the responsible Vanguard where applicable.

If the environment finishes a target, an enemy Vanguard may still receive kill credit if they meaningfully contributed within the configured kill-credit window. Otherwise the death is an Execution/environmental death with no enemy killer.

### Assists

Assist credit requires meaningful contribution during the configured assist window, such as:

- damage;
- CC;
- meaningful debuff;
- meaningful reveal;
- healing/shielding/buffing an ally who participated.

Merely standing nearby does not grant an assist.

Damage to shields counts as combat participation.

A dead Vanguard may still receive kill/assist credit from effects applied before death.

Current prototype assist-window starting point: **10 seconds**, data-driven.

### Trigger terminology

- **Kill** — receives actual kill credit.
- **Assist** — qualifies for assist credit.
- **Takedown** — Kill or Assist.
- **Solo Kill** — kill credit with no allied assist.
- **Execution** — death with no enemy Vanguard kill credit.
- **On Death** — triggers when the unit actually dies.

### Death prevention and revival-style saves

Death Prevention intercepts a qualifying lethal event **before actual death is finalized**. A simple effect may leave the Vanguard at 1 Health. A more elaborate **revival-style save** (for example, a Guardian Angel-type item or timed ally save) may put the Vanguard into a temporary reconstitution, downed, or revival animation and restore Health afterward. **Both are death prevention, not death followed by respawn.**

While a revival-style save succeeds:

- The Vanguard does **not** die or enter the ordinary death/respawn timer.
- The enemy receives **no Kill, Assist, Takedown, kill Gold, kill XP, First Blood, or bounty payout** from the prevented lethal event.
- The Vanguard gains **no death count, death-streak increment, or bounty reset**.
- **On Death** and **On Kill/Takedown** effects do not trigger from the prevented lethal event.
- Death-only status cleanup does not happen merely because the saving animation plays; the saving effect explicitly defines its temporary combat state, its allowed incoming effects, and what it removes or retains.
- The saving effect defines its protection window, interruption rules (if any), restore timing, restored Health/resources, and cooldown.
- If enemies actually kill the Vanguard after the save finishes or ceases protecting them, **that subsequent actual death** resolves normal kill/assist rewards, bounty rules, death statistics, and respawn. It is not a "second death" payout.

Multiple available Death Prevention effects resolve by explicit priority. A successful save consumes only the effect(s) specified by that priority/ability behavior; it must not generate multiple death or kill events.

### Execute

An Execute is a specifically tagged lethal mechanic, not merely high missing-Health damage.

Execute thresholds use actual Health, not Health plus shields.

A true Execute may kill through shields once its threshold is satisfied.

Ordinary Executes still respect Death Prevention unless they explicitly bypass it.

### Terminology: save versus respawn

In current Veyra design, a combat **revive** means a **revival-style save before actual death**, as defined above. Its animation may resemble dying and getting back up, but no death or kill reward has occurred.

**Respawn** happens only after an actual death, following that Vanguard's death timer at the fountain. It does not undo the prior kill, assist, bounty payout, or death events.

A future mechanic that explicitly resurrects a Vanguard **after an actual finalized death**, bypassing or shortening the ordinary death timer, is not presently part of this system. If designed later, it must be specified separately rather than treated as an ordinary combat revive.

## 19. Spell Shields

A Spell Shield blocks the next eligible hostile ability hit entirely.

When a Spell Shield triggers:

- the blocked hit deals no damage;
- applies no associated CC/debuff;
- does not count as a successful hit on that target.

Basic attacks do not normally consume Spell Shields.

An empowered attack may have a basic-attack component and a separately blockable ability component.

Targeted abilities, skillshots, and AoEs can all consume a Spell Shield if they successfully hit.

Polymorph and Suppression may be blocked by a Spell Shield unless the specific ability explicitly bypasses Spell Shields.

For a multi-hit ability, the first eligible hit consumes the Spell Shield; later hits may then affect the Vanguard.

Existing DoTs are not removed when a Spell Shield appears.

## 20. Projectile interception

Projectile Interception is separate from Spell Shields.

A world-space projectile blocker may intercept eligible:

- targeted projectiles;
- skillshot projectiles.

It does not inherently block:

- melee attacks;
- ground-targeted effects;
- persistent AoEs;
- instant/hitscan effects that are not tagged as projectiles.

Projectile blockers may be stationary world barriers or directional guards attached to a Vanguard.

A blocked projectile is destroyed at the interception point.

Multi-projectile abilities resolve each projectile independently.

A blocked projectile does not count as hitting its intended target.

Projectile Immunity and Projectile Reflection are separate mechanics and must be explicitly defined.

## 21. Haste and cooldowns

### Ability Haste

```text
Cooldown Multiplier = 100 / (100 + Ability Haste)
```

Ability Haste affects Vanguard ability cooldowns unless an ability explicitly opts out.

Gaining/losing Haste while an ability is cooling down recalculates the remaining cooldown proportionally.

### Item Haste

Item Haste uses the same formula.

It affects item actives and item-owned internal cooldowns unless explicitly excluded.

Ability Haste and Item Haste do not cross-apply.

### Flux Spells

Flux Spells have fixed cooldowns.

There is no Flux Haste stat.

Ability Haste and Item Haste do not modify Flux Spell cooldowns.

### Cooldown refunds

Explicit flat/percentage cooldown refunds are separate from Haste.

## 22. Attack Speed

Normal Attack Speed cap: **2.5 attacks per second**.

A Vanguard ability may explicitly raise the current Attack Speed cap temporarily.

The normal 2.5 cap remains the permanent reference point for overflow calculations even while a temporary cap increase allows higher actual Attack Speed.

### Overflow Attack Speed

Attack Speed above the normal 2.5 reference contributes bonus basic-attack damage with diminishing returns.

Current prototype formula:

```text
Overflow Basic-Attack Damage % =
132 × Overflow AS / (200 + Overflow AS)
```

Where **Overflow AS** is the percentage of uncapped Attack Speed that would exceed the normal 2.5 attacks/sec reference.

Examples:

- 10% overflow → ~+6.3% basic-attack damage
- 30% → ~+17.2%
- 50% → +26.4%
- 100% → +44%
- 200% → +66%
- 400% → +88%

A temporary skill that raises the current cap to 4.0 attacks/sec does **not** remove the overflow damage benefit. A Vanguard may simultaneously attack above 2.5 and receive overflow damage based on the same underlying uncapped Attack Speed.

Overflow damage amplifies the base basic-attack damage event, not separate On-Hit effects by default.

Minimum ordinary Attack Speed: **0.2 attacks/sec**.

## 23. Movement Speed

Movement Speed is calculated from base/flat values, percentage modifiers, and then soft caps.

Current prototype soft caps:

- Up to 415 → full value
- 415 to 490 → 80% of additional value
- Above 490 → 50% of additional value

There is no ordinary hard maximum.

Specific abilities may explicitly bypass soft-cap behavior.

Slows reduce Movement Speed.

Strongest active Slow controls the result; weaker Slows remain tracked.

Tenacity reduces Slow duration.

Slow Resistance reduces Slow magnitude.

Dashes, Blinks, and forced displacement ignore ordinary Movement Speed unless explicitly scaled by it.

Minimum Movement Speed from ordinary slowing: **100**.

Explicit hard CC such as Root/Stun may reduce effective movement to 0.

## 24. Unit collision and Ghosting

Enemy Vanguards have unit collision and can body-block.

Allied Vanguards do not hard-body-block one another; pathing should use soft separation so allies cannot permanently trap teammates.

Enemy Fluxborn have unit collision.

Allied Fluxborn have collision but pathing should aggressively avoid trapping allied Vanguards.

Jungle wildlife has collision while valid/alive.

**Ghosted** means ignoring unit collision, not terrain.

Server-side unstuck/separation logic should prevent permanent embedding from displacement, spawning, or latency.

## 25. Canonical damage-resolution pipeline

Every damage event uses the same order:

1. Validate the hit.
2. Build the raw damage event.
3. Apply source-side generic Damage Amplification.
4. Resolve resistance reduction/penetration.
5. Apply Armor/MR mitigation. True Damage skips this step.
6. Apply target-side generic Damage Reduction. Ordinary reduction does not affect True Damage.
7. Check Invulnerability.
8. Apply eligible shields using shield-priority rules.
9. Apply remaining damage to Health.
10. Resolve On Damage / On Health Damage and other eligible triggers.
11. Check eligible Death Prevention / revival-style saves against any lethal result (including Executes), then finalize actual death and kill/assist attribution **only if no valid save prevents the death**.

Multi-type damage components resolve independently through their appropriate mitigation channels.

No intermediate rounding is performed.

A final result of 0 damage is valid.

## 26. Casting, channels, charges, and interruptions

### Instant Cast

Resolves immediately when activated, subject to its defined action lock.

### Cast-Time Ability

Has a windup and a defined Commit point.

If interrupted before Commit:

- the effect fails;
- no resource is spent;
- the ability enters cooldown at **20% of its normal cooldown** (an 80% failed-cast cooldown reduction).

### Channel

Continues over time while maintained.

The ability explicitly declares whether movement, attacks, casts, and specific CC interrupt it.

Channels may use an upfront resource cost or a resource drain over time.

### Charged Ability

Has explicit charge and release phases.

The ability defines how range/power/area scale and what interruption does to the charge.

### General timing rules

- Every ability has an explicit Commit point.
- Costs are normally paid at Commit.
- Cooldown normally begins at Commit unless a special rule applies.
- Effects already spawned after Commit are not retroactively erased by ordinary interruption.
- Uninterruptible and Unstoppable are separate.
- Brief input buffering is allowed near the end of action lockouts but never bypasses cooldown, CC, targeting, or resource restrictions.

## 27. Resource costs

An ability checks resource availability when the cast begins.

The resource is normally spent at Commit.

Resources do not go negative unless a mechanic explicitly supports debt/overdraft.

Interrupted pre-Commit Cast-Time abilities pay no resource cost.

Resource-less Vanguards simply skip the resource check.

Costs may be:

- flat;
- percentage of Max Resource;
- percentage of Current Resource;
- another explicitly defined formula.

Cost reduction cannot reduce a cost below 0.

Refunds are separate effects after spending.

The server validates resource availability and spending.

## 28. Combat State

A Vanguard enters Vanguard Combat State when they:

- damage an enemy Vanguard;
- take damage from an enemy Vanguard;
- apply meaningful hostile CC/debuff to an enemy Vanguard;
- meaningfully heal/shield/buff an allied Vanguard actively participating in Vanguard combat.

Prototype out-of-combat delay: **5 seconds**, data-driven.

Base resource regeneration normally continues both in and out of combat unless a Vanguard resource says otherwise.

PvE damage from Fluxborn, structures, or wildlife does not automatically count as Vanguard Combat State unless a specific system needs that distinction.

Lingering DoTs may keep the participants in combat.

Stasis does not clear Combat State.

Death clears Combat State.

## 29. Friendly fire and team targeting

Vanguards cannot normally damage or negatively CC allies.

Offensive abilities treat allied Vanguards as invalid hostile targets unless explicitly overridden.

Beneficial effects target allies/self according to their own rules.

No friendly-fire damage exists by default, including True Damage.

Neutral wildlife, Fluxborn, structures, and other unit classes are explicit targeting categories rather than implicit "enemy" catch-alls.

## 30. Target validity and range

Basic attacks require valid range at attack start and Commit.

Targeted Instant abilities validate target/range when they resolve.

Targeted Cast-Time abilities require a valid target at cast start and again at Commit.

If a Cast-Time target leaves range, dies, becomes Untargetable, enters Stasis, or otherwise becomes invalid before Commit, the cast fails using the normal interrupted-Cast-Time rule:

- 20% normal cooldown;
- no resource spent.

Skillshots and ground-targeted abilities validate direction/location/cast range rather than requiring an enemy target.

Once a projectile or persistent effect is spawned, it uses its own collision/area rules.

Server range validation may include small latency tolerance but should not produce visibly unbelievable connections.

## 31. Blind

While Blinded, a Vanguard may move, cast, and attempt basic attacks.

Basic attacks miss.

A missed basic attack:

- performs its normal windup;
- consumes the attack cycle;
- may trigger On Attack;
- does not trigger On Hit;
- does not deal damage;
- does not generate Lifesteal;
- does not deal Crit damage.

Empowered basic attacks follow the same rule for their basic-attack-dependent components.

Blind does not affect ordinary abilities/skillshots.

Blind is Tenacity-reducible and normally Cleanseable.

Veyra has no generic passive Dodge/Evasion stat by default.

## 32. Summons, companions, clones, and decoys

Owned combat entities have an owner Vanguard and an explicit classification.

- **Companion** — persistent/semi-persistent combat entity with its own declared rules.
- **Summon** — temporary combat unit.
- **Clone** — separate entity that explicitly defines which owner properties it copies.
- **Decoy** — entity primarily used for deception/interaction.

Owned entities do not automatically inherit:

- item effects;
- On-Hit effects;
- Crit;
- Lifesteal;
- Omnivamp;
- buffs.

Any inheritance must be explicit.

Owned damage/CC/kill attribution traces back to the responsible Vanguard.

Killing an owned entity does not count as killing the owning Vanguard unless explicitly linked.

Owner death behavior is per-entity.

Ownership ultimately resolves back to a Vanguard or world system; recursive summon ownership does not create a new attribution root.

## 33. Structures and combat

Structures take damage primarily from basic attacks.

### Primary Damage Type conversion

Every Vanguard has an explicit **Primary Damage Type**.

When basic attacking a damageable structure:

- Physical-primary Vanguards use their Physical Power for the structure attack.
- Magic-primary Vanguards use their Magic Power for the structure attack.

The structure attack then deals the corresponding Physical or Magic damage and checks the structure's matching defensive stat.

The engine must not guess a Vanguard's Primary Damage Type based on whichever stat happens to be higher at the moment.

### Structure Effectiveness

Current prototype **Structure Effectiveness** for secondary attack riders: **50%**, data-driven.

The underlying converted basic-attack damage remains at full effectiveness.

At current prototype tuning:

- Crit bonus damage → 50% effectiveness
- offensive On-Hit damage → 50%
- empowered basic-attack bonus damage → 50%
- Lifesteal generated from structure attacks → 50%

Utility CC/debuffs do not affect structures unless explicitly allowed.

### Abilities vs structures

Normal abilities do not damage structures.

Exceptions:

1. Empowered-basic-attack abilities, because they modify a basic attack.
2. Abilities explicitly flagged as able to damage structures.

A structure-enabled ability explicitly defines whether it uses standard Structure Effectiveness or its own structure ratio.

### Structure defenses

Structures have their own Armor and MR.

Normal Vanguard Armor/MR reduction and penetration do not affect structures unless specifically flagged as structure-enabled.

### Spire aggro

Corrupted Spires normally prioritize hostile Fluxborn over hostile Vanguards.

If an enemy Vanguard damages an allied Vanguard within the Spire's protection, the attacker receives an Aggression Mark and the Spire immediately prioritizes that Vanguard even while Fluxborn are present.

The Aggression Mark persists briefly so trivial step-outs do not instantly reset aggression.

Summons/companions require explicit structure-priority classifications.

### Ramping Spire damage

Current prototype:

Each consecutive Spire hit on the same Vanguard increases the next hit's damage by **20%**, up to 5 ramp stacks:

100% → 120% → 140% → 160% → 180% → 200%

Ramp resets when:

- the Spire switches targets;
- the Vanguard leaves range;
- the target becomes invalid;
- the reset timer expires.

Prototype reset timer: **3 seconds**, data-driven.

Ramp is tracked per Spire, per Vanguard.

## 34. Percent-Health damage

Veyra supports:

- Max-Health damage
- Current-Health damage
- Missing-Health damage

Percent-Health damage may be Physical, Magic, or True and follows that damage type's normal rules.

Shields do not increase Max/Current/Missing Health calculations.

Percent-Health damage still hits shields normally after its amount is calculated.

Percent-Health damage does not crit unless explicitly allowed.

Non-Vanguard targets may have explicit percent-Health damage caps.

Percent-Health damage does not affect structures unless explicitly structure-enabled.

If Max Health changes, future percent-Health calculations use the new Max Health.

Missing-Health calculations use actual missing Health, not missing effective Health including shields.

## 35. Simultaneous damage and same-step resolution

Damage events validated for the same server simulation step may resolve from the same starting state before death cleanup.

True simultaneous lethal trades are allowed.

If two Vanguards deal lethal damage to one another in the same simulation step, both may die.

Existing shields at the start of the step apply normally.

Same-step heals/shields may save a target only if their explicit resolution timing precedes lethal cleanup.

Effects created by one event do not retroactively protect against already-resolved events in the same batch unless specifically designed to do so.

## 36. Damage reflection

Reflected damage is a secondary damage event with a **Reflected** tag.

Reflected damage:

- cannot itself trigger further reflection;
- does not generate Lifesteal/Omnivamp by default;
- may be Physical, Magic, or True;
- follows the normal damage pipeline for its type;
- does not normally trigger recursive damage-proc chains unless explicitly allowed.

Proportional reflection is calculated from post-mitigation damage actually received by shields/Health, not raw pre-mitigation damage.

## 37. Damage redirection and sharing

A redirect/share effect splits an incoming damage event after source-side calculation but before each recipient's final target mitigation.

Example:

100 Magic damage with 30% redirect becomes:

- 70 Magic damage event to original target
- 30 Magic damage event to protector

Each recipient then uses their own MR, damage reduction, shields, Invulnerability, etc.

Rules:

- redirected damage keeps the original damage type;
- redirected damage is tagged Redirected;
- redirected damage cannot normally be redirected again;
- redirection does not duplicate On-Hit/On-Ability-Hit against the protector;
- On Damage/On Health Damage may trigger on the recipient who actually receives damage;
- Lifesteal/Omnivamp is based on actual total damage caused, not duplicated by the split;
- kill attribution remains with the original attacker;
- True Damage may be redirected and remains True Damage.

## 38. Dispels and Purges

- **Cleanse** — handles allowed CC categories.
- **Dispel** — removes ordinary negative status effects that are tagged Dispellable.
- **Purge** — removes ordinary positive effects from an enemy that are tagged Purgeable.
- **Protected** — ordinary Dispel/Purge cannot remove the effect.

Shields individually declare whether they are Purgeable.

Removing a DoT/status ends future effects but does not undo past damage.

Dispelling a stackable status removes the full status by default unless the dispel explicitly removes individual stacks.

Polymorph and Suppression remain governed by their stronger explicit rules.

## 39. Stat floors and limits

### Health

Current Health floor: **0**

Max Health cannot be 0 or negative under normal gameplay.

### Resources

Normal resource floor: **0**

### Armor / MR

No hard negative floor.

### Attack Speed

- ordinary maximum: 2.5 attacks/sec;
- temporary cap-breaking abilities may raise the current cap;
- ordinary minimum: 0.2 attacks/sec.

### Movement Speed

- no ordinary hard maximum;
- soft caps apply;
- ordinary slow floor: 100;
- explicit hard CC may reduce effective movement to 0.

### Crit

Crit Chance cannot fall below 0%.

Overflow above 100% follows the Crit overflow rule.

### Haste

Ability Haste and Item Haste have an ordinary floor of 0.

If Veyra later needs a mechanic that lengthens cooldowns, use a distinct cooldown-slow modifier rather than negative Haste.

Damage/healing calculations do not become negative unless an explicit conversion mechanic says so.

## 40. Range geometry

Targeted attack/cast range uses **edge-to-edge gameplay hitbox measurement** by default, not center-to-center.

AoEs and skillshots use their defined gameplay geometry against gameplay hitboxes.

Visual range indicators must closely match actual gameplay geometry.

## 41. Stat modifier order

For most stats:

1. Base stat
2. Flat bonuses
3. Percentage bonuses
4. Percentage reductions/debuffs

Multiple percentage modifiers stack multiplicatively unless explicitly stated otherwise.

This framework applies to Max Health, Physical Power, Magic Power, Armor, MR, resource maximums/regeneration, and other compatible stats.

Attack Speed and Movement Speed then continue through their own cap/soft-cap rules.

### Max Health changes

Changing Max Health preserves current Health percentage by default.

Example:

500 / 1000 Health → +20% Max Health → 600 / 1200

A Max Health increase does not implicitly heal beyond preserving percentage unless explicitly stated.

## 42. Damage-category immunity

Specific immunity is separate from full Invulnerability.

Possible categories include:

- Physical Damage Immunity
- Magic Damage Immunity
- Basic-Attack Damage Immunity
- Ability Damage Immunity
- explicitly defined True Damage Immunity

Category immunity prevents the relevant damage component, but does not automatically prevent attached non-damage effects.

Example: Magic Damage Immunity may reduce the damage portion of a Magic Root ability to 0 while the Root still applies unless the immunity also blocks the effect itself.

## 43. Tethers and links

A Tether is an active relationship between two entities.

Each Tether explicitly declares:

- maximum range;
- whether line of sight matters;
- whether terrain breaks it;
- Untargetable/Stasis behavior;
- death behavior;
- displacement behavior;
- break consequences.

Default rules:

- losing ordinary vision does not break an established Tether;
- entering stealth/camouflage does not break an established Tether;
- an active Tether grants persistent vision of the tethered target through ordinary fog of war, stealth, and camouflage;
- **Dense Fog overrides tether vision**;
- exceeding maximum tether range breaks the Tether unless the effect has an explicit grace rule;
- ordinary hostile Tethers break on Untargetability;
- ordinary Tethers break on Stasis unless explicitly allowed to persist;
- death breaks ordinary Tethers;
- displacement may naturally break a Tether by exceeding range;
- a Tether does not inherently restrain movement unless explicitly designed to do so.

## 44. Actual-death cleanup and save-state distinction

**Only finalized actual death** is a hard cleanup boundary for ordinary temporary combat effects. Entering a revival-style save animation or reconstitution state is **not** actual death and does not run the death-cleanup pipeline.

On death, ordinary temporary:

- buffs
- debuffs
- DoTs
- marks
- tethers
- shields
- CC
- temporary stat modifiers
- stealth states

are removed.

A **pre-death revival-style save** defines its own temporary protection, status handling, and restored Health/resources. It does not silently perform death cleanup, award a kill, clear a bounty, or create a respawn event.

A **real respawn** occurs after actual death and restores Health/resources according to respawn rules; the temporary statuses removed on death do not automatically return.

Cooldowns continue ticking through a saving animation, actual death, and respawn by default unless explicitly overridden.

Permanent match progression persists through actual death.

Effects that persist through actual death must explicitly declare that behavior.

## 45. Multi-hit abilities and proc frequency

Each hit/tick of a multi-hit ability is a real combat event for damage, mitigation, shields, and death checks.

Proc effects explicitly declare one trigger-frequency policy:

- Per Hit
- Per Target Per Cast
- Once Per Cast
- Internal Cooldown

Persistent zones count as one cast instance per placement.

Each cast receives a unique server-side **Cast ID** so later hits/ticks can be attributed to the same cast.

## 46. Generic buff/debuff stacking policies

Every status declares a stacking policy:

- **Unique — Refresh**
- **Unique — Replace Strongest**
- **Stacking**
- **Independent Sources**
- **Non-Stacking Protected**

Default rule:

The same named buff/debuff from the same source does not stack unless explicitly allowed.

Different effects may coexist and flow through the normal stat-modifier pipeline.

Statuses use stable internal IDs. Gameplay must not depend on display names.

## 47. Health Costs and Self-Damage

### Health Cost

A Health Cost is a resource payment, not damage.

It:

- ignores Armor/MR;
- does not interact with shields;
- does not trigger On Damage / On Health Damage;
- does not generate Lifesteal/Omnivamp;
- is not modified by damage amps/reductions.

By default, a Health Cost cannot reduce the Vanguard below **1 Health**.

### Self-Damage

Self-Damage is an actual damage event sourced from the Vanguard themselves.

It can interact with normal damage systems according to its definition, but:

- Self-Damage can never directly kill the Vanguard;
- Self-Damage cannot reduce the Vanguard below **1 Health**;
- Self-Damage never grants self kill credit.

A rare mechanic that intentionally allows self-killing must be an explicit exception rather than ordinary Self-Damage.

## 48. Action lockouts, recovery, and animation cancelling

Veyra uses a responsive competitive-MOBA action-flow model.

### Basic attacks

Basic attacks follow:

Windup → Commit → Backswing

Moving or issuing an incompatible action before Commit cancels the attack.

After Commit, the hit/projectile is secured.

Backswing may normally be cancelled by movement or another legal action, enabling standard attack-move/kiting behavior.

### Ability action locks

Normal Cast-Time abilities prevent incompatible actions until the relevant cast lock ends.

Whether movement is allowed during a cast is defined by the ability.

Instant abilities normally have little or no movement lock, but may still use a short cast lock for clean sequencing.

Channels explicitly define which actions they lock.

### Animation and gameplay timing

Animation length does **not** own gameplay timing.

Gameplay Commit and recovery timings are authoritative.

Animation should be synchronized to those gameplay timings.

A visual flourish must not secretly extend a gameplay lockout after recovery has ended.

Recovery/backswing may often be animation-cancelled after Commit.

Animation cancelling may never make an effect occur earlier than its gameplay Commit point.

### Input buffering

Inputs may be briefly buffered near the end of an action lockout so combat remains responsive.

Input buffering never bypasses:

- cooldowns;
- CC;
- resources;
- targeting rules;
- cast restrictions.

### Explicit exceptions

Abilities may explicitly declare behaviors such as:

- Can Cast While Moving
- Can Cast During Attack Recovery
- Uninterruptible
- No Recovery

These are explicit exceptions, not assumptions.

## 49. Current prototype tuning values

These values are intentionally data-driven and expected to change through testing:

- Normal Crit Damage: 175%
- Crit overflow conversion: 0.5 Crit Damage per 1 overflow Crit Chance
- Standard Healing Reduction: 40%
- Omnivamp AoE/DoT effectiveness: 44%
- Tenacity-reducible CC minimum duration: 0.3 sec
- Normal Attack Speed cap: 2.5 attacks/sec
- Minimum ordinary Attack Speed: 0.2 attacks/sec
- Overflow Attack Speed damage curve constant: 200
- Movement Speed soft caps: 415 / 490
- Minimum Movement Speed from ordinary slows: 100
- Cast-Time pre-Commit interruption cooldown: 20% of normal cooldown
- Vanguard Combat out-of-combat delay: 5 sec
- Assist window starting point: 10 sec
- Structure Effectiveness for secondary attack riders: 50%
- Spire ramp per consecutive hit: +20%
- Spire ramp maximum: 5 stacks
- Spire ramp reset timer: 3 sec

## 50. Delayed effects and snapshot timing

Unless an effect explicitly opts into dynamic scaling, a delayed attack/ability snapshots the source's relevant offensive values at **Commit / spawn time**.

Examples include:

- projectiles;
- delayed explosions;
- traps created by a cast;
- delayed detonations;
- other effects whose payoff occurs after the original cast commits.

The target's **current defensive state at impact** is used when the damage resolves.

This means:

- gaining Magic Power after a projectile is already committed does not retroactively strengthen that projectile;
- losing offensive stats after Commit does not weaken an already-created projectile;
- Armor/MR, shields, damage reduction, Invulnerability, and other target-side defenses are evaluated when the hit actually resolves.

DoTs keep their existing dedicated snapshot rule.

An effect may explicitly use dynamic source scaling if that behavior is part of its design.

## 51. Healing and shield amplification order

Healing and shield creation use a deterministic modifier order.

### Healing

1. Build base healing amount.
2. Apply source-side outgoing healing modifiers.
3. Apply target-side incoming healing modifiers.
4. Apply Healing Reduction.
5. Apply the final value to missing Health.
6. Discard ordinary excess healing unless an explicit Overheal mechanic exists.

Multiple percentage modifiers stack multiplicatively unless explicitly stated otherwise.

### Shields

1. Build base shield amount.
2. Apply source-side outgoing shield modifiers.
3. Apply target-side incoming shield modifiers.
4. Create the final shield.

Healing Reduction does **not** reduce shields.

Existing shield-consumption priority rules then govern how the shield is spent.

## 52. Damage conversion

Damage conversion occurs during raw damage construction, before resistance mitigation.

A conversion effect partitions an existing raw damage component into another damage type.

Example:

A 100 Physical damage event with 40% Physical-to-Magic conversion becomes:

- 60 Physical raw damage;
- 40 Magic raw damage.

Each resulting component then follows the canonical damage pipeline for its own damage type.

Rules:

- converted damage stops being its original damage type for mitigation and damage-type-specific modifiers;
- conversion does not duplicate damage;
- conversion does not inherently change the total raw damage amount;
- conversion cannot recursively convert the same damage component again unless the effect explicitly allows chained conversion;
- if multiple conversion effects exist, they resolve in a deterministic explicit priority order;
- True Damage is not converted by ordinary conversion effects unless explicitly stated.

## 53. Projectile reflection

Projectile Reflection is a specialized form of projectile interception.

A reflection effect must explicitly define:

- which projectiles are reflectable;
- reflected direction or target selection;
- whether reflection preserves or changes projectile speed/range;
- any exceptional behavior.

Default rules:

- the reflected projectile preserves its existing snapshotted damage payload and effect payload;
- ownership and combat attribution transfer to the reflecting Vanguard;
- kill/assist credit from the reflected projectile belongs to the reflector;
- the reflected projectile does not rescale from the reflector's offensive stats unless the reflection effect explicitly says so;
- a reflected projectile keeps its original damage type(s);
- a reflected projectile is tagged as **Reflected Projectile**;
- a projectile may be reflected only once by default, preventing reflection loops;
- ordinary projectile blockers may still destroy a reflected projectile;
- reflection does not retroactively count as the original target being hit.

## 54. Post-Commit failures and refunds

Commit is the normal point of no return.

After an action reaches Commit:

- its resource cost remains spent;
- its cooldown remains started;
- ordinary later failure does not refund cost or cooldown.

Examples of later failure include:

- a targeted projectile's target dies before impact;
- the target becomes Untargetable or enters Stasis before impact;
- the effect is intercepted by a projectile blocker;
- a Spell Shield blocks the hit;
- a spawned area fails to hit anyone.

A specific ability may explicitly define a partial/full refund, but refunds are exceptions rather than the default rule.

## 55. Spire attack combat rules

Corrupted Spire attacks use the normal damage pipeline with structure-specific tags.

Current prototype defaults:

- Spire attacks deal **Physical Damage**.
- Spire attacks are tagged **Structure Attack** and **Structure Projectile**.
- Spire attacks are not basic attacks and do not trigger basic-attack-specific effects.
- Ordinary Spell Shields do not block Spire attacks.
- Ordinary projectile blockers do not intercept Spire attacks.
- Basic-Attack Immunity does not protect against Spire attacks.
- Physical Damage Immunity and full Invulnerability still work according to their normal rules.
- Shields absorb Spire damage normally.
- Untargetability or Stasis before impact causes the Spire projectile to fail against that target.
- Entering stealth/camouflage after the Spire has already committed a shot does not cancel that shot.
- Spire attacks do not crit unless a future explicit mechanic says otherwise.
- Spire damage is affected by the existing consecutive-hit ramp rules.

Exact Spire base damage, attack cadence, projectile speed, Armor-penetration behavior if any, and structure-specific tuning remain data-driven battleground values.

## 56. Combat-spec completion rule

The universal combat framework is considered sufficiently specified for implementation when a new Vanguard/item mechanic can be expressed by composing these rules without inventing a private local combat system.

Future mechanics should:

1. reuse existing combat primitives whenever possible;
2. explicitly declare exceptions;
3. extend the Combat Bible when a genuinely new universal rule is required;
4. avoid champion/item-specific hacks inside the core combat pipeline.

## 57. Remaining open combat questions

The following remain intentionally open because they are content/balance values rather than missing universal combat law:

- exact per-Vanguard base stats and growth;
- exact attack-range and cast-range values;
- exact default resource families and regeneration values;
- exact structure Armor/MR values;
- exact Spire base damage and attack cadence;
- exact per-effect Dispel/Purge classifications where not yet designed;
- exact Sleep wake conditions for future Sleep abilities;
- exact numeric tuning for future special-case mechanics.

New edge cases should extend this document or a later Combat Bible version rather than creating local implementation exceptions.
