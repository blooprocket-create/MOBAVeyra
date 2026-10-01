# ADR-038: The weekly free rotation, and Co-op vs AI

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Modes, Access & Weekly Rotation Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md):
  - §1: the mode roster;
  - §3: the 12-slot weekly free rotation;
  - §4: Co-op vs AI.
- [ADR-010](ADR-010-play-flow.md) §6 and §11: the catalog, and the rotation stand-in this replaces.
- [ADR-013](ADR-013-ai-vanguards.md): AI Vanguards and their seats.
- [ADR-021](ADR-021-custom-lobbies.md): custom lobbies, which already carry bots to a match.

## Context

**The rotation is empty.**
- The catalog's stand-in offers every released Vanguard only while fewer than a full rotation are released. With 25 released and 12 slots, it offers none.
- A matchmade player can therefore pick only the Vanguards they own, which today is their starter.

**Co-op cannot be queued.**
- `coop_beginner` and `coop_intermediate` are configured with `"matchmaking":"notImplemented"`, which the party service refuses.
- The matchmaker fills two human sides.
- Standard match rules refuse bots, and so does the game server.

**Bots take their roles from their seat order,** whatever their kit. The M29 gate seated Relay, a support, as a Beginner jungler; it never cleared a camp. A co-op enemy team of five random rotation picks will meet this every match.

## Decision

### 1. The weekly free rotation (Backend: catalog; Modes §3)

- **Weeks** begin at a configured epoch and last a configured number of seconds, one week.
- **Each week offers `slots` distinct released Vanguards,** drawn by a seeded, reproducible shuffle of the week's eligible pool. The seed is the configured seed and the week index.
- **A Vanguard is eligible** from the first week that starts at least one week after its configured release date. One with no release date has been released since before the epoch.
- **Last week's Vanguards sit out this week.** When that leaves fewer than `slots` choices, the rotation relaxes that rule first, drawing as many of last week's as it needs (shuffled the same way). It never relaxes the release delay.
- **Fewer eligible Vanguards than `slots`** means no rotation that week, and the backend logs it. This is the bible's launch precondition, not permission to duplicate slots.
- **Every service computes the same rotation for a week** from configuration alone; no client chooses it. Each week's rotation follows from the one before it, starting at the epoch.
- **What a player may pick** is unchanged: owned plus the current rotation. The stand-in setting goes.

### 2. The co-op matchmaking kind (Backend: config, matchmaking; Modes §4)

- **A mode's `matchmaking` may be `coop`,** with:
  - `aiDifficulty`: beginner or intermediate;
  - `aiPerTeam`: how many enemy AI Vanguards, five.
- **A co-op match** groups `humanPlayersPerTeam` humans on one side (parties of 1 to that size, never friendly AI) against `aiPerTeam` bots on the other. Matchmaking waits for enough humans rather than adding bots to their side.
- **Only the humans accept** a co-op match found.
- **Locally, `humanPlayersPerTeam` is 1,** as Casual's is, so one player can play co-op against five bots. Canon is five.

### 3. Co-op select (Backend: selection)

- **A co-op select session opens with its bots already seated:**
  - `aiPerTeam` distinct Vanguards drawn at random from the current rotation;
  - at the mode's difficulty, on the side with no humans.

  While the rotation is empty, they are drawn from every released Vanguard (provisional).
- **Humans pick** as in Casual Select: owned or rotation Vanguards, unique among the humans. A human may pick a Vanguard an enemy bot plays, the sole cross-team mirror exception.
- **The bots' picks show** as locked enemy seats.

### 4. Matches with enemy AI (Backend: match; Game: match host)

- **A co-op match keeps Standard rules:** victory, surrender, absence, buyback and results apply as in Casual.
- **A Standard match may carry bots** only when every bot sits on a side with no human. The backend validates this when it creates the match, and the game server's match host accepts exactly that. Any other Standard match with bots stays refused.

### 5. Bot roles that suit their kits (Game: bots; ADR-013)

- **Bots.json gives each Vanguard `roles`:** the roles its bot plays, in order of preference.
- **Seats still decide which roles a team fills.** Once a team's bots are all seated, its roles are dealt to them by preference, the jungle first: the bot that most prefers a role takes it, ties by seat.
- **A Vanguard that plays no role still on offer** takes the first remaining one.

### 6. The client

- **Co-op modes are matchmade:** they show as playable cards, queue, accept and select as Casual does, with their bots in the enemy seats.
- **`Smoke.ps1 -Flow Coop`** queues one packaged client for co-op, accepts, picks, and sieges to victory against five bots.

### 7. Rewards

- **A co-op match records its result and history** as a Casual one does.
- **Account XP and Mastery** do not exist yet. When they do, Modes §4 governs them: Account XP below Account Level 10 only, Mastery always.

### 8. Provisional answers where canon is open

1. **Weeks begin Monday 00:00 UTC** (the configured epoch), and each lasts seven days.
2. **The rotation is a uniform random draw.** The bible's spread of mechanical accessibility and playstyles waits for its category data.
3. **Release dates are configuration.** A Vanguard with none counts as released before the epoch.
4. **While the rotation is empty, co-op bots are drawn from every released Vanguard.**
5. **Co-op bots buy, level and fight as the bots of any match do,** at the queue's difficulty.
6. **Only a co-op mode may seat bots in a Standard match.**
7. **The values** in `Backend/config/local.json` and Bots.json are Provisional settings.

## Consequences

- Matchmade players gain twelve playable Vanguards each week, and the rotation stops depending on how many are released.
- One player can play a full match locally against five AI Vanguards.
- Bot teams fill their roles sensibly, in co-op and in every other match with bots.
