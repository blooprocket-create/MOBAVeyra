//! Installing, updating, repairing and uninstalling the game (ADR-022 §5).
//!
//! One engine brings a folder to a release. **Update** trusts the install record: a file whose chunks
//! did not change since the installed release is left alone. **Repair** trusts nothing: it re-hashes
//! every file and keeps what verifies. Either way a file that must change is written beside the old
//! one as `<path>.veyra-part`, from, in order:
//!
//! 1. the part itself, kept from an interrupted run;
//! 2. chunks already on disk: the installed files at the offsets the record gives (Update), or the
//!    chunks that verified in place (Repair);
//! 3. the release store.
//!
//! Every chunk is checked against its hash, wherever it came from. Only once every part is whole does
//! the commit rename the parts over the old files and delete what the new release no longer has, so
//! the old game stays playable until then. The record's `target` says a run was under way: the next
//! run repairs, and uninstall knows every file the launcher may have written.

use crate::release::{self, ReleaseChunk, ReleaseFile, ReleaseManifest, PART_SUFFIX, RECORD_FILE_NAME};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeSet, HashMap, HashSet};
use std::fmt;
use std::fs::{self, OpenOptions};
use std::io::{self, Read as _, Seek as _, SeekFrom, Write as _};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::mpsc;
use std::thread;

/// The install record format this launcher reads and writes.
pub const RECORD_SCHEMA_VERSION: u32 = 1;

/// The folder the game goes into when the player picks a folder that already holds other things.
pub const GAME_FOLDER_NAME: &str = "Veyra";

/// Where the engine gets chunks it does not have: a release store. It returns the chunk's stored zstd
/// frame as the store holds it; the engine checks it.
pub trait ChunkStore: Sync {
    fn fetch(&self, chunk: &ReleaseChunk) -> Result<Vec<u8>, String>;
}

/// A release to install: its manifest, and the hash the channel names it by.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct Target {
    pub manifest_hash: String,
    pub manifest: ReleaseManifest,
}

/// `VeyraInstall.json`: what a game folder holds, and what an unfinished run was doing.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct InstallRecord {
    pub schema_version: u32,
    /// The release that is fully installed.
    pub installed: Option<Target>,
    /// The release a run was installing, until its commit finishes.
    pub target: Option<Target>,
    /// Files an interrupted run may have written that neither release lists; the next commit, or
    /// uninstall, removes them.
    pub leftovers: Vec<String>,
}

impl InstallRecord {
    /// Every file the launcher may have written in the folder.
    fn owned_paths(&self) -> BTreeSet<String> {
        let mut paths: BTreeSet<String> = self.leftovers.iter().cloned().collect();
        for release in self.installed.iter().chain(&self.target) {
            paths.extend(release.manifest.files.iter().map(|file| file.path.clone()));
        }
        paths
    }
}

/// What a folder is, for installing into it.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FolderKind {
    Missing,
    Empty,
    /// It holds an install record.
    Install,
    /// It holds other things, or is not a folder.
    Occupied,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Mode {
    Update,
    Repair,
}

/// What a run is doing, for the window and the log.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum Stage {
    Checking,
    Downloading,
    Finishing,
}

impl Stage {
    /// The stage as the player reads it.
    pub fn describe(self) -> &'static str {
        match self {
            Self::Checking => "Checking Veyra's files",
            Self::Downloading => "Downloading Veyra",
            Self::Finishing => "Finishing",
        }
    }
}

/// How far a run has got. `done_bytes` of `total_bytes` of the stage's files are ready.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Progress {
    pub stage: Stage,
    pub done_bytes: u64,
    pub total_bytes: u64,
    pub downloaded_bytes: u64,
}

/// How a run works, from the launcher's configuration.
#[derive(Debug, Clone, Copy)]
pub struct Settings {
    /// Chunks fetched, checked or copied at once.
    pub workers: usize,
    /// Times a chunk is downloaded before the run fails.
    pub attempts: u32,
}

/// What a run did.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Outcome {
    pub build_version: String,
    /// False when the folder already held the release.
    pub changed: bool,
    pub rebuilt_files: usize,
    pub removed_files: usize,
    pub downloaded_bytes: u64,
    pub reused_bytes: u64,
}

#[derive(Debug)]
pub enum InstallError {
    /// The folder cannot hold the game.
    Folder {
        folder: PathBuf,
        problem: String,
    },
    /// The install record cannot be read.
    Record {
        folder: PathBuf,
        problem: String,
    },
    Io {
        path: PathBuf,
        error: io::Error,
    },
    /// A chunk failed every attempt.
    Download {
        path: String,
        problem: String,
    },
    /// A file could not be replaced because it is open: the game is running.
    InUse(PathBuf),
    Cancelled,
}

impl fmt::Display for InstallError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Folder { folder, problem } => write!(f, "Veyra cannot be installed in {}: {problem}", folder.display()),
            Self::Record { folder, problem } => write!(f, "The Veyra install in {} cannot be read: {problem}", folder.display()),
            Self::Io { path, error } if error.kind() == io::ErrorKind::StorageFull => {
                write!(f, "There is not enough free space for Veyra on the drive that holds {}", path.display())
            }
            Self::Io { path, error } => write!(f, "Veyra could not use {} ({error})", path.display()),
            Self::Download { path, problem } => write!(f, "Part of {path} could not be downloaded: {problem}"),
            Self::InUse(path) => write!(f, "{} is in use. Close Veyra, then try again", path.display()),
            Self::Cancelled => write!(f, "The install was stopped"),
        }
    }
}

impl std::error::Error for InstallError {}

fn io_error(path: &Path) -> impl Fn(io::Error) -> InstallError + '_ {
    move |error| {
        if error.kind() == io::ErrorKind::PermissionDenied {
            InstallError::InUse(path.to_path_buf())
        } else {
            InstallError::Io {
                path: path.to_path_buf(),
                error,
            }
        }
    }
}

/// What `folder` is.
pub fn inspect_folder(folder: &Path) -> io::Result<FolderKind> {
    match fs::symlink_metadata(folder) {
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(FolderKind::Missing),
        Err(error) => Err(error),
        Ok(metadata) if !metadata.is_dir() => Ok(FolderKind::Occupied),
        Ok(_) if folder.join(RECORD_FILE_NAME).is_file() => Ok(FolderKind::Install),
        Ok(_) if fs::read_dir(folder)?.next().is_none() => Ok(FolderKind::Empty),
        Ok(_) => Ok(FolderKind::Occupied),
    }
}

/// Where the game goes when the player picks `picked`: the folder itself if it is missing, empty or a
/// Veyra install; otherwise `<picked>/Veyra`, which must be one of those.
pub fn resolve_install_folder(picked: &Path) -> Result<PathBuf, InstallError> {
    let usable = |folder: &Path| -> Result<bool, InstallError> {
        let kind = inspect_folder(folder).map_err(|error| InstallError::Folder {
            folder: folder.to_path_buf(),
            problem: error.to_string(),
        })?;
        Ok(kind != FolderKind::Occupied)
    };
    if usable(picked)? {
        return Ok(picked.to_path_buf());
    }
    let inner = picked.join(GAME_FOLDER_NAME);
    if usable(&inner)? {
        return Ok(inner);
    }
    Err(InstallError::Folder {
        folder: inner,
        problem: "the folder holds other files. Choose an empty folder".to_string(),
    })
}

/// The bytes a run would download to go from `installed` to `target`: every chunk the installed
/// release lacks, once.
pub fn download_size(target: &ReleaseManifest, installed: Option<&ReleaseManifest>) -> u64 {
    let have: HashSet<&str> = installed
        .map(|manifest| manifest.files.iter().flat_map(|file| &file.chunks).map(|chunk| chunk.hash.as_str()).collect())
        .unwrap_or_default();
    let mut counted = HashSet::new();
    target
        .files
        .iter()
        .flat_map(|file| &file.chunks)
        .filter(|chunk| !have.contains(chunk.hash.as_str()) && counted.insert(chunk.hash.as_str()))
        .map(|chunk| chunk.stored_size)
        .sum()
}

/// The space the installed game takes.
pub fn installed_size(manifest: &ReleaseManifest) -> u64 {
    manifest.files.iter().map(|file| file.size).sum()
}

/// The install record in `folder`: none if there is no record; an error if it cannot be used.
pub fn load_record(folder: &Path) -> Result<Option<InstallRecord>, String> {
    let path = folder.join(RECORD_FILE_NAME);
    let bytes = match fs::read(&path) {
        Ok(bytes) => bytes,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error.to_string()),
    };
    let record: InstallRecord = serde_json::from_slice(&bytes).map_err(|error| format!("it is not an install record: {error}"))?;
    let mut problems = Vec::new();
    if record.schema_version != RECORD_SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {RECORD_SCHEMA_VERSION}"));
    }
    for release in record.installed.iter().chain(&record.target) {
        if !release::is_hash(&release.manifest_hash) {
            problems.push("a release's manifestHash is not a SHA-256".to_string());
        }
        problems.extend(release::validate_manifest(&release.manifest));
    }
    for path in &record.leftovers {
        if let Err(problem) = release::check_path(path) {
            problems.push(format!("{path}: {problem}"));
        }
    }
    if problems.is_empty() {
        Ok(Some(record))
    } else {
        Err(problems.join("; "))
    }
}

fn write_record(folder: &Path, record: &InstallRecord) -> Result<(), InstallError> {
    let path = folder.join(RECORD_FILE_NAME);
    let temporary = folder.join(format!("{RECORD_FILE_NAME}{PART_SUFFIX}"));
    let bytes = serde_json::to_vec(record).expect("an install record serialises");
    fs::write(&temporary, bytes).map_err(io_error(&temporary))?;
    fs::rename(&temporary, &path).map_err(io_error(&path))
}

/// A release path under `folder`. Paths are checked (`release::check_path`) before they get here.
fn join(folder: &Path, path: &str) -> PathBuf {
    path.split('/').fold(folder.to_path_buf(), |joined, component| joined.join(component))
}

fn part_of(path: &Path) -> PathBuf {
    let mut name = path.as_os_str().to_owned();
    name.push(PART_SUFFIX);
    PathBuf::from(name)
}

fn read_range(path: &Path, offset: u64, length: u64) -> io::Result<Vec<u8>> {
    let mut file = fs::File::open(path)?;
    file.seek(SeekFrom::Start(offset))?;
    let mut bytes = vec![0; length as usize];
    file.read_exact(&mut bytes)?;
    Ok(bytes)
}

fn write_range(path: &Path, offset: u64, bytes: &[u8]) -> Result<(), InstallError> {
    let mut file = OpenOptions::new().write(true).open(path).map_err(io_error(path))?;
    file.seek(SeekFrom::Start(offset)).map_err(io_error(path))?;
    file.write_all(bytes).map_err(io_error(path))
}

/// Whether the bytes at `offset` in `path` are `chunk`.
fn holds(path: &Path, offset: u64, chunk: &ReleaseChunk) -> Option<Vec<u8>> {
    read_range(path, offset, chunk.size)
        .ok()
        .filter(|bytes| release::sha256_hex(bytes) == chunk.hash)
}

/// Where a chunk's bytes may already be on disk. Checked when read.
#[derive(Debug, Clone)]
struct Location {
    path: PathBuf,
    offset: u64,
}

/// A file of the release, where it goes, and where each of its chunks starts.
struct Planned<'a> {
    file: &'a ReleaseFile,
    path: PathBuf,
    part: PathBuf,
    offsets: Vec<u64>,
}

impl<'a> Planned<'a> {
    fn new(folder: &Path, file: &'a ReleaseFile) -> Self {
        let path = join(folder, &file.path);
        let offsets = file
            .chunks
            .iter()
            .scan(0u64, |offset, chunk| {
                let start = *offset;
                *offset += chunk.size;
                Some(start)
            })
            .collect();
        Self {
            part: part_of(&path),
            path,
            file,
            offsets,
        }
    }
}

/// One chunk of one planned file.
#[derive(Clone, Copy)]
struct Job {
    file: usize,
    chunk: usize,
}

/// Runs `work` over `jobs` on `workers` threads, handing each result to `done` on this thread. Stops
/// at the first error, or when `cancel` is set, and returns that error.
fn run_jobs<R: Send>(
    jobs: &[Job],
    workers: usize,
    cancel: &AtomicBool,
    work: &(dyn Fn(Job) -> Result<R, InstallError> + Sync),
    done: &mut dyn FnMut(Job, R),
) -> Result<(), InstallError> {
    if jobs.is_empty() {
        return Ok(());
    }
    let next = AtomicUsize::new(0);
    let stop = AtomicBool::new(false);
    let mut failure: Option<InstallError> = None;
    thread::scope(|scope| {
        let (sender, results) = mpsc::channel::<Result<(Job, R), InstallError>>();
        for _ in 0..workers.clamp(1, jobs.len()) {
            let sender = sender.clone();
            let (next, stop) = (&next, &stop);
            scope.spawn(move || loop {
                if stop.load(Ordering::Relaxed) {
                    break;
                }
                let result = if cancel.load(Ordering::Relaxed) {
                    Err(InstallError::Cancelled)
                } else {
                    let Some(&job) = jobs.get(next.fetch_add(1, Ordering::Relaxed)) else { break };
                    work(job).map(|value| (job, value))
                };
                let failed = result.is_err();
                if failed {
                    stop.store(true, Ordering::Relaxed);
                }
                if sender.send(result).is_err() || failed {
                    break;
                }
            });
        }
        drop(sender);
        for result in results {
            match result {
                Ok((job, value)) => done(job, value),
                // The first real error wins over the cancellations that follow it.
                Err(error) => {
                    if failure.as_ref().is_none_or(|first| matches!(first, InstallError::Cancelled)) {
                        failure = Some(error);
                    }
                }
            }
        }
    });
    failure.map_or(Ok(()), Err)
}

/// Where a built chunk came from.
enum Source {
    Resumed,
    Local,
    Downloaded(u64),
}

/// Brings `folder` to `target` (ADR-022 §5). `progress` is called on this thread as work finishes;
/// setting `cancel` stops the run, keeping what it wrote for the next.
pub fn install(
    folder: &Path,
    target: &Target,
    store: &dyn ChunkStore,
    settings: Settings,
    mode: Mode,
    cancel: &AtomicBool,
    progress: &mut dyn FnMut(&Progress),
) -> Result<Outcome, InstallError> {
    let kind = inspect_folder(folder).map_err(io_error(folder))?;
    if kind == FolderKind::Occupied {
        return Err(InstallError::Folder {
            folder: folder.to_path_buf(),
            problem: "the folder holds other files. Choose an empty folder".to_string(),
        });
    }
    fs::create_dir_all(folder).map_err(io_error(folder))?;
    let previous = match load_record(folder) {
        Ok(record) => record,
        // A repair rebuilds the record from the release.
        Err(_) if mode == Mode::Repair => None,
        Err(problem) => {
            return Err(InstallError::Record {
                folder: folder.to_path_buf(),
                problem: format!("{problem}. Repair the install"),
            })
        }
    };
    let installed = previous.as_ref().and_then(|record| record.installed.clone());
    let interrupted = previous.as_ref().and_then(|record| record.target.clone());
    let trust = mode == Mode::Update && interrupted.is_none() && installed.is_some();
    let planned: Vec<Planned> = target.manifest.files.iter().map(|file| Planned::new(folder, file)).collect();
    let mut outcome = Outcome {
        build_version: target.manifest.build_version.clone(),
        ..Outcome::default()
    };

    // Up to date: the same release, and every file still there at its size.
    let size_matches = |planned: &Planned| fs::metadata(&planned.path).is_ok_and(|metadata| metadata.is_file() && metadata.len() == planned.file.size);
    if trust && installed.as_ref().is_some_and(|release| release.manifest_hash == target.manifest_hash) && planned.iter().all(size_matches) {
        return Ok(outcome);
    }

    // The record says a run is under way before anything is written.
    let mut leftovers: BTreeSet<String> = previous.as_ref().map(|record| record.leftovers.iter().cloned().collect()).unwrap_or_default();
    if let Some(unfinished) = interrupted.as_ref().filter(|release| release.manifest_hash != target.manifest_hash) {
        leftovers.extend(unfinished.manifest.files.iter().map(|file| file.path.clone()));
    }
    let record = InstallRecord {
        schema_version: RECORD_SCHEMA_VERSION,
        installed: installed.clone(),
        target: Some(target.clone()),
        leftovers: leftovers.iter().cloned().collect(),
    };
    write_record(folder, &record)?;

    // Which files must be built, and where their chunks may already be.
    let mut locations: HashMap<String, Location> = HashMap::new();
    let mut build = vec![false; planned.len()];
    if trust {
        let old = &installed.as_ref().expect("trust needs an installed release").manifest;
        for (index, planned) in planned.iter().enumerate() {
            build[index] = old.file(&planned.file.path).is_none_or(|file| file.chunks != planned.file.chunks) || !size_matches(planned);
        }
        for file in &old.files {
            let old_planned = Planned::new(folder, file);
            for (chunk, &offset) in file.chunks.iter().zip(&old_planned.offsets) {
                locations.entry(chunk.hash.clone()).or_insert(Location {
                    path: old_planned.path.clone(),
                    offset,
                });
            }
        }
    } else {
        // Check every file already on disk, chunk by chunk, in place.
        let lengths: Vec<Option<u64>> = planned
            .iter()
            .map(|planned| {
                fs::metadata(&planned.path)
                    .ok()
                    .filter(|metadata| metadata.is_file())
                    .map(|metadata| metadata.len())
            })
            .collect();
        let mut jobs = Vec::new();
        for (file, planned) in planned.iter().enumerate() {
            match lengths[file] {
                None => build[file] = true,
                Some(length) => {
                    build[file] = length != planned.file.size;
                    for (chunk, (entry, &offset)) in planned.file.chunks.iter().zip(&planned.offsets).enumerate() {
                        if offset + entry.size <= length {
                            jobs.push(Job { file, chunk });
                        } else {
                            build[file] = true;
                        }
                    }
                }
            }
        }
        let mut status = Progress {
            stage: Stage::Checking,
            done_bytes: 0,
            total_bytes: jobs.iter().map(|job| planned[job.file].file.chunks[job.chunk].size).sum(),
            downloaded_bytes: 0,
        };
        progress(&status);
        let check = |job: Job| -> Result<bool, InstallError> {
            let planned = &planned[job.file];
            Ok(holds(&planned.path, planned.offsets[job.chunk], &planned.file.chunks[job.chunk]).is_some())
        };
        run_jobs(&jobs, settings.workers, cancel, &check, &mut |job, verified| {
            let planned = &planned[job.file];
            let chunk = &planned.file.chunks[job.chunk];
            if verified {
                locations.entry(chunk.hash.clone()).or_insert(Location {
                    path: planned.path.clone(),
                    offset: planned.offsets[job.chunk],
                });
            } else {
                build[job.file] = true;
            }
            status.done_bytes += chunk.size;
            progress(&status);
        })?;
    }

    // Build every file that must change as a part beside it.
    let mut jobs = Vec::new();
    let mut resumable = vec![false; planned.len()];
    for (file, planned) in planned.iter().enumerate().filter(|(file, _)| build[*file]) {
        if let Some(parent) = planned.part.parent() {
            fs::create_dir_all(parent).map_err(io_error(parent))?;
        }
        resumable[file] = fs::metadata(&planned.part).is_ok_and(|metadata| metadata.is_file() && metadata.len() == planned.file.size);
        let part = OpenOptions::new()
            .create(true)
            .truncate(false)
            .write(true)
            .open(&planned.part)
            .map_err(io_error(&planned.part))?;
        part.set_len(planned.file.size).map_err(io_error(&planned.part))?;
        jobs.extend((0..planned.file.chunks.len()).map(|chunk| Job { file, chunk }));
    }
    let mut status = Progress {
        stage: Stage::Downloading,
        done_bytes: 0,
        total_bytes: jobs.iter().map(|job| planned[job.file].file.chunks[job.chunk].size).sum(),
        downloaded_bytes: 0,
    };
    progress(&status);
    let fetch = |job: Job| -> Result<Source, InstallError> {
        let planned = &planned[job.file];
        let chunk = &planned.file.chunks[job.chunk];
        let offset = planned.offsets[job.chunk];
        if resumable[job.file] && holds(&planned.part, offset, chunk).is_some() {
            return Ok(Source::Resumed);
        }
        if let Some(bytes) = locations.get(&chunk.hash).and_then(|found| holds(&found.path, found.offset, chunk)) {
            write_range(&planned.part, offset, &bytes)?;
            return Ok(Source::Local);
        }
        let mut problem = String::new();
        for _ in 0..settings.attempts.max(1) {
            if cancel.load(Ordering::Relaxed) {
                return Err(InstallError::Cancelled);
            }
            match store.fetch(chunk).and_then(|stored| release::decode_chunk(&stored, chunk)) {
                Ok(bytes) => {
                    write_range(&planned.part, offset, &bytes)?;
                    return Ok(Source::Downloaded(chunk.stored_size));
                }
                Err(failed) => problem = failed,
            }
        }
        Err(InstallError::Download {
            path: planned.file.path.clone(),
            problem,
        })
    };
    run_jobs(&jobs, settings.workers, cancel, &fetch, &mut |job, source| {
        let size = planned[job.file].file.chunks[job.chunk].size;
        match source {
            Source::Resumed | Source::Local => outcome.reused_bytes += size,
            Source::Downloaded(stored) => {
                outcome.downloaded_bytes += stored;
                status.downloaded_bytes += stored;
            }
        }
        status.done_bytes += size;
        progress(&status);
    })?;
    if cancel.load(Ordering::Relaxed) {
        return Err(InstallError::Cancelled);
    }

    // Commit: delete what the release no longer has, then move every part into place.
    progress(&Progress {
        stage: Stage::Finishing,
        done_bytes: 0,
        total_bytes: 0,
        downloaded_bytes: outcome.downloaded_bytes,
    });
    let kept: HashSet<String> = target.manifest.files.iter().map(|file| file.path.to_lowercase()).collect();
    let stale: Vec<String> = record.owned_paths().into_iter().filter(|path| !kept.contains(&path.to_lowercase())).collect();
    for path in &stale {
        if remove_file(&join(folder, path))? {
            outcome.removed_files += 1;
        }
    }
    prune_folders(folder, &stale);
    for planned in planned.iter().enumerate().filter(|(file, _)| build[*file]).map(|(_, planned)| planned) {
        fs::rename(&planned.part, &planned.path).map_err(io_error(&planned.path))?;
        outcome.rebuilt_files += 1;
    }
    remove_parts(folder)?;
    write_record(
        folder,
        &InstallRecord {
            schema_version: RECORD_SCHEMA_VERSION,
            installed: Some(target.clone()),
            target: None,
            leftovers: Vec::new(),
        },
    )?;
    outcome.changed = outcome.rebuilt_files > 0 || outcome.removed_files > 0;
    Ok(outcome)
}

/// What an uninstall removed.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Uninstalled {
    pub removed_files: usize,
    /// Whether the folder itself went: false when something the launcher did not write is still in it.
    pub removed_folder: bool,
}

/// Removes the game from `folder`: only the files its install record lists, leftover parts and the
/// record, then any folder left empty. Anything else stays. A folder with no record is refused.
pub fn uninstall(folder: &Path) -> Result<Uninstalled, InstallError> {
    match inspect_folder(folder).map_err(io_error(folder))? {
        FolderKind::Install => {}
        FolderKind::Missing => return Ok(Uninstalled::default()),
        FolderKind::Empty | FolderKind::Occupied => {
            return Err(InstallError::Folder {
                folder: folder.to_path_buf(),
                problem: format!("it holds no {RECORD_FILE_NAME}, so it is not a Veyra install"),
            })
        }
    }
    let record = load_record(folder)
        .map_err(|problem| InstallError::Record {
            folder: folder.to_path_buf(),
            problem,
        })?
        .expect("an install folder has a record");
    let owned: Vec<String> = record.owned_paths().into_iter().collect();
    let mut outcome = Uninstalled::default();
    for path in &owned {
        if remove_file(&join(folder, path))? {
            outcome.removed_files += 1;
        }
    }
    remove_parts(folder)?;
    remove_file(&folder.join(RECORD_FILE_NAME))?;
    prune_folders(folder, &owned);
    outcome.removed_folder = fs::remove_dir(folder).is_ok();
    Ok(outcome)
}

/// Removes a file; false if there was none.
fn remove_file(path: &Path) -> Result<bool, InstallError> {
    match fs::remove_file(path) {
        Ok(()) => Ok(true),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(false),
        Err(error) => Err(io_error(path)(error)),
    }
}

/// Removes every `.veyra-part` file under `folder`: the launcher's own, never a release's (§3).
fn remove_parts(folder: &Path) -> Result<(), InstallError> {
    let mut folders = vec![folder.to_path_buf()];
    while let Some(current) = folders.pop() {
        let entries = match fs::read_dir(&current) {
            Ok(entries) => entries,
            Err(error) if error.kind() == io::ErrorKind::NotFound => continue,
            Err(error) => return Err(io_error(&current)(error)),
        };
        for entry in entries {
            let entry = entry.map_err(io_error(&current))?;
            let kind = entry.file_type().map_err(io_error(&entry.path()))?;
            let path = entry.path();
            if kind.is_dir() {
                folders.push(path);
            } else if kind.is_file() && entry.file_name().to_string_lossy().to_ascii_lowercase().ends_with(PART_SUFFIX) {
                remove_file(&path)?;
            }
        }
    }
    Ok(())
}

/// Removes the folders that held `paths`, deepest first, if they are now empty. Never goes above
/// `folder`.
fn prune_folders(folder: &Path, paths: &[String]) {
    let mut folders = BTreeSet::new();
    for path in paths {
        let components: Vec<&str> = path.split('/').collect();
        for depth in 1..components.len() {
            folders.insert(components[..depth].join("/"));
        }
    }
    let mut deepest_first: Vec<&String> = folders.iter().collect();
    deepest_first.sort_by_key(|path| std::cmp::Reverse(path.matches('/').count()));
    for path in deepest_first {
        let _ = fs::remove_dir(join(folder, path));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::release::{ReleaseChunk, ReleaseFile, BUILD_MANIFEST_FILE_NAME, MANIFEST_SCHEMA_VERSION};
    use std::sync::Mutex;

    /// A release store in memory, counting what it serves.
    #[derive(Default)]
    struct MemoryStore {
        chunks: HashMap<String, Vec<u8>>,
        served: Mutex<Vec<String>>,
        /// Hashes it serves wrong bytes for, as a corrupt store would.
        corrupt: HashSet<String>,
    }

    impl ChunkStore for MemoryStore {
        fn fetch(&self, chunk: &ReleaseChunk) -> Result<Vec<u8>, String> {
            self.served.lock().unwrap().push(chunk.hash.clone());
            let stored = self.chunks.get(&chunk.hash).cloned().ok_or_else(|| "no such chunk".to_string())?;
            if self.corrupt.contains(&chunk.hash) {
                // Other bytes of the same length, as a flipped bit on the wire would give.
                let mut bytes = zstd::decode_all(stored.as_slice()).unwrap();
                bytes[0] ^= 0xff;
                return Ok(zstd::bulk::compress(&bytes, 1).unwrap());
            }
            Ok(stored)
        }
    }

    impl MemoryStore {
        fn served(&self) -> usize {
            self.served.lock().unwrap().len()
        }
    }

    const CHUNK: usize = 4;

    /// A release of `files` (path, contents), cut into chunks of `CHUNK` bytes, added to `store`.
    fn release(store: &mut MemoryStore, version: &str, files: &[(&str, &[u8])]) -> Target {
        let mut all: Vec<(String, Vec<u8>)> = files.iter().map(|(path, bytes)| (path.to_string(), bytes.to_vec())).collect();
        all.push((BUILD_MANIFEST_FILE_NAME.to_string(), format!("build {version}").into_bytes()));
        let files = all
            .iter()
            .map(|(path, bytes)| ReleaseFile {
                path: path.clone(),
                size: bytes.len() as u64,
                chunks: bytes
                    .chunks(CHUNK)
                    .map(|piece| {
                        let stored = zstd::bulk::compress(piece, 1).unwrap();
                        let chunk = ReleaseChunk {
                            hash: release::sha256_hex(piece),
                            size: piece.len() as u64,
                            stored_size: stored.len() as u64,
                        };
                        store.chunks.insert(chunk.hash.clone(), stored);
                        chunk
                    })
                    .collect(),
            })
            .collect();
        let manifest = ReleaseManifest {
            schema_version: MANIFEST_SCHEMA_VERSION,
            build_version: version.to_string(),
            executable: BUILD_MANIFEST_FILE_NAME.to_string(),
            files,
        };
        assert_eq!(release::validate_manifest(&manifest), Vec::<String>::new());
        Target {
            manifest_hash: release::sha256_hex(&serde_json::to_vec(&manifest).unwrap()),
            manifest,
        }
    }

    fn folder(name: &str) -> PathBuf {
        let folder = std::env::temp_dir().join(format!("veyra-install-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        folder
    }

    const SETTINGS: Settings = Settings { workers: 3, attempts: 2 };

    fn run(folder: &Path, target: &Target, store: &MemoryStore, mode: Mode) -> Result<Outcome, InstallError> {
        let mut last = None;
        let result = install(folder, target, store, SETTINGS, mode, &AtomicBool::new(false), &mut |progress| {
            assert!(progress.done_bytes <= progress.total_bytes, "{progress:?}");
            last = Some(progress.stage);
        });
        if result.as_ref().is_ok_and(|outcome| outcome.changed) {
            assert_eq!(last, Some(Stage::Finishing));
        }
        result
    }

    /// How many times the store served the chunk of `bytes`.
    fn served_of(store: &MemoryStore, bytes: &[u8]) -> usize {
        let hash = release::sha256_hex(bytes);
        store.served.lock().unwrap().iter().filter(|served| **served == hash).count()
    }

    fn read(folder: &Path, path: &str) -> Vec<u8> {
        fs::read(join(folder, path)).unwrap()
    }

    #[test]
    fn installs_a_release_into_an_empty_folder() {
        let mut store = MemoryStore::default();
        let target = release(
            &mut store,
            "1",
            &[("Bin/Game.exe", b"the game itself"), ("Content/a.pak", b"aaaabbbbccccdddd"), ("Empty.txt", b"")],
        );
        let root = folder("fresh");
        let outcome = run(&root, &target, &store, Mode::Update).unwrap();
        assert!(outcome.changed);
        assert_eq!(read(&root, "Bin/Game.exe"), b"the game itself");
        assert_eq!(read(&root, "Content/a.pak"), b"aaaabbbbccccdddd");
        assert_eq!(read(&root, "Empty.txt"), b"");
        assert_eq!(outcome.downloaded_bytes, download_size(&target.manifest, None));
        let record = load_record(&root).unwrap().unwrap();
        assert_eq!(record.installed.unwrap().manifest_hash, target.manifest_hash);
        assert!(record.target.is_none());
        assert!(!root.join("Bin/Game.exe.veyra-part").exists());

        // Again: nothing to do, and nothing downloaded.
        let served = store.served();
        let again = run(&root, &target, &store, Mode::Update).unwrap();
        assert!(!again.changed);
        assert_eq!(store.served(), served);
    }

    #[test]
    fn an_update_downloads_only_what_changed_and_removes_what_went() {
        let mut store = MemoryStore::default();
        let first = release(
            &mut store,
            "1",
            &[
                ("Content/a.pak", b"1111222233334444"),
                ("Content/old.pak", b"gone soon"),
                ("Bin/Game.exe", b"exe!"),
            ],
        );
        let root = folder("update");
        run(&root, &first, &store, Mode::Update).unwrap();
        let second = release(
            &mut store,
            "2",
            &[
                ("Content/a.pak", b"11112222XXXX4444"),
                ("Content/new.pak", b"33334444"),
                ("Bin/Game.exe", b"exe!"),
            ],
        );
        let served = store.served();
        let outcome = run(&root, &second, &store, Mode::Update).unwrap();
        // Two chunks are new: XXXX, and the end of the new build manifest ("build 2" is "buil" and
        // "d 2"). new.pak's chunks come from the old a.pak, and Game.exe is untouched.
        assert_eq!(store.served() - served, 2);
        assert_eq!(served_of(&store, b"XXXX"), 1);
        assert_eq!(read(&root, "Content/a.pak"), b"11112222XXXX4444");
        assert_eq!(read(&root, "Content/new.pak"), b"33334444");
        assert!(!root.join("Content/old.pak").exists());
        assert_eq!(outcome.removed_files, 1);
        assert_eq!(outcome.rebuilt_files, 3);
        assert_eq!(outcome.reused_bytes, 12 + 8 + 4);
    }

    #[test]
    fn repair_rebuilds_only_what_fails() {
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("Content/a.pak", b"1111222233334444"), ("Bin/Game.exe", b"exe!")]);
        let root = folder("repair");
        run(&root, &target, &store, Mode::Update).unwrap();
        fs::write(root.join("Content/a.pak"), b"1111zzzz33334444").unwrap();
        fs::remove_file(root.join("Bin/Game.exe")).unwrap();

        // Update trusts the record: the corruption at the same size goes unseen, the missing file does not.
        let served = store.served();
        run(&root, &target, &store, Mode::Update).unwrap();
        assert_eq!(read(&root, "Bin/Game.exe"), b"exe!");
        assert_eq!(read(&root, "Content/a.pak"), b"1111zzzz33334444");
        assert_eq!(store.served() - served, 1);

        let served = store.served();
        let outcome = run(&root, &target, &store, Mode::Repair).unwrap();
        assert_eq!(read(&root, "Content/a.pak"), b"1111222233334444");
        assert_eq!(store.served() - served, 1, "only the bad chunk is downloaded");
        assert_eq!(outcome.rebuilt_files, 1);
    }

    #[test]
    fn an_interrupted_run_resumes_and_repairs() {
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("Content/a.pak", b"1111222233334444")]);
        let root = folder("resume");
        // A run stopped part-way: the record names its target, and a part holds half the file.
        fs::create_dir_all(root.join("Content")).unwrap();
        write_record(
            &root,
            &InstallRecord {
                schema_version: RECORD_SCHEMA_VERSION,
                installed: None,
                target: Some(target.clone()),
                leftovers: vec!["Content/stray.pak".to_string()],
            },
        )
        .unwrap();
        fs::write(root.join("Content/a.pak.veyra-part"), b"11112222\0\0\0\0\0\0\0\0").unwrap();
        fs::write(root.join("Content/stray.pak"), b"from an older run").unwrap();
        let outcome = run(&root, &target, &store, Mode::Update).unwrap();
        assert_eq!(read(&root, "Content/a.pak"), b"1111222233334444");
        assert_eq!(outcome.reused_bytes, 8, "the part's first half was kept");
        assert!(!root.join("Content/stray.pak").exists());
        assert!(load_record(&root).unwrap().unwrap().leftovers.is_empty());
    }

    #[test]
    fn a_corrupt_store_fails_after_its_attempts_and_keeps_the_old_game() {
        let mut store = MemoryStore::default();
        let first = release(&mut store, "1", &[("Content/a.pak", b"11112222")]);
        let root = folder("corrupt");
        run(&root, &first, &store, Mode::Update).unwrap();
        let second = release(&mut store, "2", &[("Content/a.pak", b"1111BAD!")]);
        store.corrupt.insert(release::sha256_hex(b"BAD!"));
        let error = run(&root, &second, &store, Mode::Update).unwrap_err();
        assert!(matches!(error, InstallError::Download { .. }), "{error}");
        assert!(error.to_string().contains("do not match its hash"), "{error}");
        assert_eq!(served_of(&store, b"BAD!"), SETTINGS.attempts as usize, "every attempt, then no more");
        assert_eq!(read(&root, "Content/a.pak"), b"11112222", "the old game is untouched");
        let record = load_record(&root).unwrap().unwrap();
        assert_eq!(record.installed.unwrap().manifest_hash, first.manifest_hash);
        assert_eq!(record.target.unwrap().manifest_hash, second.manifest_hash);
    }

    #[test]
    fn a_cancelled_run_stops_and_keeps_its_parts() {
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("Content/a.pak", b"1111222233334444")]);
        let root = folder("cancel");
        let error = install(&root, &target, &store, SETTINGS, Mode::Update, &AtomicBool::new(true), &mut |_| {}).unwrap_err();
        assert!(matches!(error, InstallError::Cancelled), "{error}");
        assert!(root.join("Content/a.pak.veyra-part").exists());
        assert!(!root.join("Content/a.pak").exists());
    }

    #[test]
    fn folders_that_hold_other_things_are_not_installed_into() {
        let root = folder("occupied");
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("holiday.jpg"), b"not ours").unwrap();
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("a.pak", b"aaaa")]);
        assert!(matches!(run(&root, &target, &store, Mode::Update), Err(InstallError::Folder { .. })));
        assert_eq!(resolve_install_folder(&root).unwrap(), root.join(GAME_FOLDER_NAME));
        assert_eq!(resolve_install_folder(&root.join("new")).unwrap(), root.join("new"));
        fs::create_dir_all(root.join(GAME_FOLDER_NAME)).unwrap();
        fs::write(root.join(GAME_FOLDER_NAME).join("notes.txt"), b"also not ours").unwrap();
        assert!(resolve_install_folder(&root).is_err());
        assert!(matches!(uninstall(&root), Err(InstallError::Folder { .. })));
        assert!(root.join("holiday.jpg").exists());
    }

    #[test]
    fn uninstall_removes_only_what_the_launcher_wrote() {
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("Bin/Game.exe", b"exe!"), ("Content/Paks/a.pak", b"aaaa")]);
        let root = folder("uninstall");
        run(&root, &target, &store, Mode::Update).unwrap();
        fs::write(root.join("Content/screenshot.png"), b"the player's").unwrap();
        fs::write(root.join("Bin/Game.exe.veyra-part"), b"left over").unwrap();
        let outcome = uninstall(&root).unwrap();
        assert_eq!(outcome.removed_files, 3);
        assert!(!outcome.removed_folder);
        assert!(root.join("Content/screenshot.png").exists());
        assert!(!root.join("Bin").exists());
        assert!(!root.join("Content/Paks").exists());
        assert!(!root.join(RECORD_FILE_NAME).exists());

        fs::remove_dir_all(&root).unwrap();
        run(&root, &target, &store, Mode::Update).unwrap();
        assert!(uninstall(&root).unwrap().removed_folder);
        assert!(!root.exists());
        assert_eq!(uninstall(&root).unwrap(), Uninstalled::default(), "nothing left to remove");
    }

    #[test]
    fn a_tampered_record_is_never_trusted_to_delete() {
        let mut store = MemoryStore::default();
        let target = release(&mut store, "1", &[("a.pak", b"aaaa")]);
        let root = folder("tampered");
        run(&root, &target, &store, Mode::Update).unwrap();
        let text = fs::read_to_string(root.join(RECORD_FILE_NAME))
            .unwrap()
            .replace("\"leftovers\":[]", "\"leftovers\":[\"../outside.txt\"]");
        fs::write(root.join(RECORD_FILE_NAME), text).unwrap();
        assert!(matches!(uninstall(&root), Err(InstallError::Record { .. })));
        assert!(matches!(run(&root, &target, &store, Mode::Update), Err(InstallError::Record { .. })));
        // A repair starts the record afresh.
        run(&root, &target, &store, Mode::Repair).unwrap();
        assert!(load_record(&root).unwrap().is_some());
    }

    #[test]
    fn download_size_counts_each_missing_chunk_once() {
        let mut store = MemoryStore::default();
        let first = release(&mut store, "1", &[("a", b"11112222"), ("b", b"1111")]);
        let stored = |bytes: &[u8]| store.chunks[&release::sha256_hex(bytes)].len() as u64;
        let build_one = stored(b"buil") + stored(b"d 1");
        assert_eq!(download_size(&first.manifest, None), stored(b"1111") + stored(b"2222") + build_one);
        let mut more = MemoryStore::default();
        let second = release(&mut more, "2", &[("a", b"11113333")]);
        let stored_more = |bytes: &[u8]| more.chunks[&release::sha256_hex(bytes)].len() as u64;
        assert_eq!(
            download_size(&second.manifest, Some(&first.manifest)),
            stored_more(b"3333") + stored_more(b"d 2")
        );
        assert_eq!(installed_size(&second.manifest), 8 + 7);
    }
}
