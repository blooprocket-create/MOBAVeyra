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

Seven modules: **identity** (the launcher → game login handoff), **social** (friends, friend requests, blocks), **party** (parties, invites, Ready, mode and the Find Match queue lock), **catalog** (released Vanguards, starters and the rotation), **account** (onboarding and Vanguard entitlements), **selection** (champion select and solo Custom practice, [ADR-010](../Docs/ADR/ADR-010-play-flow.md)) and **match** (match servers, join tickets and results, [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md)). A player reaches a match through practice: champion select creates it. Matchmaking itself, match-found acceptance and live presence come next; a development-only route also creates matches directly, for scripts.

To play through all of it, `Game/Scripts/Play.ps1` starts this stack and opens the [launcher](../Launcher/README.md).

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

### Onboarding and Vanguards

Which Vanguards a player may pick, and the stubbed first-time tutorial ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §6).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `GET /v1/me/profile` | `Bearer <game token>` | — | `account` (`id`, `displayName`) and `tutorial` (`completed`, `starterVanguardId`) |
| `GET /v1/me/vanguards` | `Bearer <game token>` | — | `owned`, `rotation`, `available` (owned and rotation, released only) and `starters` |
| `POST /v1/me/starter` | `Bearer <game token>` | `{"vanguardId"}` | the profile; the starter is owned from now on. Once only: `already_completed`; only a starter: `not_a_starter` |
| `GET /v1/dev/accounts` | — | — | `accounts` (`displayName`), the seeded dev accounts for the launcher's picker. **Local only**, with dev login |
| `POST /v1/dev/accounts/{name}/reset-onboarding` | — | — | `204`; the dev account is back before its starter choice, for repeatable test runs. **Local only**, with dev login |

The catalog is `vanguards` in `config/local.json`: the released Vanguards, which must equal the Playable ones in `Game/Tuning/Vanguards.json` (`internal/catalog`'s contract test checks it), the starters (3 to 5, Account, Collection & Mastery Bible §1) and the rotation. **Provisional:** every released Vanguard is a starter, and until the weekly rotation exists a stand-in rotation offers every released Vanguard while fewer than `rotation.slots` (12, canon) are released.

### Custom practice and champion select

Solo Custom practice opens a champion select with no lobby; the select creates the match ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §7–8).

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/practice` | `Bearer <game token>` | — | `201` and the `select`: the player alone on `customPractice.hostSide`. Refused as `tutorial_required` before the starter choice, and `busy` with a match, a select or a queued party |
| `GET /v1/me/select` | `Bearer <game token>` | — | `{"select": null}` or the player's active select |
| `GET /v1/me/selects/{selectId}` | `Bearer <game token>` | — | a select the player was in, active or over: how it ended |
| `PUT /v1/me/select/hover` | `Bearer <game token>` | `{"vanguardId"}` | the select |
| `POST /v1/me/select/lock` | `Bearer <game token>` | `{"vanguardId"}` | the select; once every pick is locked it creates the match first, so the answer is `started` with its `matchId`, or `cancelled` |

A select is `id`, `kind`, `mode`, `state` (`picking`, `starting`, `started`, `cancelled`), `deadline`, `remainingSeconds` (by the server's clock), `seats` (`displayName`, `side`, `you`, `locked`, and `hover` for the player's own team only), `matchId` and `cancelReason` (`timed_out`, `allocation_failed`, `starting_timed_out`). Errors: `not_available` (a Vanguard the player may not pick), `already_locked`, `expired`, `invalid_state`, `select_not_found`.

Rules the code enforces: a player is in at most one active select (enforced by the database); a lock is permanent; a select creates at most one match (a unique `select_id` on the match). When the pick timer (`customPractice.pickDuration`) ends, a seat's hover is locked for it, and a seat with nothing to lock cancels the select. **Provisional** (ADR-010 §11), like the 30-second pick time. A select whose match creation never finishes is settled after `selection.startingTimeout`, which must exceed the allocator's request timeout.

### Matches

How a client joins its assigned match is [ADR-007](../Docs/ADR/ADR-007-match-join-contract.md); how matches carry rules and Vanguards is [ADR-010](../Docs/ADR/ADR-010-play-flow.md) §7–9.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/dev/matches` | — | `{"mode", "rules": "standard"\|"practice", "hostAccountId", "participants": [{"accountId", "side": "A"\|"B", "vanguardId"}]}`; `rules` defaults to standard, and only practice names a host | `201` and the match; the backend starts its server. **Local only**; the route does not exist unless `matches.devCreate.enabled`. It stands in for champion select in scripts, so any Vanguard the game defines is accepted |
| `GET /v1/dev/matches/{matchId}` | — | — | the match, its rules, Vanguards, server port and result. **Local only**; never returns a secret |
| `GET /v1/me/match` | `Bearer <game token>` | — | `{"match": null}`, or the player's match: `id`, `mode`, `rules`, `state`, `side`, `vanguardId`, and once it is ready, `server` (`host`, `port`) and the join `ticket` |
| `GET /v1/me/matches/{matchId}` | `Bearer <game token>` | — | a match the player was in: `id`, `mode`, `rules`, `state`, `side`, `vanguardId`, `failureReason`, and once it has ended the verified `result` (`endReason`, `winner`, `durationSeconds`, and the player's own `joined` and `connectedAtEnd`). Anyone else's match is `match_not_found` |
| `POST /v1/server/matches/{matchId}/ready` | `Bearer <server credential>` | `{}` | the server accepts players |
| `POST /v1/server/matches/{matchId}/result` | `Bearer <server credential>` | `{"endReason", "winner", "durationSeconds", "participants": [{"accountId", "joined", "connectedAtEnd"}]}` | the result is recorded |

Error codes include `already_in_match`, `invalid_roster`, `invalid_rules`, `invalid_vanguard`, `no_server_capacity`, `allocation_failed`, `invalid_state`, `invalid_result` and `result_conflict`.

Rules the code enforces:

- A match moves from `allocating` to `ready` to `ended`, or to `failed` from either earlier state. An account has at most one active match (enforced by the database).
- A standard match's sides each hold at most the mode's `humanPlayersPerTeam`, every account at most once. A practice match is its host alone, on `customPractice.hostSide`, in the mode `customPractice.mode`, which no party can queue for.
- Every participant plays a Vanguard, a content ID; the match server refuses one `Game/Tuning/Vanguards.json` does not define.
- Only a practice match can end `host_ended`: its host ended it (Custom Matches Bible §4).
- A join ticket (`vjt_`) is derived from a random key the match keeps, so asking again gives the same ticket; the key is erased when the match ends or fails, which kills every ticket for it. The match server receives only SHA-256 hashes of the tickets.
- The match server gets its roster and its credential (`vms_`, stored only as a hash) on its standard input, never in an environment variable, a file or its command line.
- A server credential works only for its own match. A result can be reported again unchanged; a different one is refused. It must list exactly the roster.
- A match whose server does not report ready within `matches.readyTimeout`, runs past `matches.maxDuration`, or whose server stops without a result is failed. A finished match's server is removed after `matches.removeServerAfter`, and its port is reused only after that.

**Match servers.** Locally, the backend starts each match's server as a Docker container (`internal/docker`), named `veyra-match-<match id>`, from the `veyra-match-server:local` image that `docker compose --profile match-server build match-server` builds from the packaged Linux server. It publishes the server on `127.0.0.1` at a port from `allocator.docker.hostPorts` and joins the compose network, where the server reaches the backend as `http://backend:8080`. To do this the backend container mounts the Docker socket and runs as root, which gives it control of the Docker host: acceptable on a developer machine only.

## For the Unreal client

The game receives its launch code on **standard input**, one line, never on the command line, through the **launch handshake** ([ADR-010](../Docs/ADR/ADR-010-play-flow.md) §5; the lines are fixed by `Game/Source/VeyraServices/Contracts/LaunchHandshake.json`). Started with `-VeyraLaunchCode=stdin` and pipes for its standard input and output, the game's `VeyraServices` module:

1. writes `veyra-handoff/1 awaiting-launch-code` on standard output; only then does the launcher request a code and write it, so the code's 20-second life starts once the game can read it;
2. reads that line and calls `POST /v1/game-sessions` with the code and its build version (`ProjectVersion` in `Game/Config/DefaultGame.ini`), keeping the game session token in memory;
3. answers `veyra-handoff/1 signed-in`, or `veyra-handoff/1 failed <code>` if it could not sign in, and the launcher's work is done;
4. finds where the player is, in this order (ADR-010 §2): a live match from `GET /v1/me/match` leads to **Reconnect-only**, which outranks everything; then a select in progress from `GET /v1/me/select` resumes; then a player without a starter (`GET /v1/me/profile`) chooses one; otherwise the shell;
5. from the shell, starts practice (`POST /v1/practice`), polls the select, hovers and locks, polls `GET /v1/me/match` until the match is ready, and joins the server with its ticket;
6. when the match ends, travels back to the front end and polls `GET /v1/me/matches/{id}` for the verified result.

A failure never quits the game: it shows with Retry where retrying can help. The client-state coordinator, `FVeyraClientFlow`, logs its progress as `VeyraClientFlow:` lines. The game does not use `-log`, which on Windows can replace the standard handles.

A match server started by the backend gets `-VeyraAssignment=stdin` and reads its assignment the same way. The game's copy of the assignment's shape is `Game/Source/VeyraServices/Schemas/MatchAssignment.schema.json`. `internal/match/contract_test.go` keeps an example the game's tests read, so a change to the assignment must update both.

The [launcher](../Launcher/README.md) is the real caller: it signs in with `POST /v1/dev/login`, starts the packaged game and hands it a code this way. In development, `veyra-devlaunch` starts the game the same way, speaking the handshake, with the game's build version. It copies the game's other output and waits for the game to exit; `-detach` returns once the game has signed in, as the launcher does:

```sh
go run ./cmd/veyra-devlaunch -backend http://localhost:8080 -account DevOne -build 0.1.0 -- "C:\path\to\VeyraClient.exe" -VeyraLaunchCode=stdin
```

A match created with `POST /v1/dev/matches` before the game starts waits behind Reconnect, as any live match does.

`Game/Scripts/Smoke.ps1 -Handoff` plays a whole match this way, from dev login to the recorded result, through the same handshake (`Start-VeyraHandshakeClient` and `Step-VeyraHandshake` in `Game/Scripts/VeyraProject.psm1`); its clients press Reconnect with `-VeyraSmokeFlow=join`. `Smoke.ps1 -Flow Practice` plays the whole solo path instead: starter choice, practice, champion select, the match, End Custom Match and the verified result. With `-Launcher Cli` the launcher's headless twin signs in and starts the game.

## Configuration

Everything tunable lives in [`config/local.json`](config/local.json): session and launch-code lifetimes, HTTP timeouts, the request size limit, the seeded dev accounts (`DevOne` … `DevTen`), party size, invite lifetime and default privacy, the mode list (Ranked is present but disabled, per the Modes & Access Bible), solo Custom practice (`customPractice`: whether it is on, the mode ID its matches record, the host's side and the pick time), champion select's upkeep (`selection`), match lifetimes and the allocator (the Docker endpoint, the match-server image and network, the host ports players connect to and the server's arguments). The file is validated at startup; a missing or unknown field stops the backend with an error instead of falling back to a default. Dev login is refused unless `environment` is `local`. The database URL comes from the `VEYRA_DATABASE_URL` environment variable, never from the file.

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
    ├── catalog/           released Vanguards, starters and the rotation (configuration)
    ├── account/           onboarding (the stubbed tutorial) and Vanguard entitlements
    ├── selection/         champion select and Custom practice; creates each select's match once
    ├── match/             matches, join tickets, results, the allocator interface
    ├── docker/            the local allocator: one Docker container per match
    ├── secret/            bearer secrets and their hashes
    ├── postgres/          Postgres storage and embedded migrations
    └── httpapi/           HTTP/JSON transport
```

Domain packages (`identity`, `social`, `party`, `account`, `match`, and later matchmaking) own their rules and depend only on storage interfaces; `account` reads the catalog through a small interface. `party` reads the social graph through a small interface and never writes it; a block is applied by `social` first and then handed to `party`. `postgres` implements storage; `httpapi` only translates HTTP to domain calls.

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
