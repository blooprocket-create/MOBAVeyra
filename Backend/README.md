# Veyra backend

The Go backend from [ADR-005](../Docs/ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md): one service with separate internal modules, Postgres as the database of record, run locally in Docker. **Local development only for now** — dev login is enabled and nothing here is hardened for public exposure.

## Run it

From the repository root, with Docker Desktop running:

```sh
docker compose up --build
```

- Backend: <http://localhost:8080> (`/healthz`, `/readyz`)
- Postgres: `localhost:5432`, user `veyra`, database `veyra` (throwaway local password in `compose.yaml`)

Both bind to `127.0.0.1` only. `docker compose down -v` stops everything and wipes the database.

Check the whole login handoff without the game (needs Go installed):

```sh
cd Backend
go run ./cmd/veyra-devlaunch -backend http://localhost:8080 -account DevOne -build dev -check
```

## What exists so far

Eight modules: **identity** (the launcher → game login handoff), **social** (friends, friend requests, blocks), **party** (parties, invites, Ready, mode and the queue lock), **catalog** (released Vanguards, starters and the rotation), **account** (onboarding and Vanguard entitlements), **matchmaking** (the matchmaker and Match Found), **selection** (champion select: solo Custom practice and Casual Select, [ADR-010](../Docs/ADR/ADR-010-play-flow.md)) and **match** (match servers, join tickets and results, [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md)). A player reaches a match through practice, or through the queue: Find Match, Match Found, Casual Select. Champion select creates the match. Live presence beyond champion select comes later; a development-only route also creates matches directly, for scripts.

To play through all of it, `Game/Scripts/Play.ps1` starts this stack and opens the [launcher](../Launcher/README.md).

### Identity

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/register` | — | `{"providerToken", "displayName"}` | `201`, launcher session token + account. Creates the Veyra account for a Firebase ID token ([ADR-038](../Docs/ADR/ADR-038-player-accounts-with-firebase-authentication.md)) |
| `POST /v1/login` | — | `{"providerToken"}` | launcher session token + account; `404 not_registered` when the Firebase user has no Veyra account yet |
| `POST /v1/dev/login` | — | `{"accountName"}` | launcher session token. **Local only**; the route does not exist unless dev login is enabled |
| `POST /v1/launch-codes` | `Bearer <launcher token>` | `{"buildVersion"}` | single-use launch code |
| `POST /v1/game-sessions` | — | `{"launchCode", "buildVersion"}` | game session token + account |
| `GET /v1/me` | `Bearer <game token>` | — | account |

Rules the code enforces:

- Tokens and codes are 256-bit random values with recognisable prefixes (`vls_` launcher session, `vgs_` game session, `vlc_` launch code). Only SHA-256 hashes are stored.
- A launch code is single-use, expires after `launchCodes.lifetime` (capped at one minute by ADR-005), and is bound to the account and build version. A wrong build version still uses the code up.
- Redeeming a code and creating the game session happen in one database transaction: if issuing the session fails, the code stays unused.
- Dev login only works for accounts created by dev seeding. If a seeded name belongs to an ordinary account, the backend refuses to start.
- Launcher and game sessions are not interchangeable.
- Every auth failure returns the same `401 invalid_credentials`.
- Players register and sign in with Firebase Authentication, in the launcher (ADR-038). The backend verifies the Firebase ID token itself (Google's published keys, the project ID as audience and issuer, expiry), links the Firebase user ID to a Veyra account in `identity.provider_links`, and from then on issues only Veyra's own sessions. Firebase never reaches the game. With `playerLogin.provider` set to `none`, `/v1/login` and `/v1/register` answer `404 not_found`.
- A display name is 3–16 ASCII letters, digits and underscores, and unique regardless of letter case (provisional, ADR-038 §4). Taking a name and linking the Firebase user happen in one transaction: `409 display_name_taken` or `409 already_registered` leave nothing behind.

### Social and party

Every route below needs `Authorization: Bearer <game session token>`. Accounts are addressed by ID; `GET /v1/accounts?displayName=X` finds one by name.

| Endpoint | Body | What it does |
|---|---|---|
| `GET /v1/friends` | — | friends, incoming and outgoing requests |
| `POST /v1/friends/requests` | `{"accountId"}` | send a request; if they already asked you, you become friends |
| `POST /v1/friends/requests/{accountId}/accept` · `/decline` | — | answer a request |
| `DELETE /v1/friends/requests/{accountId}` | — | withdraw your request |
| `DELETE /v1/friends/{accountId}` | — | unfriend |
| `GET /v1/blocks` · `PUT` / `DELETE /v1/blocks/{accountId}` | — | list, block, unblock |
| `GET /v1/modes` | — | modes: whether each is enabled, its Play-page `category` (`ranked`, `casual` or `ai`; ADR-039 §6), its `humanPlayersPerTeam`, and its `matchmaking`, `casualSelect`, `coop` (humans against an enemy AI team) or `notImplemented` (not yet available) |
| `GET /v1/party` | — | your party, or `{"party": null}`; its `status` is `idle`, `queued`, `found` (Match Found) or `selecting`, `queuedSeconds` how long it has been in matchmaking, and each member's `restrictedSeconds` how long they cannot queue after leaving a select (0 when free) |
| `GET /v1/me/restriction` | — | `restrictedSeconds`: how long you cannot queue after leaving a matchmade champion select, 0 when free (ADR-060) |
| `PUT /v1/party/mode` | `{"mode"}` | leader picks a mode; creates a one-person party if you have none |
| `PUT /v1/party/privacy` | `{"privacy": "public"\|"private"}` | leader only |
| `PUT /v1/party/ready` | `{"ready": true}` | mark yourself Ready or not |
| `PUT /v1/party/leader` | `{"accountId"}` | leader hands over leadership (not while queued) |
| `POST /v1/party/leave` · `DELETE /v1/party/members/{accountId}` | — | leave, or leader removes someone |
| `POST` / `DELETE /v1/party/queue` | — | leader presses Find Match / cancels |
| `GET /v1/party/invites` · `POST /v1/party/invites` | `{"accountId"}` | your invites; invite a friend (creates a party if you have none) |
| `POST /v1/party/invites/{inviteId}/accept` · `/decline` | — | answer an invite |
| `POST /v1/parties/{partyId}/join` | — | join a friend's Public party |

Errors come back as `{"error": "<code>"}` with codes such as `not_leader`, `party_full`, `party_locked`, `not_all_ready`, `mode_not_available`, `blocked`, `not_friends`, `queue_restricted` (a member left a matchmade champion select and cannot queue yet) and `invitee_in_match` (no party invitation reaches a player in a live match).

Rules the code enforces, from the Parties & Social Bible:

- Parties hold one to `party.maxSize` players (config refuses more than five); capacity is checked when an invite is **accepted**, not when it's sent, and an invite never reserves a slot.
- Any member can invite a friend. Only the leader picks the mode, privacy, removes members, transfers leadership and starts or cancels the queue.
- Adding a member or changing the mode resets everyone's Ready. Find Match needs a mode, everyone Ready, and a party no bigger than the mode's team.
- Find Match locks the party: nobody can join, accept an invite into it, send an invite from it, change Ready or mode, or take over as leader, until matchmaking lets it go. Anyone leaving, being removed or blocked out takes the party out of matchmaking and resets Ready; a match found or champion select it was in is abandoned or cancelled. Only a mode whose `matchmaking` is `casualSelect` or `coop` can be queued.
- Accepting an invite while in another party moves you, unless your current party is queued.
- Leaving a matchmade champion select on purpose (Casual or Draft) restricts the leaver from queueing for `dodges.restriction`; a dodge while restricted starts it again (ADR-060). A select cancelled by a disconnect restricts no one, nor do custom and practice selects or a declined or missed Match Found. Find Match refuses any party holding a restricted member; the others may leave and queue without them. The restriction is no moderation sanction.
- No party invitation reaches a player in a live match, nor is one kept for later; friend requests still go through.
- Blocks work in both directions: no friend requests, invites or shared party. Blocking ends the friendship and withdraws pending requests and every invite that would put the two players in one party, whoever sent it. The block and its party clean-up commit in one transaction.
- Every change to a party runs in a database transaction with the party row locked, and each account can be in only one party (enforced by the database).

**Provisional rules — the bibles leave these open.** Each is isolated in one place in the code and marked `PROVISIONAL`, so a ruling is a small change:

1. **New leader when the leader leaves:** the longest-standing remaining member (Parties Bible §1).
2. **Who may join a Public party:** a friend of *any* current member (§1 says "friends").
3. **Blocking someone in your own party:** the blocked player is removed, with no penalty.
4. **Leader cancels the queue:** everyone's Ready resets, the same as other cancellations.
5. **Invite lifetime** `2m` and **default privacy** `private` are provisional values in `config/local.json`.

### Custom lobbies

An invite-only lobby a host arranges, humans and bots on either side, before launching a custom match ([ADR-021](../Docs/ADR/ADR-021-custom-lobbies.md); Custom Matches Bible §1–§4). Every route needs `Authorization: Bearer <game session token>`.

| Endpoint | Body | What it does |
|---|---|---|
| `GET /v1/lobby` | — | your lobby, or `{"lobby": null}` |
| `POST /v1/lobby` | — | open a lobby you host, seated first on side A |
| `POST /v1/lobby/leave` | — | leave; the longest-present human becomes host, and the last one out closes it |
| `DELETE /v1/lobby/members/{accountId}` | — | host removes a human |
| `PUT /v1/lobby/members/{accountId}/seat` | `{"side", "index"}` | host moves a human to an empty seat |
| `PUT` / `DELETE /v1/lobby/seats/{side}/{index}/bot` | `{"vanguardId", "difficulty"}` | host places, changes or removes a bot: any released Vanguard, `beginner` or `intermediate` |
| `PUT /v1/lobby/settings` | `{"victoryEnabled", "startingGold"}` | host sets the session's rules; `startingGold` is `null` for the game's own |
| `POST /v1/lobby/launch` | — | host starts: every human enters a custom champion select, found at `GET /v1/me/select`, and the lobby is `selecting` until it ends |
| `GET /v1/lobby/invites` · `POST /v1/lobby/invites` | `{"accountId"}` | your lobby invites; host invites a friend |
| `POST /v1/lobby/invites/{inviteId}/accept` · `/decline` | — | answer an invite |

A lobby is `id`, `hostAccountId`, `status` (`open`, or `selecting` while its champion select runs), `playersPerSide`, `settings` (`victoryEnabled`, `startingGold`), `startingGoldRange` (`min`, `max`) and `seats`: side A's, then side B's, each `side`, `index`, `kind` (`empty`, `human` or `bot`), and `accountId`, `displayName` and `host` for a human, `vanguardId` and `difficulty` for a bot.

Rules the code enforces:

- Only the host invites, moves and removes humans, places bots, sets the rules and launches. Invites go to friends no member blocks or is blocked by, and expire after `customLobby.inviteLifetime`.
- An accepted invite seats the newcomer on the side with fewer humans, side A on a tie, at its first empty seat, or on the other side when that one is full.
- A side holds `customLobby.playersPerSide` seats and each Vanguard once, bots included; the same Vanguard may play on both sides.
- Victory needs a Vanguard on each side. Until the host chooses, it is on exactly when both sides have one. Starting Gold lies within `customLobby.startingGold`.
- One lobby per account; a lobby's members are busy for parties and matchmaking, and someone in a match, a select or a queued party can neither open nor join one. A block removes the blocked player and withdraws invites between the two, like a party's.
- Launching needs the host and at least one human, each finished with the tutorial and in no match, select or queue. The select is blind: the humans pick what they own or the rotation offers, each Vanguard once per side, the bots' included, while the same Vanguard may play on both sides. Leaving it, or its timer lapsing unfilled, returns everyone to the lobby; its match starting closes the lobby. The match is `custom`: the host may end it (`host_ended`), and it can be won (`prime_well_destroyed`, `surrender`) only with victory on.
- Errors: `not_host`, `lobby_full`, `lobby_locked`, `seat_taken`, `no_such_seat`, `not_a_bot`, `vanguard_taken`, `invalid_vanguard`, `invalid_difficulty`, `starting_gold_out_of_range`, `victory_needs_both_sides`, `already_in_lobby`, `member_busy`, `not_friends`, `blocked`, `invite_not_found`, `no_human`, `tutorial_required`.

### Account settings

The player's account-level settings, one opaque document per account ([ADR-024](../Docs/ADR/ADR-024-player-settings.md) §1; Settings & Accessibility §7). Device settings never come here. Both routes need `Authorization: Bearer <game session token>`.

| Endpoint | Body | What it does |
|---|---|---|
| `GET /v1/account/settings` | — | `{"schemaVersion": 1, "revision", "values"}`: revision `0` and no values before the first save |
| `PUT /v1/account/settings` | `{"schemaVersion": 1, "revision", "values"}` | saves `values` over the revision the client based them on, answering the new document |

`values` maps setting IDs (content IDs) to text; the backend never interprets them. A save whose revision is not the stored one answers `409 {"error": "settings_conflict", "current": <document>}`, so the client can ask the player which to keep. Other errors: `bad_settings` (another `schemaVersion`, or a key that is not a content ID) and `settings_too_large` (over `settings.maxDocumentBytes`).

### Matchmaking and Match Found

The matchmaker forms matches from queued parties every `matchmaking.interval`; every player must then accept before champion select opens ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §10; Parties & Social Bible §2–3, §6).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/match-found` | `Bearer <game token>` | — | `{"matchFound": null}` or the match waiting for the player's answer |
| `POST /v1/me/match-found/accept` | `Bearer <game token>` | — | the match found; the last acceptance opens champion select first, so the answer is `accepted` with its `selectId` |
| `POST /v1/me/match-found/decline` | `Bearer <game token>` | — | the match found, `abandoned` |

A match found is `id`, `mode`, `state` (`pending`, `accepted`, `abandoned`), `deadline`, `remainingSeconds`, `accepted` and `total` (counts; nobody learns who declined), `you` (`pending`, `accepted`, `declined`), `selectId` and `abandonReason` (`declined`, `timed_out`, `party_changed`, `select_failed`). Errors: `match_found_not_found`, `already_answered`, `expired`.

Rules the code enforces:

- Each side holds exactly the mode's `humanPlayersPerTeam`, built from whole parties, oldest queued first. Two accounts where either blocks the other are never in one match, on either team, even at the cost of a longer wait.
- Two matchmaker passes never take one party: a pass locks the queued parties it reads and skips those another holds (`FOR UPDATE SKIP LOCKED`), and a player is in at most one pending match found (enforced by the database).
- Everyone must accept within `matchFound.acceptDuration`. A decline, or the timer ending, abandons the match: a party with a player who declined, or had not accepted in time, leaves the queue Not Ready; the others return to it in their places. There is no decline penalty (§3).
- When everyone accepts, a Casual Select opens with each player on their side.

**Provisional** (ADR-010 §11): the grouping rule (oldest first, parties never split) where canon leaves the algorithm open; accepters keeping their queue time; `matchFound.acceptDuration` `15s`; The committed configuration holds canon's five a side. `Smoke.ps1` and `Play.ps1 -Opponent` size their mode to their two clients in a generated `config/scripted.json`, which `compose.yaml` runs through `VEYRA_BACKEND_CONFIG` (ADR-039 §6).

### Onboarding and Vanguards

Which Vanguards a player may pick, and the stubbed first-time tutorial ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §6).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/profile` | `Bearer <game token>` | — | `account` (`id`, `displayName`) and `tutorial` (`completed`, `starterVanguardId`) |
| `GET /v1/me/vanguards` | `Bearer <game token>` | — | `owned`, `rotation`, `available` (owned and rotation, released only) and `starters` |
| `POST /v1/me/starter` | `Bearer <game token>` | `{"vanguardId"}` | the profile; the starter is owned from now on. Once only: `already_completed`; only a starter: `not_a_starter` |
| `GET /v1/dev/accounts` | — | — | `accounts` (`displayName`), the seeded dev accounts for the launcher's picker. **Local only**, with dev login |
| `POST /v1/dev/accounts/{name}/reset-onboarding` | — | — | `204`; the dev account is back before its starter choice, for repeatable test runs. **Local only**, with dev login |

The catalog is `vanguards` in `config/local.json`: the released Vanguards, which must equal the Playable ones in `Game/Tuning/Vanguards.json` (`internal/catalog`'s contract test checks it), the starters (3 to 5, Account, Collection & Mastery Bible §1) and the rotation. The weekly free rotation offers `rotation.slots` (12, canon) distinct released Vanguards a week, from `rotation.epoch` in weeks of `rotation.weekSeconds`, drawn by a shuffle seeded with `rotation.seed` and the week. Last week's sit out unless too few others remain, and a Vanguard in `rotation.releases` waits a week after its release ([ADR-039](../Docs/ADR/ADR-039-weekly-rotation-and-co-op-vs-ai.md) §1). **Provisional:** weeks begin Monday 00:00 UTC.

### Progression, the Collection and purchases

Account level and XP, the account currencies, buying Vanguards and Vanguard Mastery ([ADR-045](../Docs/ADR/ADR-045-account-progression-collection-mastery.md)). **Flux** and **Refined Flux** here are persistent account currencies, never in-match Team Flux. Every grant comes from a verified match result, in the transaction that stores it, once per match and account (`internal/progression`); the client never mints anything.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/progression` | `Bearer <game token>` | — | `progression`: `level`, `levelXp`, `levelNeed` (what this level takes), `lifetimeXp`, `flux`, `refinedFlux` |
| `GET /v1/me/collection` | `Bearer <game token>` | — | `vanguards`: every released Vanguard in catalog order, whatever the player owns: `vanguardId`, `owned`, `source` (`starter`, `purchase` or `null`), `rotation`, `price` (`flux`, `refinedFlux`), `purchasable`, and the player's `mastery` (`level`, `levelPoints`, `levelNeed`, `lifetimePoints`, `emoteTier`) |
| `POST /v1/me/purchases` | `Bearer <game token>` | `{"purchaseId", "vanguardId", "currency": "flux" \| "refinedFlux"}` | `purchase` (`purchaseId`, `vanguardId`, `currency`, `price`) and the new `progression`. The client generates `purchaseId` (8–64 letters, digits and hyphens), so a retry returns the same purchase without spending again. Refusals: `insufficient_balance`, `already_owned`, `not_for_sale`, `invalid_purchase`, and `purchase_conflict` for an ID already used otherwise |
| `POST /v1/dev/accounts/{name}/progression-grant` | — | `{"flux", "refinedFlux"}` | the dev account's `progression` after adding the currency, recorded as a development adjustment. **Local only**, with dev login and `progression.devGrant` |
| `POST /v1/dev/accounts/{name}/progression-reset` | — | — | `204`; the Vanguards the dev account bought are taken back and its purchases forgotten, so scripted runs can buy again; balances stay. **Local only**, as above |

`GET /v1/me/matches/{matchId}` carries `rewards` once its result is adjudicated (`null` before): `reason` (`null` when earned; otherwise `custom`, `not_matchmade`, `no_contest`, `not_completed`, `not_joined`, `personal_loss` or `coop_level`), `accountXp`, `levelBefore`, `levelAfter`, `flux`, `refinedFlux`, `vanguardId`, `masteryPoints`, `masteryBefore`, `masteryAfter`.

The tuning is `progression` in `config/local.json`, all of it provisional (ADR-045 §10): account XP per minute and win bonus, the Co-op account-XP level, the account and Mastery level curves, Flux per level-up, Refined Flux milestones, the Mastery weights and cap, the emote tiers, and a price in both currencies for exactly the released Vanguards.

### Chat

Party Chat, friend direct messages, champion-select team chat and post-match chat ([ADR-046](../Docs/ADR/ADR-046-party-direct-select-postmatch-chat.md)); in-match Team and All Chat stay on the match server. Who reads a conversation is checked when a message is sent and again each time it is delivered (`internal/chat`): a party's current members, from when each joined; two friends with no block; one side of an active champion select; and the participants of an ended match who opted into its post-match chat by sending. Delivery drops every message across a block.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/chat?after=<seq>` | `Bearer <game token>` | — | `messages` the player may read now, oldest first (`seq`, `kind`, `conversation`, `sender` (`id`, `displayName`), `recipientId` for a direct message, `text`, `sentAt`, `clientId`), `next` (the cursor for the next poll) and `more` (the page was full). Without `after`, the newest `chat.historyMessages`, so a restarted client recovers its conversations |
| `POST /v1/me/chat/party/{partyId}` | `Bearer <game token>` | `{"clientId", "text"}` | `message` to the party the player wrote in. Refusals: `not_in_party`, `conversation_changed` (the player is in another party now) |
| `POST /v1/me/chat/direct/{accountId}` | `Bearer <game token>` | `{"clientId", "text"}` | `message`. Refusals: `not_friends`, `blocked` |
| `POST /v1/me/chat/select/{selectId}` | `Bearer <game token>` | `{"clientId", "text"}` | `message` to the player's side in the select they wrote in. Refusals: `no_select`, `conversation_changed` |
| `POST /v1/me/chat/matches/{matchId}` | `Bearer <game token>` | `{"clientId", "text"}` | `message`; the first opts the player in, and it reads only what follows. Refusals: `not_participant`, `postmatch_closed` (left, moved on to a select or match even if it is over now, or `chat.postMatchWindow` after the end), `all_chat_off` |
| `DELETE /v1/me/chat/matches/{matchId}` | `Bearer <game token>` | — | `204`; the player leaves the match's post-match chat for good |
| `PUT` / `DELETE /v1/me/chat/matches/{matchId}/mutes/{accountId}` | `Bearer <game token>` | — | `204`; mutes or unmutes a participant for the player only |

Every send's text is cleaned (control characters become spaces, ends trimmed) and refused as `empty_message` or `message_too_long`; more than `chat.maxPerWindow` in `chat.window` is `rate_limited`. The client generates `clientId` (8–64 letters, digits and hyphens), so a resend after a lost answer returns the first message; reusing one for another kind is `client_id_conflict`. Expired messages are pruned every `chat.pruneInterval`, whether or not anyone sends. The tuning is `chat` in `config/local.json`, all of it provisional (ADR-046 §9).

### Reports and commendation

A match's participants report other humans in it, and commend one teammate ([ADR-047](../Docs/ADR/ADR-047-reports-commendation-player-menu.md); `internal/conduct`). Players are named as the results show them; the backend resolves the name among the match's recorded participants, so results never expose account IDs. A match's reports group into one case. A report is an allegation and changes nothing else; nothing reads a commendation.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/me/matches/{matchId}/reports` | `Bearer <game token>` | `{"reportedName", "reason", "details", "clientId"}` | `report` (`reportedName`, `status`: `received`), whatever the case holds. One per reporter, reported player and match: a repeat, or a resend with the same `clientId`, returns the first. Refusals: `not_participant`, `unknown_player`, `invalid_reason`, `details_too_long`, `report_closed` (more than `conduct.reportWindow` after the end, or no result), `invalid_report` |
| `POST /v1/me/matches/{matchId}/commendation` | `Bearer <game token>` | `{"name"}` | `commendation` (`name`): one teammate, once. Refusals: `not_teammate`, `unknown_player`, `already_commended`, `commend_closed` (more than `conduct.commendWindow` after the end) |
| `GET /v1/me/matches/{matchId}/conduct` | `Bearer <game token>` | — | `conduct`: `reported` (the names the player reported), `commended` (or `null`), `players` (the match's other humans: `name`, `teammate`), and the `reasons` and `detailsMaxCharacters` a report form offers |
| `GET /v1/dev/matches/{matchId}/conduct` | — | — | the match's `case` (`open`, `reports`) and `commendations`. **Local only**, with `matches.devCreate` |

The tuning is `conduct` in `config/local.json`, all of it provisional (ADR-047 §6): the reasons, the details' length and the two windows.

### Display names

A player may change their display name ([ADR-049](../Docs/ADR/ADR-049-display-name-changes-and-claims.md); `internal/identity`). Names stay unique ignoring case.
- The first voluntary change is free. Later ones cost Flux or Refined Flux, charged through progression in the same unit of work as the change.
- A cooldown applies between voluntary changes, paid or not.
- The old name is anyone's as soon as the change commits.
- A name whose holder has not logged into the launcher for `names.claimAfter` is claimed by whoever changes to it. The holder gets a unique placeholder and must choose a new name, for free, before they can queue, practise, or join or make a party or lobby.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/display-name` | `Bearer <game token>` | — | `displayName` (`name`, `freeChangeAvailable`, `nextChangeAt` or `null`, `renameRequired`, `price` {`flux`, `refinedFlux`}) |
| `PUT /v1/me/display-name` | `Bearer <game token>` | `{"name", "currency"}` (`flux` or `refinedFlux`, after the free change) | the same, after the change. Refusals: `invalid_display_name`, `same_display_name`, `display_name_taken`, `rename_cooldown`, `invalid_currency`, `insufficient_balance` |

`POST /v1/dev/accounts/{name}/name-reset` with `{"current"}` (local only, with `devLogin`) gives a development account its name back, whatever it changed it to, with its name changes forgotten.

`GET /v1/me/profile` also reports `renameRequired`. Requests refused with `rename_required` until the player chooses: `PUT /v1/party/mode`, `POST /v1/party/queue`, party invitations and joins, `POST /v1/practice`, and making or joining a lobby. The tuning is `names` in `config/local.json`: the 24-hour cooldown is canon, the one-year claim threshold is the bible's working value, and the prices are provisional.

### Player profiles

Each account has a public profile ([ADR-048](../Docs/ADR/ADR-048-player-profiles.md); `internal/profile`). It shows the display name, an official icon and background, the account level, and one permanently owned Vanguard the player chose to feature, with its Mastery level. Match History is private until the owner shares it. A block in either direction makes a profile read as unknown. Profiles never carry account IDs.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/profiles/{name}` | `Bearer <game token>` | — | `profile` (`name`, `icon`, `background`, `level`, `featured` {`vanguardId`, `masteryLevel`} or `null`, `sharesMatchHistory`). Refusal: `profile_unavailable` (an unknown name, or a block either way) |
| `GET /v1/profiles/{name}/matches` | `Bearer <game token>` | — | the owner's Match History page, as `GET /v1/me/matches`, while they share it. Refusals: `history_private`, `profile_unavailable` |
| `GET /v1/profiles/{name}/matches/{matchId}` | `Bearer <game token>` | — | one of those matches as the owner sees it, without the owner's rewards. Refusals as above, and `match_not_found` |
| `GET /v1/me/profile-settings` | `Bearer <game token>` | — | `settings` (`icon`, `background`, `featuredVanguardId` or `null`, `showMatchHistory`) and the `catalog`, with `featuredChoices`: the Vanguards the player permanently owns |
| `PUT /v1/me/profile-settings` | `Bearer <game token>` | `settings`' fields | the saved `settings` and the `catalog`. Refusals: `invalid_icon`, `invalid_background`, `not_owned` (only a permanently owned Vanguard may be featured, never one lent by the rotation) |

`POST /v1/dev/accounts/{name}/profile-reset` (local only, with `devLogin`) forgets a development account's choices, so scripted runs start from the defaults with the history private.

The catalog is `profile` in `config/local.json` (ADR-048 §2, provisional): a neutral default, plus each released Vanguard's portrait as an icon and its hero art as a background.

### Favorite Vanguards

A player marks Vanguards as favorites in the Collection, and champion select shows them under its Favorites tab ([ADR-058](../Docs/ADR/ADR-058-search-filters-and-favorites.md) §5; `internal/favorites`). Favorites are account data, so they follow the player to any machine. Any released Vanguard may be one, owned or not.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/favorites` | `Bearer <game token>` | — | `favorites`: Vanguard IDs in the order marked |
| `PUT /v1/me/favorites/{vanguardId}` | `Bearer <game token>` | — | `favorites` after marking it; marking a favorite again changes nothing. Refusals: `unknown_vanguard` (not released), `playing` (never during champion select or a match), `favorites_full` |
| `DELETE /v1/me/favorites/{vanguardId}` | `Bearer <game token>` | — | `favorites` after unmarking it. Refusal: `playing` |

The most an account keeps is `favorites.maxPerAccount` in `config/local.json`. Canon sets no limit, so the provisional value allows the whole released roster.

### Custom practice and champion select

Solo Custom practice opens a champion select with no lobby; an accepted match found opens a Casual Select. The select creates the match ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §7–8, §10).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/practice` | `Bearer <game token>` | — | `201` and the `select`: the player alone on `customPractice.hostSide`. Refused as `tutorial_required` before the starter choice, and `busy` with a match, a select or a queued party |
| `GET /v1/me/select` | `Bearer <game token>` | — | `{"select": null}` or the player's active select. Polling it while picking is the player's presence |
| `GET /v1/me/selects/{selectId}` | `Bearer <game token>` | — | a select the player was in, active or over: how it ended |
| `PUT /v1/me/select/hover` | `Bearer <game token>` | `{"vanguardId"}` | the select |
| `POST /v1/me/select/lock` | `Bearer <game token>` | `{"vanguardId"}` | the select; once every pick is locked it creates the match first, so the answer is `started` with its `matchId`, or `cancelled` |
| `PUT /v1/me/select/spells` | `Bearer <game token>` | `{"fluxSpells": ["blink", ""]}` | the select, with the player's own seat's `fluxSpells`: two starting Flux Spells in slot order, `""` for an empty slot, each on `fluxSpells.roster`, none twice (ADR-015 §5). Free while the select is picking, before or after lock-in; `invalid_flux_spells` otherwise |
| `POST /v1/me/select/leave` | `Bearer <game token>` | — | the select, `cancelled` as `left`: a dodge. Casual Select only; practice is `cannot_leave` |

A select is `id`, `kind`, `mode`, `state` (`picking`, `starting`, `started`, `cancelled`), `deadline`, `remainingSeconds` (by the server's clock), `pickSeconds` (the timer's full length, for a countdown bar), `seats` (`displayName`, `side`, `you`, `locked`, and `hover` for the player's own team only), `matchId` and `cancelReason` (`timed_out`, `allocation_failed`, `starting_timed_out`, `left`, `presence_lost`). Its `kind` is `practice`, `casual` or `custom`; a custom select also lists its `bots` (`side`, `vanguardId`, `difficulty`), in each side's seat order, which the players see as seats already locked. Errors: `not_available` (a Vanguard the player may not pick), `taken` (another player locked it), `already_locked`, `expired`, `invalid_state`, `cannot_leave`, `select_not_found`.

Rules the code enforces: a player is in at most one active select (enforced by the database); a lock is permanent; a select creates at most one match (a unique `select_id` on the match). When the pick timer (`customPractice.pickDuration`) ends, a seat's hover is locked for it, and a seat with nothing to lock cancels the select. **Provisional** (ADR-010 §11), like the 30-second pick time. A select whose match creation never finishes is settled after `selection.startingTimeout`, which must exceed the allocator's request timeout.

Casual Select (Battleground Bible §15) adds: everyone picks at once within `casualSelect.pickDuration`; a locked Vanguard is taken for everyone, on both teams, while a hover reserves nothing; a player whose client stops polling for `casualSelect.presenceTimeout` cancels it (`presence_lost`, a disconnect); leaving cancels it (`left`, a dodge, recorded as `leftBy` with no penalty, since Match Flow §2 sets no schedule). When it starts its match, every party is let go, Not Ready (UX-15); when it is cancelled, the parties of the players who left, disconnected or never locked leave the queue Not Ready, and the others return to it. **Provisional:** `60s` to pick and `10s` of presence; select trades wait for their protocol.

Co-op vs AI ([ADR-039](../Docs/ADR/ADR-039-weekly-rotation-and-co-op-vs-ai.md) §2–§4) matches one side of a `coop` mode's `humanPlayersPerTeam` humans, never friendly AI, against its `aiPerTeam` bots of its `aiDifficulty`. Only the humans accept. Its select is a Casual Select that opens with the enemy team seated: distinct Vanguards drawn at random from this week's rotation (from every released Vanguard while the rotation offers too few). A human may pick what an enemy bot plays, the sole cross-team mirror. The match keeps Standard rules and carries its bots; no other Standard match may. **Provisional:** one human locally, as Casual's, so one player can play against five bots; canon is five.

### Matches

How a client joins its assigned match is [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md); how matches carry rules and Vanguards is [ADR-010](../Docs/ADR/ADR-010-play-flow.md) §7–9.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/dev/matches` | — | `{"mode", "rules": "standard"\|"practice", "map": "development"\|"play", "hostAccountId", "participants": [{"accountId", "side": "A"\|"B", "vanguardId"}]}`; `rules` defaults to standard, `map` to the development grey box, and only practice names a host | `201` and the match; the backend starts its server. **Local only**; the route does not exist unless `matches.devCreate.enabled`. It stands in for champion select in scripts, so any Vanguard the game defines is accepted |
| `GET /v1/dev/matches/{matchId}` | — | — | the match, its rules, Vanguards, server port and result. **Local only**; never returns a secret |
| `GET /v1/me/match` | `Bearer <game token>` | — | `{"match": null}`, or the player's match: `id`, `mode`, `rules`, `state`, `side`, `vanguardId`, and once it is ready, `server` (`host`, `port`) and the join `ticket` |
| `GET /v1/me/matches` | `Bearer <game token>` | — | Match History ([ADR-017](../Docs/ADR/ADR-017-match-statistics.md) §6): the player's completed matches newest first, `matches` (`id`, `mode`, `rules`, `endedAt`, `durationSeconds`, `side`, `vanguardId`, `outcome`: `win`, `loss` or `no_contest`, and `personalLoss`, which makes the outcome a loss whatever the team's), `next`, the cursor of the next page or `null`, and `modes`: every mode the player has a completed match in, sorted, whatever the filter, for the mode filter's choices. The query's `vanguard`, `mode` and `outcome` filter every record, and `cursor` continues from a page's `next`; `matches.historyPageSize` sets a page's size. Errors: `invalid_filter`, `invalid_cursor` |
| `GET /v1/me/matches/{matchId}` | `Bearer <game token>` | — | a match the player was in: `id`, `mode`, `rules`, `state`, `side`, `vanguardId`, `failureReason`, and once it has ended the verified `result` (`endReason`, `winner`, `durationSeconds`, the player's own `joined`, `connectedAtEnd` and `personalLoss`, `players`: the scoreboard, or `null` when the server sent none, and `wells`: each Flux Well secured, or `null`). Each scoreboard line is `side`, `name`, `vanguardId`, `you`, `statistics`, `items` and `fluxSpells`, with no account IDs. `rewards` is what the match gave the player, `null` until adjudicated (see Progression). Anyone else's match is `match_not_found` |
| `POST /v1/server/matches/{matchId}/ready` | `Bearer <server credential>` | `{}` | the server accepts players |
| `POST /v1/server/matches/{matchId}/result` | `Bearer <server credential>` | `{"endReason", "winner", "durationSeconds", "participants": [{"accountId", "joined", "connectedAtEnd", "personalLoss", "absentSeconds"}], "players": [{"side", "name", "accountId", "vanguardId", "statistics", "items", "fluxSpells"}], "wells": [{"site", "side", "atSeconds"}]}`; `players` and `wells` are optional, and a bot's `accountId` is `null` | the result is recorded |

Error codes include `already_in_match`, `invalid_roster`, `invalid_rules`, `invalid_map`, `invalid_vanguard`, `no_server_capacity`, `allocation_failed`, `invalid_state`, `invalid_result` and `result_conflict`.

Rules the code enforces:

- A match moves from `allocating` to `ready` to `ended`, or to `failed` from either earlier state. An account has at most one active match (enforced by the database).
- Flux Spells: `fluxSpells.roster` must equal `Game/Tuning/Abilities.json`'s roster (`internal/config`'s contract test checks it). Until a player chooses spells, their seat's follow the saved loadout of the Vanguard it hovers or locked: the spells that account last took into a match with that Vanguard (`match.participants.flux_spells`, migration 0013). The match's assignment carries each participant's two spells (version 4).
- A standard match's sides each hold at most the mode's `humanPlayersPerTeam`, every account at most once. A practice match is its host alone, on `customPractice.hostSide`, in the mode `customPractice.mode`, which no party can queue for, and the bots `customPractice.bots` lists: AI participants the server adds, each a side and a released Vanguard, stored with the match and sent in its assignment. No side may hold more than `customPractice.playersPerSide`, the host included; bots are no accounts, so they get no ticket and no result. Neither that nor any mode's `humanPlayersPerTeam` may exceed the match server's own cap, `teams.maxTeamSize` in `Game/Tuning/Match.json`, or the server refuses the assignment; `internal/config`'s contract test checks it.
- Every participant plays a Vanguard, a content ID; the match server refuses one `Game/Tuning/Vanguards.json` does not define.
- Only a practice match can end `host_ended`: its host ended it (Custom Matches Bible §4).
- Only a standard match can end `prime_well_destroyed`, when one side destroyed the other's Prime Well (ADR-011 §13), `surrender`, which the other side wins, or `remake`, which nobody wins (ADR-019 §5). The first two have a `winner`, and every other end must leave it `null`. Practice has no victory condition and takes no votes. The database keeps the reason and the winner together too.
- A participant's `personalLoss` is a loss its own absence earned (Match Flow Bible §6), and needs `joined`; its `absentSeconds`, AFK and disconnected together, may not exceed the match's duration.
- A join ticket (`vjt_`) is derived from a random key the match keeps, so asking again gives the same ticket; the key is erased when the match ends or fails, which kills every ticket for it. The match server receives only SHA-256 hashes of the tickets.
- The match server gets its roster and its credential (`vms_`, stored only as a hash) on its standard input, never in an environment variable, a file or its command line.
- A server credential works only for its own match. A result can be reported again unchanged; a different one is refused. It must list exactly the roster.
- A result's scoreboard ([ADR-017](../Docs/ADR/ADR-017-match-statistics.md) §5) is every player's statistics and final equipment, humans and bots, kept as a document on `match.results.players` (migration 0014). It is optional, for older servers. Each line must fit the match:
  - on side A or B, with a name and a Vanguard content ID;
  - a rostered account at most once, on its side with its Vanguard, or one of the match's bots;
  - every statistic a finite amount or count, never negative;
  - every item and Flux Spell slot empty (`""`) or a content ID, with two spell slots.

  The Flux Wells secured, each a site, a side and a match-clock time within the match, are kept on `match.results.wells` (migration 0015); the results' team summary counts each capture once.

  `Game/Source/VeyraDeveloper/TestData/MatchResult.example.json` is the game's result body, which a Go test posts, so the two agree.
- A match whose server does not report ready within `matches.readyTimeout`, runs past `matches.maxDuration`, or whose server stops without a result is failed. A finished match's server is removed after `matches.removeServerAfter`, and its port is reused only after that.

**Match servers.** Locally, the backend starts each match's server as a Docker container (`internal/docker`), named `veyra-match-<match id>`, from the `veyra-match-server:local` image that `docker compose --profile match-server build match-server` builds from the packaged Linux server. The server's command line starts with its map (ADR-011 §12): `matches.maps.play`, the battleground, for every match players make through champion select, and `matches.maps.development`, the one-lane grey box, for development matches unless they ask for `"map": "play"`; `allocator.docker.serverArgs` holds the rest and may not name a map. It publishes the server on `127.0.0.1` at a port from `allocator.docker.hostPorts` and joins the compose network, where the server reaches the backend as `http://backend:8080`. To do this the backend container mounts the Docker socket and runs as root, which gives it control of the Docker host: acceptable on a developer machine only.

## For the Unreal client

The game receives its launch code on **standard input**, one line, never on the command line, through the **launch handshake** ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §5; the lines are fixed by `Game/Source/VeyraServices/Contracts/LaunchHandshake.json`). Started with `-VeyraLaunchCode=stdin` and pipes for its standard input and output, the game's `VeyraServices` module:

1. writes `veyra-handoff/1 awaiting-launch-code` on standard output; only then does the launcher request a code and write it, so the code's 20-second life starts once the game can read it;
2. reads that line and calls `POST /v1/game-sessions` with the code and its build version (`ProjectVersion` in `Game/Config/DefaultGame.ini`), keeping the game session token in memory;
3. answers `veyra-handoff/1 signed-in`, or `veyra-handoff/1 failed <code>` if it could not sign in, and the launcher's work is done;
4. finds where the player is, in this order (ADR-010 §2): a live match from `GET /v1/me/match` leads to **Reconnect-only**, which outranks everything; then a select in progress from `GET /v1/me/select` resumes; then a player without a starter (`GET /v1/me/profile`) chooses one; otherwise the shell;
5. in the shell, reads `GET /v1/modes` and polls `GET /v1/party`. The player either starts practice (`POST /v1/practice`), or chooses a matchmade mode, readies up and queues (`PUT /v1/party/mode`, `PUT /v1/party/ready`, `POST` and `DELETE /v1/party/queue`);
6. when the party is found, polls `GET /v1/me/match-found` until every player has answered (`POST /v1/me/match-found/accept` or `/decline`). If it goes ahead, champion select follows; otherwise the player is back in the shell, queued again or not;
7. in champion select, polls the select, hovers and locks (a matchmade select may be left, `POST /v1/me/select/leave`), polls `GET /v1/me/match` until the match is ready, and joins the server with its ticket;
8. when the match ends, travels back to the front end and polls `GET /v1/me/matches/{id}` for the verified result.

A failure never quits the game: it shows with Retry where retrying can help. The client-state coordinator, `FVeyraClientFlow`, logs its progress as `VeyraClientFlow:` lines. The game does not use `-log`, which on Windows can replace the standard handles.

A match server started by the backend gets `-VeyraAssignment=stdin` and reads its assignment the same way. The game's copy of the assignment's shape is `Game/Source/VeyraServices/Schemas/MatchAssignment.schema.json`. `internal/match/contract_test.go` keeps an example the game's tests read, so a change to the assignment must update both.

The [launcher](../Launcher/README.md) is the real caller: it signs in with `POST /v1/dev/login`, starts the packaged game and hands it a code this way. In development, `veyra-devlaunch` starts the game the same way, speaking the handshake, with the game's build version. It copies the game's other output and waits for the game to exit; `-detach` returns once the game has signed in, as the launcher does:

```sh
go run ./cmd/veyra-devlaunch -backend http://localhost:8080 -account DevOne -build 0.1.0 -- "C:\path\to\VeyraClient.exe" -VeyraLaunchCode=stdin
```

A match created with `POST /v1/dev/matches` before the game starts waits behind Reconnect, as any live match does.

`Game/Scripts/Smoke.ps1 -Handoff` plays a whole match this way, from dev login to the recorded result, through the same handshake (`Start-VeyraHandshakeClient` and `Step-VeyraHandshake` in `Game/Scripts/VeyraProject.psm1`); its clients press Reconnect with `-VeyraSmokeFlow=join`. `Smoke.ps1 -Flow Practice` plays the whole solo path instead: starter choice, practice, champion select, the match, End Custom Match and the verified result. `Smoke.ps1 -Flow Casual` plays a matchmade 1v1 with two games at once: queue, Match Found, Casual Select, the match and the verified result. `-Flow CasualDecline` checks that a declined Match Found puts the decliner out of the queue and the other player back in it. With `-Launcher Cli` the launcher's headless twin signs in and starts each game.

## Configuration

Everything tunable lives in [`config/local.json`](config/local.json): session and launch-code lifetimes, HTTP timeouts, the request size limit, the seeded dev accounts (`DevOne` … `DevTen`), player login (`playerLogin`: the identity provider, `firebase` or `none`, and for Firebase the project ID, the URL of Google's token-signing keys, the timeout for fetching them and the clock skew allowed on a token's times), party size, invite lifetime and default privacy, the mode list (Ranked is present but disabled, per the Modes & Access Bible), solo Custom practice (`customPractice`: whether it is on, the mode ID its matches record, the host's side, the pick time, how many Vanguards a side may hold, and the practice bots, provisionally four enemies in the bots' seat order, Mid, Top, Jungle and Bottom: Oriel, Qazharr, Gorraveth and Bryn), custom lobbies (`customLobby`: whether they are on, the mode ID their matches record, the seats a side holds, the pick and invite times, and the range a host may set starting Gold in), the matchmaker (`matchmaking`: how often it runs and its search limit, and each mode's `matchmaking`), Match Found (`matchFound`), Casual Select (`casualSelect`), champion select's upkeep (`selection`), match lifetimes and the allocator (the Docker endpoint, the match-server image and network, the host ports players connect to and the server's arguments). The file is validated at startup; a missing or unknown field stops the backend with an error instead of falling back to a default. Dev login is refused unless `environment` is `local`. [`config/public.json`](config/public.json) is the configuration for public tests from the host PC (ADR-057): environment `public`, so no development routes, Firebase sign-in, and match servers on every address; `Game/Scripts/Host.ps1` writes `config/hosted.json` from it with the PC's public address as `publicHost`. The database URL comes from the `VEYRA_DATABASE_URL` environment variable, never from the file.

## Layout

```text
Backend/
├── cmd/veyra-backend/     the service
├── cmd/veyra-devlaunch/   launcher stand-in for development
├── config/                validated config files
└── internal/
    ├── config/            config loading and validation
    ├── identity/          accounts, sessions, launch codes (domain rules)
    ├── firebaseauth/      Firebase ID token verification (the identity provider)
    ├── social/            friends, friend requests, blocks
    ├── party/             parties, invites, Ready, queue lock
    ├── lobby/             custom lobbies: seats, bots, the session's rules, invites
    ├── catalog/           released Vanguards, starters and the rotation (configuration)
    ├── account/           onboarding (the stubbed tutorial) and Vanguard entitlements
    ├── matchmaking/       the matchmaker and Match Found; moves parties through the queue
    ├── selection/         champion select and Custom practice; creates each select's match once
    ├── match/             matches, join tickets, results, the allocator interface
    ├── docker/            the local allocator: one Docker container per match
    ├── secret/            bearer secrets and their hashes
    ├── postgres/          Postgres storage and embedded migrations
    └── httpapi/           HTTP/JSON transport
```

Domain packages (`identity`, `social`, `party`, `lobby`, `account`, `matchmaking`, `selection`, `match`) own their rules and depend only on storage interfaces and on small interfaces to each other; `account` reads the catalog through one. `matchmaking` moves parties through the queue through `party`'s methods and never writes party state itself; it and `selection` each reach the other through an interface, joined in `cmd/veyra-backend`, and their changes to parties run in the same database transaction as their own. `party` reads the social graph through a small interface and never writes it; a block is applied by `social` first and then handed to `party`. `postgres` implements storage; `httpapi` only translates HTTP to domain calls.

## Tests

On Windows, `Check.ps1` runs gofmt, go vet and the tests in the official Go image, so no Go install is needed. `-Postgres` also runs the Postgres integration tests against a separate `veyra_test` database in the compose Postgres:

```powershell
./Backend/Check.ps1
./Backend/Check.ps1 -Postgres
```

With Go installed:

```sh
cd Backend
go test ./...
```

Postgres integration tests run when `VEYRA_TEST_DATABASE_URL` points at a disposable database. They wipe the identity and match tables, so give them their own database, not the `veyra` one the backend uses:

```sh
docker compose exec postgres createdb -U veyra veyra_test
VEYRA_TEST_DATABASE_URL=postgres://veyra:veyra-local-only@localhost:5432/veyra_test?sslmode=disable go test ./internal/postgres/
```

CI runs everything against its own Postgres.

The Docker allocator's tests use a fake Engine API. One more test runs it against the real Docker Engine when `VEYRA_TEST_DOCKER_ENDPOINT` and `VEYRA_TEST_DOCKER_IMAGE` are set; the image needs an entrypoint that runs the command it is given, and `sh` and `cat` (`postgres:16` qualifies). It checks that a container receives its assignment on standard input and that `docker inspect` never shows the credential:

```powershell
docker run --rm -v "${PWD}:/src" -v /var/run/docker.sock:/var/run/docker.sock -e VEYRA_TEST_DOCKER_ENDPOINT=unix:///var/run/docker.sock -e VEYRA_TEST_DOCKER_IMAGE=postgres:16 -w /src/Backend golang:1.25 go test -run TestStartAgainstDockerEngine ./internal/docker
```
