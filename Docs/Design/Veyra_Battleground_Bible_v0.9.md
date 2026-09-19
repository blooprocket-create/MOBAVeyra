> Repository Markdown export. Embedded concept-art images from the original Word document are intentionally omitted from this text-first version.

**THE MERIDIAN CRUCIBLE**

**BATTLEGROUND BIBLE**


| *Structured battlefield. Unstructured strategy. The map gives players three Fluxways and a jungle - never a job assignment.* |
|------------------------------------------------------------------------------------------------------------------------------|

| **VERSION**       | 0.9 - Wave/Structure Rules and Match Flow / 17-Vanguard Ranked Prototype |
|-------------------|--------------------------------|
| **MATCH FORMAT**  | 5v5 objective MOBA             |
| **WIN CONDITION** | Destroy the enemy Prime Well   |
| **ROLE RULES**    | No enforced lanes or positions |

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>1. WHY THE CRUCIBLE EXISTS</strong></p>
<p><strong>A REAL PLACE; MATCHES ARE NOT LITERAL HISTORICAL CANON</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

The Meridian Crucible is an ancient paired-Prime-Well installation built to stress-test Flux infrastructure: Fluxways, defensive Spires, emergency Wells, Fluxborn production, and large-scale network behavior. It survived the First Fracture in damaged but functional form and was rediscovered in the modern age.

**RESONANT FORMS**

A Vanguard who attunes to one of the Crucible's Prime Wells can project a temporary, fully tangible Resonant Form into the battlefield. The form carries the Vanguard's fighting instincts, abilities, equipment pattern, and consciousness during the contest. If destroyed, it collapses into Flux and can be reconstructed by its Prime Well.

<table>
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>WHAT THIS SOLVES</strong></p>
<ul>
<li><p>Respawning has an in-world explanation without making Vanguards immortal outside the Crucible.</p></li>
<li><p>Raska can fight Kade, siblings can fight, allies can oppose each other, and enemies can cooperate without rewriting lore relationships.</p></li>
<li><p>A match can explore "what if these ten Vanguards fought?" without becoming a permanent historical event.</p></li>
<li><p>The battlefield is canon even when the exact team composition is not.</p></li>
</ul></th>
<th><p><strong>HARD LIMITS</strong></p>
<ul>
<li><p>Resonant Forms cannot leave the Meridian Crucible.</p></li>
<li><p>Deaths in a match do not kill the real Vanguard.</p></li>
<li><p>The Crucible does not erase personality, memory, or relationships.</p></li>
<li><p>No future story should rely on one random player match having "really happened."</p></li>
</ul></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| *Champion lore tells you who these people are. A match tells you what could happen if the Crucible placed them on opposite sides.* |
|------------------------------------------------------------------------------------------------------------------------------------|

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>2. MAP TOPOLOGY</strong></p>
<p><strong>FAMILIAR THREE-LANE LANGUAGE; DIFFERENT SPATIAL LOGIC</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>


**LOCKED TOPOLOGY PRINCIPLES**

- Three major Fluxways: top, mid, bot.
- Two Prime Wells in opposing corners anchor each side of the network.
- The jungle encompasses the lanes. Top and bot are roads inside the wilderness, not the outer edge of playable space.
- There is jungle both inside and outside top and bot, enabling outer-wrap ganks and double-roamer strategies.
- Two spawned neutral Flux Well sites sit near the north/top and south/bot macro spaces.
- Competitive distances should be balanced, but the final visual geometry does not need to be a literal mirrored Summoner's Rift shape.

**DESIGN INTENT**

A veteran MOBA player should understand the macro map quickly, but should not be able to overlay another game's wall, brush, river, or gank geometry and instantly know every route.

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>3. PRIME WELLS &amp; MATCH VICTORY</strong></p>
<p><strong>THE NETWORK ANCHORS</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| **PRIME WELL**    | A massive stabilized Flux source that anchors one side of the Crucible and produces that side's Fluxborn.                                       |
|-------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| **ATTUNEMENT**    | Each team aligns to one Prime Well for the duration of the match. The Crucible recognizes two competing network states, not political factions. |
| **VICTORY**       | Destroy the enemy Prime Well once both base towers are destroyed while at least one enemy inhibitor is down. See §18. |
| **RESPAWN**       | A destroyed Resonant Form is reconstructed by its team's Prime Well after a delay.                                                              |
| **LORE BOUNDARY** | Prime Well destruction ends the contest; it does not mean a real city or faction was canonically annihilated.                                   |

| *The Prime Well is not a renamed crystal. It is the system that makes the entire battlefield function.* |
|---------------------------------------------------------------------------------------------------------|

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>4. FLUXWAYS, FLUXBORN, AND LANE PRESSURE</strong></p>
<p><strong>THE LANES ARE ENERGY ROUTES, NOT ARBITRARY ROADS</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

Fluxways are ancient high-efficiency routes connecting the two Prime Wells. Fluxborn naturally follow them because the underlying network is easiest to traverse there. This is the lore reason the battlefield forms three lanes.

| **FLUXBORN**   | Temporary autonomous constructs generated by the Prime Wells. They are the game's minions.                                                    |
|----------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| **WAVE LOGIC** | Fluxborn march the Fluxways toward hostile Spires and ultimately the opposing Prime Well.                                                     |
| **DEATH**      | When destroyed, the body collapses and most energy returns toward the network.                                                                |
| **LAST HIT**   | A Vanguard landing the finishing blow can capture useful residue/material before it dissipates - an in-world reason for personal gold income. |
| **TEAM FLUX**  | Accumulated Flux strengthens **all allied lane Fluxborn across all three Fluxways**. It does not directly grant escalating Vanguard combat stats. |

**WORKING FLUXBORN ARCHETYPES**

- Striders - front-line melee bodies. Working name.
- Sparks - ranged Fluxborn. Working name.
- Breakers - periodic siege Fluxborn built to damage corrupted Spires.
- Higher Team Flux strengthens every allied lane simultaneously, allowing success on one side of the map to help stabilize a losing lane.
- **Current prototype scaling:** every 25 Team Flux grants all allied lane Fluxborn +5% Health and +5% Damage.
- The 25-Flux step and 5% values are tuning knobs, not immutable constants.
- Temporary Flux contributes to the same scaling while active, then falls away when its source expires.
- **No automatic elapsed-time Fluxborn stat scaling:** only Team Flux strengthens baseline Fluxborn stats; destroyed inhibitors separately add lane-specific units.
- Exact stacking math and final presentation remain prototype tuning decisions.

| *Gold makes the Vanguard stronger. Flux makes the side of the map stronger.* |
|------------------------------------------------------------------------------|

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>5. CORRUPTED SPIRES</strong></p>
<p><strong>WHY TOWERS ATTACK</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

What players casually call towers are corrupted Flux Wells / network Spires. Their corruption causes them to identify hostile Attunement signatures and discharge concentrated Flux at approaching enemies.

| **FUNCTION**         | Defend each Fluxway and gate access deeper into the enemy network.                                                            |
|----------------------|-------------------------------------------------------------------------------------------------------------------------------|
| **TARGETING**        | Hostile Vanguards and Fluxborn are read as foreign network signatures.                                                        |
| **ESCALATING SHOTS** | Repeated discharge becomes increasingly unstable and violent, providing an in-world explanation for ramping structure damage. |
| **DESTRUCTION**      | Breaking a Spire is the act of dismantling a corrupted network node, not merely knocking over a military tower.               |
| **FLUX PAYOUT**      | Each destroyed Spire grants **+25 permanent Team Flux** at the current prototype tuning. There are three Spires per lane.   |

**VISUAL RULE**

A destroyed Spire should visibly crack open, vent corruption, and release energy back into the battlefield network. Structure destruction should feel like the map itself changed state.

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>6. NORTH &amp; SOUTH FLUX WELLS</strong></p>
<p><strong>THE MACRO OBJECTIVES</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

The Crucible has two major neutral Well sites: one near the top-side macro space and one near the bot-side macro space. They replace giant neutral boss objectives. The Wells themselves are the objective.

**CURRENT PROTOTYPE TIMING**

- The first Flux Wells do not open until **6:00**.
- Securing a Flux Well grants **+50 temporary Team Flux for 3 minutes**.
- A secured Well enters a **5-minute respawn cycle** before becoming available again.
- These values are prototype tuning and must remain data-driven.

<table>
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>LOCKED RULES</strong></p>
<ul>
<li><p>Flux Wells are spawn-based neutral objectives, not permanent control points.</p></li>
<li><p>North and South create competing rotations and cross-map trades.</p></li>
<li><p>Securing a Well grants temporary Team Flux, strengthening all allied lane Fluxborn while the reward remains active.</p></li>
<li><p>A Well is <strong>solo-capturable</strong>, but multiple allied Vanguards accelerate the stabilization/capture process.</p></li>
<li><p>Additional capture contribution is capped or diminished so a five-player dogpile is not automatically required. The exact curve is tunable.</p></li>
<li><p>When both teams contest the site, control pressure determines progress. Equal control stalls; superior control can continue progress at a reduced rate.</p></li>
<li><p>A jungler is naturally well-positioned to contest Wells, but ownership is a team problem rather than a role-locked mechanic.</p></li>
<li><p>There is no Dragon/Baron-style monster health bar and no last-hit secure mechanic at the center of the objective.</p></li>
</ul></th>
<th><p><strong>STILL TO PROTOTYPE</strong></p>
<ul>
<li><p>Exact capture time for one, two, or more allied Vanguards.</p></li>
<li><p>Exact contribution cap/diminishing-return curve.</p></li>
<li><p>How long an opened but unclaimed Well remains available.</p></li>
<li><p>Exact visual/audio language for stabilization progress and contested control.</p></li>
</ul></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| *The objective fight should be about controlling the Well site, not maximizing damage per second into a neutral boss.* |
|------------------------------------------------------------------------------------------------------------------------|

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>7. JUNGLE GEOMETRY</strong></p>
<p><strong>THE JUNGLE CONTAINS THE LANES</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

The defining map difference is spatial: top and bot are not boundary lanes. Wilderness continues beyond them. Every lane has jungle on both sides, allowing outer and inner approaches while preserving readable lane structure.

| **INNER JUNGLE**    | The spaces between top/mid and mid/bot. Common rotations, farming routes, and access to the two Flux Well sites.                                                                     |
|---------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **OUTER JUNGLE**    | Wilderness above top and below bot. Enables wraparound ganks, counter-jungling, double-roamer paths, and map movement that does not always pass through mid/river-style chokepoints. |
| **GANK PHILOSOPHY** | More approach options than a traditional edge-lane map, but not so many entrances that laning becomes impossible to read.                                                            |
| **SYMMETRY**        | Gameplay access and travel times should be competitively fair even if visual environments are not literal mirror copies.                                                             |

**DOUBLE ROAMING**

Because there is meaningful jungle both around and between lanes, two roaming Vanguards can operate on distinct routes and converge on lanes or objectives. This should emerge from opportunity costs rather than from a dedicated "roamer" role.

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>8. JUNGLE WILDLIFE</strong></p>
<p><strong>NATIVE ECOLOGY, NOT FLUX BATTERIES</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

Jungle camps are living Veyran fauna. They are not Fluxborn, not summoned constructs, and not automatically corrupted. The jungle should remind players that Veyra existed before the network was built across it.

<table>
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>LOCKED RULES</strong></p>
<ul>
<li><p>Camps have fixed identities throughout the match.</p></li>
<li><p>No jungle evolution system.</p></li>
<li><p>Killing a camp grants normal economy plus a simple temporary trait/buff associated with that species.</p></li>
<li><p>Traits create natural champion synergies without explicitly naming champion-specific bonuses.</p></li>
<li><p>Wildlife can vary by future battlefield biome while preserving gameplay roles.</p></li>
</ul></th>
<th><p><strong>WORKING CAMP CONCEPTS</strong></p>
<ul>
<li><p>Ashfang - movement / pursuit trait; naturally attractive to Raska-like mobility champions.</p></li>
<li><p>Stonehorn - defensive trait.</p></li>
<li><p>Gloomwing - vision / tracking trait.</p></li>
<li><p>Skittermaw pack - repeated-attack trait.</p></li>
<li><p>Miremother - multi-target sustain trait.</p></li>
<li><p>Razorback - impact / first-contact trait.</p></li>
</ul></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| *Depth comes from routing and synergy, not from camps becoming a separate progression game.* |
|----------------------------------------------------------------------------------------------|

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>9. MATCH ECONOMY</strong></p>
<p><strong>THREE DIFFERENT KINDS OF POWER</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| **GOLD / ITEMS**    | Personal Vanguard power. Earned through Fluxborn last hits, jungle farming, takedowns, structures, and other tuned sources.                                  |
|---------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **TEAM FLUX**       | Collective lane pressure. Permanent Flux comes from destroyed Spires; temporary Flux comes from Flux Wells and destroyed inhibitors. All current Team Flux strengthens allied lane Fluxborn globally, not Vanguards. |
| **WILDLIFE TRAITS** | Temporary tactical adaptations from jungle camps. Strong enough to matter for routing and timing, but not permanent team scaling.                            |

**WHY THIS MATTERS**

The systems create different answers to "who is ahead?" A team can have stronger individual itemization, superior Flux-powered wave pressure, or better temporary jungle preparation. No single resource should collapse every advantage into raw champion stats.

| *Kills can win fights. Flux can win the map. Items can create a carry. None should replace the others.* |
|---------------------------------------------------------------------------------------------------------|

## 10. STRUCTURE PROGRESSION & INHIBITORS

Each Fluxway contains **three Corrupted Spires** before the enemy base layer at current map design.

### Permanent Flux from Spires

Each destroyed Spire grants **+25 permanent Team Flux** at current prototype tuning.

Permanent Flux never expires during the match.

This means structural success on one Fluxway helps all three lanes because every allied lane Fluxborn benefits from the team's global Flux total.

### Inhibitors

Each lane has a reconstructing inhibitor structure beyond its Spires.

Destroying an enemy inhibitor grants **+25 temporary Team Flux for 3 minutes**.

After the same 3-minute window, the inhibitor reconstructs and becomes available to destroy again.

The reward is deliberately temporary: an inhibitor kill creates a strong push window rather than permanently increasing the winning team's baseline.

Temporary Flux sources track their own expiration independently. Securing another temporary Flux source does not refresh an older source's timer unless a future mechanic explicitly says otherwise.

All numerical values in this section are prototype tuning values and must remain data-driven.

## 11. VISION, WARDS & DENSE FOG

Veyra uses standard **fog of war** and player-placed **wards**.

Veyra does **not** currently use traditional brush/bush concealment. Instead, the battleground contains **Dense Fog** volumes.

### Dense Fog rules

- A Vanguard outside a Dense Fog volume cannot directly see enemy Vanguards inside it.
- This remains true even if an allied Vanguard is currently inside that fog.
- Champion vision inside Dense Fog is **local to the observer**: to directly see enemy Vanguards in the fog, your own Vanguard must also be inside that same fog volume.
- Dense Fog therefore creates commitment zones rather than ordinary shared-vision bushes.

### Wards inside Dense Fog

A ward placed inside Dense Fog acts as a **presence sensor**, not a remote champion-vision source.

- It does not reveal the exact position/model of enemy Vanguards inside the fog to allies outside.
- If an enemy Vanguard enters that Dense Fog while the ward is active, the ward pings enemy presence.
- If the ward is placed while an enemy Vanguard is already inside the fog, it immediately pings that the fog is occupied.
- The ping communicates **presence in the fog zone**, not exact enemy coordinates.
- Exact ping cadence, cooldown, persistence, and UI treatment remain tunable.

The fundamental rule remains: **if you want direct visual confirmation of an enemy inside Dense Fog, somebody has to go in.**

## 12. FOUNTAIN, RECALL & SHOPPING

The Fountain is the team's **item-delivery, equipment-conversion, resale, and loadout-swap space**. Remote shop purchases provide convenience without immediate battlefield power.

- Players can **browse the shop anywhere** and remotely **purchase/queue normal items** while outside their Fountain; Gold is spent immediately.
- Remotely purchased items remain **pending delivery**, providing **no stats or other gameplay benefits** until the Vanguard returns to their own Fountain or dies and has those purchases assigned at the Fountain.
- Existing components reserved for an upgrade remain active and unconsumed until that upgrade is delivered. The queue processes purchases in order, respects the six-slot delivered inventory limit, and can combine queued components at delivery.
- Undelivered purchases can be canceled **from anywhere for a full refund**. Invalidated purchases and their dependents are automatically canceled/refunded.
- Returning to the Fountain delivers pending purchases; actual death also delivers them to Fountain inventory for the next respawn. Shopping while dead is allowed. Delivery does not shorten the death timer.
- **Selling equipped items and swapping vision tools or Flux Spells require being at the Fountain**. Remote buying cannot remotely equip items or change dedicated loadout slots.
- **Recall** returns the Vanguard to the Fountain after its channel completes.
- Exact recall duration, interruption rules, shop radius, and Fountain recovery values remain tunable.

The **Economy & Progression Bible v0.1** owns detailed Gold/accounting, XP, queue, undo/resale, and buyback rules; this section states the battleground's fountain behavior.

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>13. OPEN COMPOSITION</strong></p>
<p><strong>THE GAME CREATES INCENTIVES, NOT ASSIGNMENTS</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

The Meridian Crucible does not ask players to declare top, jungle, mid, carry, support, or any other role. Five Vanguards load into the battlefield and the team decides how to distribute them.

<table>
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>EXPLICITLY ALLOWED</strong></p>
<ul>
<li><p>Five players mid at level one.</p></li>
<li><p>Two roamers and three solo lanes.</p></li>
<li><p>A traditional 1-1-1-2 structure.</p></li>
<li><p>No dedicated jungler if the team accepts the cost.</p></li>
<li><p>A solo bot carry with four players pressuring the rest of the map.</p></li>
<li><p>Unusual lane swaps and temporary deathballs.</p></li>
</ul></th>
<th><p><strong>WHAT BALANCES FREEDOM</strong></p>
<ul>
<li><p>Shared XP and gold opportunity costs.</p></li>
<li><p>Unfarmed jungle wildlife and lost temporary traits.</p></li>
<li><p>Unattended Fluxborn waves and structure pressure.</p></li>
<li><p>North/South Flux Well access.</p></li>
<li><p>Travel time through inner and outer jungle.</p></li>
<li><p>The enemy team being free to exploit whatever your composition gives up.</p></li>
</ul></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

| *If a strategy is unhealthy, fix the economy or map incentive causing it - do not outlaw it because it looks unconventional.* |
|-------------------------------------------------------------------------------------------------------------------------------|


## 14. FLUX SPELLS

Flux Spells are a separate tactical loadout system tied to the team's shared Flux progression.

They are **not inventory items** and do not occupy normal item slots.

### Dedicated slots

- Each player has up to **two dedicated Flux Spell slots**.
- Flux Spell slots are separate from the normal inventory.
- Each player **preselects up to two Flux Spells for free before the match**. They start equipped but locked until their dedicated slots unlock through permanent Team Flux.
- Players do **not** need to visit the Fountain or purchase their initial preselected Flux Spells when a threshold is reached.

### Permanent Flux thresholds

Flux Spell access is unlocked by **permanent Team Flux only**.

Temporary Team Flux from Flux Wells, inhibitors, or other temporary sources does **not** contribute toward Flux Spell unlock thresholds.

This prevents a team from temporarily unlocking a spell and then losing access when a temporary Flux source expires.

Under the current prototype tuning:

- **25 permanent Team Flux** unlocks the player's **first Flux Spell slot**.
- **75 permanent Team Flux** unlocks the player's **second Flux Spell slot**.

These thresholds are current prototype tuning values and must remain data-driven.

Once a slot is unlocked through permanent Team Flux, it remains unlocked for the rest of the match under the current rules because permanent Team Flux is not lost through normal temporary-expiration mechanics.

- An equipped Flux Spell remains inactive until its slot's permanent-Flux threshold has been reached.
- **Unlocks apply immediately when the threshold is reached, anywhere on the map**, with no return to Fountain required.
- Once unlocked, the spell operates on its normal cooldown.
- Casting a Flux Spell does **not** consume Team Flux.
- Temporary Flux still contributes to Fluxborn strength while active; it simply does not unlock Flux Spell slots.
- Team Flux is progression and access, not spell ammunition.

### Swapping and adaptation

Players may change their equipped Flux Spells **only at their own Fountain shop**.

- Replacing an equipped Flux Spell costs **gold**.
- The replacement cost creates a real adaptation tradeoff because that gold is no longer available for item progression.
- Swapping does not bypass the new spell's Team Flux threshold.
- Remote queued item purchases **cannot** pre-equip or change Flux Spells. The original prematch selections cost no Gold.
- Exact replacement cost remains tunable.

The intended relationship is:

**Gold/items strengthen the Vanguard. Team Flux strengthens the team's Fluxborn and unlocks tactical Flux Spell options.**

Flux Spells should create strategic adaptation without turning shared Flux into a generic champion-stat ladder or a consumable mana pool.

## 15. PROTOTYPE GAME MODES & CHAMPION SELECT

The prototype supports standard 5v5 champion-select flows built around **hidden hovers, visible lock-ins, permanent roster commitment, and teammate trades**.

**Cross-reference:** [Modes & Access Bible v0.1](Veyra_Modes_Access_Bible_v0.1.md) now owns Ranked account/ownership gates, weekly free rotation, and separate Co-op vs AI queues. The champion-select rules below apply to PvP unless a mode is explicitly distinguished there. **Only Co-op vs AI permits cross-team mirror Vanguard selections; PvP remains globally unique.**

### Shared lock-in rules

These rules apply anywhere champion selection is used:

- A hovered or selected-but-not-locked Vanguard is private to the player's own team.
- The opposing team cannot see what a player is hovering or considering.
- Once a Vanguard is **locked in**, that Vanguard becomes visible to the opposing team.
- Locking in permanently commits that Vanguard to the team's five-character roster for that match.
- A locked player cannot unlock and choose a different Vanguard.
- Two teammates who are both locked may **trade their locked Vanguards with each other**.
- A trade only reassigns which teammate will play which already-locked Vanguard. It does **not** change the team's locked roster.
- No trade may introduce a Vanguard that was not already locked by that team.
- Global uniqueness/no-mirror rules remain in effect unless changed by a later design decision.

The important distinction is:

> **Locking commits the Vanguard to the team. Trading can change the player assigned to that Vanguard, but cannot change the team's five locked picks.**

### Casual Select

All players select Vanguards during the same champion-select phase.

- No ban phase.
- Enemy hovers remain hidden.
- Enemy locked picks are visible.
- Lock-in is permanent at the roster level.
- Locked teammates may trade Vanguards with one another.

The intent is a fast standard queue with counter-information only after an opponent has actually committed.

### Draft Pick

Draft Pick includes a ban phase followed by a fixed alternating pick phase.

#### Ban phase

Each team receives **3 bans**.

The ban order is:

1. Team A bans 1 Vanguard.
2. Team B bans 2 Vanguards.
3. Team A bans 2 Vanguards.
4. Team B bans 1 Vanguard.

This is a **1-2-2-1** sequence, producing 3 bans per team.

Bans exist only in **Draft Pick** and **Ranked** modes. Casual Select has no bans.

Banned Vanguards cannot be selected by either team for that match.

#### Pick phase

After bans, the draft uses:

1. Team A locks 1 Vanguard.
2. Team B locks 2 Vanguards.
3. Team A locks 2 Vanguards.
4. Team B locks 2 Vanguards.
5. Team A locks 2 Vanguards.
6. Team B locks 1 Vanguard.

This produces the full ten-player draft as a **1-2-2-2-2-1** sequence.

Draft visibility follows the shared lock rules:

- Hovers are hidden from the opposing team.
- Only locked picks are revealed.
- A locked Vanguard cannot be replaced with a different Vanguard.
- Locked teammates may trade already-locked Vanguards with one another.

### Ranked

Ranked uses the **Draft Pick** champion-select structure:

- 3 bans per team;
- **1-2-2-1** ban order;
- **1-2-2-2-2-1** pick order;
- hidden hovers;
- visible lock-ins;
- permanent team-level roster commitment;
- teammate trades among already-locked Vanguards.

With global unique picks and six total distinct bans, a roster of **16 Vanguards is the mathematical minimum** required to leave ten unique playable picks after the ban phase.

The current prototype roster contains **17 Vanguards**, which meets this **draft-wide distinct-pick minimum only**. Under the later [Modes & Access Bible v0.1](Veyra_Modes_Access_Bible_v0.1.md), individual Ranked entry additionally requires **Account Level 30 and 20 permanently owned Vanguards**, and weekly rotation cannot be used in Ranked. Therefore, a released roster of only 17 ownable Vanguards does **not** yet make Ranked player-accessible; the intended Ranked format is a future target pending sufficient released roster and further Ranked design.

Exact rating, placement, matchmaking, season, queue-penalty, and progression systems remain separate implementation/design decisions.

<table>
<colgroup>
<col style="width: 100%" />
</colgroup>
<thead>
<tr class="header">
<th><p><strong>16. FIRST-PLAYABLE GUARDRAILS</strong></p>
<p><strong>WHAT THE PROTOTYPE MUST PROVE</strong></p></th>
</tr>
</thead>
<tbody>
</tbody>
</table>

**TARGET MATCH PACING**

- Standard matches should generally resolve between **20 and 45 minutes**.
- Permanent Flux from Spires creates match-long structural progression.
- Temporary Flux from Wells creates midgame push windows.
- Temporary Flux from inhibitors helps convert successful base pressure into a real opportunity to end.
- These systems should produce escalating lane pressure without directly turning the leading Vanguards into larger stat checks.

**THE BATTLEGROUND SUCCEEDS IF...**

- A new MOBA player can understand lanes, jungle, structures, and the win condition quickly.
- A veteran MOBA player cannot solve the map simply by importing another game's routes and role assumptions.
- Flux Wells create meaningful cross-map decisions without becoming mandatory boss timers.
- Flux-powered waves matter enough to reward macro control without making champions stat-check harder simply because their team has more Flux.
- Outer jungle creates real strategic options without making top/bot impossible to defend.
- Open composition produces experimentation while economy and opportunity cost prevent one obvious five-player deathball from dominating every match.
- Jungle wildlife traits create routing decisions but remain easy to understand.
- The lore logic - Prime Wells, Fluxways, Fluxborn, Spires, Resonant Forms - reinforces rather than complicates gameplay readability.

**HIGHEST-PRIORITY UNRESOLVED DESIGN QUESTIONS**

- Exact Flux Well capture-time curve and contribution cap.
- Exact final Fluxborn scaling values/stacking formula after prototype testing.
- Final base geometry around inhibitors, the two base-defense towers, and the Prime Well.
- Playtest the established Gold/XP sharing rules from the Economy & Progression Bible with unconventional lane groupings.
- Final camp roster and trait values.
- Respawn timing curve and whether Resonant Form reconstruction has any interactive presentation.
- Final battleground art direction and name lock: Meridian Crucible is current working canon.

| *Prototype the decisions first. Numbers, cosmetics, and extra systems come after the map proves it can generate interesting choices.* |
|---------------------------------------------------------------------------------------------------------------------------------------|

## 17. WAVE AND JUNGLE SPAWN PROTOTYPE

**Every number below is designer-editable configuration, not hardcoded logic.** Veyra's own map travel times, lane lengths, and jungle routes determine final tuning. The Match Flow Bible owns the pre-match countdown and active match clock.

### Fluxborn waves

- A synchronized **15–20-second preparation countdown** precedes the match clock. The exits open together at **0:00**, and the first Fluxborn wave **spawns at 0:30**.
- All **three lanes spawn simultaneously** on the same schedule. Wave arrival/meeting times may vary with actual path length. Lane-specific spawn offsets can be added as data **if playtesting requires**, not assumed in advance.
- Provisional wave interval: **30 seconds until 14:00; 25 seconds from 14:00 to 30:00; 20 seconds thereafter**. Explicit editable phase boundaries and spawn alignment must avoid duplicate/missed waves when crossing a phase.
- Ordinary waves contain **frontline and ranged Fluxborn**, with a **tougher siege Fluxborn periodically**. Quantities, unit variants, and siege periodicity are configurable.
- **No automatic time-based stat growth** for individual Fluxborn. Team Flux alone increases their baseline strength; accelerating wave frequency does not add a second hidden stat-scaling system.
- All one-to-five-player lane Gold/XP splits remain as defined by the Economy & Progression Bible, including Level 18 exclusion from XP sharing.

### Wildlife

- Initial camp availability is staggered into **two editable spawn groups**, provisionally **0:55** and **1:07**. Which Veyra camps belong in each group is not yet assigned; playtest the actual jungle routes rather than copying another game's camp layout.
- Every ordinary camp **respawns on its own timer after the entire camp is cleared**, not on a universal global respawn. A multi-creature camp still awards Gold/XP **per creature when each dies**.
- Wildlife respects a configurable **leash/operating area**; when pulled beyond it, wildlife disengages, returns to its camp, and resets under the camp's defined policy. Actual ranges and reset behavior remain to be tested.
- No jungle wildlife camp evolves during a match.

## 18. INHIBITORS, FINAL STRUCTURES, AND PRIME WELL

### Inhibitors and extra wave units

- Each destroyed inhibitor grants its attacker the already-established **+25 temporary Team Flux for 3 minutes**, strengthening allied Fluxborn globally. The inhibitor reconstructs after its current **3-minute** prototype timer.
- **Every newly spawned wave in the lane whose enemy inhibitor is currently down gains additional Fluxborn units**. These are extra bodies, **not** a lane-specific stat boost and **not** replacements for the ordinary wave.
- Each downed inhibitor affects **only its corresponding lane**. If all three enemy inhibitors are down, all three lanes get their respective extra units; there is **no additional all-three-down bonus**.
- Extra units award **normal Gold and XP** for their unit type when defenders kill them, subject to the standard Economy & Progression Bible rules.
- Once an inhibitor rebuilds, extra units stop joining **future** waves in that lane; already-spawned units **remain on the battlefield** until their normal end.
- Counts, composition, inhibitor rebuild time, and temporary Flux reward duration are separately owned, validated, editable gameplay data.

### Final base-defense towers

- Two **base-defense towers** defend the enemy Prime Well after the lane Spires and inhibitors. Destroying **any one enemy inhibitor** opens the path to the base-defense towers; all three inhibitors need not fall.
- While **all of that team's inhibitors stand**, surviving base towers are **invulnerable**. Taking down any inhibitor makes surviving base towers vulnerable again.
- Destroyed base towers **never respawn**. Each destroyed base tower grants the attacking team the same prototype **+25 permanent Team Flux** as a lane Spire, and that Flux does not disappear if an inhibitor reconstructs.
- **No tower Health regeneration**, including regular lane Spires and both base-defense towers. Damage dealt to a surviving tower remains, even while it is protected or invulnerable.

### Passive Prime Well vulnerability and healing

- The Prime Well is **passive**; it does not attack anyone.
- It can take damage **only if both base-defense towers are already destroyed AND at least one of its team's inhibitors is currently down**. One downed inhibitor suffices.
- If the last downed inhibitor reconstructs, the Prime Well **becomes invulnerable immediately**, even though the permanently destroyed base towers stay down. Re-destroying any inhibitor can reopen the Prime Well.
- When **all three inhibitors are standing**, the invulnerable Prime Well **gradually regenerates missing Health up to full** at a configurable rate. Its regeneration **stops immediately when any inhibitor is down**, even if the still-standing base towers continue to make it invulnerable.
- Health already removed is only restored by actual regeneration; invulnerability alone neither resets nor instantly fills Health.
- Destroying a vulnerable Prime Well ends the match. There is **no mandatory match-length cutoff** or automatic sudden-death winner.

## 19. BACKDOOR PROTECTION, TOWER AGGRO, AND FLUXBORN TARGETING

- **Lane Spires, base-defense towers, and the Prime Well** gain backdoor damage protection when **no allied attacking-team Fluxborn** are inside the structure's configured protection radius. Allied Vanguards, companions, and summons do **not** disable this protection.
- When the last eligible Fluxborn dies or leaves the radius, protection **gradually ramps up** toward its data-driven maximum; existing resolved damage stays dealt. An allied Fluxborn entering range **removes/resets protection immediately**, with no gradual ramp-down.
- Backdoor damage reduction and structure **invulnerability** are distinct rules. An allied wave never makes a locked Prime Well or base tower damageable while its prerequisite is unmet.
- Lane Spires and base towers use the **same tower aggro rules**. Normally they target hostile Fluxborn. An enemy Vanguard damaging a defending Vanguard in tower range takes target priority; a valid current priority target retains focus despite other attackers or newly arriving Fluxborn.
- That priority ends when the attacking Vanguard **leaves range, dies, or becomes untargetable**. Leaving range also resets the tower's consecutive-hit damage ramp. Returning to range does **not** restore priority unless the Vanguard damages a defender again, though the tower may normally attack the Vanguard if no hostile Fluxborn are available.
- Damage-over-time ticks draw tower priority when their owner is in tower range at the time the defender takes damage, even if the DoT was applied earlier. Owned companion/summon damage traces to the Vanguard owner for the same aggression test.
- Tower shots against the **same Vanguard** escalate in damage up to a data-driven cap and reset on target loss/switch. The Combat Bible owns exact combat, damage, and attribution behavior.
- Fluxborn normally fight opposing Fluxborn and push toward structures; if an enemy Vanguard attacks a nearby allied Vanguard, nearby allied Fluxborn may switch aggression to the attacker. They disengage and return to normal targeting if that Vanguard exits their configurable engagement range.
- **Siege Fluxborn prioritize structures within attack range** except when responding to eligible aggression against their allied Vanguard.

**Map geometry, wave/camp schedules, protection radii, thresholds, ramp durations, and AI engagement parameters must remain editable validated data; do not transplant another MOBA's timings as engine assumptions.**
