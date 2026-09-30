//! `veyra-publish`'s work (ADR-022 §3–§4): turning a packaged client into a release in a release
//! store. Every file is cut into content-defined chunks (FastCDC), each chunk named by its SHA-256 and
//! stored as a zstd frame; only chunks the store lacks are written. The manifest follows, then the
//! channel moves to it. Everything goes in under a temporary name and is renamed into place, so a
//! web server serving the store never serves half a file.

use fastcdc::v2020::{self, StreamCDC};
use serde::Deserialize;
use std::collections::HashSet;
use std::fmt;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{mpsc, Mutex};
use std::thread;
use veyra_launcher_core::manifest;
use veyra_launcher_core::release::{self, Channel, ReleaseChunk, ReleaseFile, ReleaseManifest, BUILD_MANIFEST_FILE_NAME};

/// The publisher configuration format this tool reads.
pub const SCHEMA_VERSION: u32 = 1;

/// Launcher/config/publish.json: how releases are cut and compressed. Parsed strictly.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct PublishConfig {
    pub schema_version: u32,
    pub chunks: ChunkSizes,
    /// The zstd level each chunk is compressed at.
    pub zstd_level: i32,
}

/// FastCDC's sizes: no chunk is smaller than `min_bytes` (except a file's last) or larger than
/// `max_bytes`, and they average about `average_bytes`.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ChunkSizes {
    pub min_bytes: u32,
    pub average_bytes: u32,
    pub max_bytes: u32,
}

/// Reads and validates the publisher configuration at `path`.
pub fn load_config(path: &Path) -> Result<PublishConfig, String> {
    let text = fs::read_to_string(path).map_err(|error| format!("{} could not be read: {error}", path.display()))?;
    parse_config(&text).map_err(|problems| format!("{} cannot be used: {}", path.display(), problems.join("; ")))
}

/// Parses and validates a publisher configuration. Every problem is reported.
pub fn parse_config(text: &str) -> Result<PublishConfig, Vec<String>> {
    let config: PublishConfig = serde_json::from_str(text).map_err(|error| vec![format!("it is not a publisher configuration: {error}")])?;
    let mut problems = Vec::new();
    if config.schema_version != SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {SCHEMA_VERSION}"));
    }
    let sizes = &config.chunks;
    let largest = (v2020::MAXIMUM_MAX as u64).min(release::MAX_CHUNK_BYTES) as u32;
    for (name, value, low, high) in [
        ("chunks.minBytes", sizes.min_bytes, v2020::MINIMUM_MIN as u32, v2020::MINIMUM_MAX as u32),
        ("chunks.averageBytes", sizes.average_bytes, v2020::AVERAGE_MIN as u32, v2020::AVERAGE_MAX as u32),
        ("chunks.maxBytes", sizes.max_bytes, v2020::MAXIMUM_MIN as u32, largest),
    ] {
        if !(low..=high).contains(&value) || !value.is_multiple_of(2) {
            problems.push(format!("{name} must be an even number from {low} to {high}"));
        }
    }
    if !(sizes.min_bytes <= sizes.average_bytes && sizes.average_bytes <= sizes.max_bytes) {
        problems.push("chunks must have minBytes ≤ averageBytes ≤ maxBytes".to_string());
    }
    let levels = 1..=zstd::zstd_safe::max_c_level();
    if !levels.contains(&config.zstd_level) {
        problems.push(format!("zstdLevel must be {} to {}", levels.start(), levels.end()));
    }
    if problems.is_empty() {
        Ok(config)
    } else {
        Err(problems)
    }
}

/// What a publish did.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Published {
    pub build_version: String,
    pub manifest_hash: String,
    pub files: usize,
    pub bytes: u64,
    pub chunks: usize,
    /// Chunks the store did not have.
    pub new_chunks: usize,
    pub new_stored_bytes: u64,
}

#[derive(Debug)]
pub struct PublishError(pub String);

impl fmt::Display for PublishError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for PublishError {}

fn failed(path: &Path) -> impl Fn(io::Error) -> PublishError + '_ {
    move |error| PublishError(format!("{}: {error}", path.display()))
}

/// Publishes the packaged client in `build` (a folder holding VeyraBuild.json) to `store`, and moves
/// `channel` to it. `log` hears each file as it is cut.
pub fn publish(build: &Path, store: &Path, channel: &str, config: &PublishConfig, log: &mut dyn FnMut(&str)) -> Result<Published, PublishError> {
    if !release::is_channel_name(channel) {
        return Err(PublishError(format!(
            "{channel} is not a channel name: 1 to 32 lowercase letters, digits or '-'"
        )));
    }
    let game = manifest::load(&build.join(BUILD_MANIFEST_FILE_NAME)).map_err(|error| PublishError(error.to_string()))?;
    let executable = relative(build, &game.executable).ok_or_else(|| PublishError("the build's executable is not inside the build".to_string()))?;
    let paths = files(build)?;
    for path in &paths {
        release::check_path(path).map_err(|problem| PublishError(format!("{path} cannot be released: {problem}")))?;
    }

    let staging = Staging::new(store);
    let (chunks, new_chunks, new_stored_bytes) = cut(build, &paths, config, &staging, log)?;
    let files: Vec<ReleaseFile> = paths
        .iter()
        .zip(chunks)
        .map(|(path, chunks)| ReleaseFile {
            path: path.clone(),
            size: chunks.iter().map(|chunk| chunk.size).sum(),
            chunks,
        })
        .collect();
    let manifest = ReleaseManifest {
        schema_version: release::MANIFEST_SCHEMA_VERSION,
        build_version: game.version.clone(),
        executable,
        files,
    };
    let problems = release::validate_manifest(&manifest);
    if !problems.is_empty() {
        return Err(PublishError(format!("the release would be invalid: {}", problems.join("; "))));
    }
    let bytes = serde_json::to_vec(&manifest).expect("a manifest serialises");
    if bytes.len() as u64 > release::MAX_MANIFEST_BYTES {
        return Err(PublishError("the release's manifest is too large; raise chunks.averageBytes".to_string()));
    }
    let manifest_hash = release::sha256_hex(&bytes);
    staging.put(&release::manifest_object(&manifest_hash), &bytes, false)?;
    let channel_file = Channel {
        schema_version: release::CHANNEL_SCHEMA_VERSION,
        build_version: game.version.clone(),
        manifest: manifest_hash.clone(),
    };
    staging.put(
        &release::channel_object(channel),
        &serde_json::to_vec_pretty(&channel_file).expect("a channel serialises"),
        true,
    )?;
    Ok(Published {
        build_version: game.version,
        manifest_hash,
        files: manifest.files.len(),
        bytes: manifest.files.iter().map(|file| file.size).sum(),
        chunks: manifest.files.iter().map(|file| file.chunks.len()).sum(),
        new_chunks,
        new_stored_bytes,
    })
}

/// `path` relative to `root`, `/`-separated; none if it is not inside.
fn relative(root: &Path, path: &Path) -> Option<String> {
    let inside = path.strip_prefix(root).ok()?;
    let parts: Option<Vec<&str>> = inside.components().map(|component| component.as_os_str().to_str()).collect();
    Some(parts?.join("/"))
}

/// Every file in the build, relative and sorted. Links and names that are not UTF-8 are refused.
fn files(build: &Path) -> Result<Vec<String>, PublishError> {
    let mut found = Vec::new();
    let mut folders = vec![build.to_path_buf()];
    while let Some(folder) = folders.pop() {
        for entry in fs::read_dir(&folder).map_err(failed(&folder))? {
            let entry = entry.map_err(failed(&folder))?;
            let path = entry.path();
            let kind = entry.file_type().map_err(failed(&path))?;
            if kind.is_dir() {
                folders.push(path);
            } else if kind.is_file() {
                found.push(relative(build, &path).ok_or_else(|| PublishError(format!("{} has a name that is not UTF-8", path.display())))?);
            } else {
                return Err(PublishError(format!("{} is a link; a release holds only files", path.display())));
            }
        }
    }
    found.sort();
    Ok(found)
}

/// Cuts every file into chunks on worker threads, storing each chunk the store lacks. Returns each
/// file's chunks in order, how many chunks were new and their stored bytes.
#[allow(clippy::type_complexity)]
fn cut(
    build: &Path,
    paths: &[String],
    config: &PublishConfig,
    staging: &Staging,
    log: &mut dyn FnMut(&str),
) -> Result<(Vec<Vec<ReleaseChunk>>, usize, u64), PublishError> {
    let workers = thread::available_parallelism().map_or(1, |count| count.get());
    let written: Mutex<HashSet<String>> = Mutex::new(HashSet::new());
    let new_stored = AtomicU64::new(0);
    let mut chunks: Vec<Vec<Option<ReleaseChunk>>> = vec![Vec::new(); paths.len()];
    let mut failure = None;
    let (work, pieces) = mpsc::sync_channel::<(usize, usize, Vec<u8>)>(workers * 2);
    let pieces = Mutex::new(pieces);
    thread::scope(|scope| {
        let (sender, results) = mpsc::channel::<Result<(usize, usize, ReleaseChunk), PublishError>>();
        for _ in 0..workers {
            let sender = sender.clone();
            let (pieces, written, new_stored) = (&pieces, &written, &new_stored);
            scope.spawn(move || loop {
                let next = pieces.lock().unwrap().recv();
                let Ok((file, index, data)) = next else { break };
                let result = store_chunk(staging, config.zstd_level, &data, written, new_stored).map(|chunk| (file, index, chunk));
                if sender.send(result).is_err() {
                    break;
                }
            });
        }
        drop(sender);
        // The producer reads the files in order; a failed read ends it.
        let producer = scope.spawn(move || -> Result<Vec<usize>, PublishError> {
            let mut counts = Vec::with_capacity(paths.len());
            for (file, path) in paths.iter().enumerate() {
                let full = path.split('/').fold(build.to_path_buf(), |joined, part| joined.join(part));
                let reader = fs::File::open(&full).map_err(failed(&full))?;
                let sizes = &config.chunks;
                let mut count = 0;
                for piece in StreamCDC::new(reader, sizes.min_bytes as usize, sizes.average_bytes as usize, sizes.max_bytes as usize) {
                    let piece = piece.map_err(|error| PublishError(format!("{}: {error}", full.display())))?;
                    if work.send((file, count, piece.data)).is_err() {
                        return Err(PublishError("the chunk workers stopped".to_string()));
                    }
                    count += 1;
                }
                counts.push(count);
            }
            Ok(counts)
        });
        let mut announced = vec![false; paths.len()];
        for result in results {
            match result {
                Ok((file, index, chunk)) => {
                    if !announced[file] {
                        log(&paths[file]);
                        announced[file] = true;
                    }
                    let slots = &mut chunks[file];
                    if slots.len() <= index {
                        slots.resize(index + 1, None);
                    }
                    slots[index] = Some(chunk);
                }
                Err(error) => {
                    failure.get_or_insert(error);
                }
            }
        }
        match producer.join().expect("the producer does not panic") {
            Ok(counts) => {
                for (file, count) in counts.into_iter().enumerate() {
                    chunks[file].resize(count, None);
                }
            }
            Err(error) => {
                failure.get_or_insert(error);
            }
        }
    });
    if let Some(error) = failure {
        return Err(error);
    }
    let new_chunks = written.into_inner().unwrap().len();
    let chunks = chunks
        .into_iter()
        .map(|slots| slots.into_iter().collect::<Option<Vec<_>>>().expect("every chunk was stored"))
        .collect();
    Ok((chunks, new_chunks, new_stored.into_inner()))
}

/// Hashes a chunk and, if the store lacks it, compresses and writes it.
fn store_chunk(staging: &Staging, level: i32, data: &[u8], written: &Mutex<HashSet<String>>, new_stored: &AtomicU64) -> Result<ReleaseChunk, PublishError> {
    let hash = release::sha256_hex(data);
    let object = release::chunk_object(&hash);
    let path = staging.path(&object);
    let stored_size = match fs::metadata(&path) {
        Ok(metadata) if !written.lock().unwrap().contains(&hash) => metadata.len(),
        _ => {
            let stored = zstd::bulk::compress(data, level).map_err(failed(&path))?;
            if written.lock().unwrap().insert(hash.clone()) {
                staging.put(&object, &stored, true)?;
                new_stored.fetch_add(stored.len() as u64, Ordering::Relaxed);
            }
            stored.len() as u64
        }
    };
    Ok(ReleaseChunk {
        hash,
        size: data.len() as u64,
        stored_size,
    })
}

/// Writes into the store under temporary names, renaming each file into place.
struct Staging {
    root: PathBuf,
    counter: AtomicU64,
}

impl Staging {
    fn new(root: &Path) -> Self {
        Self {
            root: root.to_path_buf(),
            counter: AtomicU64::new(0),
        }
    }

    fn path(&self, object: &str) -> PathBuf {
        object.split('/').fold(self.root.clone(), |joined, part| joined.join(part))
    }

    /// Writes `bytes` at `object`; unless `replace`, an object already there is kept (it is content-addressed).
    fn put(&self, object: &str, bytes: &[u8], replace: bool) -> Result<(), PublishError> {
        let path = self.path(object);
        if !replace && path.is_file() {
            return Ok(());
        }
        let parent = path.parent().expect("an object has a folder");
        fs::create_dir_all(parent).map_err(failed(parent))?;
        let temporary = parent.join(format!(
            ".{}.{}-{}.tmp",
            path.file_name().expect("an object has a name").to_string_lossy(),
            std::process::id(),
            self.counter.fetch_add(1, Ordering::Relaxed)
        ));
        fs::write(&temporary, bytes).map_err(failed(&temporary))?;
        fs::rename(&temporary, &path).map_err(failed(&path))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CONFIG: &str = r#"{ "schemaVersion": 1, "chunks": { "minBytes": 64, "averageBytes": 256, "maxBytes": 1024 }, "zstdLevel": 3 }"#;

    #[test]
    fn the_configuration_is_strict_and_bounded_by_fastcdc() {
        assert!(parse_config(CONFIG).is_ok());
        let bad = CONFIG
            .replace(r#""minBytes": 64"#, r#""minBytes": 63"#)
            .replace(r#""maxBytes": 1024"#, r#""maxBytes": 33554432"#)
            .replace(r#""zstdLevel": 3"#, r#""zstdLevel": 0"#);
        assert_eq!(parse_config(&bad).unwrap_err().len(), 3);
        let inverted = CONFIG.replace(r#""averageBytes": 256"#, r#""averageBytes": 2048"#);
        assert!(parse_config(&inverted)
            .unwrap_err()
            .iter()
            .any(|problem| problem.contains("minBytes ≤ averageBytes")));
        assert!(parse_config(&CONFIG.replace("}, \"zstd", ", \"extra\": 1 }, \"zstd")).is_err());
    }

    #[test]
    fn the_committed_configuration_is_valid() {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../config/publish.json");
        load_config(&path).expect("config/publish.json");
    }

    #[test]
    fn relative_paths_use_forward_slashes() {
        let root = Path::new("/builds/Veyra");
        assert_eq!(relative(root, &root.join("Windows").join("Game.exe")).as_deref(), Some("Windows/Game.exe"));
        assert_eq!(relative(root, Path::new("/elsewhere/Game.exe")), None);
    }
}
