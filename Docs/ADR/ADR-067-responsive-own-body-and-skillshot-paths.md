# ADR-067: The player's own body answers at once, and skillshots show their path

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. The closing section lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-06
**Related:**
- [ADR-006](ADR-006-unreal-project-scaffold.md), its M3 amendment: the server moves every unit, with no client prediction (author ruling, 2026-09-25). This record keeps that.
- [ADR-062](ADR-062-side-based-unit-collision-local-avoidance-and-hosted-diagnostics.md) §6: the click mark, shown at once.
- [ADR-065](ADR-065-first-match-feedback-waves-spires-rewards-and-patch.md) §12: drawn bodies ease between the server's updates.
- [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md) §4: projectiles drawn from their launch data, with a trail.

## Context
The author and a friend played 0.2.2 as a hosted 5v5 with bots. Both felt "crazy lag", and found skillshots hard to judge.

The hosted server measured itself healthy:
- 30 frames a second, 4–5 ms busy each;
- no packet loss;
- round trips of 29–63 ms for the author and 113–135 ms for the friend, with 13–15 ms of jitter.

What the players felt is the design:
- **The click waits.** No client predicts movement (ADR-006, M3 amendment). A move click shows its mark at once, but the player's own body does nothing until the server's next update brings its movement back: a round trip plus up to a server tick.
- **The ease adds to it.** Since ADR-065 §12, a drawn body eases over 0.1 s (place) and 0.1 s (facing) to every correction. The moments that correct most are a start, a turn and a stop, which are the moments a dodge is made of.
- **Projectiles are already exact.** They are drawn where the server's flight has them now, but bodies trail their own truth. A skillshot that the server says missed can look like a hit, and the reverse.

## Decision

### 1. Bodies ease over about a server tick
Vanguards', companions' and Echoes' drawn bodies ease over **0.04 s** for place and **0.05 s** for facing, about one server tick (were 0.1 and 0.1). Fluxborn and creatures, updated three times less often, ease over **0.1 s** for both (were 0.15). The ease still hides the step of each update; it no longer holds a turn or a stop back.

### 2. The player's own body leads its latest order
On the player's own machine only, and in presentation only:
- **What the lead does.** The body the player commands turns toward its latest order's point at once, as fast as a set turn rate allows. For a move or an Attack Move it also starts its run.
- **When it ends.** As soon as the server's movement heads within a set angle of the point, or after a set time, whichever comes first.
- **When it never starts.** No lead runs while the body is dead, while its statuses forbid moving, or for a point within the body's own reach.
- **An attack order** turns the body toward its target. It runs only if the target lies beyond its reach, since the server will chase it; within reach the body stands, whatever its last movement was.
- **The server stays in charge.** Its movement is what the body then shows. A refused order simply ends the lead, and nothing reaches the server.
- **Values:** a lead of at most **0.35 s**, turning at **1,080°** a second, ending once the server's movement is within **30°** of the point.

### 3. A skillshot shows the rest of its path
Every projectile that flies a line is drawn with a faint lane on the ground. The lane runs from where the server's flight has it now to where it will end, as wide as the projectile, in its side's colour. A player sees where a shot is going while there is still time to step out of its way. Homing projectiles show none: they follow their target.

### 4. Not decided here
- Client-side movement prediction, which would amend ADR-006's author ruling.
- The match server's network path on Windows hosts. Docker's UDP forwarding adds a few milliseconds and some jitter even for the host's own client.

## Consequences
- Turns and stops show sooner, for everyone.
- The player's own Vanguard answers a click within a frame. Its true movement follows one round trip later.
- A lead can show a turn the server then refuses, as when a stun lands with the click. It lasts at most 0.35 s and leaves no trace.
- Skillshots read by their lanes as well as their spheres.

## Provisional answers for the author
1. Ease times of 0.04 s and 0.05 s for Vanguards, companions and Echoes, and 0.1 s for Fluxborn and creatures (§1).
2. A lead of at most 0.35 s, turning at 1,080° a second, ending within 30° (§2).
3. A lane under every line projectile, for every player (§3).
