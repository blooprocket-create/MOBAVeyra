# Ride state — open questions

**Version:** 0.1
**Date:** 2026-09-20
**Status:** **Questions only. Nothing here is canon and nothing here is a decision.**

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

**Q1. Where does this canon live?**
A new Combat Bible section, a standalone Ride State Bible, or ADR-004? It spans locomotion, input, netcode, CC and entity ownership, so it does not sit cleanly inside any current owner. ADR-003 deliberately did not decide this.

**Q2. Is Hound an owned field entity after Bail Out or Last Exit?**
While ridden it is a locomotion state. Once Raska leaves it, it travels independently, collides and applies control — which is the behaviour ADR-003 categorises. If it qualifies, which of the three primitives is it, and does Combat Bible §32 attribution apply to its damage and kills? If it qualifies as none, that is itself a finding about ADR-003's categories.

**Q3. Does Hound exist when not summoned?**
Spawned per cast, or a persistent world object that travels to her? This changes whether it can be seen, targeted or intercepted before Kickstart resolves.

---

## B. The movement model — the hard one

This is the question that decides whether the feature is a week or a quarter, and it should be answered first.

**Q4. What does "wide turning behavior at high speed" mean mechanically?**
Veyra uses click-to-move with effectively instantaneous facing. A turn radius makes Raska's movement **non-holonomic**: she can no longer reach an arbitrary clicked point directly, and her reachable set depends on current heading and speed. Options, roughly in ascending cost:

- **a.** Pure fiction — animation leans into turns, movement stays ordinary click-to-move. Cheapest, and loses the identity guardrail the bible sets ("not a generic puncher with a motorcycle skin").
- **b.** Turn-rate cap on an otherwise ordinary pathing agent — she pivots at a limited angular rate, still pathfinds to the click.
- **c.** True steering model — directional input, momentum-carrying, genuinely non-holonomic.

Each has different consequences for pathfinding, input, prediction and counterplay, and (c) is a different movement system from every other Vanguard.

**Q5. Does the ride state change the input model?**
If turning is real, click-to-move may not express it. Does Raska use directional/WASD-style input while mounted, and if so what happens to queued move commands, attack-move and right-click attack orders?

**Q6. Does ride mode bypass the Movement Speed soft caps?**
§23 permits explicit bypass. NO BRAKES says speed becomes "enormous". Does it bypass, and is there a hard ceiling? Note the answer interacts with Q4: a turn radius at very high speed is what makes the fantasy work, and also what makes her hardest to control.

**Q7. How is ride movement predicted client-side?**
ADR-002 requires server authority and forbids prediction granting the client final authority. A fast non-holonomic vehicle is the worst case for prediction and reconciliation — a mispredicted heading diverges far more than a mispredicted position. Does ride movement predict at all, and what does a server correction look like at speed?

**Q8. Does Momentum accrue by distance, by time, or by speed?**
The passive says Momentum builds "especially" from high-speed riding. Each basis rewards different play, and the answer determines whether riding in circles in base is a valid way to enter a fight Redlined.

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

**Q12. What does each CC type do to a mounted Raska?**
This needs a row per type, not a general principle. §8's list: Stun, Root, Silence, Disarm, Slow, Knockup, Knockback, Pull, Polymorph, Suppression, Fear, Taunt, Charm, Sleep, Blind.

Specifically:
- **Root** is "cannot move or dash" — does a rooted motorcycle stop instantly at 600 Movement Speed, or decelerate?
- **Knockup/Knockback/Pull** take ownership of movement under §9. Does displacement dismount her, carry the bike with her, or is she Unstoppable while mounted?
- **Fear/Taunt/Charm** remove normal control and force movement. Can a non-holonomic vehicle even execute forced movement toward a taunter?
- Does any CC force a dismount, and if so does Hound persist?

**Q13. Is any part of the ride state Unstoppable or Uninterruptible?**
§9 separates these. NO BRAKES in particular reads as though it should resist being stopped, but the bible does not say so, and granting it silently would be a power increase nobody signed off.

---

## E. Hound as a threat

**Q14. What exactly does bailed-out Hound do on contact?**
- Which named CC — Knockback or Knockup?
- Does it hit once and stop, pierce and continue, or stop at the first enemy Vanguard?
- Per-enemy internal cooldown, or one hit per enemy per cast?
- Damage type and attribution to Raska?

**Q15. Can Hound be interacted with?**
Does it have Health, can it be damaged, destroyed, intercepted by a Spell Shield (§19) or projectile interception (§20)? Is it a projectile, a summon, or something else? This is the concrete form of Q2.

**Q16. Does Hound collide with terrain and structures?**
Ghosting is unit collision only (§24), so terrain should stop it — confirm. Does it collide with towers and the Prime Well, and does it draw tower aggression per Combat Bible §32's owner-attribution rule?

---

## F. Combat interactions

**Q17. Can she basic-attack while mounted?**
If not, say so. If so: does Roadhouse — a basic attack — proc from horseback, and how does the attack timer behave at speed?

**Q18. Are the mounted actions abilities?**
Do Sideswipe, Powerslide and Bail Out count as Ability for damage tags (§2), On Ability Hit (§16), item procs, and Ability Haste (§21)? Does Sideswipe count as a basic attack for On-Hit?

**Q19. Do the mounted actions replace Q/W/E, and what happens to their cooldowns?**
If Breakneck is on cooldown when she mounts, does Sideswipe inherit it, share it, or run independently? Compare Angeru, whose two stances explicitly keep **completely independent** cooldown sets — is that the precedent, or the exception?

**Q20. Does Redlined apply to mounted actions?**
The passive empowers "the next basic ability". Are mounted actions basic abilities for that purpose?

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

1. **Q4** (the movement model) before anything else — it determines the cost of the entire feature and constrains Q5, Q6, Q7, Q12 and Q21.
2. **Q1** (where the canon lives) and **Q2** (is Hound an owned entity), because they decide which document answers the rest.
3. Section D (crowd control), which is the largest source of implementation ambiguity and of accidental power.
4. Everything else.

Q4 is worth answering with a grey-box prototype rather than on paper. It is the one question in this document that a paragraph cannot honestly settle.
