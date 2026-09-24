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

Only the **identity module**: the launcher → game session handoff.

| Endpoint | Auth | Body | Returns |
|---|---|---|---|
| `POST /v1/dev/login` | — | `{"accountName"}` | launcher session token. **Local only**; the route does not exist unless dev login is enabled |
| `POST /v1/launch-codes` | `Bearer <launcher token>` | `{"buildVersion"}` | single-use launch code |
| `POST /v1/game-sessions` | — | `{"launchCode", "buildVersion"}` | game session token + account |
| `GET /v1/me` | `Bearer <game token>` | — | account |

Rules the code enforces:

- Tokens and codes are 256-bit random values with recognisable prefixes (`vls_` launcher session, `vgs_` game session, `vlc_` launch code). Only SHA-256 hashes are stored.
- A launch code is single-use, expires after `launchCodes.lifetime` (capped at one minute by ADR-005), and is bound to the account and build version. A wrong build version still uses the code up.
- Launcher and game sessions are not interchangeable.
- Every auth failure returns the same `401 invalid_credentials`.

## For the Unreal client

The game receives its launch code on **standard input**, one line, never on the command line. On startup it should read that line, then call `POST /v1/game-sessions` with the code and its own build version, and keep the returned game session token in memory. In development, `veyra-devlaunch` starts the game the same way:

```sh
go run ./cmd/veyra-devlaunch -backend http://localhost:8080 -account DevOne -build dev -- "C:\path\to\Veyra.exe"
```

## Configuration

Everything tunable lives in [`config/local.json`](config/local.json): session and launch-code lifetimes, HTTP timeouts, the request size limit and the seeded dev accounts (`DevOne` … `DevTen`). The file is validated at startup; a missing or unknown field stops the backend with an error instead of falling back to a default. Dev login is refused unless `environment` is `local`. The database URL comes from the `VEYRA_DATABASE_URL` environment variable, never from the file.

## Layout

```text
Backend/
├── cmd/veyra-backend/     the service
├── cmd/veyra-devlaunch/   launcher stand-in for development
├── config/                validated config files
└── internal/
    ├── config/            config loading and validation
    ├── identity/          accounts, sessions, launch codes (domain rules)
    ├── postgres/          Postgres storage and embedded migrations
    └── httpapi/           HTTP/JSON transport
```

Domain packages (`identity`, and later party, matchmaking, match allocation) own their rules and depend only on a storage interface. `postgres` implements storage; `httpapi` only translates HTTP to domain calls.

## Tests

```sh
cd Backend
go test ./...
```

Postgres integration tests run when `VEYRA_TEST_DATABASE_URL` points at a disposable database. They wipe the identity tables, so give them their own database, not the `veyra` one the backend uses:

```sh
docker compose exec postgres createdb -U veyra veyra_test
VEYRA_TEST_DATABASE_URL=postgres://veyra:veyra-local-only@localhost:5432/veyra_test?sslmode=disable go test ./internal/postgres/
```

CI runs everything against its own Postgres.
