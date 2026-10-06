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
- **One owner:** `VeyraUnitCollision` (VeyraCombat) sets it for every unit kind and answers which channels hold units. Ghosted, riders and attached bodies ignore both sides' channels and the pawn channel, never terrain (§24).

### 2. Allies separate softly
- **Moving allies:** moving allies keep apart through avoidance (§3). Fluxborn steer around each other and make way for their Vanguards, and nothing allied ever blocks.
- **Standing allies:** allied Vanguards standing still may overlap, as passing through each other allows. A pass that pushes standing allies apart is deferred: it changes only how a stack looks, never what blocks.
- **Embedding:** the character movement component's own depenetration frees a unit that displacement or spawning left inside an enemy or neutral body. Ghosted bodies are never embedded, since they block nothing.

### 3. Local avoidance
- **Steering:** Vanguards and Fluxborn steer around the units in their way with the movement component's reciprocal avoidance, on the server that moves them. Fluxborn already did, but only among themselves. A navmesh path knows nothing of units.
- **Groups:** avoidance groups by side and role decide who steers around whom:
  - a Vanguard steers around enemy and neutral units, and walks on through its allies, which it passes anyway (§1);
  - a Fluxborn steers around everyone, its own side's Vanguards included, so an allied wave makes way for them.
- **Settings:** data. A Vanguard's consideration radius and weight are `Match.json` `orders`; Fluxborn keep theirs in `World.json`.
- **Arriving at a body:** a destination that an enemy or a neutral unit stands on can't be reached, and steering would circle that body without end. A move order there ends when the Vanguard reaches the body's edge, within the order's arrival tolerance. An ally standing there is walked through, as §1 allows.
- **Not crowd following:** a Detour crowd was considered. It would replace both controllers' path following and needs a raised agent cap. Reciprocal avoidance gives the same yield and steer-around with one setting on the units that already move by it.

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
- **The feel work** therefore targets what the player sees between a click and the server's answer, and the stop-and-slide near waves (§1–§3).
- **Click feedback:** the local player's move, Attack Move and attack orders leave a mark at once, a round trip before the server answers.
  - The mark is a ring that closes on the ground ordered, or round the unit an attack names, and fades. Moves and attacks have different colours.
  - The grey-box presentation draws it from the order as the client gave it. It decides nothing, and the server never sees it.
  - Its look is presentation settings, kept with the grey-box's other settings. Under Reduce Interface Animation it stays still and only fades.
  - Click markers are optional (Settings & Accessibility Bible §3.3): Interface settings' Click Markers turns them off.
- **Smoothing:** no change.
  - Vanguards already replicate every server tick: the engine's default rate is above the 30 Hz tick. Fluxborn replicate every third tick (ADR-011 §7).
  - Clients draw both as simulated units, with the movement component's exponential smoothing between updates.
  - **Amended by ADR-065 §12:** that smoothing eases only a character's mesh, and bodies hung from the capsule, so it never reached them. Drawn bodies now hang from the mesh.
- **No prediction:** movement prediction stays out of scope (ADR-009 §6).

### 7. Provisional answers where canon is open
1. Allied Fluxborn don't hard-block allied Vanguards. They yield through avoidance and soft separation, which is this record's reading of "collision, but pathing should aggressively avoid trapping".
2. The avoidance settings and the statistics interval are provisional data.
3. A move onto a body that blocks the Vanguard ends at that body's edge (§3).
4. The click mark's look and timing are provisional presentation (§6).
5. Click Markers is on by default: the canon makes them optional without naming a default, and the playtest asked for feedback on every click.

## Consequences
- **Readability:** allies never trap each other, and enemies still body-block as the canon means.
- **One owner** for collision and avoidance groups, rather than per-unit fixes.
- **Feedback before the answer:** a click shows where it went at once, whatever the round trip.
- **Playtests can be diagnosed:** hosted logs and connection statistics come from what the player actually saw.
