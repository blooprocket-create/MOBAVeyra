# ADR-013: AI Vanguards: bot brains that play through the players' order paths

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent; §8 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M9 pull request that adds it.  
**Date:** 2026-09-28  
**Related:** [ADR-006](ADR-006-unreal-project-scaffold.md) (§3 modules and layers, §7 server-side movement), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (content as data, archetype maps), [ADR-010](ADR-010-play-flow.md) (§7 practice and its bots), [ADR-011](ADR-011-battleground-runtime.md) (lanes, structures, Fluxborn, towers), [ADR-012](ADR-012-items-and-shop.md) (the shop, Recall), [Custom Matches Bible](../Design/Veyra_Custom_Matches_Bible_v0.1.md) §1–§3, [Modes & Access Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md) §4, [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §4, [Architecture Constitution](../../ARCHITECTURE.md) §1.1, §1.3, §1.7, [Project Structure](../../PROJECT_STRUCTURE.md) §2.

## Context

A practice match seats four bots against its host (`customPractice.bots`), and they only wander (`UVeyraBotWanderComponent`). Canon asks for more:
- **AI Vanguards that play:** Custom Matches §1 and §3 (friendly or enemy bots, each with its own difficulty), and Modes §4 (Co-op vs AI, five humans against five bots).
- **Two defined behaviours, Beginner and Intermediate** (Custom §3). Difficulty changes behaviour only, never rules (Modes §4).
- **No stated AI logic:** "AI logic … remain[s] … open" (Modes §4).

Facts in the code:
1. Every order path in `AVeyraGameMode` takes an `AVeyraPlayerController`. Bots have none. A bot that called `VeyraAbilities::TryCast` itself would skip the phase and pause checks and would not end its Recall.
2. The Match layer holds the game mode that creates bots. A brain there would add behaviour code to orchestration; a brain above it cannot be named by it.
3. Everything a bot must see already exists as queries: `UVeyraBattlegroundSubsystem` (structures, Fluxborn), `VeyraLayout` (lane waypoints, fountains), the combat attributes, the cooldown and loadout components, the shop and the inventory rules.

## Decision

### 1. VeyraBots, in a new Autonomy layer (amends ADR-006 §3 and Project Structure §2)

`VeyraBots` sits in a new **Autonomy** layer, directly above Orchestration (VeyraMatch) and below Services. It may use every layer below it. Nothing depends on it, and the layer check holds that. It is a server module (bots think only on the server), loaded by clients but idle there.

### 2. Match announces bots and never names their brain

When the game mode seats a bot, it raises `UVeyraMatchEvents::OnBotAdded` (a Match world subsystem). The event carries the bot's PlayerState and its seat: side, Vanguard, difficulty and lane role. `UVeyraBotSubsystem` in VeyraBots subscribes and gives the bot's Vanguard controller a `UVeyraBotBrainComponent`. Events go up and calls go down (Architecture §1.7). `UVeyraBotWanderComponent` and `Match.json` `bots` are retired.

### 3. Bots use the players' order paths

The game mode's order handlers gain overloads by participant (`AVeyraPlayerState&`): move, attack, attack-move, cast and Recall. The player-controller versions wrap them. So a bot obeys phase, pause, crowd control, cooldowns, costs and ranges exactly as a player does, and its orders end its Recall. The per-player order allowance, a guard against client spam, stays with the player controller; a bot's think rate (data) bounds it instead. There are no bot-only verbs.

### 4. A brain senses, decides and acts

- **Sense:** `VeyraBotSenses` reads the world into `FVeyraBotView`, plain data:
  - the bot itself: Health, level, Gold, position, whether it is recalling or at the fountain, basic attack reach and damage, and each slot's readiness and targeting;
  - its lane: the allied wave's front, enemy Fluxborn with their Health, and the next standing enemy structure with whether it targets the bot;
  - enemy and allied Vanguards within a sight radius.
- **Decide:** `VeyraBotRules::Decide(View, Difficulty, Memory)` is a pure function returning one intent. The priorities are League's (§8):
  1. shop and spend skill points when dead or at the fountain;
  2. retreat, then recall, when hurt;
  3. recall when the next item is affordable and the lane is quiet;
  4. fight an enemy Vanguard when the trade favours the bot, no enemy tower covers it and the enemy wave would not turn on it;
  5. last-hit Fluxborn about to die;
  6. siege with the wave;
  7. push: attack the weakest enemy Fluxborn near it;
  8. otherwise hold behind the wave.
- **Act:** `UVeyraBotBrainComponent` thinks on a world-time timer, so a pause holds bots too. It turns the intent into orders through §3, re-issuing only when the intent changes.

The targeting kind of each ability comes from the archetype map that defines it (ADR-008): targeted, area, skillshot, dash, self-buff or empowered attack. Each kind has one aiming rule, so no ability is named in code.

### 5. Difficulty and behaviour are data

`Game/Tuning/Bots.json` (owner VeyraBots, with a schema) holds:
- **`difficulties.beginner` and `.intermediate`:** think and reaction times, last-hit, push and cast chances, the last-hit lead, whether skillshots lead, how many enemy Fluxborn a fight may provoke, and the retreat, recall and fight thresholds.
- **`roles`:** what each seat plays: a lane, or the jungle (ADR-014 §7), with `jungle` holding how a jungler plays.
- **`vanguards.<id>`:** each Vanguard's build order (item IDs), skill priority and cast conditions.

`check_tuning.py` checks the item and Vanguard references. Randomness (a missed last hit, a withheld cast) draws from a stream seeded by the participant and its seat, so a bot's choices repeat for the same seating.

### 6. Seats carry difficulty

`FVeyraAssignedBot` and the backend's `AssignedBot` gain `difficulty` (Beginner or Intermediate), validated; practice bots default to Beginner in `customPractice.bots`. A developer server adds playing bots with the map URL options `VeyraPlayingBots=<n>` and `VeyraBotDifficulty=`, which `Smoke.ps1 -PlayingBots` uses to run whole bot matches.

### 7. Scope

- **In M9:** bots that lane, last-hit, trade with abilities, retreat, recall, shop, rank up, siege and return to lane; practice's bots use it; a bot-vs-bot soak smoke.
- **Deferred:** Co-op vs AI matchmaking (M9b: five humans against five bots, needing queue changes), custom-lobby bot seats, jungling (no jungle yet), vision-aware play (no fog yet), pings and chat.

### 8. League answers where canon is silent (for the author to overturn)

1. Roles: one top, one mid, two bottom, and the fifth jungles (ADR-014 §7; until M10a it played top).
2. Retreat below a Health fraction, then recall once no enemy is near; with Gold past a threshold (`shopRecallGold`) and the next purchase affordable, recall to spend it when no enemy is near.
3. Bots buy when dead or at the fountain, from their Vanguard's build order, as League's bots follow fixed builds.
4. Bots never dive towers: they fight only outside an enemy tower's range unless allied Fluxborn hold its aggro.
5. Target choice: the enemy Vanguard in reach with the lowest Health fraction.
6. Beginner reacts later, misses some last hits, casts less often, does not lead skillshots and retreats earlier. Intermediate reacts sooner, last-hits reliably, leads skillshots and trades harder.
7. Bots buy no consumables yet: builds hold equipment only, and Field Tonics wait for a consumables rule in `Bots.json`.
8. Bots respect minion aggro: they start no fight with an enemy Vanguard while more of its side's Fluxborn than `fluxbornTolerance` stand within their aggression response range of it (World.json), since hitting it would turn them on the bot (Battleground Bible §19).
9. Bots farm and push as League's do: they go for a last hit once a Fluxborn has at most `lastHitLead` basic attacks' damage left, to cover walking up and winding up, and otherwise shove the wave by attacking the weakest enemy Fluxborn within `positioning.pushRange` (`pushChance` per decision). Once they choose a Fluxborn they keep attacking it until it dies or leaves their sight, unless a last hit comes up, so a fresh choice each decision never throws a windup away. Neither happens under an enemy tower unless their own wave holds its aggro.

## Consequences

- Solo practice has opponents that play, and Co-op vs AI needs only matchmaking.
- One new module and layer. Match gains order paths by participant and one event, not behaviour.
- Rules are pure and tested per priority; senses and actions are tested in the compact battleground.

## Amendments to earlier records

- **ADR-006 §3:** the Autonomy layer and VeyraBots.
- **ADR-010 §7:** practice bots play instead of wandering; `Match.json` `bots` is retired.

## Open items

- Co-op vs AI queues (M9b); custom-lobby bot seats; jungle and vision behaviour once those exist.
- Difficulty values are provisional, to be tuned in playtests.
