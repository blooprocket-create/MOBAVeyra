//! The launcher end to end: `veyra-launch-cli` against a fake backend, launching `veyra-fake-game`
//! through the launch handshake (ADR-010 §5).

mod support;

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use std::time::Duration;
use support::FakeBackend;

/// A configuration and build manifest for one test, in a folder of its own. The manifest sits beside
/// the fake game, whose executable it must hold.
fn configure(test: &str, backend: &FakeBackend, await_sign_in_seconds: f64) -> PathBuf {
    let game = PathBuf::from(env!("CARGO_BIN_EXE_veyra-fake-game"));
    let manifest = game.with_file_name(format!("VeyraBuild-{test}.json"));
    let executable = game.file_name().unwrap().to_string_lossy().into_owned();
    fs::write(
        &manifest,
        serde_json::json!({ "schemaVersion": 1, "buildVersion": "0.1.0", "executable": executable }).to_string(),
    )
    .unwrap();
    let folder = std::env::temp_dir().join(format!("veyra-launch-cli-{test}-{}", std::process::id()));
    fs::create_dir_all(&folder).unwrap();
    let config = serde_json::json!({
        "schemaVersion": 2,
        "backend": { "baseUrl": backend.url },
        "http": { "timeoutSeconds": 5 },
        "launch": { "awaitReadySeconds": 10, "awaitSignInSeconds": await_sign_in_seconds },
        "game": { "buildManifest": manifest.to_string_lossy(), "arguments": [] }
    });
    let path = folder.join("local.json");
    fs::write(&path, config.to_string()).unwrap();
    path
}

fn launch(config: &Path, account: &str, game_arguments: &[&str]) -> Output {
    Command::new(env!("CARGO_BIN_EXE_veyra-launch-cli"))
        .arg("--config")
        .arg(config)
        .args(["--account", account, "--"])
        .args(game_arguments)
        .output()
        .expect("run veyra-launch-cli")
}

/// Everything the launcher printed, which must never hold a credential.
fn printed(output: &Output) -> String {
    let text = format!("{}{}", String::from_utf8_lossy(&output.stdout), String::from_utf8_lossy(&output.stderr));
    for prefix in ["vls_", "vlc_", "vgs_"] {
        assert!(!text.contains(prefix), "the launcher printed a credential:\n{text}");
    }
    text
}

#[test]
fn signs_in_and_hands_the_game_its_code() {
    let backend = FakeBackend::start();
    let config = configure("signs-in", &backend, 5.0);
    let record = config.with_file_name("code.txt");
    let output = launch(&config, "DevOne", &["--fake-record", &record.to_string_lossy()]);
    let text = printed(&output);
    assert!(output.status.success(), "{text}");
    assert!(String::from_utf8_lossy(&output.stdout).starts_with("veyra-launch signed-in pid "), "{text}");
    assert!(text.contains("DevOne is signed in"), "{text}");
    assert_eq!(
        fs::read_to_string(&record).unwrap(),
        support::launch_code(),
        "the game received the code the backend issued"
    );
    assert_eq!(backend.routes(), ["POST /v1/dev/login", "POST /v1/launch-codes"]);
    // The session goes only in the header of the request that needs it.
    let requests = backend.requests.lock().unwrap();
    assert!(requests[0].authorization.is_none());
    assert_eq!(
        requests[1].authorization.as_deref(),
        Some(format!("Bearer {}", support::launcher_session()).as_str())
    );
}

#[test]
fn the_launcher_leaves_once_the_game_signs_in() {
    // The game plays on; a script reading the launcher's output must not wait for it (ADR-005 L4).
    const GAME_PLAYS_ON_MS: u64 = 8000;
    let backend = FakeBackend::start();
    let config = configure("leaves", &backend, 5.0);
    let started = std::time::Instant::now();
    let output = launch(&config, "DevOne", &["--fake-stay-ms", &GAME_PLAYS_ON_MS.to_string()]);
    let text = printed(&output);
    assert!(output.status.success(), "{text}");
    assert!(
        started.elapsed() < Duration::from_millis(GAME_PLAYS_ON_MS / 2),
        "the launcher waited for the game: {:?}",
        started.elapsed()
    );
}

#[test]
fn a_slow_start_still_signs_in() {
    // The game asks for its code later than the sign-in wait; the code is issued only then.
    let backend = FakeBackend::start();
    let config = configure("slow", &backend, 1.0);
    let output = launch(&config, "DevOne", &["--fake-delay-ms", "1500"]);
    let text = printed(&output);
    assert!(output.status.success(), "{text}");
    let login = backend.time_of("POST /v1/dev/login").unwrap();
    let issued = backend.time_of("POST /v1/launch-codes").unwrap();
    assert!(issued - login >= Duration::from_millis(1500), "the code was issued before the game asked");
}

#[test]
fn a_game_that_cannot_sign_in_says_why() {
    let backend = FakeBackend::start();
    let config = configure("refused", &backend, 5.0);
    let output = launch(&config, "DevOne", &["--fake-mode", "failed:sign_in_refused"]);
    let text = printed(&output);
    assert_eq!(output.status.code(), Some(1), "{text}");
    assert!(text.contains("The game could not sign in: Veyra's services refused its launch code."), "{text}");
}

#[test]
fn a_game_that_exits_early_gets_no_code() {
    let backend = FakeBackend::start();
    let config = configure("exits", &backend, 5.0);
    let output = launch(&config, "DevOne", &["--fake-mode", "exit-early"]);
    let text = printed(&output);
    assert_eq!(output.status.code(), Some(1), "{text}");
    assert!(text.contains("The game closed before it could sign in."), "{text}");
    assert_eq!(backend.routes(), ["POST /v1/dev/login"], "no code is issued to a game that never asked");
}

#[test]
fn an_unknown_account_is_refused_before_the_game_starts() {
    let backend = FakeBackend::start();
    let config = configure("unknown", &backend, 5.0);
    let output = launch(&config, "Nobody", &[]);
    let text = printed(&output);
    assert_eq!(output.status.code(), Some(1), "{text}");
    assert!(
        text.contains("Signing in failed: Veyra's services refused (HTTP 404, account_not_found)."),
        "{text}"
    );
}

#[test]
fn a_bad_configuration_is_refused() {
    let backend = FakeBackend::start();
    let config = configure("bad-config", &backend, 5.0);
    let text = fs::read_to_string(&config)
        .unwrap()
        .replace("\"timeoutSeconds\":5", "\"timeoutSeconds\":5,\"retries\":3");
    fs::write(&config, text).unwrap();
    let output = launch(&config, "DevOne", &[]);
    let text = printed(&output);
    assert_eq!(output.status.code(), Some(2), "{text}");
    assert!(text.contains("unknown field `retries`"), "{text}");
    assert!(backend.routes().is_empty(), "nothing is asked of the backend");
}

#[test]
fn lists_the_development_accounts() {
    let backend = FakeBackend::start();
    let config = configure("accounts", &backend, 5.0);
    let output = Command::new(env!("CARGO_BIN_EXE_veyra-launch-cli"))
        .arg("--config")
        .arg(&config)
        .arg("--accounts")
        .output()
        .unwrap();
    assert!(output.status.success());
    assert_eq!(String::from_utf8_lossy(&output.stdout).lines().collect::<Vec<_>>(), ["DevOne", "DevTwo"]);
}
