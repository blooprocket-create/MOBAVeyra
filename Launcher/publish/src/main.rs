//! `veyra-publish`: publishes a packaged client to a release store (ADR-022 §4), or the Veyra Setup a
//! launcher updates itself with (ADR-022 §11), for `Game/Scripts/Publish.ps1` and the launcher's tests.
//!
//!     veyra-publish --build <package folder> --store <store folder> --channel <name> [--config <file>]
//!     veyra-publish --setup <Setup .exe> --version <launcher version> --store <store folder> --channel <name>
//!
//! The package folder is the one `Game/Scripts/Package.ps1` writes, with VeyraBuild.json at its root.
//! `--config` names the publisher configuration; otherwise Launcher/config/publish.json, two folders up
//! from `Launcher/target/<profile>/`. Progress goes to standard error. On success one line goes to
//! standard output: `veyra-publish published <build version> <manifest hash>`, or
//! `veyra-publish published-setup <launcher version> <Setup hash>`. Exit status: 0 published; 1 the
//! publish failed; 2 the command line or configuration is wrong.

use std::path::PathBuf;
use std::process::ExitCode;
use veyra_publish::{load_config, publish, publish_setup};

const USAGE: &str = "usage: veyra-publish --build <package folder> --store <store folder> --channel <name> [--config <file>]\n       veyra-publish --setup <Setup .exe> --version <launcher version> --store <store folder> --channel <name>";

/// What to publish.
enum Source {
    Build { folder: PathBuf, config: Option<PathBuf> },
    Setup { file: PathBuf, version: String },
}

struct Arguments {
    source: Source,
    store: PathBuf,
    channel: String,
}

fn parse_arguments() -> Result<Arguments, String> {
    let (mut build, mut setup, mut version, mut store, mut channel, mut config) = (None, None, None, None, None, None);
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        let mut value = |name: &str| arguments.next().ok_or(format!("{name} needs a value"));
        match argument.as_str() {
            "--build" => build = Some(PathBuf::from(value("--build")?)),
            "--setup" => setup = Some(PathBuf::from(value("--setup")?)),
            "--version" => version = Some(value("--version")?),
            "--store" => store = Some(PathBuf::from(value("--store")?)),
            "--channel" => channel = Some(value("--channel")?),
            "--config" => config = Some(PathBuf::from(value("--config")?)),
            other => return Err(format!("unknown argument {other}")),
        }
    }
    let source = match (build, setup) {
        (Some(folder), None) if version.is_none() => Source::Build { folder, config },
        (None, Some(file)) if config.is_none() => Source::Setup {
            file,
            version: version.ok_or("give --version with --setup")?,
        },
        (Some(_), Some(_)) => return Err("give --build or --setup, not both".to_string()),
        (None, None) => return Err("give --build or --setup".to_string()),
        (Some(_), None) => return Err("--version goes with --setup".to_string()),
        (None, Some(_)) => return Err("--config goes with --build".to_string()),
    };
    Ok(Arguments {
        source,
        store: store.ok_or("give --store")?,
        channel: channel.ok_or("give --channel")?,
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
    let (folder, config) = match arguments.source {
        Source::Setup { file, version } => {
            eprintln!(
                "veyra-publish: publishing Setup {} (launcher {version}) to {}, channel {}.",
                file.display(),
                arguments.store.display(),
                arguments.channel
            );
            return match publish_setup(&file, &version, &arguments.store, &arguments.channel) {
                Ok(published) => {
                    eprintln!(
                        "veyra-publish: launcher {} is on channel {}: Setup of {} bytes.",
                        published.version, arguments.channel, published.size
                    );
                    println!("veyra-publish published-setup {} {}", published.version, published.hash);
                    ExitCode::SUCCESS
                }
                Err(error) => {
                    eprintln!("veyra-publish: {error}");
                    ExitCode::FAILURE
                }
            };
        }
        Source::Build { folder, config } => (folder, config),
    };
    let config_path = config.unwrap_or_else(|| {
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
        folder.display(),
        arguments.store.display(),
        arguments.channel
    );
    let mut log = |path: &str| eprintln!("veyra-publish: {path}");
    match publish(&folder, &arguments.store, &arguments.channel, &config, &mut log) {
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
