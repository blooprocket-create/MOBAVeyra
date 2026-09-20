# ADR-003: Owned field entities (companions, deployables, decoys, world volumes, ride states)

**Status:** Accepted
**Date:** 2026-09-20

Accepted as **Option B together with Option D**: three primitives with the ride state scoped separately, and the first playable slice built from Vanguards that need no owned entity at all.

## Context

Sixteen of the twenty-five designed Vanguards place something on the battlefield that outlives the cast and is owned by the caster. They are described in the Character Bible as if they were similar to one another. They are not.

Sorting them by what the engine actually has to do:

**1. Persistent combat units with their own Health that can be destroyed**

- **Nix** (Marek) — own Health bar, attacks independently, prioritises targets Marek recently damaged, can be killed and later reforms, swaps position with its owner, grows to a larger form under R, and ends R if killed.
- **Picket** (Eudora) — finite Health, lifetime and attack range; fires autonomously at legal targets; switches between a gun mode and a directional shield mode; under R unanchors and follows a chosen ally, then re-anchors.
- **Waterling** (Neris) — one at a time, targetable and killable, follows an ally or hunts an enemy depending on Sea State, redirectable on recast.

**2. Placed objects, beacons and decoys with no autonomous combat behaviour**

- **Waymark** (Sylra), **False Body** (Angeru), Tavi's **Hide!** illusion, **Anchor** (Torr), **Pressure Mine** (Korruk).

**3. Persistent world volumes that modify the battlefield itself**

- **Molten Ground** and the cooled **Iron Wall** (Varkesh) — the wall "blocks both teams", i.e. runtime pathing modification.
- **Lay the Mist** and **Through the White** (Sylra) — create *true Dense Fog* obeying "the battleground's normal Dense Fog rules", i.e. runtime-spawned vision volumes.
- **Caustic Line** residue and **CODE BLACK** (Mavra), **Buried Alive** and **Sandstorm** (Silt), **Magnetic Field** and **Full Grid** (Relay), **Wildstorm** (Moro), **Briar Scatter** (Celandrine), Aurelisse's three currents, Neris's **Tidebreaker** trail.

This is the largest category: **18 volumes across 9 Vanguards**, more than the other three categories combined.

**4. A player-piloted ride state**

- **Hound** (Raska) — an entire alternate locomotion mode: very high movement speed, unit ghosting, wide turning behaviour at speed, a *replacement ability set* (Sideswipe, Powerslide, Bail Out), and — after Bail Out or Last Exit — Hound continues travelling as an independent entity that collides with and knocks enemies.

The Combat Bible §32 already defines the **semantics** that categories 1 and 2 need: summon/clone/decoy definitions, ownership resolution, damage attribution back to the owning Vanguard, and tower-aggression inheritance. That work is done and it is good.

What does not exist anywhere in `Docs/` is the **architectural** answer: how many distinct primitives back these behaviours, which module owns them, and how their lifetime, replication and authority work. Category 4 additionally has no design canon at all — there is no mount, vehicle or ride-state section in the Combat Bible, the Battleground Bible or the Vision Bible. Raska is Vanguard #1.

### Why this cannot be deferred to implementation time

- Building it per champion produces up to sixteen private implementations of ownership, replication, lifetime, death cleanup and attribution — directly contrary to `ARCHITECTURE.md` §1.3's reusable-primitive posture and to ADR-002's rule that champion abilities compose shared primitives rather than owning private copies.
- Categories 3 and 4 are not ability features. A runtime-spawned Dense Fog volume is a **Vision system** capability; a wall that blocks both teams is a **Battleground/navigation** capability; a ride state with a replacement ability set is a **locomotion and input** capability. Deciding them inside a champion's ability class puts core rules in the wrong module.
- Whichever way this goes, it changes which Vanguards are cheap to build first, and therefore what a first playable slice should contain.

## The question

**What is the smallest set of owned-field-entity primitives Veyra will support, which module owns each, and is Raska's ride state in scope for the first playable?**

Two sub-questions must be answered at the same time, because they are the ones that leak outside the ability system:

- **Vision:** are ability-created Dense Fog volumes the *same* construct as authored map fog volumes, spawnable at runtime? The Character Bible says Sylra creates "true Dense Fog" following normal rules, which strongly implies yes — but the Vision Bible does not currently say fog volumes are runtime-spawnable, and that is an engine requirement, not a design flourish.
- **Navigation:** may an ability modify pathing for both teams at runtime (Varkesh's Iron Wall)? If yes, dynamic navmesh modification becomes a shipping requirement with its own performance and determinism cost.

## Decision

**Three primitives, ride state scoped separately, and the first slice built from Vanguards that need none of them.**

### The three primitives

1. **Combat entity** — Health, autonomous behaviour, destructible. Serves **Nix** (Marek), **Picket** (Eudora), **Waterling** (Neris).
2. **Placed marker** — no autonomous combat, optional trigger, optional vision contribution. Serves **Waymark** (Sylra), **False Body** (Angeru), Tavi's **Hide!** illusion, **Anchor** (Torr), **Pressure Mine** (Korruk).
3. **World volume** — owned by the system whose rules it modifies, **not** by a single shared type:
   - **Vision** owns fog volumes (Sylra's Lay the Mist and Through the White);
   - **Battleground/navigation** owns pathing-affecting terrain (Varkesh's Iron Wall);
   - **Combat** owns damage, status and movement fields (Mavra's hazards, Silt's zones, Relay's Magnetic Field and Full Grid, Moro's Wildstorm, Celandrine's Briar Scatter, Aurelisse's currents, Neris's Tidebreaker trail).

Ownership, lifetime and damage attribution factor into a small common base shared by all three, so that Combat Bible §32's attribution rules are implemented once. Everything above that base is specific to the primitive.

**Raska's ride state is not an owned entity.** It is a separate locomotion feature with its own decision, its own canon (which does not yet exist in any bible) and its own schedule.

### Why the world volume is split rather than unified

This is the part that decides the ADR, and the data settles it. World volumes are the largest category by a wide margin — **18 volumes across 9 Vanguards**, more than the other three combined — and their members do not share an owner. A fog bank is a Vision construct, a wall is a navigation construct, a corrosive field is a Combat construct. A single generic type spanning all three would have to reach into every one of those systems, which is precisely the god class `CLAUDE.md` prohibits and the dependency shape `ARCHITECTURE.md` forbids. Splitting by owning system keeps each rule in the document and module that already owns it.

### First playable slice

Nine Vanguards place no persistent entity at all: **Kade, Patch, Vera, Qazharr, Cairn, Bryn, Oriel, Mimzi, Gorraveth**. The slice is drawn from these, so core combat is proven before any of the three primitives is built.

The recommended four are **Cairn** (tank, control, shielding), **Qazharr** (melee fighter, sustained pressure), **Oriel** (ranged mage, poke and scaling) and **Bryn** (ranged physical carry, attack-interval floor and the Dense Fog presence rule). They cover four archetypes, need no entity work, and between them exercise shields, displacement, the Attack Speed overflow reference and the Vision Bible's fog rules.

### Consequent obligations

Two rulings this decision defers to the owning bibles, both now required before the corresponding Vanguard is built:

- the **Vision Bible** must state whether fog volumes are runtime-spawnable, which Sylra's kit assumes;
- the **Battleground Bible** must state whether an ability may modify pathing for both teams at runtime, which Varkesh's Iron Wall assumes.

## Options considered

### Option A — One generic owned-entity primitive

A single `OwnedFieldEntity` abstraction with composable modules (health, autonomous targeting, mode switching, follow behaviour, volume effects), configured by data per champion.

- **For:** one place for ownership, replication, lifetime, death cleanup and attribution. Maximum reuse. Matches §1.3's posture most literally.
- **Against:** high risk of becoming the god class `CLAUDE.md` prohibits. A wall that blocks pathing and a shadow that mimics one ability share almost nothing but an owner pointer; forcing them into one type buys little and costs clarity.

### Option B — Three primitives, ride state scoped separately

1. **Combat entity** — Health, autonomous behaviour, destructible. Serves Nix, Picket, Waterling.
2. **Placed marker** — no autonomous combat, optional trigger, optional vision contribution. Serves Waymark, False Body, illusions, Anchor, mines, nodes.
3. **World volume** — owned by the systems whose rules it modifies: Vision owns fog volumes, Battleground owns navigation-affecting terrain, Combat owns damage/status fields.

Raska's ride state is treated as a **separate locomotion feature** with its own decision, not as an owned entity.

- **For:** each primitive has one coherent job and an obvious owning module. Category 3 lands in the systems that already own those rules, which is where `ARCHITECTURE.md` wants it. Isolates the single most expensive feature so it can be scheduled or cut on its own merits.
- **Against:** three primitives plus a locomotion feature is more surface than one. Some sharing (ownership, attribution) must be factored into a small common base to avoid triplication.

### Option C — Compose per champion from GAS primitives, no Veyra abstraction

Each entity is a bespoke actor assembled from Gameplay Abilities, Gameplay Effects and Ability Tasks.

- **For:** no speculative abstraction; fastest path to a single working champion.
- **Against:** the roster has thirteen of these. By the fourth, ownership and attribution logic is copy-pasted, which is the exact outcome ADR-002 and §1.3 forbid. Not recommended, recorded for completeness.

### Option D — Defer, and cut entity-heavy Vanguards from the first playable

Take the decision later; build the first playable from Vanguards needing no owned entity — for example Oriel, Moro, Qazharr and Cairn — and revisit once core combat is proven in engine.

- **For:** the honest sequencing answer. A grey-box slice would answer several of these questions empirically rather than on paper, and costs nothing that is not already needed.
- **Against:** leaves Raska, the lead character, unbuildable and unestimated for longer. Does not remove the decision, only postpones it — and the roster keeps growing in the meantime.

Options B and D are compatible and can be taken together. **This is what was accepted.**

## Consequences

- The first-playable roster selection becomes a deliberate, costed choice rather than an inherited one.
- Categories 3 and 4 acquire explicit owning documents. The Vision Bible gains a ruling on runtime fog volumes; the Battleground Bible gains a ruling on runtime navigation modification; ride states gain a Combat Bible section or an ADR of their own.
- Whichever option is taken, `Docs/Design/Veyra_Combat_Bible_v0.4.md` §32 remains the semantic authority — this ADR governs structure, not rules.

## Affected documents

- `ARCHITECTURE.md` §1.3 (reusable primitives, data-driven configuration), §1.4 (thin Blueprints)
- `Docs/ADR/ADR-002-gameplay-ability-system.md` (abilities compose shared primitives)
- `Docs/Design/Veyra_Combat_Bible_v0.4.md` §32 (summons, companions, clones, decoys)
- `Docs/Design/Veyra_Vision_Bible_v0.1.md` (runtime-spawned Dense Fog volumes — currently unaddressed)
- `Docs/Design/Veyra_Battleground_Bible_v0.9.md` (runtime navigation modification — currently unaddressed)
- `Docs/Design/Veyra_Initial_Roster_Character_Bible_v0.6.md` (Raska ride state has no supporting canon)
