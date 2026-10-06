//! The launcher updating itself (ADR-022 §11, ADR-005 L2). An installed launcher asks the release
//! store it installs the game from for its own release on the same channel. When the channel names
//! another version, the launcher downloads that Veyra Setup, checks it against the hash the channel
//! names, starts it silently with `/RELAUNCH` and closes, so Setup can replace it and open the new one.
//!
//! The Setup it downloaded stays in the update folder under its hash. If the launcher opens again
//! still the old version with that same Setup there, the update did not take, and it does not run
//! Setup again: that would be a loop on every open. A launcher that is current clears the folder.

use crate::release::{LauncherChannel, PART_SUFFIX};
use crate::releases::{ReleaseError, ReleaseServer};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

/// This launcher's version: its workspace's (Launcher/Cargo.toml), as Setup and the publisher name it.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// The switches Setup is started with to update the launcher: silent, then opening the launcher it
/// installed (ADR-022 §2, §11).
pub const SETUP_ARGUMENTS: [&str; 2] = ["/S", "/RELAUNCH"];

/// The folder, inside the system's temporary folder, where a downloaded Setup waits.
pub const UPDATE_FOLDER_NAME: &str = "VeyraLauncherUpdate";

/// What the channel asks of this launcher.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Check {
    /// It is the channel's launcher, or the store holds no launcher release.
    Current,
    /// The channel names another launcher, whose Setup has not run yet.
    Update(LauncherChannel),
    /// The channel's Setup has run and this launcher is still the old one: the update did not take.
    Failed(LauncherChannel),
}

/// Where downloaded Setups wait.
pub fn update_folder() -> PathBuf {
    std::env::temp_dir().join(UPDATE_FOLDER_NAME)
}

fn setup_path(folder: &Path, launcher: &LauncherChannel) -> PathBuf {
    folder.join(format!("VeyraSetup-{}.exe", launcher.setup))
}

/// Asks `server` for the launcher on `channel`, as a launcher of `version` whose downloaded Setups are
/// in `folder`.
pub fn check(server: &ReleaseServer, channel: &str, version: &str, folder: &Path) -> Result<Check, ReleaseError> {
    let launcher = match server.launcher(channel) {
        Ok(launcher) => launcher,
        // A store published before launchers updated themselves asks nothing of them.
        Err(ReleaseError::Missing(_)) => return Ok(Check::Current),
        Err(error) => return Err(error),
    };
    if launcher.version == version {
        Ok(Check::Current)
    } else if setup_path(folder, &launcher).is_file() {
        Ok(Check::Failed(launcher))
    } else {
        Ok(Check::Update(launcher))
    }
}

/// Removes the downloaded Setups, once the launcher is current. Best effort: a Setup left behind only
/// takes space in the temporary folder.
pub fn clear(folder: &Path) {
    let _ = fs::remove_dir_all(folder);
}

/// Downloads `launcher`'s Setup into `folder`, checked against its size and hash, and returns it,
/// waiting under its part name. A failed download is tried again, up to `attempts` in all, as a
/// game chunk is (the launcher configuration's `downloadAttempts`). `start_setup` gives it its own name
/// only as it starts it, so the Setup's presence is what `check` reads as the update having been tried.
pub fn download(server: &ReleaseServer, launcher: &LauncherChannel, folder: &Path, attempts: u32) -> Result<PathBuf, String> {
    let mut left = attempts.max(1);
    let bytes = loop {
        match server.setup(launcher) {
            Ok(bytes) => break bytes,
            Err(_) if left > 1 => left -= 1,
            Err(error) => return Err(error.to_string()),
        }
    };
    fs::create_dir_all(folder).map_err(|error| format!("{} could not be created ({error})", folder.display()))?;
    let waiting = folder.join(format!("VeyraSetup-{}.exe{PART_SUFFIX}", launcher.setup));
    fs::write(&waiting, &bytes).map_err(|error| format!("{} could not be written ({error})", waiting.display()))?;
    Ok(waiting)
}

/// Starts the downloaded Setup `waiting` (from `download`) to update the launcher, and returns where
/// it now is. The launcher must close straight after, so Setup can replace it; Setup waits for it to go
/// (Launcher/setup/VeyraSetup.nsi). A Setup the system will not start goes back under its part name, or
/// failing that is removed: only one that started may read as tried, so the next open tries again.
pub fn start_setup(waiting: &Path) -> io::Result<PathBuf> {
    let name = waiting
        .file_name()
        .and_then(|name| name.to_str())
        .and_then(|name| name.strip_suffix(PART_SUFFIX))
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "not a downloaded Setup"))?;
    let path = waiting.with_file_name(name);
    fs::rename(waiting, &path)?;
    let started = Command::new(&path)
        .args(SETUP_ARGUMENTS)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn();
    if let Err(error) = started {
        if fs::rename(&path, waiting).is_err() {
            let _ = fs::remove_file(&path);
        }
        return Err(error);
    }
    Ok(path)
}
