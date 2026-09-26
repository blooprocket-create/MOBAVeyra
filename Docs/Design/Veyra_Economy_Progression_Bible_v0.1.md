# Veyra Economy & Progression Bible

**Version:** 0.1 — Initial economy, leveling, shopping-queue, and buyback rules  
**Status:** Current working design canon  
**Scope:** Gold and XP sources/distribution; leveling; skill progression; shop and inventory economy; fountain delivery; death economy; and buyback.

**Tuning:** All prices, payout amounts, radii, participation windows, cooldowns, curves, caps, multipliers, item refund values, and timing values described as prototype settings must be authorable in gameplay data and changed without source-code edits. Examples illustrate behavior rather than final balance.

**Companion documents:** The Combat Bible owns actual-death resolution, credit for kills and assists, damage, death-prevention saves, and target attribution. The Battleground Bible owns map structure, Flux objectives, and Flux Spell thresholds. The Item Bible owns item recipes, stats, and effects. The Vision Bible owns vision tools, stealth interactions, and placed wards. This document owns *economy and progression rewards*, not those domains' underlying mechanics.

## 1. Design intent and economic resources

- Each Vanguard has **individual Gold and XP**. These resources are not pooled, transferred, or traded between players.
- **Team Flux is a separate shared team resource**, not currency and not individual XP. It primarily strengthens allied lane Fluxborn and, through *permanent* Flux thresholds, unlocks Flux Spell slots. It does not grant automatic escalating Vanguard stats.
- No **passive Gold or passive XP** generation. Players earn both through explicitly specified sources. Fixed equal **starting Gold** is the one guaranteed initial Gold grant, sized for an opening Tier 1 item or item plus consumable.
- Gold, XP, item and progression calculations use full fractional internal precision. UI may round amounts for display; rounding never changes affordability, XP thresholds, splits, or stored balances.
- Everyone starts at Vanguard **Level 1**. The level cap is **18**.
- Map structure permits free lane and role assignments. Reward sharing and opportunity costs, not forced role locks, shape play.

## 2. Reward ownership and common eligibility

- A kill by a Vanguard-owned damage-over-time effect, summon, or companion is attributed to its owning Vanguard when ordinary combat attribution rules grant that credit.
- Living/dead state, proximity, participation, and kill credit are **separate** tests. Receiving remote credited Kill Gold does not grant remote proximity XP.
- Walls, ordinary fog of war, and Dense Fog **do not obstruct distance-only Gold/XP proximity checks**. Visibility/target-acquisition rules remain separate.
- Dead Vanguards do not collect **proximity farm XP** or proximity participation Gold. They may still receive earned kill or assist rewards from effects/contributions credited after their death, subject to the distinct rules below.
- Only actual, finalized Vanguard deaths pay kill/assist Gold and XP. A successful death-prevention or revival-style save grants none of those death rewards and does not reset a bounty or create a death streak.
- If an effect lists no economy reward, do not manufacture one from generic damage, presence, objective participation, or a death animation.

## 3. Lane Fluxborn: last hits, participation Gold, and XP

### 3.1 Last-hit Gold

- An enemy lane Fluxborn's current listed Gold is paid **100% to the Vanguard credited with its killing blow**, including a remote last hit from their existing DoT or an owned summon/companion.
- The last hitter receives the full last-hit reward **even outside ordinary Gold/XP proximity**.
- The last hitter does **not** also receive the nearby 10% participation reward for that same Fluxborn.
- If friendly Fluxborn, a structure, or another unowned effect secures the death, the full last-hit Gold is **unclaimed**, not awarded to the nearby Vanguard who damaged the unit.

### 3.2 Nearby participation Gold

- If **any allied Vanguard damaged that enemy Fluxborn during the configured recent-combat window**, each *other living, nearby allied Vanguard* receives **10% of the Fluxborn's current listed Gold**.
- This can pay nearby allies even when an allied Fluxborn or Spire delivers the final blow. Presence alone does not turn on the reward; an allied Vanguard must have dealt recent damage.
- A qualifying nearby allied Vanguard need not personally be the one who dealt that damage. The last-hit owner is excluded from the extra 10% regardless of where they stand.
- If **no allied Vanguard** damaged it recently, no allied participation Gold is generated. If no one secures Vanguard-attributed last-hit Gold, the 100% reward remains unclaimed.
- The 10% reward is **per qualifying nearby Vanguard**, not one pool split among allies.

### 3.3 Fluxborn XP

- Every enemy Fluxborn death pays lane XP to **living nearby allied Vanguards**. Neither last hitting nor recent allied Vanguard damage is required.
- Only one eligible Vanguard nearby: that Vanguard gets **100% of base XP**.
- Two or more eligible nearby Vanguards: create **one shared XP pool equal to 120% of base XP**, then divide it equally among those eligible Vanguards. Examples: two receive 60% each; three receive 40% each.
- A Level 18 Vanguard is **excluded from XP eligibility and the sharing divisor**. If one leveling Vanguard stands beside four Level 18 allies, that leveling Vanguard still gets the solo 100% base XP. If every nearby ally is Level 18, XP goes unclaimed.
- Ordinary XP radius and base reward values are data-driven.

## 4. Rewards for Flux-strengthened enemy Fluxborn

- Active Team Flux strengthens *that team's* Fluxborn on all lanes. Destroying their stronger Fluxborn provides a **modestly increased enemy farm reward**, offering a small economic opening to the opposing team.
- Current prototype rule: for every **25 active Team Flux** on the Fluxborn's team, increase that Fluxborn's **Gold and XP reward value by 1%**, capped at **+10%**.
- Both active temporary and permanent Flux contribute to this *farm-reward* calculation. Evaluate the relevant active Flux/reward value **at the Fluxborn's death**.
- The increased listed value feeds the ordinary last-hit, participation Gold, and XP-sharing rules above. This is a small tuning lever, not an automatic comeback guarantee.

## 5. Enemy Vanguard kills: Gold, assists, First Blood, and bounty

### 5.1 Base kill and assist Gold

- A Vanguard has a **fixed prototype base kill-Gold reward**, independent of their level. Death-streak devaluation can modify their **current** base reward.
- The credited killer gets **100% of the victim's current, devaluation-adjusted base kill Gold**.
- Additionally create **one Assist Gold pool equal to 50% of the victim's current, devaluation-adjusted base kill Gold**. Split that pool evenly among all qualifying credited assistants; it is **not** deducted from the killer's base award.
- No assistants means no Assist Gold pool is paid. Neither proximity nor being alive at payout is required for an otherwise valid kill/assist **Gold** credit; the Combat Bible determines meaningful participation.
- Bounty and First Blood bonuses are separate from the base Gold used to compute the Assist Gold pool.

### 5.2 First Blood

- The match's **first actual enemy-credited Vanguard kill** grants the killer an extra **50% of the normal base kill Gold** at prototype tuning. The bonus is in addition to the standard current kill payout; it does **not** enlarge the Assist Gold pool.
- Only **one First Blood** can be awarded in a match. A simultaneous candidate is resolved by a deterministic authoritative server tie-breaker.
- An environmental Execution without enemy kill credit and a lethal event prevented by a save **cannot** claim First Blood.

### 5.3 Visible kill-streak bounty

- Consecutive **kills**, not assists, build a publicly visible bounty on that Vanguard. Bounty increments, thresholds, and maximum are data-driven.
- On that Vanguard's next **actual enemy-credited death**, the entire bounty is paid **only to the credited killer**, separately from normal kill/assist Gold, and that bounty resets.
- An environmental Execution does **not** pay or clear the bounty; it survives death and respawn, preventing a player from clearing a bounty through an uncredited jungle/terrain death.
- A death prevented by a save is not a bounty-claiming event.

### 5.4 Death-streak devaluation

- Consecutive **enemy-credited deaths without a takedown** progressively reduce that Vanguard's **current base kill-Gold value**. Prototype illustration: 100% → 85% → 70% → 55% → 40% minimum.
- Each subsequent **takedown (Kill or Assist)** restores **one step** of base kill-Gold value, up to 100%; it does not instantly clear the entire death streak.
- The Assist Gold pool is based on the same **current reduced base reward**. A separate bounty remains separate and is not reduced by death-streak devaluation.
- An environmental Execution neither increases nor resets the existing enemy-death devaluation, although it remains an actual death with normal respawn consequences.
- There is **no additional repeat-target/farming-the-same-victim penalty**. Disconnected/Vanguard AFK status does not itself modify combat rewards; match-state and reconnect handling belong in the match-flow rules.

## 6. Vanguard kill XP

- Base kill XP is chiefly a **data-driven function of the victim's level**. Unlike base kill Gold, kill XP may increase for higher-level victims.
- The **credited killer always qualifies for kill XP participation regardless of their distance from the death or whether they have already died**, provided they are below Level 18.
- A credited assisting ally qualifies for the **kill XP pool** only while **living, within the kill-XP radius, and otherwise assist-qualified**. Remote assistants may receive Assist Gold but not kill XP. Merely standing nearby without assist credit grants no kill XP.
- For the participation bonus, count the killer plus all living, nearby credited assistants **including those already Level 18**. The killer counts even if dead, distant, or Level 18. Assistants who are distant or dead do not count toward the bonus.
- **Total kill XP pool = base kill XP × [1 + 20% × (qualifying participants − 1)].** One through five participants yield 100%, 120%, 140%, 160%, or 180% total base XP. This is **one** expanded pool, not a full kill-XP award per participant.
- If the victim is higher level than the **killer at the instant of the kill**, apply a modest data-driven higher-level-victim multiplier **once to the shared pool**, not separately per recipient. No such multiplier applies for an equal- or lower-level victim.
- Divide the final pool **equally among participants who can actually gain XP**. Level 18 participants receive **zero XP** and are excluded from the divisor but **still count toward the participation bonus**. Example: a Level 18 killer and one living, nearby Level 14 credited assistant produce a 120% pool; the Level 14 assistant receives all 120%.
- If every otherwise qualifying participant is Level 18, the pool goes unclaimed. The Level 18 killer still receives their ordinary Kill Gold and bounty as applicable.
- The killer's unusual remote/postdeath kill-XP eligibility does **not** extend to remote lane minion XP, wildlife XP, or Spire XP (Spires grant none).

## 7. Jungle wildlife Gold and XP

- Wildlife Gold goes **100% to the Vanguard credited with each creature's killing blow**. An owned companion/summon killing the creature gives its owner that Gold, even if the owner is outside XP range.
- There is **no lane-style nearby 10% participation Gold** on wildlife.
- Wildlife Gold and XP are awarded **for each individual creature at its death**, including multi-creature camps; rewards do not wait for a final camp clear.
- On a contested Vanguard-credited kill, the **killing Vanguard's team** owns that creature's Gold and proximity XP opportunity. Damage dealt earlier by the opposing team earns that team no partial farm rewards. A steal therefore transfers Gold and the eligible nearby XP opportunity to the securing team.
- Eligible living nearby Vanguards on the securing team share each creature's XP under the same rule as Fluxborn: one receives 100% base XP; multiple share a single 120% pool equally. Level 18 players are excluded from XP sharing and do not dilute leveling allies.
- If an **unowned environmental/structure effect** kills wildlife without Vanguard-attributed kill credit, the Gold is unclaimed; living nearby Vanguards may still receive proximity XP under ordinary XP-sharing rules. No team is credited a Vanguard-owned securing blow in that case.
- Summons and companions **cannot serve as remote XP proxies**. Their owner needs to be personally within ordinary XP range to receive wildlife XP.
- Every summon/companion has a defined maximum operating distance from its owner. Exceeding it stops independent fighting and initiates return or an ability-specific recovery/despawn after a grace period. Remote full-map autonomous farming is not permitted.
- Killing enemy summons, clones, decoys, and similar created units pays **no default Gold or XP** unless that specific unit is explicitly marked reward-bearing.
- Wildlife identities do not evolve during a match; simple temporary camp traits/buffs belong to the Battleground Bible.

## 8. Structures, neutral objectives, vision, and non-reward actions

### 8.1 Spires

- Destroying a Spire creates a **fixed local Gold pool** divided equally among allied Vanguards who **meaningfully damaged that Spire within the configured recent-participation window**.
- Eligible recent contributors may receive their share **even if dead or away** when the structure falls. The last hitter receives no additional Gold merely for taking the final hit.
- If allied Fluxborn finish the Spire, the qualifying recent contributors still split the pool. If **nobody qualifies**, the pool remains unclaimed; the team still receives its ordinary permanent Team Flux.
- The **first Spire destroyed anywhere in the match** additionally grants a **small global Gold bonus to every member of the destroying team**, including nonparticipants and players elsewhere or dead. This team bonus applies even if allied Fluxborn deliver the final blow.
- Spire damage/chunks grant **no per-hit Gold** and Spires give **no XP**.
- Spire destruction continues to grant the team its map-defined permanent Team Flux (prototype +25).
- **Base-defense towers pay exactly like lane Spires:** the same recent-contributor Gold pool rules, no per-hit Gold and no XP. The first base-defense tower destroyed counts toward the **first Spire destroyed** team bonus if no lane Spire has fallen yet. Their permanent Team Flux is set by the Battleground Bible §18.

### 8.2 Inhibitors, Flux Wells, Prime Well

- Destroying an inhibitor grants **no Gold or XP**; its relevant reward is temporary Team Flux under battleground rules.
- Securing a jungle Flux Well grants a **small fixed Gold pool**, prototype approximately the Gold of one ordinary jungle creature, **split evenly only among Vanguards actively contributing to the capture at the moment it is secured**. A solo capturer gets the entire pool; allies elsewhere get no share.
- A Flux Well grants **no XP**; its main reward is temporary Team Flux and the associated map pressure.
- The enemy Prime Well's destruction ends the match. There is no additional post-victory Gold/XP farming payout.

### 8.3 Wards, poke, assistance

- Destroying an enemy ward grants a **small fixed Gold payment only to the destroying Vanguard**; no team share, nearby Gold, or XP.
- Simply damaging, poking, crowd-controlling, revealing, healing, shielding, or buffing a Vanguard pays **no generic per-action Gold or XP**. Meaningful contribution can lead to an ordinary assist payout; specific item effects may explicitly grant Gold under their own trigger/cooldown rules.
- No generic Gold/XP is generated for destroying unmarked player-created units or for ordinary terrain/vision interactions.

## 9. Leveling and skill progression

- All Vanguards start at **Level 1**, use the **same base, data-driven XP-to-level curve**, and cap at **Level 18**. XP gained past the cap is discarded.
- XP **carries over fully** through level boundaries. A single large reward may grant multiple levels and skill points immediately, without waste at intermediate thresholds.
- On level-up, a Vanguard gains their **Vanguard-specific data-driven stat growth** automatically.
- Increasing maximum Health or another maximum resource **adds the same flat increase to the corresponding current Health/resource**, keeping the absolute deficit unchanged; it does **not** refill the bar. This specific level-up behavior supersedes generic proportional-max-stat adjustment defaults where they differ.
- Gain **one skill point per level**. Skill points may be spent immediately via HUD/hotkey or saved indefinitely; allocation is instant and does not require leaving combat.
- The standard kit has **three basic abilities with a maximum of five ranks each** and an **ultimate with a maximum of three ranks**: 5 + 5 + 5 + 3 = 18 total ranks.
- **No Vanguard-level/rank gate on Q/W/E.** For example, a player may invest their first five points in one basic ability at Levels 1–5 while leaving the others unlearned.
- Ultimate rank eligibility opens at **Levels 6, 11, and 16**. Unusual champion kits may have **explicitly documented exceptions**; do not impose standard Q/W/E/ultimate structure on a canon exception. Documented exception: a ride state's replacement abilities share the rank of the basic ability in the same slot and cost no extra skill points (Combat Bible §56). Documented exception (2026-09-25): Angeru's two stances share each Q, W and E rank, Q, W and E rank to 6, and his stance switch costs no skill point and never ranks, keeping the 18-point total (Character Bible §15).
- **Allocated points cannot be respecced during the match.** Saving unspent points is allowed.

## 10. Inventory, starting Gold, and fountain shop

- Every Vanguard has **six normal inventory slots** shared by equipment, Boots, components, and consumables.
- The **one dedicated vision-tool slot and two dedicated Flux Spell slots** do not consume any of those six item slots.
- Starting Gold is equal for every Vanguard, independent of selected Vanguard or role, and can buy an opening Tier 1 item or a modest item-and-consumable combination.
- Items **do not auto-combine**. Completing a recipe requires its explicit purchase/completion cost.
- The fountain remains the **only place to receive, equip, complete, or activate newly purchased items**, and the only place to sell equipped items or change equipped vision tools and Flux Spells.
- Players can browse the full shop and **pay for regular purchases remotely** while alive elsewhere on the map, but these purchases become an ordered **pending-delivery queue**, not active inventory. They grant no stats, passives, actives, consumable effects, or other gameplay benefit until delivered at the fountain.
- Shopping while dead is allowed; equipment bought during the death timer is assigned at the fountain and cannot benefit the Vanguard until their next respawn (ordinary or buyback).
- The item-shop interface is available for browsing and planning anywhere. Recall returns a living Vanguard to their own fountain after its normal channel completes.

## 11. Remote purchase queue and fountain delivery

### 11.1 Queueing, capacity, and payment

- Remote item purchase **spends Gold immediately** and records an ordered pending purchase. This is *not* a remote item equip and provides no battlefield power.
- Queue validation simulates the existing six active inventory slots **plus the pending post-delivery outcomes**. It must reject a purchase whose eventual delivery cannot fit within six slots after permitted recipe component consumption. Pending purchases are **not free extra inventory/storage slots**.
- A pending upgrade can reserve existing components as recipe inputs **without consuming, unequipping, disabling, or changing them while the Vanguard is still in the field**.
- Multiple queued purchases resolve **in queue order** when delivery occurs. A component bought earlier in the queue may feed a later recipe. Two queued recipes cannot both consume the same single component.
- Each pending transaction must have enough available Gold at purchase time; no negative balance, unsecured Gold debt, or speculative future farm is permitted.

### 11.2 Delivery and item behavior

- Returning alive to the **own fountain** automatically processes all valid pending transactions in order. Each transaction consumes its recipe components **at delivery**, frees resulting slots, and equips/delivers the newly completed or purchased item.
- Until delivery, existing owned components continue to function normally, including their stats and effects.
- **Actual death also causes pending purchases to be delivered to the Vanguard's fountain inventory**; the Vanguard can continue shopping during the death timer. The new equipment has no combat effect until respawn.
- Delivery on death does not itself shorten the death timer and does not count as a combat revive.

### 11.3 Cancellation, dependencies, and invalidation

- An **undelivered purchase may be canceled from anywhere for a full 100% refund**. Pending items never granted gameplay value.
- Canceling a queued transaction revalidates the remaining queue. If a later transaction depended on the canceled item, that dependent purchase is also canceled and refunded in full.
- Revalidate the whole queue whenever active inventory or pending entries change. More free space may preserve or permit purchases; a sold, consumed, or otherwise missing required component invalidates its dependent upgrade.
- Every invalidated pending purchase and its now-invalid dependents are **automatically canceled and fully refunded**. Unrelated valid purchases remain queued; already delivered items are unaffected.
- Neither purchases nor upgrades may consume a component twice or deliver an impossible inventory state. The authoritative server owns transactions and applies inventory/Gold changes atomically.

## 12. Selling, full undo, and refund boundaries

- **Equipped/delivered item selling happens only at the fountain.**
- Current prototype **normal item resale** is **70% of total Gold spent** on that item's present assembled form, including component and recipe costs, once the item has been used or the Vanguard has left the fountain after delivery.
- A purchase can instead be fully **undone for 100% Gold** while the Vanguard remains at the fountain and the relevant item has **not been used, consumed, activated, or otherwise provided gameplay benefit**. Undo may cover multiple eligible purchase steps, including components, without yielding extra Gold or duplicating components.
- Normal partial/used consumables may sell for reduced value or no value as explicitly defined for that consumable; never assume a spent consumable can be refunded in full.
- An **undelivered remote purchase** always falls under the simpler full-cancellation rule above, independently of the ordinary fountain undo/resale state.
- In-match Gold spent to replace a Flux Spell or vision tool is **not a refundable normal-item resale** merely because the player later changes back. Initial loadout/vision grants have separate rules below.

## 13. Vision tools and Flux Spells: separate equipment economies

### 13.1 Vision tools

- Each Vanguard begins with **Persistent Ward equipped for free** in the dedicated vision slot. Sweeper and Quick Sight are alternative tools as fully defined by the Vision Bible.
- Vision-tool replacement is allowed **only at the fountain shop**, costs a **small flat data-driven Gold amount every time**, **including switching back** to Persistent Ward.
- Previously placed wards survive tool swaps and remain on their normal lifetimes. Vision-tool cooldowns are not reset through swapping/death, under Vision Bible rules.
- Destroying a hostile ward pays only its specified fixed Gold bounty to the destroyer, as above.

### 13.2 Flux Spells

- Before each match, every Vanguard **preselects up to two Flux Spells for free**. They begin **equipped but locked**, not purchased after loading into the match.
- The first slot unlocks at prototype **25 permanent Team Flux**; the second at **75 permanent Team Flux**. The equipped spell becomes usable **immediately on threshold achievement**, without a return to the fountain.
- Temporary Flux **never unlocks** a Flux Spell slot. Permanent slot unlocks persist; casting consumes **no Team Flux** and follows the spell's normal cooldown.
- **Only at the fountain shop** may a player replace a preselected/equipped Flux Spell. Each replacement costs Gold and **does not bypass** the target slot's permanent-Flux unlock requirement.
- Both spell slots are separate from the normal six-slot inventory and from the vision-tool slot. Gold purchase queuing elsewhere on the map does not remotely swap equipped Flux Spells.

## 14. Death, respawn, and resource retention

- An **actual finalized death** starts a respawn timer at the Vanguard's fountain. The timer grows based on **Vanguard level and elapsed match time**, with a data-driven curve; Gold, bounty, and kill count do not directly set its duration.
- Death **never deducts existing Gold, XP, or levels**. Its ordinary economic cost is lost farm, map pressure, and time.
- Dead Vanguards cannot gain nearby lane/jungle farm XP or nearby participation Gold. Legitimate remote Kill Gold/XP and credited Assist Gold from predeath contributions follow their specific rules.
- A successful **revival-style save before actual death** starts **no** respawn timer and pays **no** kill/assist reward; consult the Combat Bible. A buyback (below) occurs **after** an actual death and cannot undo its consequences.

## 15. Buyback

- **Buyback is unavailable before the 10:00 elapsed match-time mark.** It appears as a purchasable death-time shop option starting at 10:00.
- A Vanguard who **has actually died** may buy back if they have enough Gold and their **personal buyback cooldown is ready**. A living Vanguard cannot pre-purchase or queue a future buyback.
- Buying back **spends Gold and immediately ends the current death timer**, respawning the Vanguard at their own fountain with their currently delivered items. Pending item purchases delivered on death are available on that respawn.
- The Gold cost is **expensive** and increases along **two independent data-driven dimensions**: (1) progression/elapsed match time and (2) **the number of buybacks that same Vanguard has already purchased this match**. Each subsequent personal purchase is more expensive, not merely the first purchase at a later time.
- Buyback also has its own **substantial per-Vanguard cooldown**. Its cooldown begins **when the buyback is purchased**, then runs normally while alive or dead; dying again does not restart it.
- Buyback **does not reverse the prior actual death**, death statistic, enemy Kill/Assist/First Blood/XP/Gold payout, bounty claim/reset, death-streak change, or any on-death effects. It is an **early respawn**, not Combat Bible death prevention or a revival-style save.
- Buyback cannot be used to obtain another payout of rewards or re-trigger the original death; all state changes are applied once by the authoritative server.
- Buyback's exact initial cost, time-scaling formula, per-purchase surcharge, and cooldown are prototype tuning values.

## 16. Explicit exclusions, implementation ownership, and tuning checklist

**No baseline rewards for:** passive elapsed time; lane-wave damage without a credited last hit or specified participation condition; nearby wildlife participation Gold; structure damage before Spire destruction; inhibitor destruction; Flux Well capture XP; Spire destruction XP; ordinary poke/healing/shielding/CC; unmarked owned units; a prevented would-be death; or an enemy Prime Well after the match ends.

**Authoritative ownership:**
- Economy system owns Gold balances, all reward calculation, reward ownership, payouts, devaluation, bounty amounts, purchase accounting, pending transactions, refunds, and buyback transactions.
- Progression system owns XP balances, eligibility-aware XP splits, thresholds, levels, level-up stat increments, unspent/allocated skill points, and max-level exclusion.
- Combat owns actual death and contributor/kill/assist attribution and emits authoritative result events consumed by the economy; it does not independently award duplicate Gold/XP.
- Battleground owns objective completion. The **Battleground Bible** owns Team Flux's design rules (sources, amounts, expiry and Fluxborn scaling); at runtime the single **Flux system** holds each team's authoritative permanent/temporary Flux state (see `ARCHITECTURE.md` §3). Economy consumes the relevant death/capture/destruction event to pay rewards, and receives the active Team Flux it needs for Fluxborn farm rewards **in the Fluxborn death event** rather than querying the Flux system.
- Item/inventory systems own item instances, recipes, slot placement, activation state, and atomic delivery; UI displays server-owned queue, costs, and balances without inventing purchases.
- Vision and Flux Spell loadout systems own their respective dedicated slots and cooldowns; economy validates and charges fountain swaps.

**Keep data-driven:** base kill Gold/XP, victim-level XP curve and higher-level bonus, assist fraction, bounty and death-streak steps, First Blood amount, Spire/Wells/ward Gold, Fluxborn base and Flux-scaling rewards, every farm sharing pool and range, participation/attribution windows, starting Gold, level XP curve, Vanguard stat-growth curves, undo/resale settings, remote queue validation behavior, Flux Spell thresholds and swap costs, vision swap costs, respawn timer curve, and buyback unlock time/cost scaling/cooldown.

**Examples are not final balance.** All prototype percentages and thresholds here express agreed system behavior and starting values, not a mandate to hardcode them into C++ or Blueprints.
