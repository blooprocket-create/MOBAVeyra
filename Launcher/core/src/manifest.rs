//! A game build's manifest, VeyraBuild.json, which Game/Scripts/Package.ps1 writes beside the
//! packaged client: its build version, which a launch code is bound to (ADR-005 L3), and where its
//! executable is. Parsed strictly, like the configuration.

use serde::Deserialize;
use std::fmt;
use std::fs;
use std::path::{Component, Path, PathBuf};

/// The manifest format this launcher reads.
pub const SCHEMA_VERSION: u32 = 1;

/// The longest build version the backend accepts.
const MAX_BUILD_VERSION_LENGTH: usize = 64;

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct ManifestFile {
    schema_version: u32,
    build_version: String,
    /// The game's executable, relative to the manifest's folder.
    executable: String,
}

/// A game build the launcher can start.
#[derive(Debug, Clone)]
pub struct GameBuild {
    pub version: String,
    pub executable: PathBuf,
}

#[derive(Debug)]
pub struct ManifestError {
    pub path: PathBuf,
    pub problems: Vec<String>,
}

impl fmt::Display for ManifestError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "the game build at {} cannot be used: {}", self.path.display(), self.problems.join("; "))
    }
}

impl std::error::Error for ManifestError {}

/// Reads the manifest at `path` and checks that its executable exists.
pub fn load(path: &Path) -> Result<GameBuild, ManifestError> {
    let fail = |problems: Vec<String>| ManifestError {
        path: path.to_path_buf(),
        problems,
    };
    let text = fs::read_to_string(path).map_err(|error| {
        fail(vec![format!(
            "its manifest could not be read ({error}); package the client with Game/Scripts/Package.ps1"
        )])
    })?;
    let file: ManifestFile = serde_json::from_str(&text).map_err(|error| fail(vec![format!("its manifest is not a build manifest: {error}")]))?;
    let mut problems = Vec::new();
    if file.schema_version != SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {SCHEMA_VERSION}"));
    }
    if !is_build_version(&file.build_version) {
        problems.push("buildVersion must be 1 to 64 letters, digits, '.', '_', '+' or '-'".to_string());
    }
    let relative = Path::new(&file.executable);
    if file.executable.is_empty() || relative.is_absolute() || relative.components().any(|part| !matches!(part, Component::Normal(_))) {
        problems.push("executable must be a path inside the build's folder".to_string());
    }
    let executable = path.parent().unwrap_or_else(|| Path::new(".")).join(relative);
    if problems.is_empty() && !executable.is_file() {
        problems.push(format!("its executable {} does not exist", executable.display()));
    }
    if !problems.is_empty() {
        return Err(fail(problems));
    }
    Ok(GameBuild {
        version: file.build_version,
        executable,
    })
}

/// A build version the backend accepts (Backend/internal/identity).
pub fn is_build_version(text: &str) -> bool {
    !text.is_empty()
        && text.len() <= MAX_BUILD_VERSION_LENGTH
        && text
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'.' | b'_' | b'+' | b'-'))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn folder(name: &str) -> PathBuf {
        let folder = std::env::temp_dir().join(format!("veyra-manifest-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        fs::create_dir_all(folder.join("Bin")).expect("create the build folder");
        fs::write(folder.join("Bin/Game.exe"), b"").expect("write the executable");
        folder
    }

    #[test]
    fn reads_a_build_and_resolves_its_executable() {
        let root = folder("good");
        fs::write(
            root.join("VeyraBuild.json"),
            r#"{"schemaVersion":1,"buildVersion":"0.1.0","executable":"Bin/Game.exe"}"#,
        )
        .expect("write");
        let build = load(&root.join("VeyraBuild.json")).expect("a valid manifest");
        assert_eq!(build.version, "0.1.0");
        assert_eq!(build.executable, root.join("Bin/Game.exe"));
    }

    #[test]
    fn refuses_an_executable_outside_the_build_or_missing() {
        let root = folder("bad");
        for executable in ["../Game.exe", "C:/Windows/notepad.exe", "Bin/Missing.exe"] {
            fs::write(
                root.join("VeyraBuild.json"),
                format!(r#"{{"schemaVersion":1,"buildVersion":"0.1.0","executable":"{executable}"}}"#),
            )
            .expect("write");
            assert!(load(&root.join("VeyraBuild.json")).is_err(), "{executable}");
        }
        fs::write(
            root.join("VeyraBuild.json"),
            r#"{"schemaVersion":1,"buildVersion":"0.1 beta","executable":"Bin/Game.exe","extra":1}"#,
        )
        .expect("write");
        assert!(load(&root.join("VeyraBuild.json")).is_err());
    }

    #[test]
    fn build_versions_match_the_backends() {
        assert!(is_build_version("1.2.3+ci_4-rc"));
        assert!(!is_build_version(""));
        assert!(!is_build_version("1.0 beta"));
        assert!(!is_build_version(&"1".repeat(65)));
    }
}
