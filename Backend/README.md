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

Four modules: **identity** (the launcher → game login handoff), **social** (friends, friend requests, blocks), **party** (parties, invites, Ready, mode and the Find Match queue lock) and **match** (match servers, join tickets and results, [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md)). Matchmaking itself, match-found acceptance and live presence come next; until then a development-only route creates matches directly.

### Identity

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
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
| `GET /v1/modes` | — | modes and whether each is enabled |
| `GET /v1/party` | — | your party, or `{"party": null}` |
| `PUT /v1/party/mode` | `{"mode"}` | leader picks a mode; creates a one-person party if you have none |
| `PUT /v1/party/privacy` | `{"privacy": "public"\|"private"}` | leader only |
| `PUT /v1/party/ready` | `{"ready": true}` | mark yourself Ready or not |
| `PUT /v1/party/leader` | `{"accountId"}` | leader hands over leadership (not while queued) |
| `POST /v1/party/leave` · `DELETE /v1/party/members/{accountId}` | — | leave, or leader removes someone |
| `POST` / `DELETE /v1/party/queue` | — | leader presses Find Match / cancels |
| `GET /v1/party/invites` · `POST /v1/party/invites` | `{"accountId"}` | your invites; invite a friend (creates a party if you have none) |
| `POST /v1/party/invites/{inviteId}/accept` · `/decline` | — | answer an invite |
| `POST /v1/parties/{partyId}/join` | — | join a friend's Public party |

Errors come back as `{"error": "<code>"}` with codes such as `not_leader`, `party_full`, `party_locked`, `not_all_ready`, `blocked` and `not_friends`.

Rules the code enforces, from the Parties & Social Bible:

- Parties hold one to `party.maxSize` players (config refuses more than five); capacity is checked when an invite is **accepted**, not when it's sent, and an invite never reserves a slot.
- Any member can invite a friend. Only the leader picks the mode, privacy, removes members, transfers leadership and starts or cancels the queue.
- Adding a member or changing the mode resets everyone's Ready. Find Match needs a mode, everyone Ready, and a party no bigger than the mode's team.
- Find Match locks the party: nobody can join, accept an invite into it, send an invite from it, change Ready or mode, or take over as leader. Anyone leaving, being removed or blocked out cancels the queue for everyone and resets Ready.
- Accepting an invite while in another party moves you, unless your current party is queued.
- Blocks work in both directions: no friend requests, invites or shared party. Blocking ends the friendship and withdraws pending requests and every invite that would put the two players in one party, whoever sent it. The block and its party clean-up commit in one transaction.
- Every change to a party runs in a database transaction with the party row locked, and each account can be in only one party (enforced by the database).

**Provisional rules — the bibles leave these open.** Each is isolated in one place in the code and marked `PROVISIONAL`, so a ruling is a small change:

1. **New leader when the leader leaves:** the longest-standing remaining member (Parties Bible §1).
2. **Who may join a Public party:** a friend of *any* current member (§1 says "friends").
3. **Blocking someone in your own party:** the blocked player is removed, with no penalty.
4. **Leader cancels the queue:** everyone's Ready resets, the same as other cancellations.
5. **Invite lifetime** `2m` and **default privacy** `private` are provisional values in `config/local.json`.

### Matches

How a client joins its assigned match is [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/dev/matches` | — | `{"mode", "participants": [{"accountId", "side": "A"\|"B"}]}` | `201` and the match; the backend starts its server. **Local only**; the route does not exist unless `matches.devCreate.enabled` |
| `GET /v1/dev/matches/{matchId}` | — | — | the match, its server port and its result. **Local only**; never returns a secret |
| `GET /v1/me/match` | `Bearer <game token>` | — | `{"match": null}`, or the player's match: `id`, `state`, `side`, and once it is ready, `server` (`host`, `port`) and the join `ticket` |
| `POST /v1/server/matches/{matchId}/ready` | `Bearer <server credential>` | `{}` | the server accepts players |
| `POST /v1/server/matches/{matchId}/result` | `Bearer <server credential>` | `{"endReason", "winner", "durationSeconds", "participants": [{"accountId", "joined", "connectedAtEnd"}]}` | the result is recorded |

Error codes include `already_in_match`, `invalid_roster`, `no_server_capacity`, `allocation_failed`, `invalid_state`, `invalid_result` and `result_conflict`.

Rules the code enforces:

- A match moves from `allocating` to `ready` to `ended`, or to `failed` from either earlier state. An account has at most one active match (enforced by the database).
- Each side holds at most the mode's `humanPlayersPerTeam`, every account at most once.
- A join ticket (`vjt_`) is derived from a random key the match keeps, so asking again gives the same ticket; the key is erased when the match ends or fails, which kills every ticket for it. The match server receives only SHA-256 hashes of the tickets.
- The match server gets its roster and its credential (`vms_`, stored only as a hash) on its standard input, never in an environment variable, a file or its command line.
- A server credential works only for its own match. A result can be reported again unchanged; a different one is refused. It must list exactly the roster.
- A match whose server does not report ready within `matches.readyTimeout`, runs past `matches.maxDuration`, or whose server stops without a result is failed. A finished match's server is removed after `matches.removeServerAfter`, and its port is reused only after that.

## For the Unreal client

The game receives its launch code on **standard input**, one line, never on the command line. On startup it should read that line, then call `POST /v1/game-sessions` with the code and its own build version, and keep the returned game session token in memory. In development, `veyra-devlaunch` starts the game the same way:

```sh
go run ./cmd/veyra-devlaunch -backend http://localhost:8080 -account DevOne -build dev -- "C:\path\to\Veyra.exe"
```

## Configuration

Everything tunable lives in [`config/local.json`](config/local.json): session and launch-code lifetimes, HTTP timeouts, the request size limit, the seeded dev accounts (`DevOne` … `DevTen`), party size, invite lifetime and default privacy, the mode list (Ranked is present but disabled, per the Modes & Access Bible), match lifetimes and the allocator (the Docker endpoint, the match-server image and network, the host ports players connect to and the server's arguments). The file is validated at startup; a missing or unknown field stops the backend with an error instead of falling back to a default. Dev login is refused unless `environment` is `local`. The database URL comes from the `VEYRA_DATABASE_URL` environment variable, never from the file.

## Layout

```text
Backend/
├── cmd/veyra-backend/     the service
├── cmd/veyra-devlaunch/   launcher stand-in for development
├── config/                validated config files
└── internal/
    ├── config/            config loading and validation
    ├── identity/          accounts, sessions, launch codes (domain rules)
    ├── social/            friends, friend requests, blocks
    ├── party/             parties, invites, Ready, queue lock
    ├── match/             matches, join tickets, results, the allocator interface
    ├── secret/            bearer secrets and their hashes
    ├── postgres/          Postgres storage and embedded migrations
    └── httpapi/           HTTP/JSON transport
```

Domain packages (`identity`, `social`, `party`, `match`, and later matchmaking) own their rules and depend only on storage interfaces. `party` reads the social graph through a small interface and never writes it; a block is applied by `social` first and then handed to `party`. `postgres` implements storage; `httpapi` only translates HTTP to domain calls.

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
