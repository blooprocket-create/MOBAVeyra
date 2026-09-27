//! `veyra-fake-game`: a stand-in for the game that speaks the launch handshake (ADR-010 §5), for
//! testing a launcher without the engine. It expects `-VeyraLaunchCode=stdin` first, as the game
//! does, and takes its behaviour from the arguments after it:
//!
//! - `--fake-delay-ms <n>`: wait this long before asking for the code, as a cold engine start does;
//! - `--fake-mode signed-in | failed:<code> | exit-early`: what it answers, or exit before asking;
//! - `--fake-record <file>`: write the code it received there, for a test to compare;
//! - `--fake-stay-ms <n>`: keep running this long after answering, as the game plays on.
//!
//! It writes a few lines that are not part of the handshake first, as the engine does, and checks
//! that its standard input closes after the code.

use std::io::{self, BufRead, Write};
use std::time::Duration;

fn main() {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    if arguments.first().map(String::as_str) != Some("-VeyraLaunchCode=stdin") {
        eprintln!("veyra-fake-game: the first argument must be -VeyraLaunchCode=stdin");
        std::process::exit(2);
    }
    let value = |name: &str| {
        arguments
            .iter()
            .position(|argument| argument == name)
            .and_then(|index| arguments.get(index + 1))
            .cloned()
    };
    let mode = value("--fake-mode").unwrap_or_else(|| "signed-in".to_string());
    let delay = value("--fake-delay-ms").and_then(|text| text.parse().ok()).unwrap_or(0);
    if mode == "exit-early" {
        std::process::exit(3);
    }

    let mut out = io::stdout().lock();
    // Lines a launcher must ignore, including one that only looks like the handshake.
    let _ = writeln!(out, "LogInit: Display: the fake game is starting");
    let _ = writeln!(out, "veyra-handoff/1 signed-in-early-but-not-really");
    let _ = out.flush();
    std::thread::sleep(Duration::from_millis(delay));
    let _ = writeln!(out, "veyra-handoff/1 awaiting-launch-code");
    let _ = out.flush();

    let stdin = io::stdin();
    let mut code = String::new();
    let _ = stdin.lock().read_line(&mut code);
    let code = code.trim_end().to_string();
    let mut rest = String::new();
    let closed = matches!(stdin.lock().read_line(&mut rest), Ok(0));
    if let Some(path) = value("--fake-record") {
        let _ = std::fs::write(path, &code);
    }
    let valid = code.len() == 4 + 43 && code.starts_with("vlc_") && closed;
    let answer = match (valid, mode.strip_prefix("failed:")) {
        (false, _) => "veyra-handoff/1 failed invalid_launch_code".to_string(),
        (true, Some(failure)) => format!("veyra-handoff/1 failed {failure}"),
        (true, None) => "veyra-handoff/1 signed-in".to_string(),
    };
    let _ = writeln!(out, "{answer}");
    let _ = writeln!(out, "LogVeyraClientFlow: Display: the fake game carries on");
    let _ = out.flush();
    // Playing on after signing in, as the game does once its launcher has gone.
    let stay = value("--fake-stay-ms").and_then(|text| text.parse().ok()).unwrap_or(0);
    std::thread::sleep(Duration::from_millis(stay));
}
