# ADR-020: The camera, the minimap, and the kill economy's bounty, devaluation and buyback

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §6 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M15 pull requests that add it.  
**Date:** 2026-09-29  
**Related:**
- [ADR-006](ADR-006-unreal-project-scaffold.md): §7, server-side movement.
- [ADR-011](ADR-011-battleground-runtime.md): the layout, rewards and respawn.
- [ADR-012](ADR-012-items-and-shop.md): the shop.
- [ADR-016](ADR-016-vision.md): the fog gate and pings.
- [ADR-017](ADR-017-match-statistics.md): the record.
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md): §1.8, §2 and §3.2.
- [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md): §5.3, §5.4, §14 and §15.
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1 and §1.3.

## Context

**The camera.** A player's view hangs from their Vanguard's spring arm, so the camera is always locked. The Settings Bible §2 approves:
- a **Free** camera as the default, with edge scrolling, rebindable keys and middle-mouse drag;
- a **Locked** mode and a **Semi-Locked** mode;
- **Hold to Center**.

**The minimap.** There is none. §3.2 approves one in a fixed place, with its scale and icon size as settings. Clicking or dragging it moves the camera, and a right-click on it moves the Vanguard (each can be turned off). It never shows what the fog hides.

**The kill economy.** It pays base kill Gold, assists and First Blood (ADR-011), but not:
- the visible kill-streak bounty (§5.3);
- death-streak devaluation (§5.4);
- buyback (§15).

## Decision

### 1. The camera is the local player's, and presentation only

- **The rig:** a client-only `AVeyraCameraRig` owns the view. The local PlayerController views through it rather than through its Vanguard's arm. It keeps the same distance and pitch (`UVeyraCameraSettings`).
- **The modes:**
  - **Free** (the default) moves by screen-edge scrolling, the camera keys and middle-mouse drag;
  - **Locked** follows the Vanguard;
  - **Semi-Locked** follows it, allowing an offset up to `SemiLockedMaxOffset`.
- **The keys:** `CameraLockKey` cycles the mode, and `HoldToCenterKey` follows the Vanguard while held.
- **The rules:** the pure `VeyraCamera` rules move the focus each frame from these inputs, clamped to the battleground's floor.
- **Settings:** its speeds and edge are presentation settings in `DefaultGame.ini`, not gameplay tuning.
- **No sight:** it grants none. The fog gate already decides what a client has.
- **The one game-driven pan:** at the end of a match the camera pans to the Prime Well that fell (§2, Proposal 16).
- **Watching the end (Provisional, so every player sees the Prime Well fall):**
  - An ended match stays up `ending.showSeconds` (Match.json) after it ends. The server reports its result at once, then quits once that time has passed, and each client leaves for the results after the same time.
  - A client whose server quits first leaves as from an ended match, not a lost connection.
  - Meanwhile the camera eases to the fallen Prime Well over `EndPanSeconds` (a presentation setting) and ignores the player's camera input.
  - The HUD's headline reads Victory or Defeat by whose Well fell. A match that ended another way reads Match Over; so does a practice match, where a Well decides nothing.

### 2. The minimap is drawn from what the client already has

**What it draws.** A grey-box canvas panel in the bottom-right draws the battleground's layout from `World.json`:
- the floor, the lanes and the river;
- each standing structure, by side;
- each unit the client has, whether Vanguard, Fluxborn, wildlife or ward. The fog gate means those are exactly the ones the side may see.
- the side's pings, and the camera's view.

**Its geometry.** It lives in VeyraUI, and the world-to-map projection is pure.

**Clicks.** The PlayerController, below UI, asks a hook that the UI sets whether a click falls on the minimap:
- a left click or drag there moves the camera;
- a right click there issues a move order to that point.

This follows the layering (ADR-006 §3): UI registers, Match never calls UI.

**Settings.** Its scale, icon size and the two click toggles are presentation settings.

**Team pings (Provisional).** The Chat & Communication Bible leaves the ping system to be designed. The Settings Bible already assumes pings exist: ping persistence, ping sounds, and text labels (SET-68). So two basic pings stand in until that design:
- **Sending:** holding `PingKey` (G) or `DangerPingKey` (V) and clicking pings "look here" or "danger" where the cursor points, on the ground or on the minimap.
- **The server** (`UVeyraPingSubsystem`, Match) refuses three kinds of ping:
  - one from a player who is not seated;
  - one outside preparation and live play (a pause stops play, not talk);
  - more than `pings.maxPerWindow` in any `pings.windowSeconds`.
- **Delivery:** it hands each ping to the controllers of the sender's side only, so the other side never receives one.
- **Keeping:** a client keeps a ping at most `pings.keepSeconds`. The player's ping persistence (`PingSeconds`, Settings Bible §3.2) decides how long it shows on the minimap and on the ground. Type labels are off by default (SET-68).

### 3. The kill economy (Economy owns the rules; Match orchestrates)

- **Kill streak and bounty (§5.3):**
  - consecutive credited kills build a bounty from `bounty.byStreak`, shown on the scoreboard and HUD;
  - on the Vanguard's next enemy-credited death, the credited killer alone receives it, beside ordinary kill Gold, and it resets;
  - an Execution (an uncredited death) neither pays nor clears it.
- **Death-streak devaluation (§5.4):**
  - consecutive enemy-credited deaths without a takedown step the Vanguard's base kill Gold down `killGold.devaluationSteps`;
  - each takedown (kill or assist) restores one step;
  - the assist pool follows the reduced base, and the bounty is never reduced;
  - an Execution changes nothing.
- **Buyback (§15):**
  - available from `buyback.availableFromSeconds`, to a dead Vanguard with the Gold whose personal cooldown is ready;
  - its cost is `baseCost + costPerMinute × (minutes past availability) + costPerPurchase × (buybacks already bought)`;
  - its cooldown starts when bought;
  - it respawns the Vanguard at its fountain now, and reverses nothing;
  - it is an order from the shop panel, refused with the reason its rule gives;
  - the record counts each buyback (`buybacks`), and a claimed bounty counts as Gold from kills.
- **The shop takes a new layout** (amending ADR-012 §11), since the shop panel is where buyback lives:
  - on the left, quick-buy panels: consumables with the vision tools beside them, boots, and the inventory;
  - in the middle, every item as a tile with its price now, grouped by tier, and a tab for the Flux Spell swaps;
  - on the right, the chosen item: what it builds into, its recipe, the one purchase button, and what it gives;
  - along the foot, sale of the chosen slot, undo, the purchases waiting for the fountain, and Gold;
  - a tile chooses its item and the purchase button buys it; a vision tool's tile swaps at once, as a trinket does.

### 4. Values

- **Economy.json** (schema bump): `bounty`, `killGold.devaluationSteps` and `buyback`. Each is Provisional unless the bible gives it: the devaluation steps are the bible's own illustration.
- **Match.json** (schema 8): `pings` and `ending`, both Provisional.
- **DefaultGame.ini:** the camera, minimap and ping presentation values.

### 5. Delivery

- **M15a:** the camera, the minimap and its clicks, pings on it, and the end-of-match pan.
- **M15b:** kill streaks and bounty, devaluation, buyback and its UI.

### 6. Provisional answers where canon is silent (for the author to overturn)

- **Bounty by streak:**
  - 0 and 0 Gold at 1–2 kills, then 150, 300, 450 and 600, capped at 1000;
  - a streak counts kills only, and a death resets it.
- **Buyback:** a cost that grows with match time and with each earlier buyback, on a cooldown:
  - 300 Gold base;
  - +25 per minute past 10:00;
  - +150 per earlier buyback;
  - a 240 s cooldown.
- **Camera keys:** Y cycles the camera mode, and Space holds to centre.
- **The shop's layout** (§3), for now without search, class and stat filters, recommended or saved-build tabs, or item icons.

## Consequences

- A player can look around the map, and the minimap makes the fog, the lanes and the objectives legible.
- Throwing a lead has a price (bounty), a feeding player costs less (devaluation), and a late fight can be rejoined (buyback).

## Amendments to earlier records

- **ADR-011:** kill Gold is adjusted by devaluation, and bounty is its own payout.
- **ADR-017:** the record counts buybacks (`buybacks`); a claimed bounty counts as Gold from kills.
- **ADR-012 §11:** the shop screen takes the new layout (§3 above).

## Open items

- The HUD art pass for the minimap.
- A spectator camera (Replay Bible).
- Buyback in practice matches, which the Custom Matches Bible leaves open. It is off here.
- The rest of the shop: search (typing must not reach the match's keys), class and stat filters, a Recommended tab, and item icons in place of initials.
- AI Vanguards buying back.
