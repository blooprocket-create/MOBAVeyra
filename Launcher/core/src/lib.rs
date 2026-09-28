//! The Veyra launcher's core (ADR-005 L1–L4, ADR-010 §5): its configuration, the backend
//! conversation, the game build it launches and the launch handshake. The Tauri app and the
//! headless `veyra-launch-cli` are thin shells over it, so both behave the same.
//!
//! Only login and launch exist so far; install, patch, repair and the launcher's own updates arrive
//! with ADR-005's build-order step 6.

pub mod backend;
pub mod config;
pub mod handshake;
pub mod launch;
pub mod manifest;
pub mod secret;

use std::path::{Path, PathBuf};

/// The environment variable that names a configuration file, for tools that start the launcher.
pub const CONFIG_ENVIRONMENT_VARIABLE: &str = "VEYRA_LAUNCHER_CONFIG";

/// The configuration a launcher at `executable` uses when none is named: `config/local.json` in the
/// launcher's source folder, two folders up from `Launcher/target/<profile>/`.
pub fn default_config_path(executable: &Path) -> PathBuf {
    if let Some(named) = std::env::var_os(CONFIG_ENVIRONMENT_VARIABLE) {
        return PathBuf::from(named);
    }
    let folder = executable.parent().unwrap_or_else(|| Path::new("."));
    folder.join("../../config/local.json")
}
