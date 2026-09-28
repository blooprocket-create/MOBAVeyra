//! The Veyra launcher's window (ADR-005 L1–L4, ADR-010 §5): a Tauri app whose static UI (Launcher/ui)
//! asks three commands of the launcher's core. It signs in with the development login, starts the
//! game build and hands it a launch code, then closes once the game has signed in. All HTTP is the
//! core's, in Rust; the web view makes no requests of its own.
//!
//! `--config <file>` names the configuration; otherwise VEYRA_LAUNCHER_CONFIG, otherwise
//! Launcher/config/local.json beside the build folder.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::Serialize;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::thread;
use tauri::{AppHandle, State};
use veyra_launcher_core::backend::Backend;
use veyra_launcher_core::config::{self, LoadedConfig};
use veyra_launcher_core::launch::{self, LaunchStage};
use veyra_launcher_core::manifest::{self, GameBuild};

/// What the window shows first: the build and the accounts to sign in as, or why it cannot.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Startup {
    build_version: Option<String>,
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

#[derive(Default)]
struct Launcher {
    status: Mutex<LaunchStatus>,
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

fn load() -> Result<(LoadedConfig, GameBuild), String> {
    let loaded = config::load(&config_path()).map_err(|error| error.to_string())?;
    let build = manifest::load(&loaded.manifest_path).map_err(|error| error.to_string())?;
    Ok((loaded, build))
}

#[tauri::command]
fn startup() -> Startup {
    let (loaded, build) = match load() {
        Ok(found) => found,
        Err(problem) => {
            return Startup {
                build_version: None,
                accounts: Vec::new(),
                problem: Some(problem),
            }
        }
    };
    match Backend::new(&loaded.config.backend.base_url, loaded.http_timeout()).dev_accounts() {
        Ok(accounts) => Startup {
            build_version: Some(build.version),
            accounts,
            problem: None,
        },
        Err(error) => Startup {
            build_version: Some(build.version),
            accounts: Vec::new(),
            problem: Some(format!("{error}.")),
        },
    }
}

#[tauri::command]
fn start_launch(app: AppHandle, launcher: State<'_, Arc<Launcher>>, account: String) -> Result<(), String> {
    {
        let mut status = launcher.status.lock().unwrap();
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
        let result = load().and_then(|(loaded, build)| {
            let mut progress = |stage: LaunchStage| launcher.status.lock().unwrap().stage = stage.describe().to_string();
            launch::sign_in_and_launch(&loaded, &build, &account, &[], &mut progress).map_err(|error| error.to_string())
        });
        match result {
            // The launcher's work is done once the game has signed in (ADR-005 L4).
            Ok(_) => app.exit(0),
            Err(problem) => {
                let mut status = launcher.status.lock().unwrap();
                status.running = false;
                status.problem = Some(problem);
            }
        }
    });
    Ok(())
}

#[tauri::command]
fn launch_status(launcher: State<'_, Arc<Launcher>>) -> LaunchStatus {
    launcher.status.lock().unwrap().clone()
}

fn main() {
    tauri::Builder::default()
        .manage(Arc::new(Launcher::default()))
        .invoke_handler(tauri::generate_handler![startup, start_launch, launch_status])
        .run(tauri::generate_context!())
        .expect("the launcher could not start");
}
