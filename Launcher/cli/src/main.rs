//! `veyra-launch-cli`: the launcher without its window, for scripts and automation (ADR-010 §5). It
//! uses the same core as the Tauri app: it signs in as a development account, starts the game build
//! the configuration names and hands it a launch code through the launch handshake, then exits,
//! leaving the game running (ADR-005 L4).
//!
//!     veyra-launch-cli [--config <file>] --account <name> [-- <more game arguments>]
//!     veyra-launch-cli [--config <file>] --accounts
//!
//! Progress goes to standard error. On success one line goes to standard output,
//! `veyra-launch signed-in pid <process id>`, for scripts that then follow the game. Exit status: 0
//! signed in; 1 the launch failed; 2 the command line or configuration is wrong. Nothing it prints
//! holds a credential.

use std::path::PathBuf;
use std::process::ExitCode;
use veyra_launcher_core::{backend::Backend, config, default_config_path, launch, manifest};

const USAGE: &str =
    "usage: veyra-launch-cli [--config <file>] --account <name> [-- <more game arguments>]\n       veyra-launch-cli [--config <file>] --accounts";

struct Arguments {
    config: Option<PathBuf>,
    account: Option<String>,
    list_accounts: bool,
    game_arguments: Vec<String>,
}

fn parse_arguments() -> Result<Arguments, String> {
    let mut parsed = Arguments {
        config: None,
        account: None,
        list_accounts: false,
        game_arguments: Vec::new(),
    };
    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "--config" => parsed.config = Some(PathBuf::from(arguments.next().ok_or("--config needs a file")?)),
            "--account" => parsed.account = Some(arguments.next().ok_or("--account needs a name")?),
            "--accounts" => parsed.list_accounts = true,
            "--" => parsed.game_arguments.extend(arguments.by_ref()),
            other => return Err(format!("unknown argument {other}")),
        }
    }
    if parsed.list_accounts == parsed.account.is_some() {
        return Err("give either --account or --accounts".to_string());
    }
    Ok(parsed)
}

fn main() -> ExitCode {
    let arguments = match parse_arguments() {
        Ok(arguments) => arguments,
        Err(problem) => {
            eprintln!("veyra-launch: {problem}\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    let config_path = arguments
        .config
        .clone()
        .unwrap_or_else(|| default_config_path(&std::env::current_exe().unwrap_or_default()));
    let loaded = match config::load(&config_path) {
        Ok(loaded) => loaded,
        Err(error) => {
            eprintln!("veyra-launch: {error}");
            return ExitCode::from(2);
        }
    };

    if arguments.list_accounts {
        return match Backend::new(&loaded.config.backend.base_url, loaded.http_timeout()).dev_accounts() {
            Ok(accounts) => {
                accounts.iter().for_each(|account| println!("{account}"));
                ExitCode::SUCCESS
            }
            Err(error) => {
                eprintln!("veyra-launch: {error}");
                ExitCode::FAILURE
            }
        };
    }

    let build = match manifest::load(&loaded.manifest_path) {
        Ok(build) => build,
        Err(error) => {
            eprintln!("veyra-launch: {error}");
            return ExitCode::from(2);
        }
    };
    let account = arguments.account.expect("checked by parse_arguments");
    eprintln!("veyra-launch: launching build {} as {account}.", build.version);
    let mut progress = |stage: launch::LaunchStage| eprintln!("veyra-launch: {}.", stage.describe());
    match launch::sign_in_and_launch(&loaded, &build, &account, &arguments.game_arguments, &mut progress) {
        Ok(launched) => {
            eprintln!(
                "veyra-launch: {} is signed in; Veyra runs as process {}.",
                launched.display_name, launched.process_id
            );
            println!("veyra-launch signed-in pid {}", launched.process_id);
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("veyra-launch: {error}");
            ExitCode::FAILURE
        }
    }
}
