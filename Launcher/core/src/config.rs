//! The launcher's configuration (Launcher/config/local.json, and `VeyraLauncher.json` beside an
//! installed launcher): where the backend is, how long to wait, where the game comes from: a
//! packaged build or a release store (ADR-022 §6), and how players sign in (ADR-038). Parsed
//! strictly: every field is required, except `playerLogin`, whose absence means a development
//! launcher with only the development sign-in; an unknown field is an error (ADR-010 §12), and the
//! values are validated before anything uses them.

use crate::release;
use serde::Deserialize;
use std::fmt;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::Duration;

/// The configuration format this launcher reads.
pub const SCHEMA_VERSION: u32 = 2;

/// The switch the launcher gives the game itself; configuration never sets it (ADR-005 L3).
pub const LAUNCH_CODE_SWITCH: &str = "-VeyraLaunchCode=stdin";

/// The switch that names the backend to the game, which the launcher sets from its own `backend.baseUrl`, so one
/// configuration steers both; configuration never sets it (ADR-057 §5).
pub const BACKEND_URL_SWITCH: &str = "-VeyraBackendUrl=";

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct LauncherConfig {
    pub schema_version: u32,
    pub backend: BackendConfig,
    pub http: HttpConfig,
    pub launch: LaunchConfig,
    pub game: GameConfig,
    /// Player registration and sign-in (ADR-038). Absent: only the development sign-in.
    #[serde(default)]
    pub player_login: Option<PlayerLoginConfig>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct PlayerLoginConfig {
    pub firebase: FirebaseConfig,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct FirebaseConfig {
    /// Firebase Authentication's REST API, https://identitytoolkit.googleapis.com.
    pub auth_url: String,
    /// The Firebase project's web API key. It names the project; it is not a secret.
    pub api_key: String,
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

/// Where the game comes from: exactly one of `build_manifest` and `install`.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct GameConfig {
    /// A packaged build, for development: its manifest (VeyraBuild.json, which
    /// Game/Scripts/Package.ps1 writes), relative to this configuration file.
    pub build_manifest: Option<String>,
    /// The game installed from a release store (ADR-022).
    pub install: Option<InstallConfig>,
    /// More arguments for the game, after the launch-code switch the launcher adds itself.
    pub arguments: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct InstallConfig {
    /// The release store, such as http://127.0.0.1:8090: a scheme, a host, an optional port and an
    /// optional path.
    pub releases_url: String,
    /// The channel whose release the launcher installs.
    pub channel: String,
    /// Where the game goes unless the player chooses, relative to this configuration file.
    pub default_folder: String,
    /// The launcher's own state (the folder the player chose), relative to this configuration file.
    pub state_file: String,
    /// Chunks downloaded at once.
    pub parallel_downloads: u32,
    /// Times a chunk is downloaded before the install fails.
    pub download_attempts: u32,
    /// Seconds one request to the release store may take.
    pub download_timeout_seconds: f64,
}

/// The largest `parallelDownloads`: a bound on the threads the launcher starts, not a tuning value.
pub const MAX_PARALLEL_DOWNLOADS: u32 = 64;

/// Where the game comes from, with its paths resolved against the configuration file.
#[derive(Debug, Clone)]
pub enum GameSource {
    /// A packaged build: the path of its manifest.
    Packaged(PathBuf),
    Install(InstallSource),
}

#[derive(Debug, Clone)]
pub struct InstallSource {
    pub releases_url: String,
    pub channel: String,
    pub default_folder: PathBuf,
    pub state_file: PathBuf,
    pub parallel_downloads: usize,
    pub download_attempts: u32,
    pub download_timeout: Duration,
}

/// A configuration that parsed and validated, with its paths resolved.
#[derive(Debug, Clone)]
pub struct LoadedConfig {
    pub config: LauncherConfig,
    pub game: GameSource,
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
    // Paths in the configuration are relative to its file, and resolved to absolute ones: the window
    // shows them, and a game folder must not move with the working directory.
    let directory = std::path::absolute(path)
        .ok()
        .and_then(|absolute| absolute.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| path.parent().unwrap_or_else(|| Path::new(".")).to_path_buf());
    let game = match (&config.game.build_manifest, &config.game.install) {
        (Some(manifest), _) => GameSource::Packaged(directory.join(manifest)),
        (None, Some(install)) => GameSource::Install(InstallSource {
            releases_url: install.releases_url.trim_end_matches('/').to_string(),
            channel: install.channel.clone(),
            default_folder: directory.join(&install.default_folder),
            state_file: directory.join(&install.state_file),
            parallel_downloads: install.parallel_downloads as usize,
            download_attempts: install.download_attempts,
            download_timeout: Duration::from_secs_f64(install.download_timeout_seconds),
        }),
        (None, None) => unreachable!("validate requires a source"),
    };
    Ok(LoadedConfig { config, game })
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
    if let Some(login) = &config.player_login {
        if !login.firebase.auth_url.starts_with("https://") || !is_base_url(&login.firebase.auth_url) {
            problems.push("playerLogin.firebase.authUrl must be an https URL with no path, such as https://identitytoolkit.googleapis.com".to_string());
        }
        if !is_firebase_api_key(&login.firebase.api_key) {
            problems.push("playerLogin.firebase.apiKey must be a Firebase web API key (AIza and 35 more letters, digits, '-' or '_')".to_string());
        }
    }
    match (&config.game.build_manifest, &config.game.install) {
        (Some(_), Some(_)) | (None, None) => {
            problems.push("game must have exactly one of buildManifest (a packaged build) and install (a release store)".to_string())
        }
        (Some(manifest), None) if manifest.trim().is_empty() => problems.push("game.buildManifest must name the game build's VeyraBuild.json".to_string()),
        (Some(_), None) => {}
        (None, Some(install)) => problems.extend(validate_install(install)),
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
    if config
        .game
        .arguments
        .iter()
        .any(|argument| argument.to_ascii_lowercase().starts_with(&BACKEND_URL_SWITCH.to_ascii_lowercase()))
    {
        problems.push(format!(
            "game.arguments must not name the backend; the launcher adds {BACKEND_URL_SWITCH} from backend.baseUrl itself"
        ));
    }
    problems
}

/// Every problem with the `game.install` section.
fn validate_install(install: &InstallConfig) -> Vec<String> {
    let mut problems = Vec::new();
    if !is_releases_url(&install.releases_url) {
        problems.push("game.install.releasesUrl must be an http or https URL with no query, such as http://127.0.0.1:8090".to_string());
    }
    if !release::is_channel_name(&install.channel) {
        problems.push("game.install.channel must be 1 to 32 lowercase letters, digits or '-'".to_string());
    }
    for (name, value) in [
        ("game.install.defaultFolder", &install.default_folder),
        ("game.install.stateFile", &install.state_file),
    ] {
        if value.trim().is_empty() {
            problems.push(format!("{name} must name a path"));
        }
    }
    if !(1..=MAX_PARALLEL_DOWNLOADS).contains(&install.parallel_downloads) {
        problems.push(format!("game.install.parallelDownloads must be 1 to {MAX_PARALLEL_DOWNLOADS}"));
    }
    if install.download_attempts == 0 {
        problems.push("game.install.downloadAttempts must be at least 1".to_string());
    }
    if !(install.download_timeout_seconds.is_finite() && install.download_timeout_seconds > 0.0) {
        problems.push("game.install.downloadTimeoutSeconds must be a positive number of seconds".to_string());
    }
    problems
}

/// A base URL (`is_base_url`) with an optional path of plain segments, as a CDN may need.
pub fn is_releases_url(text: &str) -> bool {
    let Some(scheme_end) = text.find("://") else {
        return false;
    };
    let after_host = text[scheme_end + 3..].find('/').map(|index| scheme_end + 3 + index);
    let (base, path) = match after_host {
        Some(index) => text.split_at(index),
        None => (text, ""),
    };
    is_base_url(base)
        && path.split('/').skip(1).all(|segment| {
            segment.is_empty()
                || (segment != "." && segment != ".." && segment.bytes().all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'.' | b'_' | b'-')))
        })
}

/// Whether `text` has the form of a Google API key, as Firebase issues for web apps.
fn is_firebase_api_key(text: &str) -> bool {
    text.len() == 39 && text.starts_with("AIza") && text.bytes().all(|byte| byte.is_ascii_alphanumeric() || byte == b'-' || byte == b'_')
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
        "schemaVersion": 2,
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
    fn player_login_is_optional_and_validated() {
        assert!(parse(VALID).expect("valid").player_login.is_none());
        let with = |auth_url: &str, api_key: &str| {
            VALID.replace(
                r#""http": {"#,
                &format!(r#""playerLogin": {{ "firebase": {{ "authUrl": "{auth_url}", "apiKey": "{api_key}" }} }}, "http": {{"#),
            )
        };
        let key = format!("AIza{}", "x".repeat(35));
        let config = parse(&with("https://identitytoolkit.googleapis.com", &key)).expect("valid player login");
        assert_eq!(config.player_login.expect("present").firebase.api_key, key);
        let problems = parse(&with("http://identitytoolkit.googleapis.com", "nope")).expect_err("invalid");
        assert_eq!(problems.len(), 2, "{problems:?}");
        assert!(problems[0].contains("authUrl") && problems[1].contains("apiKey"), "{problems:?}");
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
            .replace(r#""schemaVersion": 2"#, r#""schemaVersion": 1"#)
            .replace(r#"["-windowed"]"#, r#"["-VeyraLaunchCode=vlc_x"]"#);
        let problems = parse(&bad).expect_err("four problems");
        assert_eq!(problems.len(), 4, "{problems:?}");
    }

    #[test]
    fn the_game_arguments_never_name_the_backend() {
        let named = VALID.replace(r#"["-windowed"]"#, r#"["-VeyraBackendUrl=https://elsewhere.example"]"#);
        let problems = parse(&named).expect_err("the launcher names the backend itself");
        assert!(problems.iter().any(|problem| problem.contains(BACKEND_URL_SWITCH)), "{problems:?}");
    }

    const INSTALL: &str = r#""install": {
            "releasesUrl": "http://127.0.0.1:8090",
            "channel": "local",
            "defaultFolder": "Game",
            "stateFile": "LauncherState.json",
            "parallelDownloads": 4,
            "downloadAttempts": 3,
            "downloadTimeoutSeconds": 60
        }"#;

    fn installed() -> String {
        VALID.replace(r#""buildManifest": "../build/VeyraBuild.json""#, INSTALL)
    }

    #[test]
    fn the_game_comes_from_exactly_one_source() {
        let config = parse(&installed()).expect("an installed game is valid");
        let install = config.game.install.expect("the install source");
        assert!(config.game.build_manifest.is_none());
        assert_eq!(install.channel, "local");
        assert_eq!(install.parallel_downloads, 4);

        let both = VALID.replace(r#""arguments""#, &format!("{INSTALL}, \"arguments\""));
        assert!(parse(&both).unwrap_err()[0].contains("exactly one"));
        let neither = VALID.replace(r#""buildManifest": "../build/VeyraBuild.json","#, "");
        assert!(parse(&neither).unwrap_err()[0].contains("exactly one"));
    }

    #[test]
    fn install_values_are_validated() {
        let bad = installed()
            .replace("http://127.0.0.1:8090", "http://127.0.0.1:8090/../x")
            .replace(r#""local""#, r#""Local Channel""#)
            .replace(r#""parallelDownloads": 4"#, r#""parallelDownloads": 0"#)
            .replace(r#""downloadAttempts": 3"#, r#""downloadAttempts": 0"#)
            .replace(r#""downloadTimeoutSeconds": 60"#, r#""downloadTimeoutSeconds": -1"#)
            .replace(r#""stateFile": "LauncherState.json""#, r#""stateFile": " ""#);
        let problems = parse(&bad).expect_err("six problems");
        assert_eq!(problems.len(), 6, "{problems:?}");
    }

    #[test]
    fn install_paths_resolve_beside_the_configuration() {
        let folder = std::env::temp_dir().join(format!("veyra-config-install-{}", std::process::id()));
        fs::create_dir_all(&folder).unwrap();
        let path = folder.join("VeyraLauncher.json");
        fs::write(&path, installed()).unwrap();
        let loaded = load(&path).expect("loads");
        let GameSource::Install(install) = loaded.game else {
            panic!("an install source")
        };
        assert_eq!(install.default_folder, folder.join("Game"));
        assert_eq!(install.state_file, folder.join("LauncherState.json"));
        assert_eq!(install.download_timeout, Duration::from_secs(60));

        // A relative configuration path still gives absolute paths.
        let relative = Path::new("veyra-relative-config.json");
        let _ = fs::remove_file(relative);
        fs::write(relative, installed()).unwrap();
        let loaded = load(relative);
        fs::remove_file(relative).unwrap();
        let GameSource::Install(install) = loaded.expect("loads").game else {
            panic!("an install source")
        };
        assert!(install.default_folder.is_absolute(), "{}", install.default_folder.display());
    }

    #[test]
    fn a_releases_url_may_have_a_plain_path() {
        assert!(is_releases_url("http://127.0.0.1:8090"));
        assert!(is_releases_url("https://cdn.example.com/veyra/live/"));
        for bad in ["ftp://cdn", "https://cdn/a?b", "https://cdn/../x", "https://user@cdn/x", "https://cdn/a b"] {
            assert!(!is_releases_url(bad), "{bad}");
        }
    }

    #[test]
    fn the_committed_configurations_are_valid() {
        let folder = Path::new(env!("CARGO_MANIFEST_DIR")).join("../config");
        let local = load(&folder.join("local.json")).expect("config/local.json");
        assert!(matches!(local.game, GameSource::Packaged(_)), "development launches the packaged build");
        let installed = load(&folder.join("installed.json")).expect("config/installed.json, which Setup installs");
        assert!(matches!(installed.game, GameSource::Install(_)), "Setup's launcher installs the game");
        assert!(
            local.config.player_login.is_some() && installed.config.player_login.is_some(),
            "players can sign in"
        );
        // The players' Setup: the public address over https, its release store on the public channel (ADR-057 §6).
        let public = load(&folder.join("public.json")).expect("config/public.json, which players' Setup installs");
        assert!(public.config.backend.base_url.starts_with("https://") && public.config.player_login.is_some());
        match &public.game {
            GameSource::Install(install) => {
                assert!(install.releases_url.starts_with(&public.config.backend.base_url) && install.channel == "public")
            }
            GameSource::Packaged(_) => panic!("players' Setup installs the game"),
        }
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
