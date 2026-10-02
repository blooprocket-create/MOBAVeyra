# ADR-049: Display-name changes, claims and forced renames

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Profiles & Identity Bible](../Design/Veyra_Profiles_Identity_Bible_v0.1.md):
  - §4: globally unique display names and voluntary changes.
  - §5: claiming names from inactive accounts.
  - §7: the open name rules.
- [Account, Collection & Mastery Bible](../Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md): Flux and Refined Flux, which pay for a change.
- [ADR-045](ADR-045-account-progression-collection-mastery.md): the currencies and how a spend is recorded.
- [ADR-048](ADR-048-player-profiles.md): the Profile page, where a player changes their name.

## Context

Display names are globally unique: identity checks their format and enforces uniqueness case-insensitively. A name is chosen only at account creation, and nothing changes it afterwards. The Profiles Bible locks how names change and how they pass on.

**Changing a name:**
- One free voluntary change after creation. Later changes cost Flux or Refined Flux, at prices it leaves to data.
- A 24-hour wait between voluntary changes, which payment never bypasses.
- The relinquished name is claimable by anyone as soon as the change commits.
- A change never moves the account's ID, purchases, currencies, Mastery, history, blocks, reports or moderation record.

**Claiming a name:**
- A name whose holder has not logged into the launcher for at least a year may be claimed. Match play is not needed to count as active, and website visits do not count. The year is a working threshold, never hardcoded.
- The name is never stripped automatically at the one-year mark.
- The old holder keeps everything. When they return, they choose a new name for free, which does not consume their free voluntary change.
- Every check is atomic and authoritative.

## Decision

### 1. Identity owns names

Accounts record four things:
- the last successful launcher login (dev, player or new account);
- whether the free voluntary change is used;
- when the last voluntary change was made;
- whether a rename is required.

Nothing else keys on the name: friends, parties, chat, conduct and profiles name players by account ID or by what a match recorded.

### 2. One way to change a name

`PUT /v1/me/display-name` carries `{name, currency}`. In one unit of work it:
1. checks the name's format;
2. checks the cooldown, unless a rename is required;
3. finds who holds the name;
4. charges the price through progression, unless the change is free;
5. renames the account.

If another account holds the name, the change is refused as `display_name_taken`, unless that account's last launcher login is older than `names.claimAfter`. In that case the name is claimed: the old holder gets a unique placeholder name and is marked rename-required, all in the same unit of work.

- There is no availability-search route. A failed attempt reveals only that a name is taken, and a successful one claims it.
- A rename to exactly the player's current name is refused as `same_display_name`. A change of case alone is a change.

### 3. Price and cooldown

- **The first voluntary change is free.** After that, `names.renamePrice` in the chosen currency.
- **A spend is recorded with its reason,** as purchases are (ADR-045).
- **`names.renameCooldown` (24 h)** applies to every voluntary change, the free one included.
- **A required rename** is free and exempt from the cooldown, and it does not use the free change.

### 4. A required rename blocks play until done

- `GET /v1/me/profile` reports `renameRequired`.
- The client shows a **Choose Your Name** screen after sign-in and before anything else.
- Until a name is chosen, the backend refuses the actions that put the placeholder in front of others: queueing, joining or making a party or lobby, and practice. It answers `rename_required`.

### 5. Recorded names stay as recorded

A match's scoreboard, its reports and its chat lines keep the name they recorded. Live lists (friends, party members, profiles) read the current name by account.

### 6. The client

- **The Profile page gains a Display Name section** showing:
  - the current name;
  - the cost of the next change (free, or both prices) and the time left in the cooldown;
  - a field and a Change Name action for each currency the change may be paid with.
- **A confirmation names the price** and says the old name becomes available to anyone at once.
- **After a change** the client shows the new name everywhere it names the player.

### 7. Tests

- **Go:**
  - the free first change;
  - the cooldown, and that payment never bypasses it;
  - the price in each currency, and an insufficient balance changing nothing;
  - case-insensitive uniqueness, and the old name free at once;
  - a claim only after the configured inactivity, the old holder's placeholder, and their free required rename;
  - the account ID unchanged;
  - two concurrent changes to one name, of which exactly one wins.

  Against both stores, with Postgres under `-Postgres`, and through the route.
- **Services:** the protocol, the change and its refusals, and the Choose Your Name state.
- **UI:** the Display Name section's cost, cooldown and confirmation, and the Choose Your Name screen.
- **Smoke:** `Smoke.ps1 -Flow Rename` uses a development account no other flow uses. It renames the account and restores its name through a local-only development route.

## 8. Provisional answers where canon is open

1. **The price** is 6000 Flux or 600 Refined Flux, about twice a Vanguard's.
2. **A required rename** is free, exempt from the cooldown, and leaves the free voluntary change unused (canon for a claim's victim). It does not start a cooldown.
3. **The name rules stay as at creation:** 3–16 letters, digits or underscores, unique ignoring case. Profanity, impersonation, Unicode and confusables are the later pass the bible calls for.
4. **The claim threshold** is `names.claimAfter` = 8760 h, one year (the bible's working threshold).
5. **There is no availability search,** so another account's inactivity is never revealed by asking.
6. **A placeholder** is `Player_` plus eight random characters, unique like any name.
7. **No public name history** (Bible §7: not approved).

## Out of scope

- Account creation on the website (§3), already covered by the launcher's registration.
- Security re-verification for identity changes.
- A cooldown after forced renames, beyond §8.2.
