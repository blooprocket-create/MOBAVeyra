//! The Veyra launcher's window (ADR-005 L1–L4, ADR-010 §5, ADR-022 §7): a Tauri app whose static UI
//! (Launcher/ui) shows what the launcher's core reports and asks it to act. It installs, updates or
//! repairs the game from a release store (or uses a packaged build, in development), signs the player
//! in or registers them with Firebase (ADR-038; a local backend also offers the development
//! accounts), starts the game and hands it a launch code, then closes once the game has signed in. All HTTP is the core's, in Rust; the web view makes no requests of its own.
//!
//! `--config <file>` names the configuration; otherwise VEYRA_LAUNCHER_CONFIG, otherwise
//! `VeyraLauncher.json` beside the launcher, otherwise Launcher/config/local.json beside the build
//! folder. `--uninstall-game` removes the installed game and exits without a window, for Setup's
//! uninstaller (ADR-022 §2): status 0 when it is gone or there was none, 1 otherwise.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::Serialize;
use std::path::PathBuf;
use std::process::ExitCode;
use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};
use std::thread;
use tauri::{AppHandle, Manager, State};
use tauri_plugin_dialog::DialogExt;
use veyra_launcher_core::backend::{Backend, BackendError, LauncherSession};
use veyra_launcher_core::config::{self, GameSource, InstallSource, LoadedConfig};
use veyra_launcher_core::install::{self, Mode, Progress, Target};
use veyra_launcher_core::launch::{self, LaunchStage};
use veyra_launcher_core::player::{self, SignIn};
use veyra_launcher_core::secret::Secret;
use veyra_launcher_core::update::{self, Check};
use veyra_launcher_core::{game, manifest};

/// The switch Setup's uninstaller starts the launcher with.
const UNINSTALL_GAME_SWITCH: &str = "--uninstall-game";

/// Where the game is, and what the window should offer.
#[derive(Default, Serialize)]
#[serde(rename_all = "camelCase")]
struct GameStatus {
    /// "ready", "install", "update" or "repair"; empty with a problem.
    state: &'static str,
    /// Whether the game is installed from a release store, so it can be repaired.
    installable: bool,
    /// The build that would launch.
    build_version: Option<String>,
    /// The channel's build, when it is not the one installed.
    available_version: Option<String>,
    folder: Option<String>,
    download_bytes: u64,
    install_bytes: u64,
    /// Something the player should know that does not stop them playing.
    note: Option<String>,
    problem: Option<String>,
}

/// How a player may sign in: with an account (ADR-038), as a development account (a local backend
/// only), or both. `problem` is set when neither is possible.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct SignInOptions {
    player_login: bool,
    accounts: Vec<String>,
    /// Who is signed in already, if anyone: the window offers Play rather than sign-in.
    signed_in_as: Option<String>,
    problem: Option<String>,
}

/// Where signing in or registering got to: "signedIn" with the name, or "chooseName".
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct SignInResult {
    state: &'static str,
    display_name: Option<String>,
}

/// Where a launch is. The window asks for it while one runs.
#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
struct LaunchStatus {
    running: bool,
    stage: String,
    problem: Option<String>,
}

/// What the launcher's own update asks of the window (ADR-022 §11).
#[derive(Default, Serialize)]
#[serde(rename_all = "camelCase")]
struct LauncherUpdate {
    /// "current", or "ready": a checked Setup waits to be started, and the launcher then closes.
    state: &'static str,
    /// The launcher version the waiting Setup installs.
    version: Option<String>,
    /// Something the player should know that does not stop them playing.
    note: Option<String>,
}

/// Where an install, update or repair is. The window asks for it while one runs.
#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
struct InstallStatus {
    running: bool,
    stage: String,
    done_bytes: u64,
    total_bytes: u64,
    downloaded_bytes: u64,
    problem: Option<String>,
}

#[derive(Default)]
struct Launcher {
    launch: Mutex<LaunchStatus>,
    install: Mutex<InstallStatus>,
    /// The channel's release as the window last found it: what Install installs.
    release: Mutex<Option<Target>>,
    /// The signed-in player's session, in memory only (ADR-005 L4).
    session: Mutex<Option<LauncherSession>>,
    /// Firebase's proof for a player who still has to choose a display name (player::SignIn).
    pending: Mutex<Option<Secret>>,
    /// The checked Setup that updates the launcher, once one is downloaded (ADR-022 §11).
    setup: Mutex<Option<PathBuf>>,
}

fn config_path() -> PathBuf {
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        if argument == "--config" {
            if let Some(path) = arguments.next() {
                return PathBuf::from(path);
            }
        }
    }
    veyra_launcher_core::default_config_path(&std::env::current_exe().unwrap_or_default())
}

fn load() -> Result<LoadedConfig, String> {
    config::load(&config_path()).map_err(|error| error.to_string())
}

fn install_source(loaded: &LoadedConfig) -> Result<&InstallSource, String> {
    match &loaded.game {
        GameSource::Install(source) => Ok(source),
        GameSource::Packaged(_) => Err("This launcher starts a packaged build; it has nothing to install.".to_string()),
    }
}

/// A problem as a sentence.
fn sentence(problem: impl ToString) -> String {
    let problem = problem.to_string();
    if problem.ends_with('.') {
        problem
    } else {
        format!("{problem}.")
    }
}

// Network work runs off the window's thread (`async`), so a slow server never freezes the window.
/// Whether the launcher should update itself first (ADR-022 §11). Only a launcher that installs the
/// game from a release store does; one that starts a packaged build is a developer's. A store that
/// does not answer leaves it as it is: the game's own check says so.
#[tauri::command(async)]
fn launcher_update(launcher: State<'_, Arc<Launcher>>) -> LauncherUpdate {
    let current = LauncherUpdate {
        state: "current",
        ..LauncherUpdate::default()
    };
    let Ok(loaded) = load() else { return current };
    let GameSource::Install(source) = &loaded.game else { return current };
    let server = game::release_server(source);
    let folder = update::update_folder();
    match update::check(&server, &source.channel, update::VERSION, &folder) {
        Ok(Check::Current) => {
            update::clear(&folder);
            current
        }
        Ok(Check::Failed(release)) => LauncherUpdate {
            note: Some(format!(
                "The launcher could not update itself to {}. Run Veyra Setup again to update it.",
                release.version
            )),
            ..current
        },
        Ok(Check::Update(release)) => match update::download(&server, &release, &folder, source.download_attempts) {
            Ok(waiting) => {
                *launcher.setup.lock().unwrap() = Some(waiting);
                LauncherUpdate {
                    state: "ready",
                    version: Some(release.version),
                    note: None,
                }
            }
            Err(problem) => LauncherUpdate {
                note: Some(format!(
                    "The launcher's update to {} could not be downloaded: {}",
                    release.version,
                    sentence(problem)
                )),
                ..current
            },
        },
        Err(_) => current,
    }
}

/// Starts the waiting Setup and closes the launcher, so Setup can replace it and open the new one.
#[tauri::command]
fn apply_launcher_update(app: AppHandle, launcher: State<'_, Arc<Launcher>>) -> Result<(), String> {
    let waiting = launcher.setup.lock().unwrap().take().ok_or("No launcher update is waiting.")?;
    update::start_setup(&waiting).map_err(|error| sentence(format!("Veyra Setup could not start ({error})")))?;
    app.exit(0);
    Ok(())
}

#[tauri::command(async)]
fn game_status(launcher: State<'_, Arc<Launcher>>) -> GameStatus {
    let problem = |problem: String| GameStatus {
        problem: Some(sentence(problem)),
        ..GameStatus::default()
    };
    let loaded = match load() {
        Ok(loaded) => loaded,
        Err(error) => return problem(error),
    };
    let source = match &loaded.game {
        GameSource::Packaged(path) => {
            return match manifest::load(path) {
                Ok(build) => GameStatus {
                    state: "ready",
                    build_version: Some(build.version),
                    ..GameStatus::default()
                },
                Err(error) => problem(error.to_string()),
            }
        }
        GameSource::Install(source) => source,
    };
    let folder = match game::folder(source) {
        Ok(folder) => folder,
        Err(error) => return problem(error),
    };
    // A record that cannot be read is offered a repair, which rebuilds it.
    let record = install::load_record(&folder.path);
    let readable = record.as_ref().ok().and_then(Option::as_ref);
    let installed = readable.and_then(|record| record.installed.as_ref());
    let unfinished = readable.is_some_and(|record| record.target.is_some());
    let mut status = GameStatus {
        installable: true,
        build_version: installed.map(|release| release.manifest.build_version.clone()),
        folder: Some(folder.path.display().to_string()),
        ..GameStatus::default()
    };
    match game::release_server(source).current(&source.channel) {
        Ok(target) => {
            status.state = match (&record, installed, unfinished) {
                (Err(_), _, _) => "repair",
                (_, Some(release), false) if release.manifest_hash == target.manifest_hash => "ready",
                (_, None, false) => "install",
                _ => "update",
            };
            if status.state != "ready" {
                status.available_version = Some(target.manifest.build_version.clone());
                status.download_bytes = install::download_size(&target.manifest, installed.map(|release| &release.manifest));
                status.install_bytes = install::installed_size(&target.manifest);
            }
            *launcher.release.lock().unwrap() = Some(target);
        }
        // An installed game can still be played; the backend refuses an outdated build (ADR-005 L4).
        Err(error) if installed.is_some() && !unfinished => {
            status.state = "ready";
            status.note = Some(format!("Updates could not be checked: {}", sentence(error)));
        }
        Err(error) => return problem(error.to_string()),
    }
    status
}

/// Asks the player where to install; returns where the game would go (a folder holding other
/// things gets it in a Veyra folder inside), or nothing if they cancelled.
#[tauri::command]
async fn choose_folder(app: AppHandle, current: Option<String>) -> Result<Option<String>, String> {
    let mut dialog = app.dialog().file().set_title("Choose where to install Veyra");
    if let Some(window) = app.get_webview_window("main") {
        dialog = dialog.set_parent(&window);
    }
    if let Some(parent) = current.as_deref().map(PathBuf::from).and_then(|path| path.parent().map(PathBuf::from)) {
        dialog = dialog.set_directory(parent);
    }
    let Some(picked) = dialog.blocking_pick_folder() else {
        return Ok(None);
    };
    let picked = picked.into_path().map_err(sentence)?;
    install::resolve_install_folder(&picked)
        .map(|folder| Some(folder.display().to_string()))
        .map_err(sentence)
}

#[tauri::command]
fn start_install(launcher: State<'_, Arc<Launcher>>, folder: Option<String>, repair: bool) -> Result<(), String> {
    let loaded = load()?;
    let source = install_source(&loaded)?.clone();
    let target = launcher
        .release
        .lock()
        .unwrap()
        .clone()
        .ok_or("Veyra's download server has not answered, so nothing can be installed yet. Try again.")?;
    let folder = match folder {
        Some(folder) => PathBuf::from(folder),
        None => game::folder(&source)?.path,
    };
    {
        let mut status = launcher.install.lock().unwrap();
        if status.running {
            return Err("Veyra is already installing.".to_string());
        }
        *status = InstallStatus {
            running: true,
            stage: install::Stage::Checking.describe().to_string(),
            ..InstallStatus::default()
        };
    }
    let launcher = Arc::clone(&launcher);
    thread::spawn(move || {
        let mode = if repair { Mode::Repair } else { Mode::Update };
        let mut progress = |progress: &Progress| {
            let mut status = launcher.install.lock().unwrap();
            status.stage = progress.stage.describe().to_string();
            status.done_bytes = progress.done_bytes;
            status.total_bytes = progress.total_bytes;
            status.downloaded_bytes = progress.downloaded_bytes;
        };
        let result = game::install_into(&source, &folder, &target, mode, &AtomicBool::new(false), &mut progress);
        let mut status = launcher.install.lock().unwrap();
        status.running = false;
        status.problem = result.err().map(sentence);
    });
    Ok(())
}

#[tauri::command]
fn install_status(launcher: State<'_, Arc<Launcher>>) -> InstallStatus {
    launcher.install.lock().unwrap().clone()
}

#[tauri::command(async)]
fn sign_in_options(launcher: State<'_, Arc<Launcher>>) -> SignInOptions {
    let loaded = match load() {
        Ok(loaded) => loaded,
        Err(problem) => {
            return SignInOptions {
                player_login: false,
                accounts: Vec::new(),
                signed_in_as: None,
                problem: Some(sentence(problem)),
            }
        }
    };
    let player_login = player::available(&loaded);
    // A backend without development sign-in has no such route; that is not a problem.
    let accounts = Backend::new(&loaded.config.backend.base_url, loaded.http_timeout()).dev_accounts();
    let (accounts, problem) = match accounts {
        Ok(accounts) => (accounts, None),
        // Without development accounts, or with the backend down, players still sign in their own
        // way; a backend that is down says so when they try.
        Err(BackendError::Refused { status: 404, .. }) => (Vec::new(), None),
        Err(_) if player_login => (Vec::new(), None),
        Err(error) => (Vec::new(), Some(sentence(error))),
    };
    let problem = problem
        .or_else(|| (!player_login && accounts.is_empty()).then(|| "This launcher has no way to sign in: its configuration has no playerLogin.".to_string()));
    SignInOptions {
        player_login,
        accounts,
        signed_in_as: launcher.session.lock().unwrap().as_ref().map(|session| session.display_name.clone()),
        problem,
    }
}

/// Keeps what signing in or registering came to, and tells the window.
fn settle(launcher: &Launcher, outcome: SignIn) -> SignInResult {
    match outcome {
        SignIn::SignedIn(session) => {
            let display_name = session.display_name.clone();
            *launcher.session.lock().unwrap() = Some(session);
            *launcher.pending.lock().unwrap() = None;
            SignInResult {
                state: "signedIn",
                display_name: Some(display_name),
            }
        }
        SignIn::ChooseName(proof) => {
            *launcher.pending.lock().unwrap() = Some(proof);
            SignInResult {
                state: "chooseName",
                display_name: None,
            }
        }
    }
}

#[tauri::command(async)]
fn sign_in(launcher: State<'_, Arc<Launcher>>, email: String, password: String) -> Result<SignInResult, String> {
    let loaded = load().map_err(sentence)?;
    let outcome = player::sign_in(&loaded, &email, &password).map_err(sentence)?;
    Ok(settle(&launcher, outcome))
}

#[tauri::command(async)]
fn register(launcher: State<'_, Arc<Launcher>>, email: String, password: String, display_name: String) -> Result<SignInResult, String> {
    let loaded = load().map_err(sentence)?;
    let outcome = player::register(&loaded, &email, &password, &display_name).map_err(sentence)?;
    Ok(settle(&launcher, outcome))
}

#[tauri::command(async)]
fn choose_name(launcher: State<'_, Arc<Launcher>>, display_name: String) -> Result<SignInResult, String> {
    let loaded = load().map_err(sentence)?;
    let proof = launcher.pending.lock().unwrap().clone().ok_or("Your sign-in has expired. Sign in again.")?;
    let session = player::choose_name(&loaded, &proof, &display_name).map_err(sentence)?;
    Ok(settle(&launcher, SignIn::SignedIn(session)))
}

#[tauri::command(async)]
fn reset_password(email: String) -> Result<(), String> {
    let loaded = load().map_err(sentence)?;
    player::send_password_reset(&loaded, &email).map_err(sentence)
}

#[tauri::command]
fn sign_out(launcher: State<'_, Arc<Launcher>>) {
    *launcher.session.lock().unwrap() = None;
    *launcher.pending.lock().unwrap() = None;
}

/// Launches for the signed-in player, or, with `account`, as that development account.
#[tauri::command]
fn start_launch(app: AppHandle, launcher: State<'_, Arc<Launcher>>, account: Option<String>) -> Result<(), String> {
    let session = launcher.session.lock().unwrap().clone();
    if account.is_none() && session.is_none() {
        return Err("Sign in first.".to_string());
    }
    {
        let mut status = launcher.launch.lock().unwrap();
        if status.running {
            return Err("Veyra is already launching.".to_string());
        }
        *status = LaunchStatus {
            running: true,
            stage: LaunchStage::SigningIn.describe().to_string(),
            problem: None,
        };
    }
    let launcher = Arc::clone(&launcher);
    thread::spawn(move || {
        let result = load().and_then(|loaded| {
            let build = game::build(&loaded).map_err(sentence)?;
            let mut progress = |stage: LaunchStage| launcher.launch.lock().unwrap().stage = stage.describe().to_string();
            match (&account, &session) {
                (Some(account), _) => launch::sign_in_and_launch(&loaded, &build, account, &[], &mut progress),
                (None, Some(session)) => launch::launch(&loaded, &build, session, &[], &mut progress),
                (None, None) => unreachable!("checked before the launch started"),
            }
            .map_err(|error| error.to_string())
        });
        match result {
            // The launcher's work is done once the game has signed in (ADR-005 L4).
            Ok(_) => app.exit(0),
            Err(problem) => {
                let mut status = launcher.launch.lock().unwrap();
                status.running = false;
                status.problem = Some(problem);
            }
        }
    });
    Ok(())
}

#[tauri::command]
fn launch_status(launcher: State<'_, Arc<Launcher>>) -> LaunchStatus {
    launcher.launch.lock().unwrap().clone()
}

/// Setup's uninstaller: removes the installed game with no window.
fn uninstall_game() -> ExitCode {
    let removed = load().and_then(|loaded| game::uninstall(install_source(&loaded)?));
    match removed {
        Ok(_) => ExitCode::SUCCESS,
        Err(_) => ExitCode::FAILURE,
    }
}

fn main() -> ExitCode {
    if std::env::args().any(|argument| argument == UNINSTALL_GAME_SWITCH) {
        return uninstall_game();
    }
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(Arc::new(Launcher::default()))
        .invoke_handler(tauri::generate_handler![
            launcher_update,
            apply_launcher_update,
            game_status,
            choose_folder,
            start_install,
            install_status,
            sign_in_options,
            sign_in,
            register,
            choose_name,
            reset_password,
            sign_out,
            start_launch,
            launch_status
        ])
        .run(tauri::generate_context!())
        .expect("the launcher could not start");
    ExitCode::SUCCESS
}
