# Veyra Account, Collection & Mastery Bible

**Version:** 0.1 — Captured pre-production decisions; further design discussion required  
**Status:** Working design canon for the rules explicitly marked *Locked*; not an implementation-ready specification  
**Scope:** Persistent account progression, tutorial rewards, Vanguard ownership/collection, account currencies, storefront principles, and Vanguard Mastery.  
**Companion documents:** [Client & Platform Bible](Veyra_Client_Platform_Bible_v0.1.md) for launcher/pregame/in-game responsibilities and [Modes & Access Bible](Veyra_Modes_Access_Bible_v0.1.md) for queue access, weekly rotation, Ranked eligibility, and Co-op vs AI. [Economy & Progression Bible](Veyra_Economy_Progression_Bible_v0.1.md) owns **in-match** Gold, XP, levels and purchasing; those resources are separate from persistent account progression.

> **Design ownership:** The trusted account/progression and commerce backend, not the pre-game UI, owns durable account levels, balances, entitlements, ownership, mastery and reward awards. Match results and contributions must originate from verified authoritative match records. Service/provider architecture remains undecided. All tuning, prices, thresholds, progression curves, reward rates and milestone tables must be editable, validated data—not C++ or Blueprint magic numbers.

## 1. Locked — first-time tutorial and starter ownership

- A new account completes a **mandatory first-time tutorial** before normal play.
- The tutorial teaches movement, abilities, last hitting, XP, item shopping, Team Flux and how to win the match.
- During the tutorial, the player tries **all 3–5 curated starter Vanguards** and, at first completion, chooses **one** to unlock permanently.
- The tutorial grants account XP and the starter Vanguard **on first completion only**; replay remains available for practice and provides no repeat completion rewards.
- The exact tutorial encounters and starter identities remain to be curated. This document does not silently assume that replay grants mastery or match rewards.

## 2. Locked — account levels and account XP

- Persistent **Account Level is uncapped** and is independent of the Vanguard's **in-match Level 18 cap** and of each Vanguard's uncapped Mastery Level.
- Completed matchmade **Ranked and non-Ranked PvP** matches grant account XP.
- **Co-op vs AI grants account XP only while the account is below Level 10**. The mode remains playable thereafter, with no account XP.
- The tutorial grants account XP only on its first completion. **Custom/private matches do not grant account XP.**
- A successful **remake** grants no account XP.
- A player whose personal AFK/disconnect loss penalty remains in force at result adjudication receives **no account XP**. A returning player who earns end-of-match forgiveness under the Match Flow Bible receives the ordinary account XP applicable to that match/mode.
- Match XP is primarily based on **match duration**, plus a **modest winning bonus**. Exact values/formulas remain tunable data; do not imply a contribution-based account XP multiplier has been approved.
- XP needed per account level **grows gradually through Level 100**, then becomes **constant per level indefinitely**; never impose an account-level ceiling.
- There is **no daily cap** on account XP or on account-level-up earned currency.
- An **optional, dismissible** continuous-play break reminder appears in the **pre-game client after a match**, after a configurable play duration; no forced break, gameplay restriction, or XP penalty.

**Eligibility reconciliation:** Co-op XP requires the account-level gate **and** all ordinary valid-match/no-penalty conditions. Exact snapshot timing for the Level 10 gate (at queue entry, match start, or completion) is still open; no client-side interpretation may decide it independently.

## 3. Locked — currencies, Vanguard ownership and cosmetics

| Persistent resource | Earned/spent | Restrictions |
|---|---|---|
| **Earned Vanguard-unlock currency** | Awarded at **every account level-up**; buys permanent Vanguards. | Does **not** purchase skins. Never conflate with in-match Gold. |
| **Premium currency** | Purchased with real money **or** earned at account milestones; buys Vanguards and cosmetic skins. | Its source does not change its purchasing power. Never conflate with Team Flux or a Flux Spell. |
| **Permanent Vanguard entitlement** | Starter choice, or purchase using either currency. | Owns the Vanguard permanently and includes its **default skin**. |
| **Skin entitlement** | Cosmetic skin purchased using premium currency. | Skins confer **no gameplay advantages** and must preserve competitive clarity/readability. |

- **Every Vanguard can be purchased with either currency immediately upon release.** There is no premium-only new-release window.
- At release, an **optional premium bundle** may pair the Vanguard with a skin for less than their separate premium prices. Standalone Vanguard purchase remains available with either currency, and standalone skin purchase with premium currency.
- Vanguard purchase prices are **individual**, informed by beginner accessibility, mechanical complexity and content/engineering complexity; simple starter-friendly Vanguards are generally cheaper than highly complex ones. Both currency prices are editable storefront data.
- **Each season, the studio selects 15 Vanguards for a permanent price reduction in both earned and premium currencies**; no price rebounds in later seasons. A Vanguard already at its configurable minimum is excluded. A previously reduced Vanguard may be selected again in a later season if above minimum.
- Exact season timing, reduction amounts, minimum prices, and what to do if fewer than 15 Vanguards remain above the minimum have **not** been settled. Do not invent an automatic pricing algorithm or silently break the minimum-price rule.
- Payment provider, refunds/chargebacks, receipts, gifting, ownership-independent skin purchases, fraud controls and regional pricing need separate commerce design before implementation.

## 4. Locked — always-visible Vanguard Collection

- **Every released Vanguard is always visible in the Collection** regardless of ownership, weekly rotation availability, Ranked access or mastery progress.
- A Vanguard's collection page shows the account's own **Mastery Level, lifetime mastery points and mastery emote progression** even when that Vanguard is currently not playable for the account.
- **Visibility is not permission to select.** Selection and trades must validate entitlement and the current mode's access rules, as defined in the Modes & Access Bible.
- Mastery earned through free rotation remains attached to the account/Vanguard and carries forward unchanged when the player later purchases that Vanguard.

## 5. Locked — Vanguard Mastery

### 5.1 Lifetime progression

- **Each Vanguard has an independent persistent Mastery Level and lifetime mastery-point total.** Both progress indefinitely; **there is no maximum Mastery Level**.
- Eligible completed **matchmade** games award mastery for the Vanguard actually played, including Ranked, non-Ranked PvP, and Co-op vs AI. **Custom/private matches award none.**
- Co-op vs AI mastery remains earnable **after Account Level 10**, even when account XP in that mode has ended.
- A player **can earn and retain mastery on a weekly free-rotation Vanguard they do not own** and can use that Vanguard's mastery emote while playing it. Purchasing it later does not reset mastery.
- A losing match **never subtracts mastery points or levels**. Mastery points, levels and emote upgrades cannot be lost through match results.
- Mastery point awards have a **match-duration baseline**; individual performance can increase the award, and winning grants a bonus. Scoring must credit contributions appropriate to the Vanguard and role—including protection, utility, objectives and team play, not just kills. Exact formulas, abuse mitigation, outcome eligibility for penalized/remade games, and XP-to-mastery-level curve remain open design/tuning decisions; do not claim a specific score formula is already locked.

### 5.2 The mastery emote — the sole Mastery reward family

- The **mastery emote** is the sole Vanguard Mastery cosmetic reward family. No mastery-earned loading-screen borders, profile borders or titles.
- Each Vanguard's emote **always displays the account's current uncapped Mastery Level** for that Vanguard.
- Its **visual appearance upgrades at configurable mastery milestones**. Once a new appearance is earned, it **permanently supersedes the old one**: players cannot equip or revert to an earlier appearance.
- Emotes provide no combat effects, stats, stealth, hitbox, vision or other gameplay advantage. Milestone thresholds, assets, visibility/readability rules and visual performance budgets are configurable/validated.
- Loading-screen borders belong to **Ranked rewards and/or events**, not Mastery. Their separate award details remain outside this bible.

### 5.3 No global mastery leaderboard

- **Do not build public per-Vanguard or global mastery leaderboards, rankings, leaderboard endpoints or public mastery records.**
- Collection shows the player's own progression; the player may show their current level to others with the mastery emote. Public leaderboard competition is **not** a planned feature.

## 6. Data, authority & consistency requirements

- Use stable account, Vanguard, entitlement, cosmetic and reward IDs; never key persisted ownership/mastery to display names.
- All progression grants and purchases must be **server-validated, idempotent and durable**, including retries after disconnects, client crashes or duplicate match-result delivery. The client must never mint XP, earned currency, premium currency or mastery by reporting its own match outcomes/performance.
- A match's result, mode, selected Vanguard, valid participation, duration, and individual penalty/forgiveness state must arrive through a trusted adjudicated result. The Match Flow Bible defines the **personal loss and forgiveness** rules; this document owns eligibility for **persistent** account XP/mastery rewards.
- Reward grant, balance change and entitlement change must be reconcilable without awarding twice. Persistence model, service boundaries and payment-specific technical decisions require an approved architecture decision before implementation.
- All proposed numbers in this bible are design defaults/data, not implementation constants.

## 7. Deliberately open — return to these when design discussion resumes

1. Exact tutorial content/starter roster and tutorial mastery eligibility, if any.
2. Final account XP rates, Level 10 gate snapshot, level curve and milestone-premium-currency award table beyond the agreed milestone examples (**30, 50, 75, 100, etc.**).
3. Mastery award formula, point-to-level curve, milestone visuals and handling of AFK penalties/remakes for mastery.
4. Store prices/floors and seasonal reduction schedule/insufficient eligible candidates.
5. Commerce/refund/chargeback policy, skin entitlement prerequisites and account/service architecture.
6. Account profile/privacy, other social/pregame systems, Ranked rating/season rewards and other client policies.

**Do not treat this v0.1 document as a signal that the broader account/client design is finished or that implementation should begin.**
