//! `veyra-publish` end to end: a packaged build into a release store, twice, and a build it refuses.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use veyra_launcher_core::release;

fn folder(test: &str) -> PathBuf {
    let folder = std::env::temp_dir().join(format!("veyra-publish-{test}-{}", std::process::id()));
    let _ = fs::remove_dir_all(&folder);
    fs::create_dir_all(&folder).unwrap();
    folder
}

/// A packaged build: an executable, some content and its VeyraBuild.json.
fn build(root: &Path) -> PathBuf {
    let build = root.join("VeyraClient-Win64");
    fs::create_dir_all(build.join("Windows/Veyra/Binaries/Win64")).unwrap();
    fs::create_dir_all(build.join("Windows/Veyra/Content/Paks")).unwrap();
    fs::write(build.join("Windows/Veyra/Binaries/Win64/VeyraClient.exe"), vec![7u8; 10_000]).unwrap();
    let content: Vec<u8> = (0..100_000u32).map(|index| (index.wrapping_mul(2654435761) >> 24) as u8).collect();
    fs::write(build.join("Windows/Veyra/Content/Paks/pakchunk0-Windows.pak"), content).unwrap();
    fs::write(
        build.join("VeyraBuild.json"),
        r#"{"schemaVersion":1,"buildVersion":"0.1.0","executable":"Windows/Veyra/Binaries/Win64/VeyraClient.exe"}"#,
    )
    .unwrap();
    build
}

fn run(root: &Path, build: &Path, store: &Path) -> Output {
    let config = root.join("publish.json");
    fs::write(
        &config,
        r#"{ "schemaVersion": 1, "chunks": { "minBytes": 1024, "averageBytes": 4096, "maxBytes": 16384 }, "zstdLevel": 3 }"#,
    )
    .unwrap();
    Command::new(env!("CARGO_BIN_EXE_veyra-publish"))
        .arg("--build")
        .arg(build)
        .arg("--store")
        .arg(store)
        .args(["--channel", "local", "--config"])
        .arg(&config)
        .output()
        .unwrap()
}

#[test]
fn publishes_a_build_and_writes_only_new_chunks_again() {
    let root = folder("twice");
    let build = build(&root);
    let store = root.join("store");
    let output = run(&root, &build, &store);
    let said = String::from_utf8_lossy(&output.stderr).into_owned();
    assert!(output.status.success(), "{said}");
    let line = String::from_utf8_lossy(&output.stdout).trim().to_string();
    let hash = line.strip_prefix("veyra-publish published 0.1.0 ").expect(&line).to_string();

    let channel = release::parse_channel(&fs::read(store.join("channels/local.json")).unwrap()).unwrap();
    assert_eq!(channel.manifest, hash);
    let bytes = fs::read(store.join(format!("manifests/{hash}.json"))).unwrap();
    assert_eq!(release::sha256_hex(&bytes), hash, "the manifest is addressed by its hash");
    let manifest = release::parse_manifest(&bytes).unwrap();
    assert_eq!(manifest.executable, "Windows/Veyra/Binaries/Win64/VeyraClient.exe");
    assert_eq!(manifest.files.len(), 3);
    for chunk in manifest.files.iter().flat_map(|file| &file.chunks) {
        let stored = fs::read(store.join(release::chunk_object(&chunk.hash))).unwrap();
        release::decode_chunk(&stored, chunk).expect("every chunk is in the store and decodes to itself");
    }
    assert!(said.contains("were new"), "{said}");

    // The same build again: the same manifest, and no new chunk.
    let again = run(&root, &build, &store);
    assert!(again.status.success());
    assert_eq!(String::from_utf8_lossy(&again.stdout).trim(), line);
    assert!(String::from_utf8_lossy(&again.stderr).contains("of which 0 were new"));
    let leftovers: Vec<_> = walk(&store).into_iter().filter(|path| path.to_string_lossy().ends_with(".tmp")).collect();
    assert!(leftovers.is_empty(), "{leftovers:?}");
}

#[test]
fn refuses_a_build_that_is_not_a_release() {
    let root = folder("refuses");
    let build = build(&root);
    fs::write(build.join("VeyraInstall.json"), b"{}").unwrap();
    let output = run(&root, &build, &root.join("store"));
    assert_eq!(output.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&output.stderr).contains("cannot be released"));
    assert!(!root.join("store/channels").exists(), "the channel does not move");

    fs::remove_file(build.join("VeyraInstall.json")).unwrap();
    fs::remove_file(build.join("VeyraBuild.json")).unwrap();
    let output = run(&root, &build, &root.join("store"));
    assert_eq!(output.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&output.stderr).contains("package the client"));
}

fn walk(root: &Path) -> Vec<PathBuf> {
    let mut found = Vec::new();
    let mut folders = vec![root.to_path_buf()];
    while let Some(folder) = folders.pop() {
        for entry in fs::read_dir(folder).unwrap().flatten() {
            if entry.path().is_dir() {
                folders.push(entry.path());
            } else {
                found.push(entry.path());
            }
        }
    }
    found
}
