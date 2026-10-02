# ADR-047: Reports, commendation, the player menu and Play Again

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §6 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Moderation, Reporting & Telemetry Bible](../Design/Veyra_Moderation_Telemetry_Bible_v0.1.md):
  - §1: who may report, and what a report creates.
  - §3: no automatic sanction.
  - §7: open items.
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md):
  - UX-56: reporting.
  - UX-57: the player menu.
  - UX-58: commendation.
  - UX-62: Play Again.
- [Profiles & Identity Bible](../Design/Veyra_Profiles_Identity_Bible_v0.1.md) §4: unique display names.
- [ADR-017](ADR-017-match-statistics.md): the scoreboard and Match History. Its open items include reporting and commendation on results.
- [ADR-044](ADR-044-party-and-social-client.md): friends, party invitations and blocks.

## Context

The results screen and Match History show every participant (UX-50–53), but offer nothing to do about them. The UX Bible approves four actions:
- **Reports** (UX-56), which are private.
- **A player menu** (UX-57) with a friend request, a party invitation and a report.
- **One commendation of one teammate** (UX-58), from the immediate results only.
- **Play Again** (UX-62), which opens the party panel.

The Moderation Bible locks who may report and how reports group (§1):
- Only a match's participants report it, from the post-match list or their own Match History.
- One reporter may report several participants.
- A match's reports form one case, each kept distinct.
- Repeats must not flood the queue.

A report is an allegation, never an automatic punishment (§3). Staff review, sanctions and appeals need decisions the bible leaves open: the vendor, roles, policy and retention (§3–§7).

## Decision

### 1. A conduct domain

`Backend/internal/conduct` keeps reports and commendations: records about a match's participants. The Moderation Bible gives case state to moderation services; this domain holds the cases until one exists, and the moderation service will take them over.

### 2. Reports

- `POST /v1/me/matches/{matchId}/reports` carries the reported player's name as the results show it, a reason, optional details and a client ID.
- **The backend checks:**
  - the reporter played the match;
  - the name is another human participant, as recorded in the match;
  - the reason is one of `conduct.reasons`;
  - the details, cleaned, are at most `conduct.detailsMaxCharacters`;
  - the match ended within `conduct.reportWindow`.
- **One report per reporter, reported player and match.** A second report, or a resend with the same client ID, returns the first, so repeats never flood a case (§1).
- **The match's first report opens its case;** the case holds every report on the match.
- **The answer** says the report was received, and nothing else. The reporter never learns of other reports, the case or any outcome.
- **Nothing else follows:**
  - No sanction, block, party change or result change (§1, §3).
  - Blocking and reporting stay independent.

### 3. Commendation

- `POST /v1/me/matches/{matchId}/commendation` names one teammate: a human on the commender's side, not the commender. It must come within `conduct.commendWindow` of the match's end, which is the immediate results (UX-58).
- **One per commender and match.** A second is refused as `already_commended`.
- **Nothing reads it:** no score, reward, rank or matchmaking effect. It is recorded only.

### 4. The player's own record

- `GET /v1/me/matches/{matchId}/conduct` lists whom the player reported in the match and whom they commended, so a screen shows *Reported* and *Commended*. It never shows anyone else's.
- `GET /v1/dev/matches/{matchId}/conduct` (local only) shows the case and the commendations, for scripted runs.

### 5. The client

- **Names, not accounts:**
  - Results keep account IDs private (ADR-017), and the client names a participant as the scoreboard does.
  - Display names are unique (Profiles Bible §4), and the backend resolves a name against the match's recorded participants.
  - A party invitation still needs a friend, whose account the friends list holds.
- **The player menu** (UX-57): each other human's row on the results screen, and in a Match History record, opens a card:
  - **Add Friend**, unless already friends or a request waits;
  - **Invite to Party**, for a friend, on the results screen;
  - **Commend**, for a teammate, once, on the results screen;
  - **Report**, which opens a form of reason buttons, an optional details field, Submit and Cancel.

  After a report the card says *Report sent*, and shows nothing more.
- **Play Again** (UX-62) leaves the results as Continue does and opens Play with the party panel. It never readies, queues, creates a party or changes the mode.
- **Allowed states:** the results screen allows friend requests and party invitations (UX-57). The backend decides as ever.

## 6. Provisional answers where canon is open

1. **Reasons** `abusive_chat`, `afk`, `griefing`, `cheating`, `offensive_name` and `other` (§7 leaves the taxonomy open).
2. **One report per reporter, reported player and match;** a repeat returns the first (§1: repeats must not flood).
3. **Reports within 14 days** of the match's end (§7: timing limits are open). Commendation within 10 minutes, the post-match chat's window.
4. **Custom and practice matches may be reported.** Their participants may report (§7); how evidence differs is for staff tooling.
5. **Details at most 500 characters**, cleaned like chat.
6. **Profile** joins the menu with player profiles.

## Out of scope

- Staff review, the moderator interface, sanctions, appeals and evidence retention (§2–§5): the moderation service, vendor and policy are undecided.
- Reports from a shared replay, which may not report (§1).
- Chat evidence: the Chat Bible §6 and this bible §2 keep it restricted, and nothing here stores it.

## Tests

- **Go:**
  - reports from participants only, about another human only;
  - the reason, details and window;
  - one report per reported player, a resend returning the first, and every report in one case;
  - commendation teammate-only, once, within its window;
  - the player's own record.

  Against both stores, with the Postgres store under `-Postgres`, and through each route.
- **Services:** the protocol, the intents, the conduct read on opening results or a record, and friend and party intents on the results screen.
- **UI:** the player menu's offers per row and screen, the report form, Commend once, and Play Again.
- **Smoke:** `-Flow Party` commends a teammate and files a test report, which the development route shows.
