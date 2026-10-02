# ADR-054: Fountain preparation and fog of war on screen

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §5 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §1 (stage 3, fountain preparation) and §3.
- [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md) §1 (team fog of war) and §2 (Dense Fog); [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §11.
- [ADR-006](ADR-006-unreal-project-scaffold.md) §5: fog of war is enforced at the data boundary.
- [ADR-016](ADR-016-vision.md): the server's vision, the fog gate and the team state; its §4 left darkening unseen ground for real presentation.
- [ADR-043](ADR-043-map-walls-and-terrain-sight.md) §3: walls block sight.

## Context

Two match-facing pieces are missing.

- **Fountain preparation.** Before 0:00, players "can move within their own fountain, buy opening items, and allocate starting skill points, but cannot leave the fountain" (Match Flow §1). Buying and skill points already work in preparation. Movement does not: the game mode refuses every order until the match is Live, and a player stands still for fifteen seconds.
- **Fog of war on screen.** The server already decides what each side sees, and its gate never sends a client a unit its side cannot see. But nothing shows the player which ground their side sees. Seen and unseen ground look the same in the world and on the minimap, so an empty lane and an unwatched one cannot be told apart.

## Decision

### 1. Preparation lets a Vanguard move inside its own fountain

- During Preparation, the game mode accepts **move and stop orders** only. Attack, attack-move, cast, Recall and vision-tool orders are still refused as the wrong phase: the bible allows moving, buying and skill points, and nothing else.
- **The fountain area** is the circle of Match.json `fountain.radius` around the side's start, the same area that heals and lets a player shop.
- A move order's destination is brought inside that circle by the pure `VeyraMatchRules::ClampToFountain`, and the order is accepted with the clamped point. A destination already inside is unchanged.
- **Staying inside:** a path between two points of the circle stays inside it, so no exit can be walked. As a safety net, each preparation tick returns a Vanguard found outside the circle (pushed by a body, say) to its edge.
- **At Live,** every order is accepted as before: the fountain exits open together. Bots keep waiting for Live.

### 2. Vision publishes each side's seen ground

- On the server, every `presentation.updateSeconds`, Vision works out which cells of a square grid over the battleground each side's sources see.
- **The grid** is `presentation.cellsAcross` cells per side, over the layout's extent. A cell is seen when one of the side's sources sees its centre, by the same rules the gate uses: its circle or lit shape, and walls blocking unless it lights through them.
- The pure `VeyraVisionRules::SeenCells` tests, for each source, only the cells within its reach.
- **Dense Fog** does not darken ground: the fog itself is always seen, and only the Vanguards inside it hide (Vision Bible §2). Its existing markings stay.
- **Replication:** each side's grid is a bitset on that side's `AVeyraVisionTeamState`. The fog gate already lets only that side receive the state, so a client learns only what its own side sees. Nothing reads the grid to decide gameplay.

### 3. The client darkens the ground its side does not see

- **In the world (grey box):** each unseen cell is darkened by a translucent quad just above the ground. Runs of unseen cells in a row are drawn as one quad. The colour and opacity are a presentation setting.
- **On the minimap:** the same cells are darkened over the map.
- **Edges** are cell-sized in the grey box. Real presentation may soften them later without changing the contract.
- **No setting turns it off.** Settings may not change fog-of-war information (Settings §4).

### 4. Ownership

| Piece | Owner |
|---|---|
| Which orders preparation accepts, and the fountain clamp | VeyraMatch (the game mode, with the pure rule beside it) |
| The seen grid and its replication | VeyraVision (the subsystem, the pure rule and the team state) |
| Drawing it in the world and on the minimap | VeyraUI |
| The grid's size and cadence | Vision.json `presentation`, validated |
| The fog's colour and opacity | VeyraGreyboxSettings |

### 5. Provisional answers where canon is open

1. **The fountain area** is `fountain.radius`, the area that heals and shops.
2. **Only moving and stopping** are allowed in preparation. Casting would let a dash or a blink leave the fountain.
3. **The grid** is 64 cells per side, updated every 0.5 s: about 220 units a cell on the battleground, and about 1 KB/s per client.
4. **Dense Fog is not darkened:** its markings already show it, and its ground is seen.

## Out of scope

- Soft or animated fog edges, and fog in real (non-grey-box) presentation.
- Remembering what a side last saw of terrain or structures: the grid shows only what is seen now.
