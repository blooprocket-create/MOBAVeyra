//! `veyra-publish`: publishes a packaged client to a release store (ADR-022 §4), for
//! `Game/Scripts/Publish.ps1` and the launcher's tests.
//!
//!     veyra-publish --build <package folder> --store <store folder> --channel <name> [--config <file>]
//!
//! The package folder is the one `Game/Scripts/Package.ps1` writes, with VeyraBuild.json at its root.
//! `--config` names the publisher configuration; otherwise Launcher/config/publish.json, two folders up
//! from `Launcher/target/<profile>/`. Progress goes to standard error. On success one line goes to
//! standard output: `veyra-publish published <build version> <manifest hash>`. Exit status: 0
//! published; 1 the publish failed; 2 the command line or configuration is wrong.

use std::path::PathBuf;
use std::process::ExitCode;
use veyra_publish::{load_config, publish};

const USAGE: &str = "usage: veyra-publish --build <package folder> --store <store folder> --channel <name> [--config <file>]";

struct Arguments {
    build: PathBuf,
    store: PathBuf,
    channel: String,
    config: Option<PathBuf>,
}

fn parse_arguments() -> Result<Arguments, String> {
    let (mut build, mut store, mut channel, mut config) = (None, None, None, None);
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        let mut value = |name: &str| arguments.next().ok_or(format!("{name} needs a value"));
        match argument.as_str() {
            "--build" => build = Some(PathBuf::from(value("--build")?)),
            "--store" => store = Some(PathBuf::from(value("--store")?)),
            "--channel" => channel = Some(value("--channel")?),
            "--config" => config = Some(PathBuf::from(value("--config")?)),
            other => return Err(format!("unknown argument {other}")),
        }
    }
    Ok(Arguments {
        build: build.ok_or("give --build")?,
        store: store.ok_or("give --store")?,
        channel: channel.ok_or("give --channel")?,
        config,
    })
}

fn main() -> ExitCode {
    let arguments = match parse_arguments() {
        Ok(arguments) => arguments,
        Err(problem) => {
            eprintln!("veyra-publish: {problem}\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    let config_path = arguments.config.unwrap_or_else(|| {
        let executable = std::env::current_exe().unwrap_or_default();
        executable.parent().unwrap_or(&executable).join("../../config/publish.json")
    });
    let config = match load_config(&config_path) {
        Ok(config) => config,
        Err(problem) => {
            eprintln!("veyra-publish: {problem}");
            return ExitCode::from(2);
        }
    };
    eprintln!(
        "veyra-publish: publishing {} to {}, channel {}.",
        arguments.build.display(),
        arguments.store.display(),
        arguments.channel
    );
    let mut log = |path: &str| eprintln!("veyra-publish: {path}");
    match publish(&arguments.build, &arguments.store, &arguments.channel, &config, &mut log) {
        Ok(published) => {
            eprintln!(
                "veyra-publish: build {} is on channel {}: {} files, {} bytes in {} chunks, of which {} were new ({} bytes stored).",
                published.build_version,
                arguments.channel,
                published.files,
                published.bytes,
                published.chunks,
                published.new_chunks,
                published.new_stored_bytes
            );
            println!("veyra-publish published {} {}", published.build_version, published.manifest_hash);
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("veyra-publish: {error}");
            ExitCode::FAILURE
        }
    }
}
