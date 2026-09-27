//! The launcher's configuration (Launcher/config/local.json): where the backend is, how long to wait,
//! and which game build to launch. Parsed strictly: every field is required and an unknown field is
//! an error (ADR-010 §12), and the values are validated before anything uses them.

use serde::Deserialize;
use std::fmt;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::Duration;

/// The configuration format this launcher reads.
pub const SCHEMA_VERSION: u32 = 1;

/// The switch the launcher gives the game itself; configuration never sets it (ADR-005 L3).
pub const LAUNCH_CODE_SWITCH: &str = "-VeyraLaunchCode=stdin";

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct LauncherConfig {
    pub schema_version: u32,
    pub backend: BackendConfig,
    pub http: HttpConfig,
    pub launch: LaunchConfig,
    pub game: GameConfig,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct BackendConfig {
    /// The backend's base URL, such as http://127.0.0.1:8080: a scheme, a host and a port, no path.
    pub base_url: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct HttpConfig {
    /// Seconds before a request to the backend fails.
    pub timeout_seconds: f64,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct LaunchConfig {
    /// Seconds to wait for the started game to ask for its launch code: a cold engine start.
    pub await_ready_seconds: f64,
    /// Seconds to wait, once the code is written, for the game to say it signed in.
    pub await_sign_in_seconds: f64,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct GameConfig {
    /// The game build's manifest (VeyraBuild.json, which Game/Scripts/Package.ps1 writes), relative to
    /// this configuration file.
    pub build_manifest: String,
    /// More arguments for the game, after the launch-code switch the launcher adds itself.
    pub arguments: Vec<String>,
}

/// A configuration that parsed and validated, with its manifest's path resolved.
#[derive(Debug, Clone)]
pub struct LoadedConfig {
    pub config: LauncherConfig,
    pub manifest_path: PathBuf,
}

impl LoadedConfig {
    pub fn http_timeout(&self) -> Duration {
        Duration::from_secs_f64(self.config.http.timeout_seconds)
    }

    pub fn await_ready(&self) -> Duration {
        Duration::from_secs_f64(self.config.launch.await_ready_seconds)
    }

    pub fn await_sign_in(&self) -> Duration {
        Duration::from_secs_f64(self.config.launch.await_sign_in_seconds)
    }
}

/// Why a configuration cannot be used. It names the file and every problem.
#[derive(Debug)]
pub struct ConfigError {
    pub path: PathBuf,
    pub problems: Vec<String>,
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} cannot be used: {}", self.path.display(), self.problems.join("; "))
    }
}

impl std::error::Error for ConfigError {}

/// Reads, parses and validates the configuration at `path`.
pub fn load(path: &Path) -> Result<LoadedConfig, ConfigError> {
    let fail = |problems: Vec<String>| ConfigError {
        path: path.to_path_buf(),
        problems,
    };
    let text = fs::read_to_string(path).map_err(|error| fail(vec![format!("it could not be read: {error}")]))?;
    let config = parse(&text).map_err(fail)?;
    let directory = path.parent().unwrap_or_else(|| Path::new("."));
    let manifest_path = directory.join(&config.game.build_manifest);
    Ok(LoadedConfig { config, manifest_path })
}

/// Parses and validates a configuration's text. Every problem is reported, not just the first.
pub fn parse(text: &str) -> Result<LauncherConfig, Vec<String>> {
    let config: LauncherConfig = serde_json::from_str(text).map_err(|error| vec![format!("it is not a launcher configuration: {error}")])?;
    let problems = validate(&config);
    if problems.is_empty() {
        Ok(config)
    } else {
        Err(problems)
    }
}

/// Every problem with `config`'s values; empty when it can be used.
pub fn validate(config: &LauncherConfig) -> Vec<String> {
    let mut problems = Vec::new();
    if config.schema_version != SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {SCHEMA_VERSION}"));
    }
    if !is_base_url(&config.backend.base_url) {
        problems.push("backend.baseUrl must be an http or https URL with no path, such as http://127.0.0.1:8080".to_string());
    }
    for (name, value) in [
        ("http.timeoutSeconds", config.http.timeout_seconds),
        ("launch.awaitReadySeconds", config.launch.await_ready_seconds),
        ("launch.awaitSignInSeconds", config.launch.await_sign_in_seconds),
    ] {
        if !(value.is_finite() && value > 0.0) {
            problems.push(format!("{name} must be a positive number of seconds"));
        }
    }
    if config.game.build_manifest.trim().is_empty() {
        problems.push("game.buildManifest must name the game build's VeyraBuild.json".to_string());
    }
    if config
        .game
        .arguments
        .iter()
        .any(|argument| argument.to_ascii_lowercase().starts_with("-veyralaunchcode"))
    {
        problems.push(format!(
            "game.arguments must not set the launch code; the launcher adds {LAUNCH_CODE_SWITCH} itself"
        ));
    }
    problems
}

/// An http or https URL with a host, an optional port and nothing else.
pub fn is_base_url(text: &str) -> bool {
    let Some(rest) = text.strip_prefix("http://").or_else(|| text.strip_prefix("https://")) else {
        return false;
    };
    let (host, port) = match rest.split_once(':') {
        Some((host, port)) => (host, Some(port)),
        None => (rest, None),
    };
    let host_ok = !host.is_empty() && host.bytes().all(|byte| byte.is_ascii_alphanumeric() || byte == b'.' || byte == b'-');
    let port_ok = port.is_none_or(|port| !port.is_empty() && port.len() <= 5 && port.bytes().all(|byte| byte.is_ascii_digit()));
    host_ok && port_ok
}

#[cfg(test)]
mod tests {
    use super::*;

    const VALID: &str = r#"{
        "schemaVersion": 1,
        "backend": { "baseUrl": "http://127.0.0.1:8080" },
        "http": { "timeoutSeconds": 10 },
        "launch": { "awaitReadySeconds": 180, "awaitSignInSeconds": 30 },
        "game": { "buildManifest": "../build/VeyraBuild.json", "arguments": ["-windowed"] }
    }"#;

    #[test]
    fn reads_a_complete_configuration() {
        let config = parse(VALID).expect("the example is valid");
        assert_eq!(config.backend.base_url, "http://127.0.0.1:8080");
        assert_eq!(config.launch.await_ready_seconds, 180.0);
        assert_eq!(config.game.arguments, ["-windowed"]);
    }

    #[test]
    fn every_field_is_required() {
        let missing = VALID.replace(r#""http": { "timeoutSeconds": 10 },"#, "");
        let problems = parse(&missing).expect_err("http is missing");
        assert!(problems[0].contains("missing field `http`"), "{problems:?}");
    }

    #[test]
    fn an_unknown_field_is_an_error() {
        let unknown = VALID.replace(r#""timeoutSeconds": 10"#, r#""timeoutSeconds": 10, "retries": 3"#);
        let problems = parse(&unknown).expect_err("retries is unknown");
        assert!(problems[0].contains("unknown field `retries`"), "{problems:?}");
    }

    #[test]
    fn values_are_validated_and_every_problem_named() {
        let bad = VALID
            .replace("http://127.0.0.1:8080", "http://127.0.0.1:8080/v1")
            .replace(r#""awaitReadySeconds": 180"#, r#""awaitReadySeconds": 0"#)
            .replace(r#""schemaVersion": 1"#, r#""schemaVersion": 2"#)
            .replace(r#"["-windowed"]"#, r#"["-VeyraLaunchCode=vlc_x"]"#);
        let problems = parse(&bad).expect_err("four problems");
        assert_eq!(problems.len(), 4, "{problems:?}");
    }

    #[test]
    fn a_base_url_has_no_path_query_or_credentials() {
        assert!(is_base_url("http://127.0.0.1:8080"));
        assert!(is_base_url("https://backend"));
        for bad in [
            "ftp://backend",
            "http://",
            "http://backend/",
            "http://backend:80/v1",
            "http://user@backend",
            "http://backend?x=1",
            "http://backend:",
        ] {
            assert!(!is_base_url(bad), "{bad}");
        }
    }
}
