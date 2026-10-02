# ADR-048: Player profiles

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Profiles & Identity Bible](../Design/Veyra_Profiles_Identity_Bible_v0.1.md):
  - §1: public profiles and viewing.
  - §2: the profile hero and appearance.
  - §6: the client's profile and privacy checkpoint (UX-68–75).
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) §8 (UX-71–75).
- [Account, Collection & Mastery Bible](../Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md): the account level, permanent ownership and Mastery that a profile shows.
- [ADR-017](ADR-017-match-statistics.md): Match History, which a profile shares only by the owner's choice.
- [ADR-044](ADR-044-party-and-social-client.md): blocks, which take precedence over profiles.
- [ADR-045](ADR-045-account-progression-collection-mastery.md): account levels, ownership and Mastery.
- [ADR-047](ADR-047-reports-commendation-player-menu.md): the player menu, which deferred its Profile entry to this record.

## Context

Players see each other only as names on scoreboards, in the friends list and in chat. The Profiles Bible locks a public profile:
- the display name;
- an official profile icon;
- the confirmed account level;
- one featured Vanguard the player chooses from those they permanently own, with its base art and the owner's Mastery level.

No Vanguard is featured automatically (UX-71). The bible also locks:
- **No public Mastery collection** (UX-73).
- **Match History private by default** (UX-72). Statistics and builds are shared only while the owner turns on *Show Match History on My Profile*.
- **Blocks take precedence:** mutually blocked accounts cannot use each other's profiles.
- **Official icons and backgrounds only,** with a default background always available and no uploads (UX-75). About 30 icons are free at launch, and events unlock more later.
- **Display only:** cosmetics never change play, statistics or Mastery.

## Decision

### 1. A profile domain

`Backend/internal/profile` keeps each account's appearance:
- the icon;
- the background;
- the featured Vanguard, or none;
- `showMatchHistory`, false until the owner turns it on.

It owns no level, ownership or Mastery. It reads those from progression through narrow interfaces adapted in `cmd/veyra-backend`, as the chat and conduct domains read theirs.

### 2. The catalog is configuration

- `profile.icons` and `profile.backgrounds` list the official choices, with `profile.defaultIcon` and `profile.defaultBackground`.
- Startup validates them: the defaults are listed, IDs are unique and well formed, and a `vanguard_<id>` entry names a released Vanguard.
- An account that never chose shows the defaults.

### 3. Viewing a profile

- `GET /v1/profiles/{name}` returns another player's public profile:
  - the name, icon, background and account level;
  - the featured Vanguard with its Mastery level, or `null`;
  - whether its Match History is shared.

  The player's own profile reads the same way.
- **A block in either direction answers `profile_unavailable`,** exactly as an unknown name does, so a block is never revealed.
- **Never in the answer:** account IDs, email, security settings, messages and moderation data.
- **While the owner shares:**
  - `GET /v1/profiles/{name}/matches` pages the owner's Match History records, with the same filters the owner uses (ADR-017).
  - `GET /v1/profiles/{name}/matches/{matchId}` opens one of them with its scoreboard, detailed statistics and final builds, as the owner sees it. The owner's line is the one marked.
- **Otherwise both answer `history_private`,** and `profile_unavailable` as above.
- **Neither ever carries** the owner's rewards or their own reports and commendations.

### 4. Choosing the appearance

- `GET /v1/me/profile-settings` returns the player's choices and the catalog. `PUT /v1/me/profile-settings` saves them.
- **The PUT checks** that the icon and background are in the catalog, and that the featured Vanguard is permanently owned. Rotation and bots never count (Bible §2).
- **A featured Vanguard the account no longer owns** is shown as none.
- **Turning sharing off deletes nothing.** Other participants keep their own copies of shared matches (Bible §1).

### 5. The client

- **A Profile page** joins the shell's pages. It shows the player's own profile as others see it, with:
  - pickers for the icon and background;
  - a featured-Vanguard picker over the player's permanently owned Vanguards, plus *None*;
  - the *Show Match History on My Profile* toggle.

  Nothing saves until the player presses Save.
- **Another player's profile** opens from a friend's card (*View Profile*) and from the player menu (*Profile*, ADR-047 §5). It shows their shared Match History when they share it. A blocked or unknown player's profile says only that it is unavailable.
- **Portraits:** a `vanguard_<id>` icon draws that Vanguard's portrait, cropped from its hero art as champion select crops it. A `vanguard_<id>` background draws its hero art as the results screen's showcase does. The defaults are neutral.
- **Access:** the pages follow Match History's access, so they are not available through Match Found, a committed champion select or Reconnect-only.

## 6. Consequences

- The friends card and the player menu gain a profile entry. The player menu's deferred *Profile* is closed.
- The catalog grows by configuration, so event unlocks need only a grant record when they arrive.

## 7. Provisional answers where canon is open

1. **The free icon set** is each released Vanguard's portrait plus a neutral default: 26 icons, against the bible's "approximately 30". Every icon is official art the project already owns.
2. **The backgrounds** are a neutral default plus each released Vanguard's hero art.
3. **Every account may use every catalog entry.** Unlocks wait for events (Bible §7).
4. **The profile shows the account level and the featured Vanguard's Mastery level only.** It shows no XP, currencies or other Vanguards (UX-73).
5. **A blocked profile reads as unavailable,** like a missing one, rather than saying "blocked".

## Out of scope

- Display-name changes and claims (Bible §4–§5).
- Website account management (§3).
- Event unlocks, borders and Ranked awards (§7).

## Tests

- **Go:**
  - catalog validation;
  - defaults for an account that never chose;
  - only owned Vanguards may be featured;
  - Match History private until shared, and readable again once turned back on;
  - a block in either direction answers as unavailable.

  Against both stores, with Postgres under `-Postgres`, and through each route.
- **Services:** the protocol, the profile and settings reads, saving, and shared Match History pages.
- **UI:** the Profile page's pickers and Save; another player's profile from the friends card and the player menu; unavailable and private states.
- **Smoke:** `Smoke.ps1 -Flow Profile`. One client sets an icon, a featured Vanguard and sharing. The other views them and reads the shared history, then blocks and sees the profile unavailable.
