//! The Veyra launcher's window (ADR-005 L1–L4, ADR-010 §5, ADR-022 §7): a Tauri app whose static UI
//! (Launcher/ui) shows what the launcher's core reports and asks it to act. It installs, updates or
//! repairs the game from a release store (or uses a packaged build, in development), signs in with
//! the development login, starts the game and hands it a launch code, then closes once the game has
//! signed in. All HTTP is the core's, in Rust; the web view makes no requests of its own.
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
use veyra_launcher_core::backend::Backend;
use veyra_launcher_core::config::{self, GameSource, InstallSource, LoadedConfig};
use veyra_launcher_core::install::{self, Mode, Progress, Target};
use veyra_launcher_core::launch::{self, LaunchStage};
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

/// The development accounts to sign in as, or why there are none.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Accounts {
    accounts: Vec<String>,
    problem: Option<String>,
}

/// Where a launch is. The window asks for it while one runs.
#[derive(Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
struct LaunchStatus {
    running: bool,
    stage: String,
    problem: Option<String>,
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
fn accounts() -> Accounts {
    let result = load().and_then(|loaded| {
        Backend::new(&loaded.config.backend.base_url, loaded.http_timeout())
            .dev_accounts()
            .map_err(|error| error.to_string())
    });
    match result {
        Ok(accounts) => Accounts { accounts, problem: None },
        Err(problem) => Accounts {
            accounts: Vec::new(),
            problem: Some(sentence(problem)),
        },
    }
}

#[tauri::command]
fn start_launch(app: AppHandle, launcher: State<'_, Arc<Launcher>>, account: String) -> Result<(), String> {
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
            launch::sign_in_and_launch(&loaded, &build, &account, &[], &mut progress).map_err(|error| error.to_string())
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
            game_status,
            choose_folder,
            start_install,
            install_status,
            accounts,
            start_launch,
            launch_status
        ])
        .run(tauri::generate_context!())
        .expect("the launcher could not start");
    ExitCode::SUCCESS
}
