//! Where the game is (ADR-022 §6): a packaged build, or a game installed from a release store into
//! the folder the launcher's state file names. The window and the headless tools ask here, so they
//! agree on which build they start.

use crate::config::{GameSource, InstallSource, LoadedConfig};
use crate::install::{self, FolderKind, InstallRecord, Mode, Outcome, Progress, Settings, Target, Uninstalled};
use crate::manifest::{self, GameBuild};
use crate::release::BUILD_MANIFEST_FILE_NAME;
use crate::releases::ReleaseServer;
use serde::{Deserialize, Serialize};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::atomic::AtomicBool;

/// The state file format this launcher reads and writes.
pub const STATE_SCHEMA_VERSION: u32 = 1;

/// What the launcher remembers on this machine: the folder the player installed the game in.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct LauncherState {
    pub schema_version: u32,
    pub game_folder: String,
}

/// The state at `path`; none if the launcher has not installed the game yet.
pub fn load_state(path: &Path) -> Result<Option<LauncherState>, String> {
    let text = match fs::read_to_string(path) {
        Ok(text) => text,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(format!("{} could not be read ({error})", path.display())),
    };
    let state: LauncherState = serde_json::from_str(&text).map_err(|error| format!("{} is not the launcher's state: {error}", path.display()))?;
    if state.schema_version != STATE_SCHEMA_VERSION || state.game_folder.trim().is_empty() {
        return Err(format!("{} is not the launcher's state", path.display()));
    }
    Ok(Some(state))
}

/// Remembers `game_folder` in the state file at `path`.
pub fn save_state(path: &Path, game_folder: &Path) -> io::Result<()> {
    let state = LauncherState {
        schema_version: STATE_SCHEMA_VERSION,
        game_folder: game_folder.to_string_lossy().into_owned(),
    };
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, serde_json::to_vec_pretty(&state).expect("the state serialises"))
}

/// The game folder: the one the player chose, or where the game would go by default.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GameFolder {
    pub path: PathBuf,
    /// False while the player has not installed the game yet: the folder is only the default.
    pub chosen: bool,
}

/// Where the game from `source` is, or would go.
pub fn folder(source: &InstallSource) -> Result<GameFolder, String> {
    if let Some(state) = load_state(&source.state_file)? {
        return Ok(GameFolder {
            path: PathBuf::from(state.game_folder),
            chosen: true,
        });
    }
    let path = install::resolve_install_folder(&source.default_folder).map_err(|error| error.to_string())?;
    Ok(GameFolder { path, chosen: false })
}

/// The install record of the game from `source`, if it has one.
pub fn record(source: &InstallSource) -> Result<(GameFolder, Option<InstallRecord>), String> {
    let folder = folder(source)?;
    let record = install::load_record(&folder.path).map_err(|problem| format!("The Veyra install in {} cannot be read: {problem}", folder.path.display()))?;
    Ok((folder, record))
}

/// The release store `source` installs from.
pub fn release_server(source: &InstallSource) -> ReleaseServer {
    ReleaseServer::new(&source.releases_url, source.download_timeout)
}

/// Installs, updates or repairs the game in the folder the player picked (resolved as
/// `install::resolve_install_folder` does), remembering the folder before anything is written so an
/// interrupted run is found again. Returns the folder with the outcome.
pub fn install_into(
    source: &InstallSource,
    picked: &Path,
    target: &Target,
    mode: Mode,
    cancel: &AtomicBool,
    progress: &mut dyn FnMut(&Progress),
) -> Result<(PathBuf, Outcome), String> {
    let folder = install::resolve_install_folder(picked).map_err(|error| error.to_string())?;
    save_state(&source.state_file, &folder).map_err(|error| format!("{} could not be written ({error})", source.state_file.display()))?;
    let settings = Settings {
        workers: source.parallel_downloads,
        attempts: source.download_attempts,
    };
    let outcome = install::install(&folder, target, &release_server(source), settings, mode, cancel, progress).map_err(|error| error.to_string())?;
    Ok((folder, outcome))
}

/// Removes the installed game, as Setup's uninstaller asks (ADR-022 §2), and forgets its folder.
/// None if there was no install to remove.
pub fn uninstall(source: &InstallSource) -> Result<Option<(PathBuf, Uninstalled)>, String> {
    let folder = folder(source)?;
    let removed = match install::inspect_folder(&folder.path).map_err(|error| error.to_string())? {
        FolderKind::Install => Some((folder.path.clone(), install::uninstall(&folder.path).map_err(|error| error.to_string())?)),
        _ => None,
    };
    match fs::remove_file(&source.state_file) {
        Ok(()) => {}
        Err(error) if error.kind() == io::ErrorKind::NotFound => {}
        Err(error) => return Err(format!("{} could not be removed ({error})", source.state_file.display())),
    }
    Ok(removed)
}

/// The build to launch: the packaged build, or the installed one once its install has finished.
pub fn build(loaded: &LoadedConfig) -> Result<GameBuild, String> {
    match &loaded.game {
        GameSource::Packaged(manifest_path) => manifest::load(manifest_path).map_err(|error| error.to_string()),
        GameSource::Install(source) => {
            let (folder, record) = record(source)?;
            match record {
                Some(InstallRecord {
                    installed: Some(_),
                    target: None,
                    ..
                }) => manifest::load(&folder.path.join(BUILD_MANIFEST_FILE_NAME)).map_err(|error| error.to_string()),
                Some(InstallRecord { target: Some(_), .. }) => Err(format!(
                    "Veyra's install in {} is unfinished. Open the launcher to finish it",
                    folder.path.display()
                )),
                _ => Err("Veyra is not installed yet. Open the launcher to install it".to_string()),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    fn source(name: &str) -> InstallSource {
        let root = std::env::temp_dir().join(format!("veyra-game-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        InstallSource {
            releases_url: "http://127.0.0.1:8090".to_string(),
            channel: "local".to_string(),
            default_folder: root.join("Game"),
            state_file: root.join("LauncherState.json"),
            parallel_downloads: 2,
            download_attempts: 2,
            download_timeout: Duration::from_secs(5),
        }
    }

    #[test]
    fn the_default_folder_until_the_player_chooses() {
        let source = source("folder");
        assert_eq!(
            folder(&source).unwrap(),
            GameFolder {
                path: source.default_folder.clone(),
                chosen: false
            }
        );
        let chosen = source.state_file.with_file_name("Elsewhere");
        save_state(&source.state_file, &chosen).unwrap();
        assert_eq!(folder(&source).unwrap(), GameFolder { path: chosen, chosen: true });
    }

    #[test]
    fn a_bad_state_file_is_an_error_not_a_default() {
        let source = source("bad-state");
        fs::write(&source.state_file, r#"{"schemaVersion":1,"gameFolder":"x","extra":1}"#).unwrap();
        assert!(folder(&source).is_err());
    }

    #[test]
    fn nothing_launches_before_the_install_finishes() {
        let source = source("build");
        let loaded = LoadedConfig {
            config: crate::config::parse(
                r#"{"schemaVersion":2,"backend":{"baseUrl":"http://127.0.0.1:8080"},"http":{"timeoutSeconds":1},
                "launch":{"awaitReadySeconds":1,"awaitSignInSeconds":1},"game":{"buildManifest":"x","arguments":[]}}"#,
            )
            .unwrap(),
            game: GameSource::Install(source),
        };
        assert!(build(&loaded).unwrap_err().contains("not installed yet"));
    }
}
