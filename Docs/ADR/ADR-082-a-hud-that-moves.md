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

### 8. The scoreboard and the shop

**Scoreboard:** it stacks the teams the way the results scoreboard does (ADR-078 §5): the viewer's team first, then the enemy's.
- Each team has its heading in its side's colour, and the column heads stand over the first team.
- Each row shows its side's bar, then the Vanguard's face with its level on a plate, then the Vanguard over its player. Its K/D/A, creep score and bounty come next, the bounty in brass while the player has one.
- The build shows as item art: each slot's name is in that slot's tooltip, and the whole build is in the row's tooltip.
- The viewer's own row stands raised, edged in the accent.

**Shop:** its parts rise into place as it opens, as a page of the menus does.
- The selected item's pane leads with its icon and name, and its whole cost beside a coin. Each stat shows as its amount in brass beside its name (`VeyraShopModel::StatParts`), then come its effect and the purchase button. "Builds from" and "Builds into" follow at the foot.
- Tiles are edged in brass while their item can be bought, and in the accent when chosen.
- Gold has its coin (`VeyraShellBrushes::Coin`).
- The panel is nearly opaque (`ShopPanelOpacity`), so the match's banners and numbers never read through it.

### 9. Collection, Profile and Settings

**Collection:** each Vanguard's card shows its art whole, standing back to `CollectionLockedShade` while locked.
- A chip at its head says owned (in the accent), free rotation (in brass) or locked.
- Its name sits at the foot over a shade, with a brass mark for a favorite, its Mastery in brass once it has some, and its Flux price while it can be bought.
- Cards are `CollectionCardWidth` by `CollectionCardHeight`, seven to a row beside the friends rail.

**Profile:** the profile card is a banner.
- The chosen background's art (the featured Vanguard's when none is chosen) is shaded from the left.
- The icon is round in a brass ring, with the Level in brass over the name in the display face.
- The featured Vanguard's art is edged in brass.
- The display name takes one row: the name now and what a change costs, the field (`ProfileNameFieldWidth`) and the change.
- Each choice's heading is set as the client's sections are.

**Settings:**
- The window's title is in the display face, and the shown category's name heads its rows.
- The window rises in as it opens, and a new category's or search's rows arrive one after another.

Every label, text and behaviour is as before.

### 10. Region panoramas for the profile banner

At the author's request (2026-10-13), the profile banner is always a region's panorama, never a Vanguard's illustration.
- **Art:** nine panoramas from `ConceptArt/ProfileBackgrounds` (Veyra's own region art). `Game/Scripts/BuildBackgroundArt.ps1` imports them through the art commandlet's `-Kind=Backgrounds` into `BackgroundArtFolder`.
- **Catalog:** the backend's background entries are `region_<id>`, such as `region_iron_march`. Each is named by its `profile_background` row.
- **Banner:** it shows the chosen region whole at its own proportions (`ProfileBackgroundAspect`), scaled to cover the banner and cut at its edges, never stretched. The default background, or one no longer offered, shows `ProfileDefaultRegion`.
- **Picker:** each tile shows the part of its panorama around `ProfileBackgroundFocus`, where the landmarks stand.
- **Release:** `Backend/config/local.json` and `public.json`, which the hosted backend reads, both list the regions, from 0.2.8, the first client able to draw them; a test keeps the two catalogs alike. A saved Vanguard background no longer offered reads as the default (the backend's `Settings`), so it saves again cleanly.

### 11. Match History, the starter choice, champion select's roster and the match menu

- **Match History:** each saved match is a card that opens it, still named Open. A bar down its edge shows its outcome's colour, with the player's Vanguard, the outcome over the Vanguard's name, the mode over when it ended, and its length (`HistoryOutcomeBarWidth`). The row's one-line summary is now its tooltip. The filters wrap within the page, however many Vanguards there are.
- **A saved record:** its headline is in the display face, coloured by that match's own outcome, with its facts in a line under it, as the results screen shows them.
- **Starter choice:** centred, with larger cards (`StarterCardWidth`, `StarterCardHeight`), since the player's first decision is the Vanguards' art.
- **Champion select:** every roster tile is its portrait's width. A long name wraps under the portrait instead of widening its tile, and the portraits stay level.
- **Match menu:** its confirm questions are in the display face, as its title is.
- **Eudora:** the client shows her given name only, at the author's request (2026-10-13). Her full name stays in the roster's canon.

Every label and behaviour is as before.

## Consequences

- The match HUD moves the way the website does, and its parts read as the approved board drew them.
- Everything that moves runs on the client's real time and changes no gameplay. No new replication.
- The HUD draws more triangles: the chamfered tiles, the round portrait, the arcs and the light. It still uses a handful of shared generated textures (a glow, rings and a streak).
