# Veyra Launcher

The launcher installs the game, keeps it up to date, signs the player in and starts it (ADR-005 L1–L5, ADR-010 §5, ADR-022). It is a Tauri v2 app with a Rust core; the game itself is the one Unreal application. Veyra Setup installs the launcher.

It does its canon jobs:
- **Install, patch and repair.** It installs the game from a release store, updates it when the store's channel moves, and repairs it (ADR-022 §3–§5).
- **Login.** Players sign in, or create an account, with an email and password through Firebase Authentication (ADR-038). The core trades Firebase's ID token with the backend for a Veyra launcher session, kept in memory only; a remembered login is not offered yet (ADR-005 L4). On a local backend the seeded development accounts are still there, folded away beneath the sign-in, with no password.
- **Launch.** It starts the game and hands it a launch code through the launch handshake, then closes once the game has signed in.

The launcher's own signed updates are still to come (ADR-022 §10).

## Layout

| Path | What it is |
|---|---|
| `core/` | `veyra-launcher-core`: the configuration, the build manifest, the backend client, Firebase sign-in and registration (`firebase.rs`, `player.rs`) and the launch handshake. All HTTP is here, in Rust over rustls, so the web view makes no requests. |
| `app/` | `veyra-launcher`: the Tauri window, commands over the core. `build.rs` draws the app icon into the git-ignored `app/icons/`, so no binary icon is committed. |
| `ui/` | The window's static HTML, CSS and JavaScript. No Node toolchain and no bundler. |
| `cli/` | The launcher without its window, for scripts and tests:<br>• `veyra-launch-cli` launches (`Game/Scripts/Smoke.ps1 -Flow Practice -Launcher Cli`, and the opponent of `Play.ps1 -Opponent`);<br>• `veyra-install` installs, updates, repairs and uninstalls;<br>• `veyra-fake-game` is a stand-in game that speaks the handshake. |
| `publish/` | `veyra-publish`: turns a packaged client into a release in a release store (ADR-022 §4). |
| `setup/` | Veyra Setup:<br>• `VeyraSetup.nsi`, the NSIS script;<br>• `veyra-setup-art`, which draws Setup's bitmaps;<br>• `art.json`, which says which splash art they come from and how each is framed. |
| `config/local.json` | The configuration for development: a local backend and the packaged build. |
| `config/installed.json` | The configuration Setup installs beside the launcher: a local backend, and the game from the local release server. |
| `config/publish.json` | How `veyra-publish` cuts releases: chunk sizes and the zstd level (provisional). |

## Build, check and run

You need Rust from rustup: stable, MSVC, with clippy and rustfmt. WebView2 comes with Windows 11.

```powershell
./Launcher/Check.ps1
```

This runs `cargo fmt --check`, `cargo clippy` with warnings as errors, and `cargo test`, then builds everything in release. CI runs the same (`.github/workflows/launcher.yml`), then builds Veyra Setup and keeps it as the run's `VeyraSetup` artifact.

The launcher starts the packaged client that `Game/Scripts/Package.ps1 -Target VeyraClient -Platform Win64` makes. That script also writes the build's manifest, `VeyraBuild.json`, beside the package: the build version a launch code is bound to, and the game's path.

`Game/Scripts/Play.ps1` does the rest for you: it starts the backend, rebuilds the match server's image, builds the launcher and opens it, with the game in a window and its log in `Game/Saved/Play/`. By hand, with the local backend running (`docker compose up -d backend`), open the launcher:

```powershell
./Launcher/target/release/veyra-launcher.exe
```

Or launch without a window:

```powershell
./Launcher/target/release/veyra-launch-cli.exe --account DevOne
```

Both read `config/local.json` from beside the build folder. Name another file with `--config <file>`, or with the `VEYRA_LAUNCHER_CONFIG` environment variable.

## Configuration

Every field is required, and an unknown field is an error. `game` has exactly one of `buildManifest` (a packaged build) and `install` (a release store).

| Field | Local value | Meaning |
|---|---|---|
| `schemaVersion` | 2 | The format. |
| `backend.baseUrl` | `http://127.0.0.1:8080` | The backend: a scheme, a host and a port, with no path. |
| `http.timeoutSeconds` | 10 | How long a request to the backend may take. |
| `launch.awaitReadySeconds` | 180 (provisional) | How long the game may take to start and ask for its launch code: a cold engine start. |
| `launch.awaitSignInSeconds` | 30 | How long the game may take to sign in once it has its code. |
| `game.buildManifest` | the package's `VeyraBuild.json` | A packaged build to launch, relative to the configuration file. |
| `game.install.releasesUrl` | `http://127.0.0.1:8090` | The release store: a scheme, a host, an optional port and an optional path. |
| `game.install.channel` | `local` | The channel whose release is installed. |
| `game.install.defaultFolder` | `Game` | Where the game goes unless the player chooses, relative to the configuration file. |
| `game.install.stateFile` | `LauncherState.json` | Where the launcher remembers the folder the player chose, relative to the configuration file. |
| `game.install.parallelDownloads` | 4 | Chunks downloaded at once, 1 to 64. |
| `game.install.downloadAttempts` | 3 | Times a chunk is downloaded before the install fails. |
| `game.install.downloadTimeoutSeconds` | 60 | How long one request to the release store may take. |
| `game.arguments` | none | More arguments for the game. The launcher adds `-VeyraLaunchCode=stdin` itself; configuration may not set it. |

The launcher reads `--config <file>`, then `VEYRA_LAUNCHER_CONFIG`, then `VeyraLauncher.json` beside itself (where Setup puts it), then `config/local.json` two folders up from its build folder.

## Install, update and repair

A release store is a static file tree (ADR-022 §3):
- `channels/<channel>.json` names the channel's release by its manifest's hash;
- the manifest lists every file of the build as FastCDC chunks;
- each chunk is named by the SHA-256 of its bytes and stored as a zstd frame.

`veyra-publish` writes releases. The launcher downloads only the chunks it lacks, checks every chunk against its hash, and writes new files beside the old ones until all of them verify. The game folder's `VeyraInstall.json` records what the launcher installed, so an update trusts it, a repair re-checks everything, and uninstall removes only what the launcher wrote.

To try the whole path on your machine:
1. Package the client: `Game/Scripts/Package.ps1 -Target VeyraClient -Platform Win64`.
2. Publish it and serve the store on `127.0.0.1:8090`: `Game/Scripts/Publish.ps1`.
3. Start the backend, for signing in: `docker compose up -d backend`.
4. Build Setup (`./Launcher/Package.ps1`, which needs NSIS 3: `winget install NSIS.NSIS`) and run `Launcher/target/setup/VeyraSetup-<version>.exe`.

The launcher opens on its Install screen. Publish again after a change, and the launcher updates when it next opens.

Without a window: `veyra-install --config config/installed.json install`, and likewise `repair`, `uninstall` and `status`.

## Veyra Setup

Setup (ADR-022 §2) installs the launcher for the current Windows user, with no administrator prompt:
- **Where:** `%LOCALAPPDATA%\Programs\Veyra` by default.
- **What it adds:** a Start menu shortcut, an optional desktop shortcut, and an entry in Windows' installed apps.
- **Upgrades:** running it again upgrades in place.
- **Silent:** `/S` installs with no pages.
- **Art:** Raska's splash art, framed as `setup/art.json` says.
- **Unsigned:** until there is a code-signing certificate, SmartScreen warns.

Uninstalling removes the launcher and asks whether to remove the game too. If so, the launcher removes only what it installed (`veyra-launcher --uninstall-game`). A silent uninstall keeps the game.

## The launch handshake

These lines are fixed by `Game/Source/VeyraServices/Contracts/LaunchHandshake.json`, which the game's, `veyra-devlaunch`'s and this launcher's tests all check.
1. The launcher starts the game with `-VeyraLaunchCode=stdin` and pipes for its standard input and output.
2. The game writes `veyra-handoff/1 awaiting-launch-code` once it can read its code.
3. Only then does the launcher ask the backend for the code, which lives seconds. It writes the code to the game's standard input and closes that.
4. The game answers `veyra-handoff/1 signed-in`, and the launcher closes. Or it answers `veyra-handoff/1 failed <code>`: the launcher then stops the game, shows why, and offers Try Again.

**Rules the launcher keeps:**
- A credential goes only into an HTTP header or the game's standard input. The launcher never prints one or puts one on a command line.
- The game inherits none of the launcher's own handles, so a script that reads the launcher's output is not kept waiting by the game.
