//! The release format (ADR-022 §3): what a release store holds, and the rules every channel file and
//! manifest must keep. `veyra-publish` writes it and the launcher reads it, so both share these types
//! and checks. Parsed strictly: every field is required and an unknown field is an error.
//!
//! A store is a static file tree:
//!
//! - `channels/<channel>.json`: the channel's current release, naming its manifest by hash;
//! - `manifests/<sha256>.json`: a release manifest, addressed by the SHA-256 of its bytes;
//! - `chunks/<first two hex digits>/<sha256>`: a zstd frame, addressed by the SHA-256 of the
//!   uncompressed chunk;
//! - `launcher/<channel>.json`: the launcher's own release on the channel, naming Veyra Setup by hash
//!   (ADR-022 §11);
//! - `setups/<sha256>.exe`: a Veyra Setup, addressed by the SHA-256 of its bytes; and
//!   `setup/VeyraSetup-<version>-<channel>.exe`, the same file under the name a person downloads.

use crate::manifest;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::HashSet;
use std::io::Read as _;

/// The channel file format this launcher reads and writes.
pub const CHANNEL_SCHEMA_VERSION: u32 = 1;

/// The manifest format this launcher reads and writes.
pub const MANIFEST_SCHEMA_VERSION: u32 = 1;

/// The largest chunk the format allows: FastCDC's largest maximum (16 MiB). A protocol limit, so a
/// manifest cannot make the launcher hold an unbounded chunk in memory; not a tuning value.
pub const MAX_CHUNK_BYTES: u64 = 16 * 1024 * 1024;

/// The largest manifest the format allows (64 MiB): a hundred-gigabyte build at the smallest average
/// chunk size stays well inside it. A protocol limit.
pub const MAX_MANIFEST_BYTES: u64 = 64 * 1024 * 1024;

/// The largest channel file the format allows (64 KiB); it holds three short fields. A protocol limit.
pub const MAX_CHANNEL_BYTES: u64 = 64 * 1024;

/// The launcher channel format this launcher reads and writes (ADR-022 §11).
pub const LAUNCHER_CHANNEL_SCHEMA_VERSION: u32 = 1;

/// The largest Veyra Setup the format allows (256 MiB): Setup carries only the launcher, a few
/// megabytes, so this bounds what a launcher holds in memory while it checks one. A protocol limit.
pub const MAX_SETUP_BYTES: u64 = 256 * 1024 * 1024;

/// The longest channel name.
const MAX_CHANNEL_NAME_LENGTH: usize = 32;

/// The install record the launcher keeps in a game folder (ADR-022 §5). No release may ship a file
/// of this name at its root.
pub const RECORD_FILE_NAME: &str = "VeyraInstall.json";

/// The suffix of a file the launcher is still writing. No release may ship a path ending in it.
pub const PART_SUFFIX: &str = ".veyra-part";

/// The build manifest that `Game/Scripts/Package.ps1` writes; every release ships it at its root, so
/// an installed game launches as a packaged build does.
pub const BUILD_MANIFEST_FILE_NAME: &str = "VeyraBuild.json";

/// A channel's current release.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct Channel {
    pub schema_version: u32,
    pub build_version: String,
    /// The SHA-256 of the manifest file's bytes.
    pub manifest: String,
}

/// The launcher's own release on a channel (ADR-022 §11): the Veyra Setup that installs it.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct LauncherChannel {
    pub schema_version: u32,
    /// The launcher's version, its workspace's (Launcher/Cargo.toml).
    pub version: String,
    /// The SHA-256 of Setup's bytes; Setup is at `setup_object(setup)`.
    pub setup: String,
    /// Setup's size in bytes.
    pub size: u64,
}

/// A release: every file of one build, each as an ordered list of chunks.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ReleaseManifest {
    pub schema_version: u32,
    pub build_version: String,
    /// The game's executable, as its path among `files`.
    pub executable: String,
    pub files: Vec<ReleaseFile>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ReleaseFile {
    /// Relative and `/`-separated (`check_path`).
    pub path: String,
    pub size: u64,
    /// In file order; their sizes add up to `size`.
    pub chunks: Vec<ReleaseChunk>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ReleaseChunk {
    /// The SHA-256 of the uncompressed chunk, in lowercase hex.
    pub hash: String,
    /// The uncompressed size.
    pub size: u64,
    /// The size of its zstd frame in the store.
    pub stored_size: u64,
}

impl ReleaseManifest {
    /// The file at `path`, compared exactly.
    pub fn file(&self, path: &str) -> Option<&ReleaseFile> {
        self.files.iter().find(|file| file.path == path)
    }
}

/// The SHA-256 of `bytes`, in lowercase hex.
pub fn sha256_hex(bytes: &[u8]) -> String {
    let digest = Sha256::digest(bytes);
    let mut text = String::with_capacity(digest.len() * 2);
    for byte in digest {
        text.push_str(&format!("{byte:02x}"));
    }
    text
}

/// A SHA-256 in lowercase hex.
pub fn is_hash(text: &str) -> bool {
    text.len() == 64 && text.bytes().all(|byte| matches!(byte, b'0'..=b'9' | b'a'..=b'f'))
}

/// A channel name: lowercase letters, digits and '-', starting with a letter or digit.
pub fn is_channel_name(text: &str) -> bool {
    !text.is_empty()
        && text.len() <= MAX_CHANNEL_NAME_LENGTH
        && text.bytes().next().is_some_and(|byte| byte.is_ascii_lowercase() || byte.is_ascii_digit())
        && text.bytes().all(|byte| byte.is_ascii_lowercase() || byte.is_ascii_digit() || byte == b'-')
}

/// Where a channel file lives in a store.
pub fn channel_object(channel: &str) -> String {
    format!("channels/{channel}.json")
}

/// Where a manifest lives in a store.
pub fn manifest_object(hash: &str) -> String {
    format!("manifests/{hash}.json")
}

/// Where a chunk lives in a store: under the first two hex digits of its hash (`is_hash`).
pub fn chunk_object(hash: &str) -> String {
    format!("chunks/{}/{hash}", hash.get(..2).unwrap_or_default())
}

/// Where a channel's launcher release lives in a store.
pub fn launcher_channel_object(channel: &str) -> String {
    format!("launcher/{channel}.json")
}

/// Where a Veyra Setup lives in a store, addressed by its hash.
pub fn setup_object(hash: &str) -> String {
    format!("setups/{hash}.exe")
}

/// Where a channel's Veyra Setup is kept under the name a person downloads.
pub fn setup_download_object(version: &str, channel: &str) -> String {
    format!("setup/VeyraSetup-{version}-{channel}.exe")
}

/// Parses and validates a launcher channel file.
pub fn parse_launcher_channel(bytes: &[u8]) -> Result<LauncherChannel, Vec<String>> {
    let channel: LauncherChannel = serde_json::from_slice(bytes).map_err(|error| vec![format!("it is not a launcher channel file: {error}")])?;
    let mut problems = Vec::new();
    if channel.schema_version != LAUNCHER_CHANNEL_SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {LAUNCHER_CHANNEL_SCHEMA_VERSION}"));
    }
    if !manifest::is_build_version(&channel.version) {
        problems.push("version is not a version".to_string());
    }
    if !is_hash(&channel.setup) {
        problems.push("setup must be a SHA-256 in lowercase hex".to_string());
    }
    if channel.size == 0 || channel.size > MAX_SETUP_BYTES {
        problems.push(format!("size must be 1 to {MAX_SETUP_BYTES} bytes"));
    }
    if problems.is_empty() {
        Ok(channel)
    } else {
        Err(problems)
    }
}

/// The largest zstd frame a chunk of `size` bytes may be stored as: zstd's own bound.
pub fn max_stored_size(size: u64) -> u64 {
    zstd::zstd_safe::compress_bound(size as usize) as u64
}

/// Decompresses a stored chunk and checks it is the chunk the manifest names. Never produces more
/// than the chunk's declared size, whatever the frame says.
pub fn decode_chunk(stored: &[u8], chunk: &ReleaseChunk) -> Result<Vec<u8>, String> {
    if stored.len() as u64 != chunk.stored_size {
        return Err(format!("it is {} bytes, not {}", stored.len(), chunk.stored_size));
    }
    let decoder = zstd::stream::read::Decoder::new(stored).map_err(|error| format!("it is not a zstd frame ({error})"))?;
    let mut bytes = Vec::with_capacity(chunk.size as usize);
    decoder
        .take(chunk.size + 1)
        .read_to_end(&mut bytes)
        .map_err(|error| format!("it does not decompress ({error})"))?;
    if bytes.len() as u64 != chunk.size {
        return Err(format!("it decompresses to the wrong size ({} bytes, not {})", bytes.len(), chunk.size));
    }
    if sha256_hex(&bytes) != chunk.hash {
        return Err("its contents do not match its hash".to_string());
    }
    Ok(bytes)
}

/// Parses and validates a channel file.
pub fn parse_channel(bytes: &[u8]) -> Result<Channel, Vec<String>> {
    let channel: Channel = serde_json::from_slice(bytes).map_err(|error| vec![format!("it is not a channel file: {error}")])?;
    let mut problems = Vec::new();
    if channel.schema_version != CHANNEL_SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {CHANNEL_SCHEMA_VERSION}"));
    }
    if !manifest::is_build_version(&channel.build_version) {
        problems.push("buildVersion is not a build version".to_string());
    }
    if !is_hash(&channel.manifest) {
        problems.push("manifest must be a SHA-256 in lowercase hex".to_string());
    }
    if problems.is_empty() {
        Ok(channel)
    } else {
        Err(problems)
    }
}

/// Parses and validates a manifest.
pub fn parse_manifest(bytes: &[u8]) -> Result<ReleaseManifest, Vec<String>> {
    let manifest: ReleaseManifest = serde_json::from_slice(bytes).map_err(|error| vec![format!("it is not a release manifest: {error}")])?;
    let problems = validate_manifest(&manifest);
    if problems.is_empty() {
        Ok(manifest)
    } else {
        Err(problems)
    }
}

/// Every problem with a manifest; empty when the launcher may install it. The first few problems
/// are named, since a bad manifest usually has thousands.
pub fn validate_manifest(manifest: &ReleaseManifest) -> Vec<String> {
    const NAMED_PROBLEMS: usize = 8;
    let mut problems = Vec::new();
    if manifest.schema_version != MANIFEST_SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {MANIFEST_SCHEMA_VERSION}"));
    }
    if !manifest::is_build_version(&manifest.build_version) {
        problems.push("buildVersion is not a build version".to_string());
    }
    let mut folded = HashSet::new();
    for file in &manifest.files {
        if let Err(problem) = check_path(&file.path) {
            problems.push(format!("{}: {problem}", file.path));
            continue;
        }
        if !folded.insert(fold(&file.path)) {
            problems.push(format!("{}: another file has the same path, ignoring case", file.path));
        }
        let mut total = 0u64;
        for chunk in &file.chunks {
            if !is_hash(&chunk.hash) {
                problems.push(format!("{}: a chunk's hash is not a SHA-256 in lowercase hex", file.path));
            }
            if chunk.size == 0 || chunk.size > MAX_CHUNK_BYTES {
                problems.push(format!("{}: a chunk is {} bytes; chunks are 1 to {MAX_CHUNK_BYTES}", file.path, chunk.size));
            }
            if chunk.stored_size == 0 || chunk.stored_size > max_stored_size(chunk.size) {
                problems.push(format!("{}: a chunk's stored size is impossible for its size", file.path));
            }
            total = total.saturating_add(chunk.size);
        }
        if total != file.size {
            problems.push(format!("{}: its chunks add up to {total} bytes, not {}", file.path, file.size));
        }
    }
    // No file may sit where another file needs a folder.
    for file in &manifest.files {
        let components: Vec<&str> = file.path.split('/').collect();
        for depth in 1..components.len() {
            if folded.contains(&fold(&components[..depth].join("/"))) {
                problems.push(format!("{}: {} is a file, not a folder", file.path, components[..depth].join("/")));
            }
        }
    }
    if manifest.file(&manifest.executable).is_none() {
        problems.push(format!("executable {} is not one of the release's files", manifest.executable));
    }
    if manifest.file(BUILD_MANIFEST_FILE_NAME).is_none() {
        problems.push(format!("the release has no {BUILD_MANIFEST_FILE_NAME} at its root"));
    }
    if problems.len() > NAMED_PROBLEMS {
        let more = problems.len() - NAMED_PROBLEMS;
        problems.truncate(NAMED_PROBLEMS);
        problems.push(format!("and {more} more"));
    }
    problems
}

/// A path as Windows compares it: ignoring case.
fn fold(path: &str) -> String {
    path.to_lowercase()
}

/// Checks a release path (ADR-022 §3): relative and `/`-separated; no component empty, `.` or `..`;
/// no character or name Windows reserves, and no component ending in a dot or a space; not one of
/// the launcher's own names. A manifest decides where the launcher writes, so this is checked before
/// anything is written, and again before anything is deleted.
pub fn check_path(path: &str) -> Result<(), String> {
    const RESERVED_CHARACTERS: &[char] = &['<', '>', ':', '"', '\\', '|', '?', '*'];
    const RESERVED_NAMES: &[&str] = &["con", "prn", "aux", "nul", "conin$", "conout$"];
    const NUMBERED_RESERVED_NAMES: &[&str] = &["com", "lpt"];
    if path.is_empty() {
        return Err("the path is empty".to_string());
    }
    if path.eq_ignore_ascii_case(RECORD_FILE_NAME) {
        return Err(format!("{RECORD_FILE_NAME} is the launcher's own file"));
    }
    if path.to_ascii_lowercase().ends_with(PART_SUFFIX) {
        return Err(format!("{PART_SUFFIX} is the launcher's own suffix"));
    }
    for component in path.split('/') {
        if component.is_empty() || component == "." || component == ".." {
            return Err("the path must be relative, with no empty, '.' or '..' part".to_string());
        }
        if component.chars().any(|c| c.is_control() || RESERVED_CHARACTERS.contains(&c)) {
            return Err("the path holds a character Windows does not allow".to_string());
        }
        if component.ends_with('.') || component.ends_with(' ') {
            return Err("a part of the path ends in a dot or a space".to_string());
        }
        let stem = component.split('.').next().unwrap_or(component).trim_end().to_ascii_lowercase();
        let numbered = NUMBERED_RESERVED_NAMES.iter().any(|name| {
            stem.strip_prefix(name)
                .is_some_and(|rest| matches!(rest, "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "¹" | "²" | "³"))
        });
        if RESERVED_NAMES.contains(&stem.as_str()) || numbered {
            return Err(format!("{component} is a name Windows reserves"));
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn chunk_of(bytes: &[u8]) -> (ReleaseChunk, Vec<u8>) {
        let stored = zstd::bulk::compress(bytes, 1).expect("compress");
        let chunk = ReleaseChunk {
            hash: sha256_hex(bytes),
            size: bytes.len() as u64,
            stored_size: stored.len() as u64,
        };
        (chunk, stored)
    }

    fn manifest_with(paths: &[&str]) -> ReleaseManifest {
        let (chunk, _) = chunk_of(b"veyra");
        let mut files: Vec<ReleaseFile> = paths
            .iter()
            .map(|path| ReleaseFile {
                path: path.to_string(),
                size: chunk.size,
                chunks: vec![chunk.clone()],
            })
            .collect();
        files.push(ReleaseFile {
            path: BUILD_MANIFEST_FILE_NAME.to_string(),
            size: chunk.size,
            chunks: vec![chunk],
        });
        ReleaseManifest {
            schema_version: MANIFEST_SCHEMA_VERSION,
            build_version: "0.1.0".to_string(),
            executable: paths.first().unwrap_or(&BUILD_MANIFEST_FILE_NAME).to_string(),
            files,
        }
    }

    #[test]
    fn hashes_are_sha256_in_lowercase_hex() {
        assert_eq!(sha256_hex(b"abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert!(is_hash(&sha256_hex(b"")));
        assert!(!is_hash(&sha256_hex(b"").to_uppercase()));
        assert!(!is_hash("abc"));
        assert_eq!(chunk_object(&sha256_hex(b"abc")), format!("chunks/ba/{}", sha256_hex(b"abc")));
    }

    #[test]
    fn a_launcher_channel_is_strict_and_names_setup_by_hash() {
        let hash = sha256_hex(b"setup");
        let good = format!(r#"{{ "schemaVersion": 1, "version": "0.2.0", "setup": "{hash}", "size": 5 }}"#);
        let channel = parse_launcher_channel(good.as_bytes()).expect("a launcher channel");
        assert_eq!(channel.version, "0.2.0");
        assert_eq!(setup_object(&channel.setup), format!("setups/{hash}.exe"));
        assert_eq!(launcher_channel_object("public"), "launcher/public.json");
        assert_eq!(setup_download_object("0.2.0", "public"), "setup/VeyraSetup-0.2.0-public.exe");
        check_path(&setup_download_object("0.2.0", "public")).expect("a download name is a store path");

        let bad = format!(r#"{{ "schemaVersion": 2, "version": "0 2", "setup": "{}", "size": 0 }}"#, hash.to_uppercase());
        assert_eq!(parse_launcher_channel(bad.as_bytes()).unwrap_err().len(), 4);
        let too_big = good.replace(r#""size": 5"#, &format!(r#""size": {}"#, MAX_SETUP_BYTES + 1));
        assert!(parse_launcher_channel(too_big.as_bytes()).is_err());
        let extra = good.replace(r#""size": 5"#, r#""size": 5, "extra": 1"#);
        assert!(parse_launcher_channel(extra.as_bytes()).is_err());
    }

    #[test]
    fn a_chunk_decodes_only_to_itself() {
        let (chunk, stored) = chunk_of(b"the meridian crucible");
        assert_eq!(decode_chunk(&stored, &chunk).unwrap(), b"the meridian crucible");

        let (other, other_stored) = chunk_of(b"another chunk entirely");
        let wrong_hash = ReleaseChunk {
            hash: other.hash.clone(),
            ..chunk.clone()
        };
        assert!(decode_chunk(&stored, &wrong_hash).unwrap_err().contains("hash"));
        // A frame that inflates past the declared size stops at the size.
        let too_small = ReleaseChunk {
            size: 4,
            stored_size: other_stored.len() as u64,
            ..other
        };
        assert!(decode_chunk(&other_stored, &too_small).unwrap_err().contains("wrong size"));
        assert!(decode_chunk(b"not zstd", &ReleaseChunk { stored_size: 8, ..chunk }).is_err());
    }

    #[test]
    fn channel_files_are_strict() {
        let hash = sha256_hex(b"manifest");
        let good = format!(r#"{{"schemaVersion":1,"buildVersion":"0.1.0","manifest":"{hash}"}}"#);
        assert_eq!(parse_channel(good.as_bytes()).unwrap().manifest, hash);
        assert!(parse_channel(good.replace("0.1.0", "0.1 beta").as_bytes()).is_err());
        assert!(parse_channel(good.replace(&hash, "../manifest").as_bytes()).is_err());
        assert!(parse_channel(good.replace("}", r#","extra":1}"#).as_bytes()).is_err());
        assert!(is_channel_name("local"));
        assert!(is_channel_name("test-realm-2"));
        for bad in ["", "Local", "-x", "a/b", "a.b", &"x".repeat(33)] {
            assert!(!is_channel_name(bad), "{bad}");
        }
    }

    #[test]
    fn a_manifest_is_checked_whole() {
        let manifest = manifest_with(&["Windows/Veyra/Binaries/Win64/VeyraClient.exe", "Windows/Veyra/Content/Paks/a.pak"]);
        assert_eq!(validate_manifest(&manifest), Vec::<String>::new());
        let text = serde_json::to_vec(&manifest).unwrap();
        assert_eq!(parse_manifest(&text).unwrap(), manifest);

        let mut sizes = manifest.clone();
        sizes.files[0].size += 1;
        assert!(validate_manifest(&sizes)[0].contains("add up to"));

        let mut executable = manifest.clone();
        executable.executable = "Missing.exe".to_string();
        assert!(validate_manifest(&executable)[0].contains("not one of the release's files"));

        let mut no_build = manifest.clone();
        no_build.files.pop();
        assert!(validate_manifest(&no_build)[0].contains(BUILD_MANIFEST_FILE_NAME));

        let mut big = manifest;
        big.files[0].chunks[0].size = MAX_CHUNK_BYTES + 1;
        big.files[0].size = MAX_CHUNK_BYTES + 1;
        assert!(validate_manifest(&big)[0].contains("chunks are 1 to"));
    }

    #[test]
    fn paths_are_unique_ignoring_case_and_never_both_file_and_folder() {
        let same = manifest_with(&["Game.exe", "game.EXE"]);
        assert!(validate_manifest(&same).iter().any(|problem| problem.contains("ignoring case")));
        let nested = manifest_with(&["Content", "content/a.pak"]);
        assert!(validate_manifest(&nested).iter().any(|problem| problem.contains("is a file, not a folder")));
    }

    #[test]
    fn paths_cannot_leave_the_folder_or_break_windows() {
        for good in [
            "VeyraClient.exe",
            "Windows/Veyra/Content/Paks/pakchunk0-Windows.pak",
            "a b/c.d.e",
            "Console.txt",
            "COM10.log",
        ] {
            assert_eq!(check_path(good), Ok(()), "{good}");
        }
        for bad in [
            "",
            "/etc/passwd",
            "../outside.exe",
            "a/../../outside.exe",
            "a//b",
            "./a",
            "C:/Windows/notepad.exe",
            "a\\b",
            "a/b?",
            "trailing./x",
            "space /x",
            "NUL",
            "aux.txt",
            "Content/com1.pak",
            "LPT9",
            "VeyraInstall.json",
            "veyrainstall.JSON",
            "Game.exe.veyra-part",
            "tab\there",
        ] {
            assert!(check_path(bad).is_err(), "{bad:?} should be refused");
        }
    }
}
