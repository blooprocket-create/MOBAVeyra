# ADR-045: Account progression, the Collection and Vanguard Mastery

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §10 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Account, Collection & Mastery Bible](../Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md) §1–§6 (the rules marked *Locked*), §7 (UX-68–90) and §8 (open items).
- [Modes & Access Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md) §1–§4: owned and rotation picks; a rotation Vanguard's Mastery is kept; Co-op vs AI account XP only below Level 10.
- [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §2.7 (pending rewards shown honestly) and §3 (trusted account state, apart from Team Flux; post-match rewards apart from in-match Gold and XP).
- [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §6–§7, §11–§12: personal loss and forgiveness, remakes, no-contest and practice matches.
- [ADR-010](ADR-010-play-flow.md) §6: onboarding, entitlements and the catalog; it deferred purchases.
- [ADR-039](ADR-039-weekly-rotation-and-co-op-vs-ai.md) §1 and §7: the weekly rotation; account XP and Mastery did not exist yet.
- [ADR-007](ADR-007-match-join-contract.md) §7, [ADR-017](ADR-017-match-statistics.md) §5 and [ADR-019](ADR-019-match-flow.md) §3, §5: the verified result, its scoreboard, personal loss, surrender and remake.

## Context

Nothing a player does persists between matches. Matchmade players own only the starter they chose, and the weekly rotation lends them the rest. There is no account level, no account XP, no earned currency, no way to own another Vanguard, no Mastery and no Collection page. The backend stores every verified result, its scoreboard and each player's personal loss, but runs nothing after a result.

The Account Bible locks the rules for account levels, the two account currencies, Vanguard ownership, the Collection and Mastery. It calls itself "not an implementation-ready specification", and its last line says v0.1 is no signal that implementation should begin. The author's standing goal is a viable game as close to the bibles as possible. This record therefore implements **only the rules marked Locked**. Every number, curve and formula the bible leaves open is Provisional data in one validated configuration section, and nothing involving real money, payment, skins, bundles, seasonal price cuts, the tutorial or name changes.

## Decision

### 1. One trusted owner: the progression domain (Bible §6; Client & Platform §3)

- **A new backend domain, `progression`,** owns:
  - account level and account XP;
  - the **Flux** and **Refined Flux** balances, persistent account currencies, never in-match Team Flux, Gold or a Flux Spell;
  - the reward ledger;
  - each account's Mastery of each Vanguard;
  - storefront prices and purchases.
- **`account` keeps onboarding and entitlements.** A purchase grants an entitlement through `account`, whose sources gain `purchase` beside `starter`.
- **`match` knows no reward rule.** After it stores a verified result, it hands the match and the result to a `Rewards` interface **inside the same transaction**. A grant commits with its result or not at all.
- **The client never mints anything.** It reads balances, levels, prices and Mastery, and asks for purchases. Every grant and purchase is the backend's decision.

### 2. Idempotent and reconcilable (Bible §6)

- **Grants** are recorded per (match, account). A result delivered twice, which `match` already accepts as a no-op, grants nothing twice.
- **Purchases** carry a purchase ID the client generates. Repeating it returns the first outcome. The same ID with a different Vanguard or currency is a conflict.
- **Balances** equal what the grants gave minus what the purchases spent. Both ledgers are kept, so every balance can be reconciled.

### 3. Who earns what from a match (Bible §2, §5.1; Modes §4; Match Flow §6–§7, §12)

One pure rule decides each participant's eligibility and gives a reason code for the results screen:

| Condition | Account XP | Mastery |
|---|---|---|
| Practice or Custom rules (ADR-021) | none (`custom`) | none |
| Remake (no contest) | none (`no_contest`) | none |
| Ended as abandoned, by the host, or by a developer | none (`not_completed`) | none |
| Did not join | none (`not_joined`) | none |
| Personal loss in force at adjudication | none (`personal_loss`) | none (provisional) |
| Co-op vs AI (mode category `ai`) at or above `coopAccountXpBelowLevel` | none (`coop_level`) | earned |
| Otherwise: a completed Standard match (Prime Well destroyed or surrender) in category `casual`, `ranked` or `ai` | earned | earned |

- Forgiveness is the match server's (Match Flow §6). A forgiven player has no personal loss, so they earn as anyone else.
- Bots have no account and earn nothing.

### 4. Account XP and levels (Bible §2–§3)

- **XP from a match:** `round(xpPerMinute × minutes) + winBonusXp` when the player's side won. The minutes are the match clock, which excludes pauses. No daily cap.
- **Level curve:** from Level n to n+1 takes `firstLevelXp + growthPerLevel × (n − 1)` through `growthUntilLevel` (100), and that last amount for every level after. No account-level ceiling. A grant may cross several levels.
- **Flux:** `fluxPerLevelUp` at **every** level-up.
- **Refined Flux:** at the listed milestones (Levels 30, 50, 75 and 100, the bible's agreed examples), then every `refinedFluxEveryLevels` levels past the last listed one, `refinedFluxAfterMilestones` each.

### 5. Mastery (Bible §5)

- **Each account has an independent Mastery Level and lifetime points per Vanguard.** Both grow indefinitely. Nothing subtracts from them.
- **Points from a match:** `round(masteryPerMinute × minutes) + masteryWinBonus` when the side won, plus a performance award from the verified scoreboard: `min(performanceCap, Σ weight × statistic)`. The weighed statistics credit more than kills:
  - kills and assists;
  - Vanguard damage;
  - damage shielded and teammate healing (protection);
  - crowd-control seconds on enemy Vanguards (utility);
  - tower damage and Wells secured (objectives);
  - wards placed and destroyed (vision).

  Deaths never subtract.
- **Points go to the Vanguard actually played**, owned or a rotation loan. Buying it later keeps them (Modes §3).
- **Mastery curve:** as the account curve: `firstLevelPoints + growthPerLevel × (n − 1)` through `growthUntilLevel`, then constant. No maximum.
- **The mastery emote** is the only Mastery reward. Its appearance tier is how many of `emoteTierLevels` the Mastery Level has reached. A newer tier supersedes the old one; there is no choosing an earlier one.
- **No leaderboard,** no public Mastery record and no endpoint that lists another account's Mastery (Bible §5.3).

### 6. The storefront (Bible §3; §7 UX-68–90)

- **Every released Vanguard has a price in Flux and one in Refined Flux,** each its own value, validated at startup. Simple starter-friendly Vanguards cost less than complex ones (§10 lists the provisional tiers).
- **Either currency buys any released Vanguard,** from release.
- **A purchase:**
  - checks that the Vanguard is released and not owned, and that the balance covers the price;
  - then, in one transaction, spends the price, grants the entitlement with source `purchase` and records the purchase.
- **Refined Flux comes only from milestones.** There is no payment provider (Bible §3 and §8.5 leave it open), so no route sells it.
- **A development route grants currency,** for smokes and local play. It is mounted only where the other development routes are.

### 7. What the client reads and asks

- `GET /v1/me/progression`: level, XP into the level, XP for the next level, lifetime XP, Flux and Refined Flux.
- `GET /v1/me/collection`: **every released Vanguard**, in catalog order, whatever the account owns (Bible §4). For each:
  - whether it is owned, and how;
  - whether the rotation offers it;
  - its prices, and whether it can be bought now;
  - the account's Mastery: level, points into the level, points for the next, lifetime points and emote tier.
- `POST /v1/me/purchases` with `{purchaseId, vanguardId, currency}`.
- `GET /v1/me/matches/{id}` gains `rewards`:
  - account XP, levels before and after, Flux, Refined Flux;
  - the Vanguard, its Mastery points, and its Mastery levels before and after;
  - the reason when nothing was earned.

  Before the result is adjudicated, `rewards` is absent and the client shows it as pending (Client & Platform §2.7).
- **Visibility is not permission** (Bible §4). Champion select still offers owned and rotation Vanguards only, and the backend still checks each pick.

### 8. The client

- **The top bar** shows the Account Level with its XP bar, Flux and Refined Flux.
- **A Collection page** shows every released Vanguard:
  - owned, in rotation, or not playable;
  - its Mastery;
  - a **Buy** action with a confirmation naming the price and the currency.
- **The results screen** shows account rewards and Mastery apart from the match's Gold, XP and Team Flux (Client & Platform §3).

### 9. The mastery emote in a match

- The match roster carries each human's Mastery Level and emote tier for the Vanguard they play, read when the match is created.
- The match server keeps them on the player's state.
- An emote key (a rebindable control) shows a badge above the Vanguard with the level, coloured by tier, for `Match.json` `masteryEmote.seconds`, at most once per `masteryEmote.cooldownSeconds`. The server decides both, by its own clock.
- It has no gameplay effect, and a client draws it only while it can see that Vanguard.

### 10. Provisional answers

Every value below is configuration in `Backend/config/*.json` (`progression`), validated at startup, except the emote's two durations, which are game tuning.

| Area | Provisional value |
|---|---|
| Account XP | 6 per minute; win bonus 30 |
| Co-op XP gate | below Level 10, judged at the account's level as the grant is made (the bible leaves the snapshot open) |
| Account curve | 150 XP from Level 1 to 2, +20 per level through Level 100, then constant |
| Flux | 400 at every level-up |
| Refined Flux | Level 30: 250; 50: 400; 75: 500; 100: 750; then 500 every 25 levels |
| Mastery points | 10 per minute; win bonus 100; performance capped at 300 |
| Mastery weights | kill 15; assist 10; per Vanguard damage 0.005; per damage shielded 0.005; per teammate healing 0.005; per crowd-control second 2; per tower damage 0.005; Well secured 20; ward placed 3; ward destroyed 5 |
| Mastery curve | 1,000 points from Level 1 to 2, +500 per level through Level 5, then constant |
| Emote tiers | Mastery Levels 1, 5, 10, 25, 50, 100 |
| Mastery emote (game tuning) | shown 3 s; at most once per 10 s |
| Prices (Flux / Refined Flux) | 1,500 / 300 for the starters and the plainest kits (Cairn, Qazharr, Oriel, Bryn, Korruk, Moro); 3,000 / 550 for most (Kade, Vera, Mimzi, Patch, Gorraveth, Mavra, Celandrine, Aurelisse, Silt, Torr); 4,500 / 800 for kits with rides, stances, stealth, companions or terrain (Raska, Tavi, Angeru, Varkesh, Relay, Marek, Neris, Sylra, Eudora) |
| Personal loss and Mastery | no Mastery while a personal loss stands (the bible leaves it open) |

## Deferred

- **The tutorial**, and choosing the starter inside it (Bible §1 and §8.1): the starter choice stays the onboarding stub.
- **Real-money Refined Flux, skins, bundles and Test Skin** (Bible §3, §7): no payment provider and no skin content.
- **Seasonal price reductions** (Bible §3): their timing, amounts and floors are open.
- **Display-name changes paid in either currency** (Profiles & Identity Bible).
- **The continuous-play break reminder** (Bible §2).
- **Ranked's Account Level 30 and 20-owned gate** (Modes §2): Ranked is not implemented.
- **A Mastery backfill** from match history.
- **Abuse mitigation for Mastery** beyond the performance cap (Bible §5.1, §8.3).

## Consequences

- Every completed matchmade match moves the account forward. Flux earned through levels buys Vanguards, and the Collection shows the whole roster with the player's own Mastery.
- The backend gains one domain, one migration and four routes. `match` gains one interface call inside its result transaction.
- All tuning is configuration. Changing a curve changes future grants only: stored levels and balances never move backward.

## Alternatives considered

- **Rewards computed by the match server.** Rejected: the client and the match server report outcomes; durable grants are the trusted backend's (Bible §6).
- **Rewards granted after the result's transaction**, by a queue or a later sweep. Rejected for now: one transaction gives idempotency and reconciliation for free. A queue can come with scale.
- **Storing only lifetime XP and deriving the level.** Rejected: a later curve change would move stored levels, which Bible §5.1 forbids for Mastery and players would read as loss.
