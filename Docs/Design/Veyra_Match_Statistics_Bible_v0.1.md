# Veyra Match Statistics Bible

**Version:** 0.1 — Post-match statistics plus UX Proposals 41–67 (2026-09-23)  
**Status:** Working canon for completed-match recording and approved scoreboard/detailed-statistics layout. The earlier Proposal 49 pause is historical; **current pre-game UX pause: after Proposal 92, pending author “continue” for 93**.  
**Related:** [Pre-Game Client UX Bible](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) §§5–6 governs player-facing selection/results presentation; [Match Flow Bible](Veyra_Match_Flow_Bible_v0.1.md) adjudicates team results, remake and personal-loss overrides; [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) governs verified results transitions. Gameplay economy, combat, Flux and inventory remain authoritative under their owning systems.

## 1. Recording and display boundaries

- Trusted match/gameplay systems author the underlying events, damage categories, target categories, attribution and final equipment. The pre-game client **does not calculate or fabricate** match statistics or turn speculative client events into approved server facts. Expose and reconcile event-derived totals through one responsible gameplay/statistics service rather than duplicating core calculations.
- Record stats for the **actual completed match**. The result field distinguishes team result from any individual AFK/disconnect personal-loss override and respects no-contest remakes and other Match Flow outcomes. Results UI must mark unavailable/pending values as pending without making up zeros or final rankings.
- An eligible statistic does **not** itself grant game rewards, force lane roles, define a Vanguard class, change Flux Well mechanics or approve final UI positioning. Never display a source category for a mechanic that does not exist.
- Proposal **41 was modified:** the final results overview, tabs and full visual layout are **deferred** until recording decisions have been made. Approved post-match baseline: verified completion permits results, normal eligible party/social browsing returns, pending reward/stat data is labeled, and players are never automatically readied or requeued.

## 2. Core per-player fields (Proposal 42 — approved with additions)

| Area | Server-confirmed recorded fields |
|---|---|
| Identity/result | Player, Vanguard, team, actual team/personal result as applicable |
| Combat | Vanguard kills, deaths, assists; **enemy-Vanguard health damage** separate from other target damage; damage dealt **physical / magical / true**; damage taken **physical / magical / true**; effective damage shielded by shields provided; effective healing **self / teammate** |
| Progression/economy | Final level; cumulative total gold earned; minion last hits; jungle-creature last hits (separate) |
| Objectives | Direct effective damage to enemy towers; count of neutral Flux Wells secured with player's eligible participation |

No per-player assigned Top/Jungle/Mid/Carry/Support role or inferred lane is required.

## 3. Effective contribution definitions (Proposal 43 — approved)

- **Damage dealt:** Count **actual health removed** by a damage event, by physical/magical/true damage type. Keep target category, especially enemy-Vanguard health damage, separate. Damage absorbed by a shield is **not also health damage dealt**.
- **Damage taken:** Count the player's **actual health lost** by damage type. Shield-absorbed damage is separately represented, **not also health damage taken**.
- **Shielding:** Credit the **provider** of a shield (on themselves or a teammate) for damage **actually absorbed by that shield**. Unused shield capacity, including that which expires, earns no shielded-damage amount.
- **Healing:** Count only actual health restored, split self and teammate healing by provider. Health above missing-health capacity is overhealing and does not count.
- Where multiple shields, unusual mitigation, area effects or chained providers create attribution ambiguity, keep one authoritative combat attribution implementation and tests; do not silently count one event twice in the same category.

## 4. Control and vision (Proposals 44–45 — approved, 45 modified)

### Crowd control

Record per-player **effective duration** of movement- or action-restricting control applied to enemy Vanguards, plus a breakdown by control type (e.g. stun, root, silence, slow, where present in actual gameplay). Misses, immunity and non-Vanguard target effects do not contribute. Overlapping control **from the same player on the same target** is not double-counted toward that player's total. Detailed control-type taxonomy and cross-provider overlap handling remain implementation definitions.

### Vision

- Record **distinct enemy-Vanguard reveal events** attributable to the player when a previously hidden Vanguard becomes revealed to that player's team; a continuously maintained reveal does not become repeated reveal events.
- Record **effective reveal duration** for otherwise-hidden enemy Vanguards; overlapping reveal by the **same player on the same target** is not double-counted.
- If supported by the actual vision system, separately record vision-object placements and enemy vision objects disabled/destroyed. Omit these fields rather than inventing wards or analogous mechanics.
- **Vision Score is approved** as a per-player summary of legitimate vision contributions while underlying stats remain inspectable. **Its formula, weights, normalization and treatment of any future vision mechanics are explicitly undecided.** Do not assign a number or invent mechanics until separately designed and approved.

## 5. Neutral objectives (Proposal 46 — approved)

Record **per player**: secured neutral Flux Wells with eligible participation; actual damage to active neutral Flux Well objectives; whether the player delivered the securing final hit where the existing objective uses that mechanic. Record **per team**: which team secured each Well and capture times, preserving objective sequence. Do not combine these measurements into an invented weighted objective score.

## 6. Explicit rejection — tower/Fluxborn extension (Proposal 47 — rejected)

The proposed **individual tower-participation credits**, **team tower-destruction timeline**, and **team Fluxborn damage to towers attributed as team wave pressure** are **not approved as post-match recording additions**. The already approved per-player **direct effective damage to enemy towers** remains. No gameplay alteration follows from this rejection.

## 7. Earned gold by source (Proposal 48 — approved)

Per player, record gold earned from enemy-Vanguard kills and assists; minion kills; jungle creatures; objectives **only when existing rules actually award gold**; passive income and other **real, supported** sources. Source subtotals must reconcile to **total gold earned**. Gold spent on items or replacing Flux Spells is separate and does **not** reduce lifetime match gold earned. This introduces no new rewards or income mechanic.

## 8. Final equipment snapshot (Proposal 49 — approved)

On authoritative completion record each player's **final** occupied and empty normal inventory slots, exact equipped item IDs/tiers and any applicable Attunement; separately record both **final equipped Flux Spell slots**, including empty slots. The final Flux Spells may differ from the starting champion-select loadout; do not overwrite per-Vanguard remembered *starting* loadout with in-match shop changes. The final snapshot does **not** include replaced purchases or spells as if they are still equipped, and does not imply a full item acquisition/upgrade history, build rating or role assignment.

## 9. Still open; do not infer approval

- **The primary Scoreboard and category-based Detailed Statistics layout was approved in Proposal 50**; remaining exact visual sizing, detailed-statistic placement, sorting and visual chart treatment are implementation/presentation details. All-ten-player per-stat comparison (Proposal 54) remains explicitly **deferred**, not approved.
- The **Vision Score computation**, and any vision-object or ward mechanics not independently approved.
- Detailed crowd-control taxonomy, multi-provider attribution edge cases, reconciliation/test definitions and data persistence schema.
- Full item/spell transaction timeline, purchase history, new gameplay stats or a weighted objective score.
- **Current checkpoint:** Author approved Proposal 92 and paused; do not propose 93 until requested to continue.


## 10. Results and saved Match History — approved UX Proposals 50–67 (2026-09-23)

- **50 — results layout approved:** On authoritative completion open a **two-team Scoreboard** displaying confirmed outcome, mode, duration and each player's Vanguard, K/D/A, final level, total gold earned, separate minion/jungle last hits, final items and inspectable final two Flux Spell slots. **Detailed Statistics** displays the already approved Combat, Objectives, Economy and Vision measures by category, with unit-appropriate values and truthful Pending states. No auto-assigned roles, performance grade or build ranking. Vision Score only after separate formula approval.
- **51 — Match History approved:** Player's accessible completed matches newest first; list date/mode/duration/Vanguard and actual personally adjudicated outcome. Open each record into saved Scoreboard and Detailed Statistics with final equipment. Never present incomplete matches as completed.
- **52 — Match Timeline rejected:** Do not add a timestamped kill/objective/event Timeline view. Team-level objective capture records approved in 46 remain available in the underlying data without implying a Timeline UI.
- **53 — team summary approved:** Per team, sum Vanguard kills and players' earned gold; count secured Flux Wells **once per team capture**, not by adding individual participation credits. Does not incorporate rejected tower/Fluxborn aggregation.
- **54 — all-ten-player metric comparison deferred:** No additional selected-stat bar/chart/table interface until separately approved.
- **55 — Progression & Rewards approved:** Results show confirmed account progression, Vanguard Mastery progression and actual awards under the owning account rules, with Pending for unprocessed fields. No forced claim or requeue.
- **56–58 — participant actions:** Report others from results through existing moderation rules; contextual permitted profile/friend/party/report actions on results and saved history; **one optional general commendation to one other teammate from immediate results only**, with no auto-award, MVP grade, honor/reputation system or matchmaking effect.
- **59–60 — post-match chat:** Optional, **inactive until player sends their first message**; do not receive/display prior messages. Opted-in conversation ends when player leaves immediate results or enters another committed match; not available in saved history/public replay and no bypass of All Chat Off, mute or block.
- **61 — Home View Last Match shortcut rejected. 62 — Play Again approved:** Only expands the existing retained-party panel and shows readiness/mode; never auto-readies, starts queue or changes mode.
- **63 — Watch Replay approved:** Eligible history entry opens existing Replay Mode **in the same Unreal application** after UX 91–92, returning to same history entry on exit; no change to Replay Bible retention/privacy or party readiness.
- **64 — History Vanguard/mode/personal outcome filters approved; 65 — participant-name search rejected; 66 — date-range filtering rejected; 67 — newest-first batch loading with Load More approved.** Filters query entire accessible saved history, not merely loaded entries; no retention extension.

Detailed UX and client-state restrictions live in the [Pre-Game Client UX Bible](Veyra_Pre_Game_Client_UX_Bible_v0.1.md) §§7–9; match-result and individual AFK-loss adjudication remains with the Match Flow Bible. **Current pause is after Proposal 92.**
