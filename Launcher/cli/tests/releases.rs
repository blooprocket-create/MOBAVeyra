//! The release server as the install engine's chunk store (ADR-022 §5): what it fetches over HTTP reaches
//! the engine's own checks, so a run refuses a bad chunk, fetches it again, and counts what it fetched.

mod support;

use std::fs;
use std::path::PathBuf;
use std::time::Duration;
use support::FileServer;
use veyra_launcher_core::install::ChunkStore;
use veyra_launcher_core::release::{self, ReleaseChunk};
use veyra_launcher_core::releases::ReleaseServer;

/// A store folder holding `body` as the chunk the manifest describes as `stored_size` bytes.
fn store_with(test: &str, chunk: &ReleaseChunk, body: &[u8]) -> PathBuf {
    let root = std::env::temp_dir().join(format!("veyra-releases-{test}-{}", std::process::id()));
    let _ = fs::remove_dir_all(&root);
    let object = root.join(release::chunk_object(&chunk.hash));
    fs::create_dir_all(object.parent().unwrap()).unwrap();
    fs::write(&object, body).unwrap();
    root
}

#[test]
fn a_chunk_body_longer_than_its_size_reaches_the_engines_check() {
    // A store that sends more than the manifest says: the engine sees the body, refuses it as the wrong size and
    // counts it as fetched, rather than the fetch failing before any of it is counted.
    let stored_size = 32;
    let chunk = ReleaseChunk {
        hash: release::sha256_hex(b"a chunk of the game"),
        size: 19,
        stored_size,
    };
    let body = vec![7u8; stored_size as usize + 64];
    let root = store_with("overlong", &chunk, &body);
    let server = FileServer::start(&root);
    let releases = ReleaseServer::new(&server.url, Duration::from_secs(10));

    let fetched = releases.fetch(&chunk).expect("the body comes back for the engine to check");
    assert_eq!(fetched.len() as u64, stored_size + 1, "no more than one byte past its size is read");
    let problem = release::decode_chunk(&fetched, &chunk).unwrap_err();
    assert!(problem.contains("bytes, not"), "{problem}");

    // A body of its size comes back whole.
    fs::write(root.join(release::chunk_object(&chunk.hash)), &body[..stored_size as usize]).unwrap();
    assert_eq!(releases.fetch(&chunk).unwrap(), &body[..stored_size as usize]);
    let _ = fs::remove_dir_all(&root);
}
