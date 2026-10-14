# ADR-083: Settings as a shell page, and collapsible friends and party panels

**Status:** Proposed. On 2026-10-14 the author called the Settings screen "a mess" and the current friends and party UI broken. They approved direction A ("Ledger") from the Settings design board, with the friends panel on the right and the party panel along the bottom as two separate, collapsible panels that remember their state, collapse everywhere and respond to the space they have. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-14
**Related:**
- [ADR-024](ADR-024-player-settings.md): the settings registry, store and screen this rebuilds. Every setting, value, scope and behaviour stays.
- [ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md): text size, transparency, focus and Reduce UI Animation, which the new layout follows.
- [ADR-044](ADR-044-party-and-social-client.md), [ADR-061](ADR-061-presence-appear-offline-and-offline-removal.md): the party, friends and presence the panels show. Their rules and intents stay.
- [ADR-078](ADR-078-the-smoked-crucible-client-look.md), [ADR-082](ADR-082-a-hud-that-moves.md): the look and motion the new surfaces use.

## Context

- **Settings** opened as a fixed window over the shell. Each setting was a large card with its description, so about three fitted at once. Controls of one kind looked different (On and Off as two tabs, ranges as − and + buttons). Casting Mode had a card for each key. Categories had no sub-sections.
- **The party bar** ran its Ready and Find Match buttons out of the bar and under the friends rail. **The friends rail** carried party chat in its bottom. Neither could be put away.
- **Nothing in the shell responded to its width:** every surface had a fixed size.

## Decision

### 1. Settings is a page of the shell

- In the shell, Settings replaces the page body, with the top bar and the friends and party panels around it.
- Over a live match, a lobby or the results, where the shell's pages are not shown, it stays a window over the screen, built from the same parts.
- Its labels, its search, Undo, resets, key capture and conflicts, and its display confirmation are unchanged.

### 2. Groups within a category

- `Settings.json`'s layout gives each category optional `groups`, each a named run of its settings starting at the one it names (`from`). The registry checks that each starts on a setting of its category, in order, once.
- A group's name and hint are text rows, `settings_group.<id>.name` and `.hint`.
- The categories list their groups as sub-sections under the shown category.

### 3. One control per kind of value

- **A toggle** is a switch.
- **A choice** is a segmented control when its options fit, by count and length set in the shell style; otherwise a dropdown, with a colour choice's swatch beside its value.
- **A range** is a slider with its value, between its step buttons.
- **A binding** is a key cap.
- **A group of choices that share one set of options** is drawn as one grid: settings down, options across.
- A changed setting shows a mark and its Reset beside it. Every row, and a grid's heading, keeps a place of `SettingsResetWidth` for Reset whether or not it shows, so no control moves when it appears.
- Every button keeps the name tests and scripts press, whatever control it sits in.
- The page builds its categories, list and About afresh on each rebuild, keeping the list's scroll. A kept widget moved into a fresh container came back undrawn and took no click, which 0.2.9 shipped. The Settings smoke therefore clicks its categories with a real mouse, through Slate, not by pressing buttons in code.

### 4. The About panel

The setting under the pointer or keyboard focus has its description, its default and its notes in an About panel beside the list, instead of in every row.

### 5. Layout that follows its space

The Settings page measures the width it is given, not the window's:
- **Wide:** categories, list and About side by side.
- **Medium:** About becomes a strip under the list.
- **Narrow:** the categories become a row above the list.

The widths are shell style settings.

The shell's top bar reflows the same way. Where its one line does not fit, the page tabs move to a line of their own beneath it, and they return once there is room again. It never scales down, so the player's Interface Text Size holds.

### 6. The friends and party panels

- **Friends** run down the right of the shell, full height. Collapsed, they keep a strip with the online count, the online friends' initials and a way back.
- **Party** runs along the bottom of the page:
  - A heading line holds the party's size against its limit ("Party · 2/5"), its mode or its time in the queue, and who is holding it out of the queue. On the right are the privacy (the leader's to switch), Leave and the collapse control.
  - Below it are the party's places, side by side and the same width, one per member up to the backend's `party.maxSize`. A member's place shows their initial and name, with You and Leader under it. The player's own place is edged in the frame colour. Each open place has a dashed edge and leads to the friends panel to invite someone.
  - Each member carries a mark instead of words: a green tick while Ready, a red cross while not. The player's own mark is the Ready control, under its usual name (Ready or Unready).
  - The leader opens another member's card from the "…" on that member's place. Its actions (Make Party Leader, which asks first, and Remove) show under the places.
  - Find Match, or Cancel while queued, is the leader's and sits at the end of the places.

  Collapsed, it keeps a bar with the party's size, each member's initial and mark, and its queue or mode.
- The backend's party answer gains `maxSize`. A client reading an older backend without it shows no open places.
- Party Chat and messages stay with the friends panel, set apart from the friends by a rule.
- A player's name used as a button (a friend, a party member, a player on the results screen) keeps its own case, so it reads as a name and not a command.
- A note that an invitation went out disappears once that player has joined the party.
- The player's own Ready mark explains in its tooltip why it cannot be pressed yet: no mode is chosen, or the party is in the queue.
- Each panel opens and closes on its own, wherever it shows. Its state is remembered on this device, in the device's user settings, apart from the account's settings: it is layout, not a preference the player sets.
- The panels' widths, heights and the collapse animation are shell style settings.
- `Smoke.ps1 -Flow FullParty` fills a party to the backend's `party.maxSize` and captures it with both panels open and both collapsed. Because every client on one machine shares the remembered state, each smoke script first opens any panel a player left collapsed, as a player would.

## Consequences

- The shell's surfaces stop overlapping, and every setting is readable at once at 1280×720.
- Each `Settings.json` category gains a list of groups, which may be empty, and the text table gains their names.
- The device's user settings gain two remembered panel states.
- The party answer gains `maxSize`, which is optional for the client. No gameplay or account data changes.
