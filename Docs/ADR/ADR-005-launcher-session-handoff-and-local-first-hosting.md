# ADR-005: Launcher, session handoff and local-first hosting

**Status:** Accepted  
**Date:** 2026-09-24  
**Approved in:** Author discussion of launcher proposals L1–L5 and hosting H1–H4 (2026-09-24)  
**Related:** [ADR-001](ADR-001-unreal-version-policy.md) (engine pin), [ADR-004](ADR-004-unified-unreal-client-states.md) (one Unreal application), [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §1, §5, §10, [Architecture Constitution](../../ARCHITECTURE.md) §4, §12.

## Context

Basic implementation is about to start. Canon already fixes *what* the launcher does (login, install, patch, repair, launch handoff) and that trusted services are logical boundaries, but left open the launcher technology, how an authenticated session reaches the game, where services run, the backend language and the asset source-control policy. The game client needs a session contract from its first networked build, and the Unreal project cannot be scaffolded without an asset policy.

The author is a solo developer aiming for a long-lived competitive MOBA. They want local testing in Docker and **no hosted services (Supabase or otherwise) until something is working**.

## Decision

### Launcher (L1–L5)

1. **Scope:** exactly the canon responsibilities — login, install, patch, repair, launch — plus updating itself. No news, store, social or other pre-game features; those belong to the Unreal application.
2. **Technology:** **Tauri** (Rust core, web-technology UI) with its signed self-updater. The launcher is never an Unreal program: the thing being patched does not patch itself.
3. **Session handoff:** the game never receives a password or the launcher's login credentials. The launcher logs in, requests from the backend a **single-use launch code** that expires within seconds and is bound to the account and client build version, and starts the game. The code is **never placed on the command line**, where other same-user processes can read it: the launcher hands it to the game over a private channel only the launched process holds (an inherited pipe on the child's standard input). The game redeems it for its own session. Single use and a seconds-long lifetime limit the damage if the channel is ever compromised; they are not the protection. Development tooling passes launch codes from the local backend over the same channel, so the game's login path is identical with or without the launcher.
4. **Lifecycle:** the launcher closes after a successful launch. A remembered login is stored in the Windows credential store **only** when the player opts in. After a crash, the player relaunches through the launcher; the game asks the backend whether an assigned match is live and, if so, enters Reconnect-only. That determination comes from trusted services, never from local files. When the backend refuses to queue an outdated build, the game may reopen the launcher to update.
5. **Patching:** content-addressed chunks plus a published manifest mapping files to chunk hashes. The launcher downloads only missing chunks; verify re-hashes, repair re-downloads failed chunks. The specific chunking/compression method is chosen when the patcher is built.

Code-signing certificate, anti-cheat installation, the production identity provider and the download CDN are deferred until there is a build for outside players.

### Platforms

- **Client:** Windows x64 only for now.
- **Dedicated match server:** Linux x64, built from the source engine (ADR-001) and run in a container.

### Hosting (H1–H4)

1. **Local-first.** Until something is working, everything runs on the developer's machine with `docker compose`: a Postgres container, the backend service, and Unreal Linux dedicated-server containers the backend starts per match. Additional infrastructure (Redis, queues) is added only when a feature needs it. No hosted vendor is selected by this ADR.
2. **Backend: Go, as one service with separate internal modules** — identity/session, account, party/social, matchmaking, match allocation and results. These are the canon's logical boundaries, not a microservice count. The game remains C++; the backend is a separate networked program. Go was chosen for long-term operation at scale: strong concurrency for presence, parties and matchmaking; small static container images; a mature ecosystem for game backends; and simple deployment.
3. **Replaceable edges.** The backend starts match servers through an allocator interface (local Docker now, a fleet host later) and issues **Veyra-owned session tokens**, so a later identity provider sits behind Veyra's endpoints and switching it never changes the game or launcher. Postgres is the database of record so a later managed Postgres (Supabase included) needs no data-model migration. Local accounts are seeded development accounts; no password login is exposed publicly until a production provider is chosen.
4. **Source control:** **Git + Git LFS on GitHub**, with LFS file locking for binary Unreal assets (`.uasset`, `.umap`). Cloud agent sessions may skip LFS content, since they mainly edit code and documents.
5. **Builds and CI:** the author's Windows PC builds the source engine and, later, runs as a self-hosted GitHub Actions runner. Because the repository is publicly viewable, that runner **only builds trusted code**: branches pushed to this repository by its maintainers and pull requests from those branches, never pull requests from forks or outside contributors. Untrusted pull requests need an ephemeral, isolated runner (a disposable VM) if they are ever built. Cloud agent sessions cannot compile Unreal and must say so rather than claim a build passed.

## Consequences

- The session-handoff contract (launch code → game session) is implemented in the backend and the game from the first networked build; the launcher UI, install and patching follow later without changing game code.
- Recommended build order: scaffold the 5.8.3 project and Windows client/Linux server targets; containerised server with dev-only direct connect; local backend login, allocation and results; minimal party → queue → Match Found; launcher login and launch; launcher install/patch/repair against a local file server; only then choose hosted vendors.
- Endpoints, timeouts, code lifetimes and retry policies are validated configuration, never literals.
- Hosted vendors (identity, database host, match-server fleet, website host, CDN), anti-cheat and Perforce remain open decisions for a later ADR.
- Tuning authored as binary Data Assets cannot be edited by agents; how tuning is authored (for example text imported into DataTables) is a scaffolding decision still to be made.

## Alternatives considered

- **Launcher in Electron, .NET/Avalonia or an Unreal program:** Electron is far larger; .NET is viable but more Windows-bound; an Unreal program is heavy and would patch itself.
- **Passing a login token or credentials to the game:** rejected; a long-lived secret is too valuable to hand across processes.
- **Launch code on the command line:** rejected; other same-user processes can read command lines and redeem the code first.
- **Unrestricted self-hosted runner:** rejected; it would run untrusted pull-request code on the developer's machine.
- **Launcher staying resident:** unnecessary; the game needs nothing from it at runtime.
- **Backend in TypeScript, C# or C++; adopting Nakama:** TypeScript shares a language with web tooling but is weaker for long-running concurrent services; C# is a close second; C++ adds cost with no benefit for services; Nakama would need its party and matchmaking model bent to fit the Parties bible.
- **Hosted services now (Supabase etc.):** deferred by the author until a working local slice exists.
- **Perforce:** the industry standard for large Unreal teams, but requires running a server and splitting the agent workflow away from GitHub; revisit if LFS stops scaling.
