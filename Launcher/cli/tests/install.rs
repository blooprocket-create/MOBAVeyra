//! Installing end to end (ADR-022): a build published to a release store with the publisher, served
//! over HTTP, installed, updated, repaired and uninstalled by `veyra-install`, then launched from its
//! install folder by `veyra-launch-cli` through the launch handshake.

mod support;

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use support::{FakeBackend, FileServer};
use veyra_publish::{parse_config, publish, PublishConfig};

/// Small chunks, so the test builds have many.
fn publish_config() -> PublishConfig {
    parse_config(r#"{ "schemaVersion": 1, "chunks": { "minBytes": 4096, "averageBytes": 16384, "maxBytes": 65536 }, "zstdLevel": 3 }"#).unwrap()
}

/// Bytes that do not compress away, the same every run.
fn noise(length: usize, seed: u64) -> Vec<u8> {
    let mut state = seed;
    (0..length)
        .map(|_| {
            state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
            (state >> 56) as u8
        })
        .collect()
}

struct Fixture {
    root: PathBuf,
    store: PathBuf,
    config: PathBuf,
    server: FileServer,
}

impl Fixture {
    /// A release store, its server, and a launcher configuration that installs from it.
    fn new(test: &str, backend_url: &str) -> Self {
        let root = std::env::temp_dir().join(format!("veyra-install-cli-{test}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let store = root.join("store");
        fs::create_dir_all(&store).unwrap();
        let server = FileServer::start(&store);
        let config = root.join("launcher").join("VeyraLauncher.json");
        fs::create_dir_all(config.parent().unwrap()).unwrap();
        write_config(&config, backend_url, &server.url);
        Self { root, store, config, server }
    }

    /// A build of the fake game, with `data` as a content file, published to the `local` channel.
    fn publish(&self, version: &str, data: &[u8]) -> PathBuf {
        let build = self.root.join(format!("build-{version}"));
        let _ = fs::remove_dir_all(&build);
        fs::create_dir_all(build.join("Bin")).unwrap();
        fs::create_dir_all(build.join("Content/Paks")).unwrap();
        let game = PathBuf::from(env!("CARGO_BIN_EXE_veyra-fake-game"));
        let name = game.file_name().unwrap().to_string_lossy().into_owned();
        fs::copy(&game, build.join("Bin").join(&name)).unwrap();
        fs::write(build.join("Content/Paks/data.pak"), data).unwrap();
        fs::write(build.join("Content/Empty.txt"), b"").unwrap();
        fs::write(
            build.join("VeyraBuild.json"),
            serde_json::json!({ "schemaVersion": 1, "buildVersion": version, "executable": format!("Bin/{name}") }).to_string(),
        )
        .unwrap();
        publish(&build, &self.store, "local", &publish_config(), &mut |_| {}).expect("publish");
        build
    }

    fn game(&self) -> PathBuf {
        self.root.join("launcher").join("Game")
    }

    fn state(&self) -> PathBuf {
        self.root.join("launcher").join("LauncherState.json")
    }
}

fn write_config(path: &Path, backend_url: &str, releases_url: &str) {
    let config = serde_json::json!({
        "schemaVersion": 2,
        "backend": { "baseUrl": backend_url },
        "http": { "timeoutSeconds": 5 },
        "launch": { "awaitReadySeconds": 10, "awaitSignInSeconds": 5 },
        "game": {
            "install": {
                "releasesUrl": releases_url,
                "channel": "local",
                "defaultFolder": "Game",
                "stateFile": "LauncherState.json",
                "parallelDownloads": 3,
                "downloadAttempts": 2,
                "downloadTimeoutSeconds": 5
            },
            "arguments": []
        }
    });
    fs::write(path, config.to_string()).unwrap();
}

fn veyra_install(config: &Path, arguments: &[&str]) -> Output {
    Command::new(env!("CARGO_BIN_EXE_veyra-install"))
        .arg("--config")
        .arg(config)
        .args(arguments)
        .output()
        .expect("run veyra-install")
}

fn said(output: &Output) -> String {
    format!("{}{}", String::from_utf8_lossy(&output.stdout), String::from_utf8_lossy(&output.stderr))
}

fn stdout(output: &Output) -> String {
    String::from_utf8_lossy(&output.stdout).trim_end().to_string()
}

/// Every file of `build` is in `folder`, byte for byte.
fn assert_installed(build: &Path, folder: &Path) {
    let mut folders = vec![build.to_path_buf()];
    while let Some(current) = folders.pop() {
        for entry in fs::read_dir(&current).unwrap().flatten() {
            let path = entry.path();
            if path.is_dir() {
                folders.push(path);
                continue;
            }
            let installed = folder.join(path.strip_prefix(build).unwrap());
            assert!(
                fs::read(&installed).ok() == fs::read(&path).ok(),
                "{} differs from the build",
                installed.display()
            );
        }
    }
}

#[test]
fn installs_the_published_release_and_launches_it() {
    let backend = FakeBackend::start();
    let fixture = Fixture::new("launches", &backend.url);
    let build = fixture.publish("0.1.0", &noise(200_000, 1));

    let output = veyra_install(&fixture.config, &["install"]);
    assert!(output.status.success(), "{}", said(&output));
    assert_eq!(stdout(&output), format!("veyra-install installed 0.1.0 {}", fixture.game().display()));
    assert_installed(&build, &fixture.game());
    assert!(fixture.game().join("VeyraInstall.json").is_file());
    assert!(fs::read_to_string(fixture.state()).unwrap().contains("Game"));

    let status = veyra_install(&fixture.config, &["status"]);
    assert_eq!(stdout(&status), "veyra-install status 0.1.0 0.1.0");

    // The launcher starts the installed copy, not the build it came from.
    fs::remove_dir_all(&build).unwrap();
    #[cfg(unix)]
    {
        // A release carries no file modes, as the client is Windows only (ADR-005); here the fake game
        // must be made runnable.
        use std::os::unix::fs::PermissionsExt as _;
        let game = PathBuf::from(env!("CARGO_BIN_EXE_veyra-fake-game"));
        let installed = fixture.game().join("Bin").join(game.file_name().unwrap());
        fs::set_permissions(&installed, fs::Permissions::from_mode(0o755)).unwrap();
    }
    let record = fixture.root.join("code.txt");
    let launched = Command::new(env!("CARGO_BIN_EXE_veyra-launch-cli"))
        .arg("--config")
        .arg(&fixture.config)
        .args(["--account", "DevOne", "--", "--fake-record"])
        .arg(&record)
        .output()
        .unwrap();
    assert!(launched.status.success(), "{}", said(&launched));
    assert_eq!(fs::read_to_string(&record).unwrap(), support::launch_code());
}

#[test]
fn an_update_downloads_only_the_changed_chunks() {
    let fixture = Fixture::new("update", &support::nothing_listening());
    let data = noise(300_000, 2);
    fixture.publish("0.1.0", &data);
    assert!(veyra_install(&fixture.config, &["install"]).status.success());
    let first_install = fixture.server.chunk_requests();

    // Bytes inserted in the middle of the content file: content-defined chunks resynchronise after it.
    let mut changed = data[..150_000].to_vec();
    changed.extend_from_slice(b"a new ability for somebody");
    changed.extend_from_slice(&data[150_000..]);
    let build = fixture.publish("0.2.0", &changed);
    let before = fixture.server.chunk_requests();
    let output = veyra_install(&fixture.config, &["install"]);
    assert!(output.status.success(), "{}", said(&output));
    assert_eq!(stdout(&output), format!("veyra-install installed 0.2.0 {}", fixture.game().display()));
    assert_installed(&build, &fixture.game());
    let update = fixture.server.chunk_requests() - before;
    // The chunks around the change, and the build manifest's one.
    assert!(
        (1..=4).contains(&update),
        "the update fetched {update} chunks of the first install's {first_install}"
    );

    // Nothing more to do the next time.
    let before = fixture.server.chunk_requests();
    assert!(veyra_install(&fixture.config, &["install"]).status.success());
    assert_eq!(fixture.server.chunk_requests(), before);
}

#[test]
fn repair_restores_a_damaged_file() {
    let fixture = Fixture::new("repair", &support::nothing_listening());
    let build = fixture.publish("0.1.0", &noise(200_000, 3));
    assert!(veyra_install(&fixture.config, &["install"]).status.success());
    let pak = fixture.game().join("Content/Paks/data.pak");
    let mut damaged = fs::read(&pak).unwrap();
    damaged[100_000..100_010].copy_from_slice(b"XXXXXXXXXX");
    fs::write(&pak, &damaged).unwrap();

    // An update trusts what it installed; a repair checks.
    assert!(veyra_install(&fixture.config, &["install"]).status.success());
    assert_eq!(fs::read(&pak).unwrap(), damaged);
    let before = fixture.server.chunk_requests();
    let output = veyra_install(&fixture.config, &["repair"]);
    assert!(output.status.success(), "{}", said(&output));
    assert_installed(&build, &fixture.game());
    assert_eq!(fixture.server.chunk_requests() - before, 1, "only the damaged chunk is downloaded");
}

#[test]
fn uninstall_removes_the_game_and_forgets_its_folder() {
    let fixture = Fixture::new("uninstall", &support::nothing_listening());
    fixture.publish("0.1.0", &noise(50_000, 4));
    // A folder with the player's own things in it gets the game in a Veyra folder inside.
    let games = fixture.root.join("Games");
    fs::create_dir_all(&games).unwrap();
    fs::write(games.join("notes.txt"), b"the player's").unwrap();
    let output = veyra_install(&fixture.config, &["--folder", &games.to_string_lossy(), "install"]);
    assert!(output.status.success(), "{}", said(&output));
    assert_eq!(stdout(&output), format!("veyra-install installed 0.1.0 {}", games.join("Veyra").display()));

    let output = veyra_install(&fixture.config, &["uninstall"]);
    assert!(output.status.success(), "{}", said(&output));
    assert_eq!(stdout(&output), format!("veyra-install uninstalled {}", games.join("Veyra").display()));
    assert!(!games.join("Veyra").exists());
    assert!(games.join("notes.txt").exists());
    assert!(!fixture.state().exists());
    assert_eq!(stdout(&veyra_install(&fixture.config, &["uninstall"])), "veyra-install nothing-installed");
}

#[test]
fn a_release_server_that_does_not_answer_fails_cleanly() {
    let fixture = Fixture::new("down", &support::nothing_listening());
    write_config(&fixture.config, &support::nothing_listening(), &support::nothing_listening());
    let output = veyra_install(&fixture.config, &["install"]);
    assert_eq!(output.status.code(), Some(1), "{}", said(&output));
    assert!(said(&output).contains("Veyra's download server did not answer"), "{}", said(&output));
    assert!(!fixture.game().exists(), "nothing is written");
    let status = veyra_install(&fixture.config, &["status"]);
    assert_eq!(stdout(&status), "veyra-install status none unknown");
}

#[test]
fn a_channel_with_no_release_says_so() {
    let fixture = Fixture::new("empty", &support::nothing_listening());
    let output = veyra_install(&fixture.config, &["install"]);
    assert_eq!(output.status.code(), Some(1));
    assert!(said(&output).contains("has no channel local"), "{}", said(&output));
}

#[test]
fn a_packaged_build_has_nothing_to_install() {
    let fixture = Fixture::new("packaged", &support::nothing_listening());
    let text = fs::read_to_string(&fixture.config).unwrap();
    let mut config: serde_json::Value = serde_json::from_str(&text).unwrap();
    config["game"] = serde_json::json!({ "buildManifest": "VeyraBuild.json", "arguments": [] });
    fs::write(&fixture.config, config.to_string()).unwrap();
    let output = veyra_install(&fixture.config, &["install"]);
    assert_eq!(output.status.code(), Some(2));
    assert!(said(&output).contains("nothing to install"), "{}", said(&output));
    assert_eq!(veyra_install(&fixture.config, &["--folder", "x", "status"]).status.code(), Some(2));
}
