# ADR-057: Public test hosting from the author's PC

**Status:** Accepted. The author decided on 2026-10-02 that outside players should be able to install and play, with the backend, database and match servers on the author's own PC, a temporary tunnel address to start with, Firebase kept for sign-in, and code signing later.
**Date:** 2026-10-02
**Related:**
- [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md): local-first hosting. Hosted vendors, the CDN and signing were open; this record settles the first two for public tests.
- [ADR-007](ADR-007-match-join-contract.md): the match join contract, which tells a client the match server's host and port.
- [ADR-022](ADR-022-installer-and-game-patching.md): Setup, the launcher's installs and updates, and the release store.
- [ADR-038](ADR-038-player-accounts-with-firebase-authentication.md): player sign-in with Firebase Authentication.

## Context

Everything runs on one PC and listens only on `127.0.0.1`:
- the backend, its Postgres and the match servers in Docker;
- the release file server that the launcher installs and updates from.

The installed launcher, and the game it starts, know only those local addresses, so nobody else can sign in, download or play. The author wants friends to install and play now without paying for hosting. Their PC stays the host, and Cloudflare (already connected to the repository) provides the public front.

## Decision

### 1. One permanent public address

The `veyra` Worker at `https://veyra.blooprocket.workers.dev`, deployed from `main` by Workers Builds, is the only address players' software knows:

| Path | Goes to |
|---|---|
| `/v1/…`, `/healthz`, `/readyz` | the backend, unchanged |
| `/v1/server/…` | refused: match servers reach the backend inside Docker, never through the Worker |
| `/releases/…` | the release store, without the `/releases` prefix |
| anything else | the placeholder page |

The backend's address has no path, because the launcher and the game accept only a bare `https://host` base URL. The release store's address may carry one.

### 2. Temporary tunnels behind it

- **The host script** (`Game/Scripts/Host.ps1`) opens two Cloudflare quick tunnels (`cloudflared`), one to the backend and one to the release server. Each gets a random `trycloudflare.com` address that lasts until the tunnel stops.
- **Recording the addresses:** the script stores them as the Worker's secrets `API_ORIGIN` and `RELEASES_ORIGIN` (`wrangler secret put`). Secrets survive every deploy from `main`, so the Worker forwards to whichever tunnels are current, and players' launchers never change.
- **While the host is down,** the Worker answers 503, "Veyra's servers are offline".
- **No inbound web port** is opened on the PC.

### 3. A public backend configuration

- **`Backend/config/public.json`** is the backend's configuration for public tests:
  - `environment` is `public`, so developer login and developer match creation cannot be enabled;
  - Firebase is the sign-in, as ADR-038 decided;
  - the match servers bind to every address (`hostIp` `0.0.0.0`).
- **The public host:** the host script writes `Backend/config/hosted.json` (ignored by git) from it, with `publicHost` set to the PC's public IPv4 address, which is what players' clients connect to. The script reads that address from Cloudflare's trace endpoint, or takes it as a parameter.
- **Postgres and the backend's own port** stay on `127.0.0.1`.

### 4. Match servers over the internet

- Players' clients reach a match server at the public address on its UDP host port, 7780–7789 (ADR-007).
- **The author's part:** their router forwards those UDP ports to this PC, and Windows lets them in. Changes to the router and the firewall stay with the author.

### 5. The launcher tells the game where the backend is

- **The launcher** starts the game with `-VeyraBackendUrl=` set to its own `backend.baseUrl`, so one launcher configuration steers both.
- **The game** prefers that switch, validated as a base URL, to `BackendBaseUrl` in its ini. The ini stays the address for a game started without the launcher.
- A configuration's `game.arguments` may not name the switch itself.

### 6. The public installer

- **The launcher's public configuration** (`Launcher/config/public.json`) names the Worker as its backend and `https://veyra.blooprocket.workers.dev/releases` as its release store, on the `public` channel.
- **Setup for players:** `Launcher/Package.ps1 -Config public` builds Setup with that configuration.
- **Publishing:** `Game/Scripts/Publish.ps1 -Channel public` publishes a packaged client to that channel.

### 7. Limits accepted for public tests

- **Uptime:** the game is playable only while the host PC runs `Host.ps1`. Matches end if the PC sleeps.
- **Quick tunnels** carry no uptime guarantee and limit concurrent requests. A permanent named tunnel waits for a domain.
- **Free-plan quota:** the Worker's free plan allows 100,000 requests a day. Each client's polling and each downloaded chunk counts, which is enough for a small group of testers.
- **Unsigned software:** Setup, the launcher and the game are unsigned, so Windows warns before Setup runs (ADR-022 §10).
- **Download speed** is the host PC's upload speed.

## Consequences

- Friends can install, update, sign in and play against the author's PC, with no paid service.
- Moving to hosted servers later changes only the Worker's origins, or the base URLs, not the launcher or the game.
- Exposure: the backend's public routes are reachable from the internet through the Worker, behind Firebase sign-in and Veyra's own sessions. The developer routes don't exist in the public environment, and the match servers' routes are refused.
