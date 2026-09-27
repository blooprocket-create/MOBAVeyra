# ADR-007: Match-join contract and local match allocation

**Status:** Accepted. The author merged the M4 pull request that adds it ([#15](https://github.com/blooprocket-create/MOBAVeyra/pull/15)) on 2026-09-26.  
**Date:** 2026-09-26  
**Approved in:** Author decisions for M4 (2026-09-26): handoff only; the server checks tickets against a roster given at start; Go tooling runs in Docker.  
**Related:** [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md) (session handoff L3, lifecycle L4, hosting H1–H3, build-order step 3), [ADR-006](ADR-006-unreal-project-scaffold.md) (§3 modules, §6 tuning, §10 container and smoke test), [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §5, [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §1, §3–4, [Architecture Constitution](../../ARCHITECTURE.md) §4, §11.

## Context

ADR-005 fixes how an authenticated session reaches the game (a single-use launch code over the game's stdin) and that the backend starts one Linux match server per match through an allocator. It does not say how a game client proves to that server that it belongs in the match, how the server learns who may join, or how the result gets back. The Client & Platform Bible says the match-server connection protocol and token formats are "not yet specified".

Today, clients reach a server by development-only direct connect, sides come from "join the smaller side", and Shipping servers refuse every login until this contract exists (ADR-006 §10). M4 implements ADR-005's build-order step 3, "local backend login, allocation and results", and needs this contract first.

Two requirements shape it:

- **Reconnect after a crash.** ADR-005 L4 has a crashed client ask the backend whether its match is live and rejoin it. A client that lost everything in memory must be able to get a valid join credential again while the match runs.
- **The server checks joiners locally.** The author chose that the server receives its roster when it starts and checks each joining client against it, with no backend call at login. A server keeps working through a backend restart, and tests need no backend.

## Decision

### 1. Flow

1. The game redeems its launch code for a game session (`vgs_`), as today.
2. A match is created. Until step 4 builds party → queue → Match Found, a development-only endpoint creates it from a list of accounts and sides.
3. The backend reserves a port, starts a match-server container through its allocator and writes the server's **assignment** to the server's standard input.
4. The server validates the assignment, loads the map and reports that it is ready.
5. The game asks the backend for its match (`GET /v1/me/match`). Once the match is ready, the answer holds the server's address and the player's **join ticket**.
6. The game connects with the ticket as a login option. The server hashes the ticket and looks it up in its roster, and puts the player on the side the roster names.
7. The match ends. The server reports the **result** and exits. The backend records it, closes the match and removes the container.

### 2. Credentials

Every credential is a prefix followed by the base64url encoding (no padding) of 32 bytes, as the backend's existing tokens are. The backend stores hashes, never the credentials themselves, except the per-match key in item 3.

| Prefix | What | Who holds it | Lifetime |
|---|---|---|---|
| `vls_`, `vgs_`, `vlc_` | Launcher session, game session, launch code | As ADR-005 | Unchanged |
| **`vjt_`** | Join ticket for one account in one match | The owner's game, in memory | Until the match ends |
| **`vms_`** | Match-server credential | The match server, in memory | Until the match ends |

### 3. Join tickets are derived, not stored

- Each match gets a random 32-byte **match key**, kept by the backend.
- A player's ticket is `vjt_` + base64url(HMAC-SHA256(match key, `"veyra-join-ticket-v1"` ‖ 0x00 ‖ match ID ‖ 0x00 ‖ account ID)).
- The backend can derive the same ticket again for the authenticated owner at any time, which is what reconnect needs.
- The server receives only the lowercase hex SHA-256 of each participant's ticket.
- When a match ends or fails, the backend **erases its match key**. Every ticket for that match is then worthless, even if one leaked.

### 4. Using a ticket

- A ticket is accepted from the moment the server is ready until the match ends, for one account in one match.
- **One connection per account at a time.** A second login with a ticket whose account is already connected is refused.
- The ticket may be presented again after a disconnect; the contract allows rejoining. M4's server still refuses a rejoin with an explicit reason, until the reconnect milestone implements Match Flow §4's "same Vanguard" rule (the engine's base game mode cannot give a returning player back their old PlayerState).
- The game sends the ticket as the login option `VeyraTicket`, added when the engine builds its login request after the server's challenge. It is never on a command line or in a travel URL.

### 5. The server's assignment

- One line of JSON on the server's standard input, validated strictly with the same schema dialect as tuning (ADR-006 §6):
  `schemaVersion`, `matchId`, `backendUrl`, `serverCredential`, and `participants` with each one's `accountId`, `displayName`, `side` (`A` or `B`) and `ticketHash`.
- The server is started with `-VeyraAssignment=stdin`, which names the channel and is not a secret.
- Standard input is ADR-005's channel for secrets handed to a process. It needs no shell (the server image is distroless), appears in no environment variable, file or `docker inspect` output, and is gone once read.
- The server checks the assignment against its own limits (for example each side's size against `Match.json teams.maxTeamSize`). An invalid assignment is fatal: the server exits, and the backend fails the match.

### 6. The server reports back

- `POST /v1/server/matches/{id}/ready` once the map is loaded and the server accepts players.
- `POST /v1/server/matches/{id}/result` when the match ends.
- Both authenticate with `Authorization: Bearer vms_…`. The credential is scoped to its match.
- The result call is idempotent: the same body again returns success, and a different body is a conflict. After the match ends, the credential can only repeat that same result.

### 7. The result (minimum for M4)

- `endReason`: `developer_request` or `abandoned`.
- `winner`: `A`, `B` or null. It is always null in M4, because no victory condition exists yet.
- `durationSeconds`: the match clock, which excludes pauses; 0 if the match never went live.
- `participants`: each rostered account with `joined` and `connectedAtEnd`. The backend checks that the list matches the roster exactly.

Statistics, rewards and outcome adjudication (Match Flow §11, Match Statistics Bible) build on this record later.

### 8. How a match ends in M4

- **Developer request.** A development build lets a player end the match, like the developer pause (ADR-006 §8). Shipping refuses it.
- **Abandonment.** No participant connected for `Match.json lifecycle.abandonAfterSeconds` (a provisional value). This counts real time: it is a server lifecycle clock, not a gameplay timer, so ADR-006 §8's world-time rule does not apply to it.
- The backend adds safety nets: a match whose server never reports ready within a configured time, runs past a configured maximum duration, or whose container exits without a result is marked failed.

### 9. Build rules

- A development server started **without** an assignment keeps M3's direct connect, for the smoke test and editor work.
- A server started **with** an assignment requires a valid ticket for every login.
- A Shipping server refuses to start without an assignment, and refuses direct connections.
- The tuning-hash check (ADR-006 §6) applies in every case.

### 10. Match states in the backend

`allocating` → `ready` → `ended`, or `failed` from either earlier state.

- An account has at most one active (allocating or ready) match.
- `GET /v1/me/match` returns the active match, or none. That is the "is my match live" query ADR-005 L4 needs for Reconnect-only.
- It returns the server's address and the ticket only once the match is ready.

### 11. The allocator

- The backend starts match servers through an interface with a local Docker implementation now and a fleet implementation later (ADR-005 H3).
- The local implementation talks to the Docker Engine API. The backend container reaches it through the Docker socket, which grants control of the Docker host, so this is acceptable only on a developer machine.
- Containers get their assignment on standard input, no environment secrets, dropped capabilities and a label naming their match.
- The image, network, port range, public host, the backend's address as seen from a server, and every timeout are validated configuration, never literals (ADR-005, Consequences).

### 12. Where the code lives

- **Backend:** a `match` module (domain rules, allocation, results) beside identity and party, a Docker allocator adapter, and Postgres tables for matches, participants, active assignments and results.
- **Game:** `VeyraMatch` owns the roster, the join rules and the match lifecycle, and knows nothing about HTTP. A new `VeyraServices` module (the trusted-services client ADR-006 §3 anticipated) reads the launch code and the assignment, talks to the backend and plugs into VeyraMatch's lifecycle. ADR-006 §3 records its layer.

## Consequences

- A client can join only the match the backend assigned it, only on its assigned side, and only once at a time. A leaked ticket is useless after its match ends.
- The server needs no backend at login, and tests can supply a roster and tickets directly.
- Reconnect after a crash needs no new credential: the game asks the backend again and gets the same ticket. Only the server's rebind to the old PlayerState is left to build.
- **Known exposure:** the engine writes the login options of each connection to the server's log, in its `LogNet` "Login request" and "Join request" lines, so a ticket appears there. This is acceptable locally, since tickets are short-lived and scoped to one match. The planned fix is to carry the ticket in the engine's network encryption-token handshake, together with packet encryption, before servers are hosted.
- The backend database holds live match keys. Anyone who can read it during a match can derive that match's tickets. Encrypting match keys at rest is left open.
- The local backend container can control Docker. A hosted allocator must not use the Docker socket this way.
- Development clients must read their launch code from standard input and must not use `-log`, which on Windows can replace the standard handles.

## Implementation and evidence (M4, 2026-09-26)

**Backend.**
- The `match` module holds the domain rules, ticket derivation, the result rules and the reaper.
- `docker` is the local allocator: it attaches to the container, starts it, then writes the assignment and closes standard input.
- `secret` is the one bearer-secret primitive. Postgres migration `0003_match.sql` holds the match tables.
- `httpapi` serves the routes in §6 and §10, and `POST /v1/dev/matches` only in the local environment.
- `Backend/Check.ps1` runs gofmt, vet and the race-enabled tests in the official Go image.

**Game.**
- `VeyraMatch` owns the roster, the join rules and the match lifecycle:
  - `UVeyraMatchHostSubsystem` takes the assignment in and raises "accepting players" and "match ended";
  - `FVeyraMatchRoster`, and ticket checks at login;
  - the Ended phase, the developer end-match request, and abandonment on a real-time clock.
- `VeyraServices` is the new trusted-services client, in the Services layer (ADR-006 §3):
  - A game started with `-VeyraLaunchCode=stdin` reads its code, redeems it with its `ProjectVersion`, polls `GET /v1/me/match` and travels with its ticket.
  - A dedicated server started with `-VeyraAssignment=stdin` reads and validates its assignment before its first map loads, then reports ready and the result, retrying transient failures. Every Shipping server takes this path.
  - The backend's address and every wait are `UVeyraServicesSettings` in `Config/DefaultGame.ini`, validated at start.
- The assignment is validated against `Game/Source/VeyraServices/Schemas/MatchAssignment.schema.json`, using the tuning dialect extended with text and arrays (ADR-006 §6).

**Contract checks.**
- A Go test writes `Game/Source/VeyraDeveloper/TestData/MatchAssignment.example.json`, built from the backend's ticket vector.
  - The game's tests bind it, and check that the first participant's ticket hash matches the vector's ticket.
  - `scripts/check_tuning.py` lints the schema and validates the example in CI.
  - A change to the assignment's shape therefore fails on both sides.

**Evidence.**
- Unit tests: `Veyra.Core.Sha256`, `Veyra.Match.JoinTicket`, `.Roster`, `.HostedAssignment` and `.JoinRules`, `Veyra.Services.*`, and the backend's Go tests (including a real-engine Docker stdin test).
- Network tests: `Veyra.Net.HostedMatch.*` checks rostered sides, refused logins, a developer end with its result, and abandonment.
- End to end, `Game/Scripts/Smoke.ps1 -Handoff` passes:
  - The backend creates a match for two dev accounts and starts its container, which reports ready.
  - Each packaged client reads a fresh launch code from a pipe, joins with its ticket and plays the scripted match; the first client ends it.
  - The backend records a developer request with no winner and both participants joined and connected at the end, then removes the server.
  - No log holds a credential except the engine lines named in the known exposure above, and every log is free of warnings.

**Settled while implementing.**
- **Standard input must be a pipe.** It is read without blocking, one line of at most 64 KiB. A match server refuses to start with the engine's `-cmdstdin`, which would run the assignment as a console command and log it.
- **Logged problems are redacted.** A JSON syntax error quotes the text around it, which for a one-line assignment includes the credential.
- **A game's handoff verdict is its log line.** Its failures log "VeyraHandoff: FAIL: <reason>". A graceful Windows exit always returns 0, while a Linux match server exits with a failure status.
- **Key spelling in cooked builds.** A cooked build keeps one spelling per engine name, the first the process registered, so the assignment's `MatchId` read back as `MatchID`. Schema keys now match struct fields ignoring case in cooked builds (ADR-006 §6).

## Open items

- The server rebind to a returning player's PlayerState and Vanguard, and the client's Reconnect-only state (Match Flow §4, ADR-004).
- Moving the ticket into the encryption-token handshake, and packet encryption.
- A hosted allocator, and how it delivers the assignment and credential.
- Encrypting match keys at rest.
- Refusing outdated builds when a party queues (step 4).
- Turning results into statistics, rewards and adjudicated outcomes.
- Credentials for spectators and replay viewers.
- AI participants in custom lobbies.
- Victory conditions, which make `winner` meaningful.

## Alternatives considered

- **Random tickets stored only as hashes:** the backend could not issue a ticket again after a crash without pushing new hashes to a running server.
- **One signing key for the whole backend:** a single leak would compromise every match. A key per match, erased at its end, limits the damage to one match.
- **Signed tokens (Ed25519 or JWT) that the server verifies:** they work offline, but add key generation, distribution and rotation, plus verification code in Unreal, and gain nothing over a roster that already arrives at start.
- **Storing tickets in plain text:** breaks the backend's rule of storing only hashes of credentials.
- **The server asking the backend at each login:** every login would depend on the backend being reachable, and the server would need its credential earlier. Rejected by the author in favour of the roster at start.
- **Delivering the assignment in an environment variable:** visible to anyone who can inspect the container.
- **Uploading the assignment as a file:** it would sit in the container's filesystem. Kept as the fallback if writing to standard input through the Docker API proves unworkable.
