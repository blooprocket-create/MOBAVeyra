# Ride state — open questions

**Version:** 0.1
**Date:** 2026-09-20
**Status:** answers being recorded as they are given. Answered items are marked **ANSWERED** with the date; everything else is still an open question and not canon.

**Progress:** 11 of 26 answered (Q4, Q1, Q2, Q12, Q13, Q6, Q8, Q19, Q20, Q18, Q14).

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

**Q3. Does Hound exist when not summoned?**
Spawned per cast, or a persistent world object that travels to her? This changes whether it can be seen, targeted or intercepted before Kickstart resolves.

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

**Q5. What happens when the move destination is behind her?** *(reduced by Q4 — the input model stays click-to-move)*
The turn-rate cap creates one case ordinary pathing does not have an answer for. Options:

- **wide U-turn** — she arcs around at speed, travelling a long way before she is heading back. Most committal, most readable, best fits "controlled recklessness".
- **slow and pivot** — she sheds speed until the turn is within her rate, then accelerates again. Forgiving, and quietly removes most of the drawback.
- **brake and reverse** — a distinct reversing state. Fiddly, and probably a different fantasy.

This also needs a rule for whether the turn rate scales with current speed, which is what would make high-speed commitment feel different from low-speed manoeuvring.

**Q6. Does ride mode bypass the Movement Speed soft caps?** — **ANSWERED 2026-09-20**

> **Neither bypass nor climb — ride mode *sets* Movement Speed to a data-driven value.** While mounted her Movement Speed is the bike's, replacing her ordinary value rather than adding to it. The question of bypassing §23's 415/490 ladder does not arise, so no exception to §23 needs writing.

**Why this shape.** It is predictable to balance, it cannot be compounded by Movement Speed stacking into an ultimate that already cannot be slowed, and it matches the fiction: a motorcycle's top speed is the motorcycle's, not the rider's boots.

**The accepted cost:** Movement Speed items do nothing while she is mounted. They still govern her dismounted movement, so they are not dead weight, but a speed-stacking build will not make Hound faster. If that later proves to narrow her itemisation too far, the lever is the set value itself, not a return to additive bonuses.

**Interactions to carry into the Combat Bible section:**

- **Slows outside R** still apply, reducing *from* the set value, and §23's ordinary slowing floor of **100** still applies. A slowed bike is slow.
- **Inside R** she is Unstoppable per Q13, so slows do not land at all.
- The set value itself, and whether NO BRAKES uses a higher one than Kickstart, are tuning values and belong in data per `ARCHITECTURE.md` §1.3.

**Q7. How is ride movement predicted client-side?** *(largely settled by Q4)*
With a turn-rate cap on an ordinary pathing agent, ride movement predicts like any other unit and ADR-002's server authority is unaffected. What remains: confirm the angular constraint is owned and applied server-side with the client mirroring it, rather than each simulating independently, since a diverged *heading* compounds into a much larger positional error than a diverged position does.

**Q8. Does Momentum accrue by distance or by time while mounted?** — **ANSWERED 2026-09-20**

> **By distance actually travelled**, mounted or on foot. A stationary bike builds nothing. Attacks and abilities remain separate flat contributions on top, as the passive already describes.

**A consequence worth noticing:** the passive says Momentum builds "especially" from high-speed riding, and with a distance basis that falls out of the arithmetic rather than needing its own rule. At the fixed ride speed from Q6 she covers far more ground per second than on foot, so riding generates Momentum proportionally faster automatically. No separate riding multiplier is required, and adding one would be double-counting.

It also closes the idling exploit: there is no way to accumulate Momentum by waiting mounted behind terrain or in base.

The rate per unit distance is a tuning value and belongs in data.

---

## C. Entering and leaving

**Q9. What cast class is Kickstart?**
§26 requires every ability to declare Instant / Cast-Time / Channel / Charged. Can mounting be interrupted, and if interrupted mid-mount, is the cooldown spent?

**Q10. How does the ride state end?**
Enumerate every exit: Bail Out, duration expiry, Last Exit, death, and any CC that forces a dismount (Q12). For each: what happens to Hound, and does Raska keep residual speed or stop dead?

**Q11. Can she recall, shop, or use Flux Spells while mounted?**
Ordinary state questions, but they need an answer before the ability bar is built.

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

**Q15. Can Hound be interacted with?**
Does it have Health, can it be damaged, destroyed, intercepted by a Spell Shield (§19) or projectile interception (§20)? Is it a projectile, a summon, or something else? This is the concrete form of Q2.

**Q16. Does Hound collide with terrain and structures?**
Ghosting is unit collision only (§24), so terrain should stop it — confirm. Does it collide with towers and the Prime Well, and does it draw tower aggression per Combat Bible §32's owner-attribution rule?

---

## F. Combat interactions

**Q17. Can she basic-attack while mounted?**
If not, say so. If so: does Roadhouse — a basic attack — proc from horseback, and how does the attack timer behave at speed?

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

**Q21. Does her gameplay hitbox change while mounted?**
§13 makes gameplay hitboxes authoritative and requires visuals to match. A bike is visually larger; if the hitbox grows, that is real counterplay, and if it does not, the visual fidelity rule needs a note.

---

## G. Vision, terrain, match flow

**Q22. Does the ride state affect vision?**
Does Hound grant vision, does riding change her vision radius, and does a bailed-out Hound scout? Any answer that grants remote sight needs the Vision Bible's agreement, which the roster otherwise caps at presence information.

**Q23. Can Hound or the ride state cross terrain Raska cannot?**
Gaps, walls, the river. Gorraveth's Ravine Bound already establishes "short designated traversable gaps, not arbitrary walls" as a pattern worth reusing.

**Q24. What happens on death while mounted, and on respawn?**
Does Hound vanish, persist, or continue? Does she respawn mounted, and is the ride cooldown affected by death?

---

## H. NO BRAKES and Last Exit

**Q25. Does Last Exit produce two simultaneous displacement sources?**
Raska crashes down with an AoE knockup while Hound continues as a separate threat. Under §9, a second valid displacement replaces the remaining movement of the first. If an enemy is caught by both, what resolves, in what order, and does the enemy take both payloads?

**Q26. Is Last Exit optional, and what happens if it is never cast?**
Does NO BRAKES expire into an ordinary dismount, and is the recast window the whole duration?

---

## Suggested order

1. ~~**Q4** (the movement model)~~ — **answered**. A turn-rate cap, which reduced Q5 and Q7 and made Q12 tractable.
2. ~~**Q1** (where the canon lives)~~ — **answered**. A generic Combat Bible section. **Q2** (is Hound an owned entity) remains, because it decides whether Raska needs an ADR-003 primitive at all.
3. ~~**Q2**~~ — **answered**. A projectile, so Raska needs no ADR-003 primitive.
4. ~~Section D (crowd control)~~ — **Q12 answered**; Q13 now scopes the Unstoppable window it granted.
5. Everything else.

Q4's answer should still be validated in a grey-box prototype before the turn rate is tuned. Deciding the *model* on paper is sound; deciding how wide the turn feels is not.
