//! The Veyra launcher's core (ADR-005 L1–L5, ADR-010 §5, ADR-022): its configuration, the backend
//! conversation, players' accounts (Firebase sign-in and registration, ADR-038), the game build it launches, the launch handshake, and installing, updating,
//! repairing and uninstalling the game from a release store. The Tauri app and the headless
//! `veyra-launch-cli` and `veyra-install` are thin shells over it, so they all behave the same.
//!
//! The launcher's own signed updates are still to come (ADR-022 §10).

pub mod backend;
pub mod config;
pub mod firebase;
pub mod game;
pub mod handshake;
pub mod install;
pub mod launch;
pub mod manifest;
pub mod player;
pub mod release;
pub mod releases;
pub mod secret;

use std::path::{Path, PathBuf};

/// The environment variable that names a configuration file, for tools that start the launcher.
pub const CONFIG_ENVIRONMENT_VARIABLE: &str = "VEYRA_LAUNCHER_CONFIG";

/// The configuration Setup installs beside the launcher (ADR-022 §6).
pub const INSTALLED_CONFIG_FILE_NAME: &str = "VeyraLauncher.json";

/// The configuration a launcher at `executable` uses when none is named: `VeyraLauncher.json` beside
/// it, as Setup installs it; otherwise `config/local.json` in the launcher's source folder, two
/// folders up from `Launcher/target/<profile>/`.
pub fn default_config_path(executable: &Path) -> PathBuf {
    if let Some(named) = std::env::var_os(CONFIG_ENVIRONMENT_VARIABLE) {
        return PathBuf::from(named);
    }
    let folder = executable.parent().unwrap_or_else(|| Path::new("."));
    let installed = folder.join(INSTALLED_CONFIG_FILE_NAME);
    if installed.is_file() {
        return installed;
    }
    folder.join("../../config/local.json")
}
