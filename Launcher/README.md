# Veyra Launcher

The launcher signs the player in and starts the game (ADR-005 L1–L4, ADR-010 §5). It is a Tauri v2 app with a Rust core; the game itself is the one Unreal application.

So far it does two of its canon jobs:
- **Login.** The development sign-in: pick a seeded account on the local backend, with no password. A real sign-in, and a remembered login, wait for the identity provider (ADR-005 H3, L4).
- **Launch.** It starts the packaged game and hands it a launch code through the launch handshake, then closes once the game has signed in.

Install, patch, repair and the launcher's own signed updates arrive with ADR-005's build-order step 6.

## Layout

| Path | What it is |
|---|---|
| `core/` | `veyra-launcher-core`: the configuration, the build manifest, the backend client and the launch handshake. All HTTP is here, in Rust over rustls, so the web view makes no requests. |
| `app/` | `veyra-launcher`: the Tauri window, three commands over the core. `build.rs` draws the app icon into the git-ignored `app/icons/`, so no binary icon is committed. |
| `ui/` | The window's static HTML, CSS and JavaScript. No Node toolchain and no bundler. |
| `cli/` | `veyra-launch-cli`: the launcher without its window, for scripts (`Game/Scripts/Smoke.ps1 -Flow Practice -Launcher Cli`, and the opponent of `Play.ps1 -Opponent`); and `veyra-fake-game`, a stand-in game that speaks the handshake, for tests. |
| `config/local.json` | The configuration for a local backend. |

## Build, check and run

You need Rust from rustup: stable, MSVC, with clippy and rustfmt. WebView2 comes with Windows 11.

```powershell
./Launcher/Check.ps1
```

This runs `cargo fmt --check`, `cargo clippy` with warnings as errors, and `cargo test`, then builds everything in release. CI runs the same (`.github/workflows/launcher.yml`).

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

Every field is required, and an unknown field is an error.

| Field | Local value | Meaning |
|---|---|---|
| `schemaVersion` | 1 | The format. |
| `backend.baseUrl` | `http://127.0.0.1:8080` | The backend: a scheme, a host and a port, with no path. |
| `http.timeoutSeconds` | 10 | How long a request to the backend may take. |
| `launch.awaitReadySeconds` | 180 (provisional) | How long the game may take to start and ask for its launch code: a cold engine start. |
| `launch.awaitSignInSeconds` | 30 | How long the game may take to sign in once it has its code. |
| `game.buildManifest` | the package's `VeyraBuild.json` | The build to launch, relative to the configuration file. |
| `game.arguments` | none | More arguments for the game. The launcher adds `-VeyraLaunchCode=stdin` itself; configuration may not set it. |

## The launch handshake

These lines are fixed by `Game/Source/VeyraServices/Contracts/LaunchHandshake.json`, which the game's, `veyra-devlaunch`'s and this launcher's tests all check.
1. The launcher starts the game with `-VeyraLaunchCode=stdin` and pipes for its standard input and output.
2. The game writes `veyra-handoff/1 awaiting-launch-code` once it can read its code.
3. Only then does the launcher ask the backend for the code, which lives seconds. It writes the code to the game's standard input and closes that.
4. The game answers `veyra-handoff/1 signed-in`, and the launcher closes. Or it answers `veyra-handoff/1 failed <code>`: the launcher then stops the game, shows why, and offers Try Again.

**Rules the launcher keeps:**
- A credential goes only into an HTTP header or the game's standard input. The launcher never prints one or puts one on a command line.
- The game inherits none of the launcher's own handles, so a script that reads the launcher's output is not kept waiting by the game.
