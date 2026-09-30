//! `veyra-install`: the launcher's install, update, repair and uninstall without its window
//! (ADR-022 §8), for scripts and tests. It uses the same core as the Tauri app and a configuration
//! whose game comes from a release store (`game.install`).
//!
//!     veyra-install [--config <file>] [--folder <folder>] install | repair
//!     veyra-install [--config <file>] uninstall | status
//!
//! `install` installs or updates to the channel's release, `repair` re-checks every file first. Both
//! use the folder the launcher remembers, or its default, unless `--folder` names one; a folder that
//! holds other things gets the game in a `Veyra` folder inside it. `uninstall` removes only what the
//! launcher installed. Progress goes to standard error. On success one line goes to standard output:
//! `veyra-install installed <build version> <folder>`, `veyra-install uninstalled <folder>`,
//! `veyra-install nothing-installed`, or `veyra-install status <installed version or none>
//! <channel's version or unknown>`. Exit status: 0 done; 1 it failed; 2 the command line or
//! configuration is wrong.

use std::path::PathBuf;
use std::process::ExitCode;
use std::sync::atomic::AtomicBool;
use veyra_launcher_core::config::{self, GameSource};
use veyra_launcher_core::install::{Mode, Progress, Stage};
use veyra_launcher_core::{default_config_path, game};

const USAGE: &str = "usage: veyra-install [--config <file>] [--folder <folder>] install | repair\n       veyra-install [--config <file>] uninstall | status";

/// How often progress is reported, in percent of a stage.
const REPORT_EVERY_PERCENT: u64 = 10;

struct Arguments {
    config: Option<PathBuf>,
    folder: Option<PathBuf>,
    command: String,
}

fn parse_arguments() -> Result<Arguments, String> {
    let (mut config, mut folder, mut command) = (None, None, None);
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "--config" => config = Some(PathBuf::from(arguments.next().ok_or("--config needs a file")?)),
            "--folder" => folder = Some(PathBuf::from(arguments.next().ok_or("--folder needs a folder")?)),
            "install" | "repair" | "uninstall" | "status" if command.is_none() => command = Some(argument),
            other => return Err(format!("unknown argument {other}")),
        }
    }
    let command = command.ok_or("give a command")?;
    if folder.is_some() && !matches!(command.as_str(), "install" | "repair") {
        return Err(format!("--folder goes with install or repair, not {command}"));
    }
    Ok(Arguments { config, folder, command })
}

/// Reports a stage when it starts, then every `REPORT_EVERY_PERCENT`.
#[derive(Default)]
struct Reporter {
    last: Option<(Stage, u64)>,
}

impl Reporter {
    fn report(&mut self, progress: &Progress) {
        let percent = (progress.done_bytes * 100).checked_div(progress.total_bytes).unwrap_or(100);
        let step = percent / REPORT_EVERY_PERCENT;
        if self.last.is_some_and(|(stage, last)| stage == progress.stage && last == step) {
            return;
        }
        self.last = Some((progress.stage, step));
        eprintln!(
            "veyra-install: {}: {percent}% ({} of {} bytes, {} downloaded).",
            progress.stage.describe(),
            progress.done_bytes,
            progress.total_bytes,
            progress.downloaded_bytes
        );
    }
}

fn main() -> ExitCode {
    let arguments = match parse_arguments() {
        Ok(arguments) => arguments,
        Err(problem) => {
            eprintln!("veyra-install: {problem}\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    let config_path = arguments
        .config
        .clone()
        .unwrap_or_else(|| default_config_path(&std::env::current_exe().unwrap_or_default()));
    let source = match config::load(&config_path) {
        Ok(loaded) => match loaded.game {
            GameSource::Install(source) => source,
            GameSource::Packaged(_) => {
                eprintln!(
                    "veyra-install: {}'s game is a packaged build (game.buildManifest); there is nothing to install.",
                    config_path.display()
                );
                return ExitCode::from(2);
            }
        },
        Err(error) => {
            eprintln!("veyra-install: {error}");
            return ExitCode::from(2);
        }
    };

    let result = match arguments.command.as_str() {
        "install" | "repair" => (|| {
            let picked = match arguments.folder {
                Some(folder) => folder,
                None => game::folder(&source)?.path,
            };
            let target = game::release_server(&source).current(&source.channel).map_err(|error| error.to_string())?;
            eprintln!(
                "veyra-install: {} build {} in {}.",
                arguments.command,
                target.manifest.build_version,
                picked.display()
            );
            let mode = if arguments.command == "repair" { Mode::Repair } else { Mode::Update };
            let mut reporter = Reporter::default();
            let (folder, outcome) = game::install_into(&source, &picked, &target, mode, &AtomicBool::new(false), &mut |progress| {
                reporter.report(progress)
            })?;
            eprintln!(
                "veyra-install: build {} is installed: {} files written, {} removed, {} bytes downloaded, {} reused.",
                outcome.build_version, outcome.rebuilt_files, outcome.removed_files, outcome.downloaded_bytes, outcome.reused_bytes
            );
            Ok(format!("veyra-install installed {} {}", outcome.build_version, folder.display()))
        })(),
        "uninstall" => game::uninstall(&source).map(|removed| match removed {
            Some((folder, uninstalled)) => {
                eprintln!("veyra-install: removed {} files from {}.", uninstalled.removed_files, folder.display());
                if !uninstalled.removed_folder {
                    eprintln!(
                        "veyra-install: {} still holds files the launcher did not install, so it stays.",
                        folder.display()
                    );
                }
                format!("veyra-install uninstalled {}", folder.display())
            }
            None => "veyra-install nothing-installed".to_string(),
        }),
        _ => (|| {
            let (_, record) = game::record(&source)?;
            let installed = record
                .and_then(|record| record.installed)
                .map_or("none".to_string(), |release| release.manifest.build_version);
            let available = match game::release_server(&source).channel(&source.channel) {
                Ok(channel) => channel.build_version,
                Err(error) => {
                    eprintln!("veyra-install: {error}.");
                    "unknown".to_string()
                }
            };
            Ok(format!("veyra-install status {installed} {available}"))
        })(),
    };
    match result {
        Ok(line) => {
            println!("{line}");
            ExitCode::SUCCESS
        }
        Err(problem) => {
            eprintln!("veyra-install: {problem}.");
            ExitCode::FAILURE
        }
    }
}
