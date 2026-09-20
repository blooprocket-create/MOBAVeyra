# Veyra Modes, Access & Weekly Rotation Bible

**Version:** 0.1 (expanded checkpoint) — Captured queue, roster access, Co-op vs AI and deferred Ranked launch scope; further discussion required  
**Status:** Working design canon for rules explicitly marked *Locked*; not an implementation-ready matchmaking specification  
**Scope:** Matchmade modes, future Ranked entry/selection access, weekly free-rotation selection, Co-op vs AI and cross-reference to invite-only custom matches.  
**Companion documents:** [Battleground Bible v0.9](Veyra_Battleground_Bible_v0.9.md) owns the map and PvP champion-select ban/pick/trade structure; [Match Flow Bible v0.1](Veyra_Match_Flow_Bible_v0.1.md) owns live-match results, votes, penalties and remakes; [Account, Collection & Mastery Bible](Veyra_Account_Collection_Mastery_Bible_v0.1.md) owns persistent account rewards/ownership and Flux/Refined Flux; [Parties, Social & Matchmaking Bible](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns parties, queue readiness/acceptance, party-rank rules and absolute block restrictions; [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) owns client handoffs; [Custom Matches & Private Lobbies Bible](Veyra_Custom_Matches_Bible_v0.1.md) owns invite-only sandbox composition, AI fill, host overrides and solo practice.

> **Implementation rule:** Queue eligibility, rotation snapshots, Vanguard ownership, pick/trade legality and results must be validated by their trusted authoritative services; UI is a view of this state, not the authority. Thresholds, rotation policies, timing and all mode-specific settings belong in editable, validated data.

## 1. Locked — mode roster and baseline rules

| Mode | Teams | Vanguard access for human player | Other rules |
|---|---|---|---|
| **Casual Select (non-Ranked PvP)** | 5 humans vs 5 humans | Owned Vanguards **plus current weekly free rotation** | Battleground Bible's Casual Select; globally unique picks across both teams. |
| **Draft Pick (non-Ranked PvP)** | 5 humans vs 5 humans | Owned Vanguards **plus current weekly free rotation** | Battleground Bible's bans/draft; globally unique picks across both teams. |
| **Ranked — deferred beyond initial launch** | 5 humans vs 5 humans | **Only permanently owned Vanguards** | Future target: Draft Pick ban/pick structure, entry gates and party-rank restrictions in §2 and Parties Bible; weekly rotation cannot be used. |
| **Co-op vs AI — Beginner** | 5 humans vs 5 enemy AI Vanguards | Owned Vanguards **plus current weekly free rotation** | Full standard battleground/match rules; Beginner enemy AI behavior. |
| **Co-op vs AI — Intermediate** | 5 humans vs 5 enemy AI Vanguards | Owned Vanguards **plus current weekly free rotation** | Same battleground and rules; Intermediate enemy AI behavior. |
| **Custom/private — invite only** | Host-configured two-team battleground, **zero to five Vanguards per side, at least one human overall**; solo 1v0, uneven, or mixed human/AI compositions are permitted | Each human: **owned plus weekly free rotation**; AI: **any released Vanguard** | Host places human/AI slots, chooses each AI's Beginner/Intermediate difficulty, and can customize settings (e.g. starting Gold, objective timings, selection rules). **No Account XP or Vanguard Mastery.** See Custom Matches Bible. |

- No role or lane assignment is enforced in the battleground.
- Among **standard matchmade modes**, only Co-op vs AI permits the same Vanguard on opposing teams (cross-team mirror pick). In **matchmade PvP**, all ten picks are globally unique, including Casual Select, Draft Pick and eventual Ranked. Custom-match Vanguard-selection rules are host-adjustable; the exact duplicate/mirror options have **not** yet been chosen, and may not override human ownership/rotation access.
- All persistent XP/mastery exclusions and personal-loss handling are defined in the Account and Match Flow bibles. The fact that a match can be played does not automatically mean it yields each kind of reward.
- Co-op uses the same standard match timeline, objectives, combat rules and surrender vote rule (**available after 15:00; three of five human teammates must vote yes**) as the main battleground, except for explicitly documented human-versus-AI composition and pick/mirror access.
- **Parties of 1–5 players** can enter each available matchmaking queue; empty allied slots are filled with other human players. All required human participants accept a found match before select (**ten** for PvP, **five** for Co-op vs AI); there is **no launch penalty for declining/missing the acceptance prompt**. Matchmaking must **never** assemble two accounts into the same match when either has blocked the other, regardless of queue-time impact. See Parties & Social Bible.

## 2. Locked — Ranked access

- **Ranked is not an initial-launch requirement.** When introduced, Ranked requires **both Account Level 30 or higher and 20 permanently owned Vanguards**.
- The permanently unlocked tutorial starter **counts** toward the 20 owned; **weekly rotation does not count**.
- **Ranked selection and any teammate trade must leave each human player assigned a Vanguard they permanently own**. A free-rotation-only Vanguard may not be played in Ranked even when currently free.
- Ranked uses the Battleground Bible's existing 3-bans-per-team and 10 globally unique picks format; this bible does not redefine bans or pick sequence.
- The owned-roster threshold provides selection resilience in a ten-pick/six-ban draft. It does **not** guarantee every individual player has all 20 picks available at any moment or define how the draft handles unusual selection/lock failures.
- **Roster readiness clarification:** The current roster target is **20 designed Vanguards** (Cairn, Tavra, and Vitra included), meeting the *global mathematical minimum* of 16 distinct draftable Vanguards on paper. Ranked still requires **at least 20 released and permanently ownable Vanguards**, each eligible player's 20 actual owned Vanguards and Account Level 30, and completion of the deferred Ranked systems. Designed roster size alone does not establish released availability, player eligibility, or Ranked launch readiness. Earlier 17-Vanguard readiness statements are superseded.
- **Ranked party size target is 1–5 players**, all meeting the owned/level gates and all ranks **within one rank step across the entire party**. No middle-ranked player may bridge members more than one rank apart. Exact tiers/divisions and edge-case eligibility remain open; see Parties & Social Bible.
- Ranked rating, placements, divisions, matchmaking, seasons, penalties and rewards remain **open design**; do not invent them or make Ranked a launch blocker.

## 3. Locked — 12-slot weekly free Vanguard rotation

- Each weekly rotation features **12 distinct Vanguards**, made playable without permanent ownership in **non-Ranked PvP and Co-op vs AI only**.
- Vanguards are **randomly selected by a trusted system under configurable roster rules** that support a spread of mechanical accessibility and playstyles. No duplicate Vanguard may occupy two slots in the same weekly rotation.
- A newly released Vanguard first becomes **eligible for rotation one week after release**. It remains available for immediate permanent purchase with either currency from launch.
- Ordinarily, a Vanguard included in this week's rotation **must sit out the following week's rotation**. After that break, it returns to the eligible selection pool.
- **Fallback when avoiding repeats would leave fewer than 12 distinct choices:** Relax the **one-week no-repeat restriction first**, for as many Vanguards as needed to fill the 12 distinct slots. **Never** relax the one-week new-release waiting period to fill slots.
- Previously earned mastery on a free-rotation Vanguard belongs permanently to the player even when that Vanguard leaves rotation. Every Vanguard remains visible in the Collection regardless of weekly access.

### Rotation population math and validation

- Guaranteeing *zero consecutive-week repeats* for 12 slots requires **at least 24 release-eligible distinct Vanguards**. The planned 25–30 roster can support that after excluding new-release waiting periods; a **20-Vanguard release-eligible pool still cannot**, so back-to-back repeats remain normal under the agreed fallback until there are at least 24 eligible choices.
- To provide 12 **distinct** slots at all, the release-eligible selection pool must contain **at least 12 Vanguards**. If it does not, neither relaxing consecutive-week repeat restrictions nor duplicating a slot can legally fill the rotation. This is an unresolved **launch/content configuration precondition**, not permission to insert duplicate slots or waive the new-release delay.
- Actual week-boundary timing/time zone, rotation activation with matches/champion select already underway, and specific category mix/randomness policy remain to be specified; selection must be reproducible/auditable, and clients must not choose rotation contents.

## 4. Locked — Co-op vs AI

- Both Co-op queues match **five real human teammates** against **five AI-controlled enemy Vanguards**. **Never fill vacant allied slots with friendly AI**; matchmaking waits for five humans rather than substituting bots.
- Provide **two distinct matchmaking queues**, **Beginner** and **Intermediate**. Difficulty changes **enemy AI behavior only**; it does **not** change the map, combat, economy, victory, surrender, progression or other gameplay rules.
- The five opposing AI Vanguards are **randomly drawn from the current weekly free-rotation roster**. Enemy AI team picks remain distinct from one another. A human may pick a Vanguard also played by an enemy AI—**the sole cross-team mirror exception**.
- Human players may select permanently owned or current free-rotation Vanguards; AI picks do not confer ownership or Ranked eligibility. **Custom match bot permissions differ intentionally**: hosts may place friendly or enemy AI and choose any released Vanguard and per-bot difficulty; this does **not** change Co-op's fixed human/AI team requirement.
- **Account XP is earned only below Account Level 10**; Co-op remains open beyond that level without account XP. **Vanguard Mastery remains earnable in Co-op at all account levels.** Custom/private games grant neither reward.
- AFK/disconnect, remake, surrender, outcome adjudication, and pause behavior should follow the ordinary Match Flow Bible wherever defined for human participants; detailed AI voting/disconnect semantics and AI tuning remain to be designed, not guessed.
- Queue matchmaking criteria, AI logic and anti-abuse criteria remain separate, open design questions.

## 5. Open design work

- Future Ranked rating/divisions, placements, season cadence, adjacent-rank validation details and exact matchmaking policies; **initial launch proceeds without Ranked**.
- Core acceptance/party/decline/dodge policy is in the Parties Bible. Exact matchmaking/queue restoration algorithms, invitation lifetime, reconnect edge cases and champion-select eligibility failure handling remain open.
- Free-rotation weekly boundary/snapshot behavior and configuration for fair random selection.
- Exact custom host-configurable options, draft/mirror settings, solo 1v0 match-end/scoreboard behavior, live custom spectator access and custom-lobby lifecycle are **open in the Custom Matches Bible**. Human selection is owned-or-rotation; bot Vanguard selection is any released Vanguard. Custom games grant no Account XP/Mastery.
- Initial released roster must reach at least 20 permanently ownable Vanguards before Ranked can satisfy the access gate, and 12 release-eligible distinct Vanguards before the full weekly rotation can operate.

**This bible records the decisions reached so far; it is not a directive to implement unfinished matchmaking, Ranked or service architecture.**
