# Ride state — open questions

**Version:** 0.1
**Date:** 2026-09-20
**Status: complete.** All 26 questions answered on 2026-09-20, plus the two follow-ups Q13b and Q16b that the answers raised.

**These rulings are decisions, but they are not yet canon.** Canon is the Combat Bible section they are to be written into, per Q1. Until that section exists this document is the record of what was decided, not the rule anyone implements against. A condensed specification for that section is at the end.

Raska's Hound is the only feature in the 25-Vanguard roster whose core mechanic has no supporting rules in any bible. [ADR-003](../ADR/ADR-003-owned-field-entities.md) scoped it as a separate locomotion feature rather than an owned field entity, and left it needing canon of its own before she can be implemented.

This document is that gap written out as answerable questions. Each cites what canon already settles, so the design work is confined to what is genuinely undecided.

**Raska is not deferred from the roster.** All 25 Vanguards ship in the first-playable roster. She is last in the implementation queue because this canon has to exist first.

## Source

Character Bible §1. Kickstart (E) summons Hound and enters ride mode, granting "very high movement speed", "ghosting/less unit obstruction", "wide turning behavior at high speed" and "strong Momentum generation", with mounted actions **Sideswipe**, **Powerslide** and **Bail Out** — the last leaving Hound travelling on alone to "collide with/knock enemies". NO BRAKES (R) is an extreme ride state whose recast, **Last Exit**, launches Raska and Hound separately.

---

## Already answered — do not re-litigate

Three things read as novel but are already covered. Confirm the mapping, then move on.

| Kit wording | Existing rule |
|---|---|
| "ghosting/less unit obstruction" | Combat Bible §24: **Ghosted** means ignoring unit collision, **not** terrain. A defined primitive; Raska needs no new concept. |
| "very high movement speed" | Combat Bible §23: soft caps at 415 and 490, no ordinary hard maximum, and "specific abilities may explicitly bypass soft-cap behavior". The *mechanism* exists. Whether ride mode uses it is Q6. |
| Hound colliding with and knocking enemies | Combat Bible §8 defines Knockback and Knockup. Which one it is, is Q14 — but it must resolve to a **named** CC type, not a new one. |

---

## A. Scope and ownership

**Q1. Where does this canon live?** — **ANSWERED 2026-09-20**

> **A new Combat Bible section**, alongside §9 (forced movement and mobility), §23 (Movement Speed) and §24 (unit collision and Ghosting), which already own this territory. Rules written anywhere else would contradict the Combat Bible from outside.
>
> **It must be written generically — "a Vanguard in a ride state", not "Hound".** `CLAUDE.md` prohibits `if Raska` branches in reusable core systems, and that applies to canon as much as to code: naming the content in the rule builds the branch in prose before anyone builds it in C++. Written generically it costs nothing now and makes a second ride user cheap later.
>
> No ADR is required. Q4's answer removed the cross-module architecture question — ride movement is an ordinary pathing agent with an angular constraint, so there is no locomotion-primitive or prediction-policy decision left that ADR-002 does not already cover.

**Q2. Is Hound an owned field entity after Bail Out or Last Exit?** — **ANSWERED 2026-09-20**

> **No. It is a projectile.** The riderless phase travels in a set direction, collides, applies control and ends. It does not chase, acquire targets or act autonomously, and canon never gives it Health — which makes it a different thing entirely from Nix, Picket and Waterling, the three ADR-003 combat units, each of which has its own Health and its own decisions.
>
> It therefore uses the rules that already exist: Combat Bible §13 for speed, width, collision profile and target eligibility, and §20 for interception. Combat Bible §32 attribution still traces its damage and kills back to Raska.

**What this settles elsewhere.** **Raska requires none of ADR-003's three primitives.** She is gated only on the ride-state Combat Bible section from Q1, not on the combat-entity, placed-marker or world-volume work. That moves her substantially earlier in the implementation order — she was last because her canon did not exist, and this Q&A is that canon.

It also leaves ADR-003's `combat_unit` category at exactly three members, and confirms the category boundaries hold: a thing that travels and hits is a projectile, not a unit.

**Q3. Does Hound exist when not summoned?** — **ANSWERED 2026-09-20**

> **Spawned per cast.** Hound materialises with Kickstart and ceases to exist when the ride ends or its projectile phase finishes. It cannot be seen, targeted or intercepted beforehand, and it is not parked anywhere between rides.

This keeps the Q9 cast-time wind-up as the single pre-mount window, rather than adding a second interceptable object with its own rules.

---

## B. The movement model — the hard one

This is the question that decides whether the feature is a week or a quarter, and it should be answered first.

**Q4. What does "wide turning behavior at high speed" mean mechanically?** — **ANSWERED 2026-09-20**

> **A turn-rate cap on an otherwise ordinary pathing agent.** Raska keeps click-to-move and ordinary navmesh pathfinding while mounted, but her facing changes at a limited angular rate rather than instantly. She overshoots her target and has to swing back around, which is what makes a "pass" a pass and what delivers the bible's identity guardrail — "repeated high-speed passes and controlled recklessness".
>
> Rejected: pure animation fiction, which would have made her a fast champion with a motorcycle model and forfeited the guardrail; and a true steering model, which would have introduced a second control scheme inside a MOBA, the hardest case for client prediction under ADR-002, and behaviour inconsistent with the other 24 Vanguards.

**What this settles elsewhere.** Because ride movement stays an ordinary pathing agent with one added constraint:

- **Q5** is mostly answered — no directional or WASD-style input is needed, and ordinary move, attack-move and right-click orders keep working. What remains of Q5 is the one genuinely new case the turn rate creates: what she does when the destination is **behind** her.
- **Q7** is mostly answered — the server still computes the path, so ride movement predicts like any other unit and needs no bespoke reconciliation. Confirm that the angular constraint is applied server-side and mirrored by the client, not simulated independently.
- **Q12** becomes tractable — she is a normal pathing agent, so Root and Stun can stop her by the ordinary rules. The live part is whether stopping is instant or decelerates.
- **Q21** is unaffected by this choice and still needs answering on its own.

The turn rate itself is a tuning value and belongs in data per `ARCHITECTURE.md` §1.3, not in this document.

**Q5. What happens when the move destination is behind her?** — **ANSWERED 2026-09-20**

> **A wide U-turn at speed.** She arcs around without shedding velocity, travelling a considerable distance before she is heading back. The turn rate does not scale with speed; it is a single constant.

This is the behaviour that makes Q4's turn-rate cap a real mechanic rather than a decoration. The rejected alternative — slowing until the turn is within her rate — would have been more forgiving and would have quietly removed most of the drawback the cap was chosen to impose.

The practical consequence is that **committing to a direction is genuinely committing**. Overshooting a target is not a small correction; it is a long arc during which she is travelling away from the fight. That is the cost that pays for the speed, and it is what makes a "pass" a pass.

The turn rate itself is a tuning value for data.

**Q6. Does ride mode bypass the Movement Speed soft caps?** — **ANSWERED 2026-09-20**

> **Neither bypass nor climb — ride mode *sets* Movement Speed to a data-driven value.** While mounted her Movement Speed is the bike's, replacing her ordinary value rather than adding to it. The question of bypassing §23's 415/490 ladder does not arise, so no exception to §23 needs writing.

**Why this shape.** It is predictable to balance, it cannot be compounded by Movement Speed stacking into an ultimate that already cannot be slowed, and it matches the fiction: a motorcycle's top speed is the motorcycle's, not the rider's boots.

**The accepted cost:** Movement Speed items do nothing while she is mounted. They still govern her dismounted movement, so they are not dead weight, but a speed-stacking build will not make Hound faster. If that later proves to narrow her itemisation too far, the lever is the set value itself, not a return to additive bonuses.

**Interactions to carry into the Combat Bible section:**

- **Slows outside R** still apply, reducing *from* the set value, and §23's ordinary slowing floor of **100** still applies. A slowed bike is slow.
- **Inside R** she is Unstoppable per Q13, so slows do not land at all.
- The set value itself, and whether NO BRAKES uses a higher one than Kickstart, are tuning values and belong in data per `ARCHITECTURE.md` §1.3.

**Q7. How is ride movement predicted client-side?** — **ANSWERED 2026-09-20**

> **The server owns and applies the angular constraint; the client mirrors it** for display and local prediction but never decides it. Ride movement otherwise predicts exactly like any other unit.

Q4 did most of the work here — a turn-rate-capped ordinary pathing agent raises no novel prediction problem, and ADR-002's prohibition on prediction granting the client final authority is untouched.

The reason to state the ownership explicitly rather than leave it implied: the two plausible implementations look identical until they diverge, and a diverged **heading** compounds into far greater positional error than a diverged position does. Server-owned with a mirroring client means a correction adjusts something the client was only ever echoing.

**Q8. Does Momentum accrue by distance or by time while mounted?** — **ANSWERED 2026-09-20**

> **By distance actually travelled**, mounted or on foot. A stationary bike builds nothing. Attacks and abilities remain separate flat contributions on top, as the passive already describes.

**A consequence worth noticing:** the passive says Momentum builds "especially" from high-speed riding, and with a distance basis that falls out of the arithmetic rather than needing its own rule. At the fixed ride speed from Q6 she covers far more ground per second than on foot, so riding generates Momentum proportionally faster automatically. No separate riding multiplier is required, and adding one would be double-counting.

It also closes the idling exploit: there is no way to accumulate Momentum by waiting mounted behind terrain or in base.

The rate per unit distance is a tuning value and belongs in data.

---

## C. Entering and leaving

**Q9. What cast class is Kickstart?** — **ANSWERED 2026-09-20**

> **Cast-Time, interruptible.** A short readable wind-up as Hound arrives. Per §26, an interrupted pre-Commit Cast-Time ability pays no resource cost — and the cooldown is likewise not spent if it is interrupted before Commit.

The wind-up is the window in which the mount itself can be answered, which matters now that the resulting state is Unstoppable under R (Q13). Without it, the only counterplay to NO BRAKES would be pre-positioning and Suppression.

**Q10. How does the ride state end?** — **ANSWERED 2026-09-20**

> **Four exits**, and no CC is among them — Q12 settled that crowd control never forces a dismount:
>
> | Exit | Hound |
> |---|---|
> | **Bail Out** (E while mounted) | continues as a piercing projectile (Q2, Q14) |
> | **Duration expiry** | ends; no projectile phase |
> | **Last Exit** (R recast) | continues as a piercing projectile |
> | **Death** | ends; see Q24 |
>
> **She carries momentum off the bike and decays to her normal Movement Speed over a short window** rather than stopping dead. Dismounting reads as leaving at speed, not hitting a wall.

**This closes the "stop dead or decelerate" thread left open by Q4 and Q12 — and it needs no new rule.** The two cases are different and Veyra already handles both:

- **Dismounting** decays, as ruled here.
- **Hard CC** does stop her outright, and §23 already says so: "Explicit hard CC such as Root/Stun may reduce effective movement to 0." No exception is required. The concern that a rooted motorcycle stopping instantly looks wrong is a presentation problem for animation and VFX to solve, not a rules problem — and inventing a rules exception for it would have weakened Root against exactly the character it most needs to work on.

**Q11. Can she recall, shop, or use Flux Spells while mounted?** — **ANSWERED 2026-09-20**

> **Flux Spells: yes.** They are universal and stay available while mounted.
> **Recall: no.** It is a channel requiring stillness and is incompatible with the ride state; using it forces a dismount first.
> **Shopping: no**, which follows anyway since it requires the fountain.

---

## D. Crowd control

**Q12. What does each CC type do to a mounted Raska?** — **ANSWERED 2026-09-20**

> **Crowd control applies by the ordinary rules and the ride state persists through it.** No CC forces a dismount. A stun stops her completely and she generates no Momentum for its duration, which is the punishment; when it ends she resumes riding. Kickstart is not cancelled by being interrupted mid-ride.
>
> **NO BRAKES (R) additionally grants Unstoppable.** Using Combat Bible §9's vocabulary precisely, this is **Unstoppable** — "ordinary CC cannot affect the Vanguard during the protected state" — and not Uninterruptible or Guaranteed Resolution, which are separate concepts there.

**What follows automatically from §8 and §9:**

- **Suppression still lands.** §8 states plainly that ordinary CC immunity does not prevent Suppression. NO BRAKES is therefore answerable, but only by the strongest CC category. This is a rule Veyra already has; no exception is needed.
- **Displacement does not affect her during R.** Knockup, Knockback and Pull are ordinary CC under §8, so Unstoppable blocks them. Outside R they apply normally and take ownership of her movement per §9.
- **Fear, Taunt and Charm** apply normally outside R. The turn-rate cap constrains *how* she executes forced movement, not whether it lands — an ordinary pathing agent can be driven toward a taunter, just not instantly.

**The trade this accepts**, recorded once so it is a known position rather than a surprise: once NO BRAKES is cast it is a committed engage that ordinary counterplay cannot answer. Counterplay moves entirely to pre-cast positioning, to burst damage during the ride, and to Suppression. That is a legitimate design stance — several MOBAs have an unanswerable committed ultimate — but it means her ultimate's power budget should be paid for in cast time, cooldown, or a readable wind-up rather than in raw output.

**Still open, and asked as Q13:** the exact scope of the Unstoppable window, which is a large power difference depending on how it is drawn.

**Still open, from Q4:** whether a stop is instant or decelerates. A Root halting a motorcycle dead at high Movement Speed reads as a bug; deceleration makes the control feel weightier, but it means Root does not fully stop her for a moment, which is a genuine exception the Combat Bible would have to state rather than imply.

**Q13. What exactly does the NO BRAKES Unstoppable window cover?** — **ANSWERED 2026-09-20**

> **Full literal Unstoppable, including immunity to Slows.** §9's definition applies verbatim with no carve-outs: every ordinary CC type in §8 is blocked, Slow included. No exception needs writing, and the rule is exactly what the Combat Bible already says.
>
> **The window runs for the whole R duration and ends when Last Exit is cast**, covering Raska's airborne phase but not her landing. Once she touches down she is an ordinary target again.

**What this means in practice.** During NO BRAKES she cannot be stunned, rooted, displaced, feared, blinded or slowed, and is travelling at her highest speed of the game. **Suppression remains the only ordinary answer**, per §8's rule that CC immunity does not prevent it.

That is a deliberately large grant, and it relocates her counterplay entirely:

- **Before the cast** — positioning, vision, and denying her the approach.
- **During the ride** — burst damage only. She is unstoppable, not invulnerable.
- **At the landing** — the main window, and the reason Q13b ends protection at Last Exit. She arrives in a crowd as an ordinary target, which is where "controlled recklessness" is supposed to bite.

**Consequently her ultimate's power budget must be paid somewhere other than CC-resistance**: cast time, cooldown, a readable wind-up, or her vulnerability on landing. It should not also carry high raw output, or there is nothing left to answer.

One upside worth recording: the literal reading is the simplest possible rule to implement correctly. There is no bespoke slow-resistance path and no exception for a future reader to misapply.

---

## E. Hound as a threat

**Q14. What exactly does bailed-out Hound do on contact?** — **ANSWERED 2026-09-20**

> **It pierces everything for the full length of its travel, knocking back each enemy once per cast.** Nothing stops it — not minions, not wildlife, not Vanguards.
>
> - **CC type: Knockback**, a named §8 type. The kit text already differentiates the two halves of Last Exit and this ruling keeps that: **Hound knocks back, Raska's landing knocks up**. No new control type is invented.
> - **One hit per enemy per cast**, so no per-enemy internal cooldown is needed — a single pass cannot strike the same target twice.
> - **Physical damage**, matching her damage profile, **attributed to Raska** under Combat Bible §32 for kills, assists and tower aggression.

Its counterplay is that it is a readable straight line she must line up while already committed, not that something can body-block it.

**Q15. Can Hound be interacted with?** — **ANSWERED 2026-09-20**

> **No Health, cannot be damaged or destroyed** — settled by Q2, since it is a projectile rather than a unit. **It is interceptable**: both Spell Shields (§19) and projectile interception (§20) apply to it exactly as they would to any projectile. No exception is written either way.

Given Q14 makes it pierce everything, interception is the **only** in-flight answer to it. That is deliberate: the recast should have some counterplay, and reusing the two mechanisms that already exist is better than inventing a bespoke one.

**Q16. Does Hound collide with terrain and structures?** — **ANSWERED 2026-09-20**

> **Terrain stops it.** Confirmed against §24: Ghosted means ignoring unit collision, never terrain. A wall-ignoring bike would be a bug, not a feature.
>
> **It damages structures and draws tower aggression.** Bail Out near a defended structure is meant to be a genuine mistake.

**This requires an explicit exception, which must be written deliberately.** §33 states that "normal abilities do not damage structures", with only two exceptions: empowered-basic-attack abilities, and abilities **explicitly flagged as able to damage structures**. Bail Out is an ability and not an empowered basic attack (Q18), so it must be flagged structure-enabled under the second exception, and §33 requires such an ability to "explicitly define whether it uses standard Structure Effectiveness or its own structure ratio". That ratio is a tuning value for data.

**A nuance that may defeat the intent, and needs deciding — see Q16b.** Tower aggression under the Battleground Bible is drawn when an enemy Vanguard damages a *defending Vanguard* inside the tower's range, and for owned entities it "draws tower priority to their owning Vanguard **if that owner is in range**". Read literally, Raska can bail out from *outside* tower range, send Hound in, and draw no aggression at all — which is precisely the sloppy dive this ruling meant to punish.

**Q16b. Is the owner-in-range condition waived for Hound?** — **ANSWERED 2026-09-20**

> **No. The condition stands.** Raska draws tower aggression from Hound's damage only when she herself is within the structure's range, exactly as every other owned entity in the roster does. No exception is written.

The practical effect: bailing out from **outside** tower range and sending Hound in is a legitimate play that draws no aggression. It is also a lower-reward one — she is not present to follow up on the knockback — so it reads as a real tactical choice rather than a loophole. The punishment lands when she commits personally, which is the dive the ruling was aimed at.

---

## F. Combat interactions

**Q17. Can she basic-attack while mounted?** — **ANSWERED 2026-09-20**

> **No. She cannot basic-attack while mounted, and Roadhouse cannot fire from the bike.** The ride state is mobility and three abilities; it is not a damage stance.

**This creates the character's core loop**, which the passive implies but only works if attacking is off the table:

**ride to build Momentum (Q8, by distance) → spend it on a Redlined mounted action (Q20) → dismount → cash Roadhouse on foot.**

The bike sets up; her feet finish. Riding becomes a commitment with an opportunity cost rather than a strictly better state.

**Follow-on effects:**

- **Sideswipe is her only mounted damage**, which raises its importance in her kit considerably.
- **She cannot attack structures while mounted** — §33 structure damage comes from basic attacks, so Hound's flagged structure damage (Q16) is her only mounted structure interaction.
- **Attack-move orders while mounted collapse to move orders.** Worth stating explicitly so the input behaviour is not surprising.
- The awkward question of how an attack timer behaves at the game's highest Movement Speed does not arise.

**Q18. Are the mounted actions abilities?** — **ANSWERED 2026-09-20**

> **All three are abilities. None is a basic attack.** Sideswipe, Powerslide and Bail Out carry the Ability damage tag (§2), trigger On Ability Hit (§16), and benefit from Ability Haste (§21). None triggers On-Hit effects or item procs.

Sideswipe is the one that could have gone either way, being described as an "offensive pass". Keeping it an ability holds the line the rest of the roster already takes against proc stacking — Bryn's explosions "never recursively trigger Breach or basic-attack On-Hit effects", Mimzi's secondary bolts "cannot apply Hex, generate further bolts, or recursively trigger Pocket Hex". One more attack-tagged ability on a high-mobility bruiser is exactly where that guard matters.

**Q19. Do the mounted actions replace Q/W/E, and what happens to their cooldowns?** — **ANSWERED 2026-09-20**

> **Slot mapping:** Q → **Sideswipe**, W → **Powerslide**, E → **Bail Out** (E was Kickstart, so it becomes the dismount). R remains **NO BRAKES**.
>
> **Cooldowns are completely independent, and both sets tick normally.** Mounted actions run their own timers; her unmounted cooldowns continue counting down while she rides, so she dismounts with Breakneck and Countersteer available.

**This follows Angeru's precedent deliberately** — his two stances "maintain completely independent basic-ability cooldowns" — so the roster has one rule for stance-like ability swaps rather than two. A future third case should follow it too.

**What gates the power** is Kickstart's own cooldown and the ride duration, not the mounted abilities. That is where her tuning pressure sits: the mounted set is intentionally generous, so the cost of entering must carry it.

**Priced deliberately, not inherited:** dismounting with her unmounted abilities refreshed is a real part of her power, chosen rather than fallen into. Cooldowns ticking regardless of state is ordinary behaviour across the roster, and pausing them would have been the exception needing justification.

**Q20. Does Redlined apply to mounted actions?** — **ANSWERED 2026-09-20**

> **Yes. Sideswipe, Powerslide and Bail Out are basic abilities for Redlined purposes**, and mounting is therefore a real Momentum sink rather than a way to sidestep spending it.

**This is what makes NO BRAKES an ultimate.** The passive empowers "the next basic ability" at maximum Momentum; NO BRAKES states that "Momentum remains full". Together, every mounted action is **Redlined for the entire duration**, because the meter never drops below maximum for the empowerment to be consumed from. R is not "E but faster" — it is a window in which her whole mounted kit is empowered continuously.

That is a large multiplier, chosen deliberately, and it is the other half of the power budget noted under Q13. With an ultimate that is Unstoppable, unslowable, at the game's highest speed, *and* continuously Redlined, its cost must be carried by cast time, cooldown, wind-up readability and her vulnerability on landing — not by raw per-hit output.

**Q21. Does her gameplay hitbox change while mounted?** — **ANSWERED 2026-09-20**

> **Yes. Her gameplay hitbox is larger while mounted**, as a single combined volume — Raska and Hound are not separately targetable.

This follows §13 rather than making an exception to it: gameplay hitboxes are authoritative and "the visible danger area must closely match the actual gameplay collision area". She is visibly a person on a motorcycle, so a person-sized hitbox would have required a written exception and would have left skillshots visibly clipping the bike without connecting.

It is also the clearest thing she pays for the ride with. She is harder to stop — Unstoppable under R, unslowable, at a set high speed — and correspondingly easier to hit. The exact size is a tuning value for data.

---

## G. Vision, terrain, match flow

**Q22. Does the ride state affect vision?** — **ANSWERED 2026-09-20**

> **No change of any kind.** Normal vision radius while mounted, and riderless Hound grants no vision, no reveal and no scouting.

This holds the roster-wide line. Sylra and Bryn are the deliberate information specialists and both are capped at **presence** rather than position; Kade's Sightline was ruled no-reveal on the same grounds. A physical bruiser with a travelling scout would have exceeded both dedicated information characters, which is the inversion that ruling exists to prevent.

**Q23. Can Hound or the ride state cross terrain Raska cannot?** — **ANSWERED 2026-09-20**

> **No. Terrain blocks her entirely while mounted**, and blocks riderless Hound too (Q16). Ghosted is unit collision only per §24, and that is the whole of the ride state's traversal privilege.

Her mobility identity is **speed, not traversal**. Gorraveth keeps the roster's designated-gap crossing to himself, and Raska gets no wall-hopping at any point, including under NO BRAKES.

**Q24. What happens on death while mounted, and on respawn?** — **ANSWERED 2026-09-20**

> **Hound continues as a piercing projectile**, exactly as on Bail Out. Dying at speed releases the bike rather than deleting it — a runaway motorcycle does not stop because its rider did. She respawns on foot, and death does not alter the Kickstart cooldown beyond its normal progression.

**Two details this requires, both settled by existing rules rather than new ones:**

- **Direction.** On Bail Out she chooses her heading by riding; on death she does not. The released Hound travels along her **heading at the moment of death**, with no player input.
- **Attribution.** Under §32 its damage and kills still trace to Raska, so a posthumous kill credits her. This is consistent with how Veyra already treats damage-over-time that outlives its caster, and needs no exception.

**Recorded as a deliberate choice:** this does mean a mid-commit death still produces teamfight control she did not survive to earn. It fits the fiction and it keeps a failed engage from being a total loss — but it is a small consolation prize on every death while mounted, and it should be remembered as a tuning lever if her engage proves too cheap.

---

## H. NO BRAKES and Last Exit

**Q25. Does Last Exit produce two simultaneous displacement sources?** — **ANSWERED 2026-09-20**

> **An enemy caught by both takes both damage payloads but is displaced only once, by Raska's knockup.** Hound's knockback is suppressed against any target already caught centrally by the landing.

This deliberately avoids the §9 displacement-replacement race. Left to the general rule — "the newer displacement replaces the remaining movement of the previous" — the outcome would depend on resolution order and would look inconsistent to players hit by the same ability twice in the same instant.

It also keeps the ultimate readable: the centre is a knockup, the line is a knockback, and nobody experiences both. Enemies caught by Hound but *not* by the central landing are knocked back normally.

**Q26. Is Last Exit optional, and what happens if it is never cast?** — **ANSWERED 2026-09-20**

> **The recast window is the whole duration, and Last Exit fires automatically at expiry if she never casts it.** The payoff is guaranteed; only its timing is hers to choose.

**What this changes.** The recast becomes a **when** decision rather than a **whether** decision. She cannot waste the ultimate by mistiming it or dying to the clock without the finisher, which removes a genuine feel-bad — at the cost of removing the choice to hold the bike and simply ride. NO BRAKES is guaranteed value once cast.

**Two interactions, both already consistent:**

- **Unstoppable (Q13b)** ends when Last Exit fires, whether cast manually or automatically. The window therefore covers the whole duration either way, with no special case.
- **Death during R (Q24)** pre-empts the auto-fire: the ride ends, Hound continues as a projectile along her heading at death, but Raska's landing knockup does not occur, because she is not there to land.

---

## Specification to write into the Combat Bible

Per Q1 this becomes a **generic** Combat Bible section — "a Vanguard in a ride state", never "Hound" — so a future second ride user costs nothing. Condensed:

**The state.** Entered by a Cast-Time, interruptible ability; no cooldown or resource is spent if interrupted before Commit (§26). Movement Speed is **set** to a data-driven value, not added, so §23's soft caps do not apply. The rider is **Ghosted** (§24: unit collision only, never terrain) and cannot cross any terrain an unmounted Vanguard could not. Gameplay hitbox is **larger** while mounted, as one combined volume (§13). Vision is **unchanged**. The rider **cannot basic-attack**; attack-move collapses to move. Flux Spells remain available; recall and shopping require leaving the state.

**Movement.** An ordinary pathing agent with a **rate-limited facing**. The constraint is server-owned, client-mirrored. Turn rate does not scale with speed. A destination behind the rider produces a **wide U-turn at speed**, not a slow-and-pivot.

**Abilities.** The mounted set replaces the basic abilities in their slots with **completely independent cooldowns**, and both sets tick normally — matching Angeru's stance precedent. Mounted actions are **abilities** for damage tags (§2), On Ability Hit (§16) and Ability Haste (§21); none is a basic attack, so none triggers On-Hit or item procs. They are eligible for the rider's own empowerment mechanics.

**Crowd control.** Applies by ordinary rules; the state **persists through all of it** and no CC forces an exit. Hard CC reduces movement to 0 per §23, with no exception — readability at speed is an animation problem, not a rules one. A ride state may additionally grant **Unstoppable** (§9), in which case §8's rule that Suppression is unaffected by CC immunity still stands.

**Leaving.** Exits are the dismount ability, duration expiry, any recast that separates rider from vehicle, and death. The rider **carries momentum out and decays** to normal speed rather than stopping dead.

**The vehicle after separation.** A **projectile** (§13), not an owned entity — no Health, not destructible, **interceptable** by Spell Shields (§19) and projectile interception (§20). It **pierces everything** for its full travel, applying one Knockback per enemy per cast. Terrain stops it. It may be flagged **structure-enabled** under §33's second exception, which requires its own defined structure ratio; tower aggression follows the ordinary owner-in-range condition with **no exception**. On death it releases along the rider's heading at the moment of death; attribution traces to the rider under §32, including posthumous kills.

**Raska-specific detail that stays in the Character Bible, not here:** Momentum accrues by **distance travelled**, so "especially from riding" falls out of the arithmetic and needs no separate multiplier. Last Exit auto-fires at expiry; an enemy caught by both the landing and the vehicle takes both damage payloads but is displaced **once**, by the knockup.

## Status of the implementation gate

Raska needs **none of ADR-003's three primitives** (Q2). With these rulings she is gated only on this Combat Bible section being written. Once it exists she is fully specified and implementable.

Every numeric value referenced above — turn rate, set speed, hitbox size, Momentum per distance, structure ratio, durations, cooldowns — is tuning and belongs in validated designer-editable data per `ARCHITECTURE.md` §1.3, not in the Combat Bible section.

One thing still warrants a grey-box prototype before tuning begins: how wide the U-turn actually feels. Deciding the model on paper was sound; deciding the numbers is not.
