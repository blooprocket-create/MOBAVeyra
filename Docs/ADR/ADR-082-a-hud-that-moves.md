# ADR-082: A HUD that moves, and the deck in the approved look

**Status:** Proposed. On 2026-10-13 the author reviewed the match HUD after ADR-080 and asked for more polish, saying "the in game UI needs the most polish". They asked for animation "like how the website does it... almost". This record sets the HUD's motion and finishes the deck, the panels and the world's bars in the look the author approved for ADR-078's HUD board. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md) §1: the graphic effect language the website's motion already follows. This takes that motion for the match: three curves, panels that cut in, a burst where something is pressed, a glint that crosses a title.
- [ADR-078](ADR-078-the-smoked-crucible-client-look.md): the client's type and surfaces, and the HUD board this finishes.
- [ADR-080](ADR-080-hud-tooltips-stats-panel-and-the-smoked-crucible-hud.md): the tooltip and the stats panel, which now cut in.
- [ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md) §3: Reduce UI Animation, which holds all of it still.
- [ADR-059](ADR-059-hud-scaling-cooldowns-statuses-and-chat-readability.md): the HUD's scales and layout, which this keeps unchanged.

## Context

After ADR-080 the HUD had the client's type and surfaces, but it still read as a prototype:
- **Nothing moved.** A cooldown ending, a level, a kill and Gold earned simply changed between one frame and the next.
- **Parts of the approved board never landed:** the round portrait with its XP ring, the brass edge on a ready tile, the coin beside Gold, and the vision tool's mark.
- **The bars over units and the combat text** were drawn in the engine's small bitmap font. Statuses over units were named by their code names.
- **The kill feed, the announcement, the level banner and the ending** were flat boxes of text.

## Decision

### 1. Three curves, as the website's

`MotionSnap`, `MotionSmear` and `MotionSettle` are cubic Bézier timing curves with the website's control points:
- **snap:** quick out of the start;
- **smear:** slow, then a rush;
- **settle:** overshoots and comes back.

`VeyraHudEase::Evaluate` reads them, so the HUD and the website move alike.

### 2. What the HUD remembers between frames

The HUD is painted fresh each frame, so motion needs memory. `FVeyraHudMotion` is a small presentation-only state, keyed by name and held by the presentation subsystem. It keeps:
- when something turned on (`OnRise`);
- how long something has been on, restarting for a new identity (`Held`);
- a bar's lost part that holds, then drains (`Trail`);
- a count that runs to its new value (`Count`).

Keys that no frame asks for are forgotten. The HUD's painter can fade any part (`Alpha`) and move it (`Offset`) without its layout knowing. `CuttingIn` rises a panel into place along the snap.

### 3. The moments

| Moment | What it does |
|---|---|
| An ability, Flux Spell or item comes ready | A white flash, a glint across the tile and a brass burst: a ring, a white-hot core and four slashes (the website's press burst). |
| It is used | A smaller burst in the accent. |
| A cooldown runs | Its sweep keeps inside the tile's cut corners, with its leading edge lit. |
| Health or the resource falls | The lost part shows in `MotionTrailColor`, holds for `MotionTrailHoldSeconds`, then drains along the smear. This applies to the deck, the selected unit's frame and every bar over a unit. |
| A level comes | The portrait's ring flashes outward and its level plate bursts. A new rank's pip glows and bursts. |
| Gold rises | The count runs up and the coin glows. A side's kill count glows as it rises. Team Flux runs to its new value. |
| The tooltip, the stats panel's more stats, and a newly selected unit's frame | They cut in. |
| A kill feed line arrives | It slides in from the right. |
| An announcement, the level, or the match's end | A banner opens from its middle, with hairlines in its colour and a brass diamond on each. Its title rises in, in the display face, and a glint crosses it. |
| Combat text appears | Each number pops in at `MotionPopScale` of its size and settles. |

### 4. The deck and the panels, as the board drew them

- **Portrait:** round, in a ring that fills with XP, with the level on a brass plate at its foot.
- **Tiles:** every ability, Flux Spell and item tile is chamfered (`TileChamfer`). Its edge shades brass from head to foot while it is ready to use, and stays dim while it is not. Its icon dims while it cools or is unlearned. Its cost sits in the corner in the resource's light.
- **Vision tool and Gold:** the vision tool has its key on a chip and a diamond mark. Gold has a coin, and the shop has its key on a chip.
- **Stats panel:** each stat has its icon (`VeyraStatGlyphs`) in its colour (`StatColors`).
- **Selected unit's frame:** its name is in the display face, beside its side's word, with its side's light along the frame's head.
- **Team Flux:** its active Flux is large, with its Fluxborn bonus in its side's colour.
- **Top strip:** kills are in the display face.
- **Minimap:** it stands in the client's surface.
- **Bars over units and combat text:** both use the client's type. Status names come from their kind's words, as ADR-080 §2 names them.
- **HUD font:** it gains the display family as `Display` and `DisplayBold`.

### 5. Values

All of these are presentation values in `[/Script/VeyraUI.VeyraGreyboxSettings]`:
- the three curves;
- `MotionPanelSeconds`, `MotionPanelRise`;
- `MotionBurstSeconds`, `MotionBurstRadius`;
- `MotionFlashSeconds`, `MotionHotColor`;
- `MotionTrailHoldSeconds`, `MotionTrailDrainSeconds`, `MotionTrailColor`;
- `MotionCountSeconds`;
- `MotionFeedSeconds`, `MotionFeedSlide`;
- `MotionBannerSeconds`, `MotionGlintSeconds`;
- `MotionPopSeconds`, `MotionPopScale`;
- `TileChamfer` and the tile colours;
- `StatColors`.

### 6. Reduce UI Animation

With the player's Reduce UI Animation on:
- every moment reads as long over;
- every trail and count shows its value;
- the kill feed, banners and combat text stand still.

Nothing flashes or pulses beyond what ADR-055 already allows.

### 7. The menus move too

The client's menus take the same motion, held by the same curves:
- **A page arriving:** when the screen changes, each of its parts fades and rises into place, each a stagger after the one before (`FVeyraShellReveal`). The shell's top bar slides in from the left with a lean instead. A new page brings in only its own parts. A rebuild of the page that shows carries a reveal under way over to the parts it makes anew, so a snapshot arriving mid-reveal never cuts it short.
- **Buttons:** a primary or secondary action takes the website's sheen, a white-hot band crossing it, as the cursor arrives. Every shell button bursts where it is pressed. The button's Slate widget is the shell's own button (`SVeyraShellButton`), so keyboard focus finds it as before.
- **Motes:** Flux motes drift up behind the shell, a lobby and the results, as on the website (`UVeyraShellMotes`).

All of these are presentation values in `[/Script/VeyraUI.VeyraShellStyleSettings]`:
- `RevealSeconds`, `RevealStagger`, `RevealRise`, `RevealSlide`, `RevealShear`;
- `SheenSeconds`, `SheenOpacity`, `PressBurstRadius`;
- `MoteArea`, `MoteMost`, `MoteRiseMin`, `MoteRiseMax`, `MoteSway`, `MoteRadiusMin`, `MoteRadiusMax`, `MoteBrassShare`, `MoteHotShare`.

Reduce UI Animation holds every one of them still.

## Consequences

- The match HUD moves the way the website does, and its parts read as the approved board drew them.
- Everything that moves runs on the client's real time and changes no gameplay. No new replication.
- The HUD draws more triangles: the chamfered tiles, the round portrait, the arcs and the light. It still uses a handful of shared generated textures (a glow, rings and a streak).
