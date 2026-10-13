//! A short record of each install, update or repair, beside the launcher's state (ADR-022 §5): when,
//! which build, what it downloaded from the release store and reused from disk, and why it failed if it
//! did. One JSON object a line, so anyone can see afterwards what an update truly fetched. The file
//! stays small: once it grows past its limit it is moved aside, keeping one older log.

use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;

use crate::install::{Outcome, Progress};

/// The log, beside the launcher's state file.
pub const LOG_FILE_NAME: &str = "LauncherInstalls.log";

/// Past this size the log is moved aside before its next line, so it never grows without end.
pub const LOG_KEEP_BYTES: u64 = 256 * 1024;

/// One run.
#[derive(Debug, Clone, Default, Serialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct Entry {
    pub at_unix_seconds: u64,
    /// "update" or "repair".
    pub mode: String,
    pub build_version: String,
    pub folder: String,
    pub changed: bool,
    /// Fetched from the release store, as stored: the download. A run that failed keeps what it had fetched.
    pub downloaded_bytes: u64,
    /// Taken from files already on disk; what a finished run reports.
    pub reused_bytes: u64,
    pub rebuilt_files: usize,
    pub removed_files: usize,
    /// The stage a run that failed had reached, as the player reads it; none for a finished run.
    pub stage: Option<String>,
    pub problem: Option<String>,
}

/// The entry for a run in `mode` toward `build_version` in `folder`, at `at`, from what it returned and the last
/// progress it reported: a finished run's outcome; for one that failed after doing work, what it had downloaded, the
/// stage it had reached and why it failed.
pub fn entry_of(at: u64, mode: &str, build_version: &str, folder: &Path, result: Result<&Outcome, String>, last: Option<&Progress>) -> Entry {
    let mut entry = Entry {
        at_unix_seconds: at,
        mode: mode.to_string(),
        build_version: build_version.to_string(),
        folder: folder.display().to_string(),
        ..Entry::default()
    };
    match result {
        Ok(outcome) => {
            entry.changed = outcome.changed;
            entry.downloaded_bytes = outcome.downloaded_bytes;
            entry.reused_bytes = outcome.reused_bytes;
            entry.rebuilt_files = outcome.rebuilt_files;
            entry.removed_files = outcome.removed_files;
        }
        Err(problem) => {
            entry.downloaded_bytes = last.map_or(0, |progress| progress.downloaded_bytes);
            entry.stage = last.map(|progress| progress.stage.describe().to_string());
            entry.problem = Some(problem);
        }
    }
    entry
}

/// The log for a launcher whose state is in `state_file`.
pub fn log_path(state_file: &Path) -> PathBuf {
    state_file.with_file_name(LOG_FILE_NAME)
}

/// Seconds since the Unix epoch now; 0 if the clock is before it.
pub fn now_unix_seconds() -> u64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_secs()).unwrap_or(0)
}

/// Adds `entry` to the log at `path` as one line, moving a log past `LOG_KEEP_BYTES` aside first.
pub fn append(path: &Path, entry: &Entry) -> io::Result<()> {
    if fs::metadata(path).is_ok_and(|metadata| metadata.len() > LOG_KEEP_BYTES) {
        let older = path.with_extension("old.log");
        let _ = fs::remove_file(&older);
        fs::rename(path, &older)?;
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut line = serde_json::to_vec(entry).map_err(io::Error::other)?;
    line.push(b'\n');
    OpenOptions::new().create(true).append(true).open(path)?.write_all(&line)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn folder(name: &str) -> PathBuf {
        let folder = std::env::temp_dir().join(format!("veyra-install-log-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        folder
    }

    #[test]
    fn each_run_is_one_line_beside_the_state() {
        let root = folder("lines");
        let path = log_path(&root.join("LauncherState.json"));
        assert_eq!(path, root.join(LOG_FILE_NAME));
        let first = Entry {
            mode: "update".to_string(),
            build_version: "0.2.6".to_string(),
            changed: true,
            downloaded_bytes: 152_028_102,
            reused_bytes: 317_000_000,
            ..Entry::default()
        };
        let failed = Entry {
            problem: Some("The install was stopped".to_string()),
            ..first.clone()
        };
        append(&path, &first).unwrap();
        append(&path, &failed).unwrap();
        let text = fs::read_to_string(&path).unwrap();
        let lines: Vec<serde_json::Value> = text.lines().map(|line| serde_json::from_str(line).unwrap()).collect();
        assert_eq!(lines.len(), 2);
        assert_eq!(lines[0]["downloadedBytes"], 152_028_102);
        assert_eq!(lines[0]["buildVersion"], "0.2.6");
        assert!(lines[0]["problem"].is_null());
        assert_eq!(lines[1]["problem"], "The install was stopped");
    }

    #[test]
    fn a_run_that_fails_after_downloading_keeps_what_it_fetched() {
        // Its last rename refused, as when the game is running: it had downloaded, and says so.
        let last = Progress {
            stage: crate::install::Stage::Finishing,
            done_bytes: 0,
            total_bytes: 0,
            downloaded_bytes: 152_028_102,
            download_total_bytes: 152_028_102,
        };
        let failed = entry_of(1, "update", "0.2.6", Path::new("Veyra"), Err("the game is running".to_string()), Some(&last));
        assert_eq!(failed.downloaded_bytes, 152_028_102);
        assert_eq!(failed.stage.as_deref(), Some("Finishing"));
        assert_eq!(failed.problem.as_deref(), Some("the game is running"));
        let outcome = Outcome {
            changed: true,
            downloaded_bytes: 7,
            reused_bytes: 9,
            ..Outcome::default()
        };
        let finished = entry_of(1, "update", "0.2.6", Path::new("Veyra"), Ok(&outcome), Some(&last));
        assert_eq!((finished.downloaded_bytes, finished.reused_bytes, finished.stage), (7, 9, None));
    }

    #[test]
    fn a_full_log_is_moved_aside_keeping_one_older() {
        let root = folder("rotate");
        fs::create_dir_all(&root).unwrap();
        let path = root.join(LOG_FILE_NAME);
        fs::write(&path, vec![b'x'; LOG_KEEP_BYTES as usize + 1]).unwrap();
        append(&path, &Entry::default()).unwrap();
        assert_eq!(fs::read_to_string(&path).unwrap().lines().count(), 1, "a fresh log");
        assert_eq!(
            fs::metadata(root.join("LauncherInstalls.old.log")).unwrap().len(),
            LOG_KEEP_BYTES + 1,
            "the full one kept aside"
        );
    }
}
