//! The launcher updating itself (ADR-022 §11): a Setup published to a release store with the publisher,
//! served over HTTP, found on the launcher's channel, downloaded and checked, then started with the
//! switches that make it update silently and reopen the launcher. A stand-in Setup records them.

mod support;

use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::{Duration, Instant};
use support::{nothing_listening, FileServer};
use veyra_launcher_core::release;
use veyra_launcher_core::releases::ReleaseServer;
use veyra_launcher_core::update::{self, Check};
use veyra_publish::publish_setup;

const CHANNEL: &str = "local";
/// How many times a test lets a Setup download be tried, as `downloadAttempts` does.
const ATTEMPTS: u32 = 3;
/// How long a test waits for the stand-in Setup to record its switches.
const SETUP_WAIT: Duration = Duration::from_secs(10);

struct Fixture {
    root: PathBuf,
    store: PathBuf,
    folder: PathBuf,
    server: FileServer,
}

impl Fixture {
    fn new(test: &str) -> Self {
        let root = std::env::temp_dir().join(format!("veyra-update-{test}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let store = root.join("store");
        fs::create_dir_all(&store).unwrap();
        let server = FileServer::start(&store);
        Self {
            folder: root.join("update"),
            root,
            store,
            server,
        }
    }

    /// Publishes the stand-in Setup as launcher `version`; returns its hash.
    fn publish(&self, version: &str) -> String {
        let setup = PathBuf::from(env!("CARGO_BIN_EXE_veyra-fake-setup"));
        publish_setup(&setup, version, &self.store, CHANNEL).expect("publish Setup").hash
    }

    fn releases(&self) -> ReleaseServer {
        ReleaseServer::new(&self.server.url, Duration::from_secs(10))
    }
}

impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.root);
    }
}

/// The switches the stand-in Setup at `path` recorded, once it has.
fn recorded_switches(path: &Path) -> String {
    let mut record = path.as_os_str().to_owned();
    record.push(".args");
    let record = PathBuf::from(record);
    let started = Instant::now();
    while started.elapsed() < SETUP_WAIT {
        if let Ok(text) = fs::read_to_string(&record) {
            return text;
        }
        thread::sleep(Duration::from_millis(50));
    }
    panic!("the stand-in Setup recorded nothing at {}", record.display());
}

#[test]
fn a_launcher_runs_the_channels_setup_once_then_counts_it_failed_until_it_is_current() {
    let fixture = Fixture::new("once");
    let hash = fixture.publish("9.9.9");
    let releases = fixture.releases();

    let Check::Update(release) = update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap() else {
        panic!("an older launcher is asked to update");
    };
    assert_eq!((release.version.as_str(), release.setup.as_str()), ("9.9.9", hash.as_str()));
    let waiting = update::download(&releases, &release, &fixture.folder, ATTEMPTS).expect("download Setup");
    assert!(waiting.to_string_lossy().ends_with(release::PART_SUFFIX), "it waits under its part name");
    assert!(
        matches!(update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Update(_)),
        "downloaded is not yet tried"
    );

    let started = update::start_setup(&waiting).expect("start Setup");
    assert_eq!(recorded_switches(&started), "/S /RELAUNCH");
    assert!(
        matches!(update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Failed(_)),
        "still the old launcher after its Setup ran: no second try"
    );

    assert_eq!(update::check(&releases, CHANNEL, "9.9.9", &fixture.folder).unwrap(), Check::Current);
    update::clear(&fixture.folder);
    assert!(!fixture.folder.exists());
}

#[test]
fn a_setup_that_does_not_start_is_not_counted_as_tried() {
    let fixture = Fixture::new("unstarted");
    // Downloaded and checked against its hash, but not a program the system will start, as when
    // security software holds a fresh executable back.
    let not_a_program = fixture.root.join("not-a-program.exe");
    fs::write(&not_a_program, b"not a program").unwrap();
    publish_setup(&not_a_program, "9.9.9", &fixture.store, CHANNEL).unwrap();
    let releases = fixture.releases();
    let Check::Update(release) = update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap() else {
        panic!("an update");
    };
    let waiting = update::download(&releases, &release, &fixture.folder, ATTEMPTS).expect("download Setup");

    assert!(update::start_setup(&waiting).is_err(), "it does not start");
    assert!(
        matches!(update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Update(_)),
        "a Setup that never started leaves nothing that reads as having run, so the next open tries again"
    );
}

#[test]
fn a_moved_channel_is_tried_again() {
    let fixture = Fixture::new("moved");
    fixture.publish("9.9.9");
    let releases = fixture.releases();
    let Check::Update(first) = update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap() else {
        panic!("an update");
    };
    let started = update::start_setup(&update::download(&releases, &first, &fixture.folder, ATTEMPTS).unwrap()).unwrap();
    recorded_switches(&started);

    // A different Setup: the stand-in with a byte appended.
    let mut bytes = fs::read(env!("CARGO_BIN_EXE_veyra-fake-setup")).unwrap();
    bytes.push(0);
    let other = fixture.root.join("other-setup.exe");
    fs::write(&other, bytes).unwrap();
    publish_setup(&other, "9.9.10", &fixture.store, CHANNEL).unwrap();
    assert!(matches!(update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Update(release) if release.version == "9.9.10"));
}

#[test]
fn a_setup_that_is_not_the_one_the_channel_names_is_refused() {
    let fixture = Fixture::new("tampered");
    let hash = fixture.publish("9.9.9");
    let stored = fixture.store.join(release::setup_object(&hash));
    let mut bytes = fs::read(&stored).unwrap();
    let last = bytes.len() - 1;
    bytes[last] ^= 0xff;
    fs::write(&stored, bytes).unwrap();

    let releases = fixture.releases();
    let Check::Update(release) = update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap() else {
        panic!("an update");
    };
    let problem = update::download(&releases, &release, &fixture.folder, ATTEMPTS).unwrap_err();
    assert!(problem.contains("not the Setup the channel names"), "{problem}");
    let setup_path = format!("/{}", release::setup_object(&hash));
    let tries = fixture.server.requested.lock().unwrap().iter().filter(|path| **path == setup_path).count();
    assert_eq!(tries, ATTEMPTS as usize, "a failed download is tried again, as often as the configuration says");
    assert!(
        matches!(update::check(&releases, CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Update(_)),
        "nothing refused counts as tried"
    );
}

#[test]
fn a_store_without_a_launcher_release_asks_nothing_and_an_unreachable_one_is_an_error() {
    let fixture = Fixture::new("none");
    assert_eq!(update::check(&fixture.releases(), CHANNEL, "0.1.0", &fixture.folder).unwrap(), Check::Current);
    let offline = ReleaseServer::new(&nothing_listening(), Duration::from_secs(2));
    assert!(update::check(&offline, CHANNEL, "0.1.0", &fixture.folder).is_err());
}

#[test]
fn publishing_a_setup_keeps_it_by_hash_and_by_its_download_name() {
    let fixture = Fixture::new("publish");
    let hash = fixture.publish("9.9.9");
    let setup = fs::read(env!("CARGO_BIN_EXE_veyra-fake-setup")).unwrap();
    assert_eq!(fs::read(fixture.store.join(release::setup_object(&hash))).unwrap(), setup);
    assert_eq!(fs::read(fixture.store.join(release::setup_download_object("9.9.9", CHANNEL))).unwrap(), setup);
    let channel = release::parse_launcher_channel(&fs::read(fixture.store.join(release::launcher_channel_object(CHANNEL))).unwrap()).unwrap();
    assert_eq!(
        (channel.version.as_str(), channel.setup.as_str(), channel.size),
        ("9.9.9", hash.as_str(), setup.len() as u64)
    );
    assert!(publish_setup(Path::new(env!("CARGO_BIN_EXE_veyra-fake-setup")), "not a version", &fixture.store, CHANNEL).is_err());
    assert!(publish_setup(Path::new(env!("CARGO_BIN_EXE_veyra-fake-setup")), "9.9.9", &fixture.store, "Not A Channel").is_err());
}
