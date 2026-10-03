# ADR-062: Side-based unit collision, local avoidance and hosted diagnostics

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-03
**Related:**
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §24: unit collision and Ghosted.
- [ADR-006](ADR-006-unreal-project-scaffold.md) §5 (network budgets) and §7 (no Mass, server-moved units).
- [ADR-009](ADR-009-runtime-combat-primitives.md) §1, §6: Vanguard movement is server-only, with no client prediction.
- [ADR-011](ADR-011-battleground-runtime.md): Fluxborn movement and their blocking collision with avoidance among themselves.
- [ADR-057](ADR-057-public-test-hosting-from-the-authors-pc.md): public tests hosted from the author's PC.

## Context

The first outside playtest (2026-10-03, a custom game between two friends and bots) found that the game felt laggy, rubberbanded, and that Fluxborn "body-block way too hard".

The causes in the code:
- **Collision:** every unit's capsule blocks every other unit on one channel. Vanguards follow navmesh paths, which know nothing of units, so a path runs straight through a wave. The Vanguard then grinds and slides along its capsules and re-paths. Fluxborn avoid only each other.
- **Feel:** a Vanguard moves only on the server (ADR-009), so the jerky movement the owner sees near a wave is the server's own stop-and-slide, made worse by any network jitter.
- **Diagnostics:** the hosted match server was removed two minutes after the match, taking its log. Its network statistics record bandwidth and server load, but not the round trip, jitter or loss a remote player has.

The canon (Combat Bible §24):
- enemy Vanguards and enemy Fluxborn have unit collision, and enemy Vanguards can body-block;
- allied Vanguards don't hard-body-block each other; pathing uses soft separation;
- allied Fluxborn "have collision but pathing should aggressively avoid trapping allied Vanguards";
- Ghosted ignores unit collision, never terrain;
- server-side separation prevents embedding from displacement, spawning or latency.

## Decision

### 1. Units collide by side
- **Channels:** each unit's capsule takes its side's collision object channel, side A or side B. It blocks the other side's channel and ignores its own. Neutral units (wildlife) keep the pawn channel and block both sides.
- **The result:**
  - enemy Vanguards and enemy Fluxborn block;
  - allies of every kind pass through each other;
  - structures and terrain block everyone, as before.
- **One owner:** a single combat verb, `VeyraCombat::ApplySideCollision`, sets it for every unit kind. Ghosted, riders and attached bodies ignore both sides' channels and the pawn channel, never terrain (§24).

### 2. Allies separate softly
Allies who overlap are pushed apart on the server, at a configured speed and only while they overlap. They never stand inside each other for long, and never block. Unstuck rules use the same pass: a unit left embedded in an enemy or neutral by displacement or spawning is moved to the nearest free spot.

### 3. Local avoidance
- **Steering:** the server controllers that move Vanguards and Fluxborn steer with crowd avoidance around the units in their way, instead of following a navmesh path blindly.
- **Fluxborn:** they weigh allied Vanguards heavily, so they step aside.
- **Vanguards:** they go around an enemy wave rather than into it.
- **Settings:** data.

### 4. Hosted diagnostics
- **Keeping logs:** the Docker allocator saves each match server's log to a configured folder before removing its container.
- **Network statistics** gain each connection's round trip, its variation and packet loss.
- **Hosted matches** log them at a configured interval.

### 5. The public address while hosting is off
When the Worker's recorded tunnel is gone, Cloudflare answers with an origin error (502, 520–530). The Worker treats those as the host being offline and shows the offline page.

### 6. The Docker path and smoothing
- **Measuring:** the milestone measured a full 5v5 battleground (two clients and eight playing bots, 2026-10-03) on the Docker server and on a native server on the same PC, with network statistics every 10 s.

| | Docker server | Native server |
|---|---|---|
| Server busy per frame, average (worst steady) | 1.3–3.3 ms (≈11 ms) | 2.0–5.0 ms (≈14 ms) |
| Round trip per connection | 30–33 ms | 29–32 ms |
| Jitter | 3–6.5 ms | 2–6.5 ms |
| Packet loss | 0% | 0% |
| Sent per client | ≤ 10 KB/s | ≤ 9.5 KB/s |

- **The Docker path:** it adds nothing measurable, so ADR-057 stands.
- **The server:** it uses about a tenth of its 33 ms frame with about 180 replicated actors, and bandwidth is far inside ADR-006 §5's budgets.
- **The round trip** has a floor near 30 ms even on one machine: an order waits for the server's next 30 Hz tick, and so does its answer. A remote player's network round trip adds to that. Hosted logs now record each player's.
- **The feel work** therefore targets what the player sees between a click and the server's answer (G5: instant click feedback), and the stop-and-slide near waves (§1–§3).
- **No prediction:** movement prediction stays out of scope (ADR-009 §6).

### 7. Provisional answers where canon is open
1. Allied Fluxborn don't hard-block allied Vanguards. They yield through avoidance and soft separation, which is this record's reading of "collision, but pathing should aggressively avoid trapping".
2. The separation speed, the crowd's avoidance settings and the statistics interval are provisional data.

## Consequences
- **Readability:** allies never trap each other, and enemies still body-block as the canon means.
- **One owner** each for collision, separation and avoidance, rather than per-unit fixes.
- **Playtests can be diagnosed:** hosted logs and connection statistics come from what the player actually saw.
