//! The launcher's side of the launch handshake (ADR-010 §5; the lines are fixed by
//! Game/Source/VeyraServices/Contracts/LaunchHandshake.json). The game says on its standard output
//! when it can read its launch code; only then is the code requested, because it lives only
//! seconds, and written to the game's standard input, which is then closed. The game answers that
//! it signed in, or why it could not. Any other line is ignored.

use crate::backend::BackendError;
use crate::secret::Secret;
use std::fmt;
use std::io::{self, Write};
use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::{Duration, Instant};

/// The game can read its launch code.
pub const AWAITING_LAUNCH_CODE: &str = "veyra-handoff/1 awaiting-launch-code";
/// The backend redeemed the code: the launcher's work is done.
pub const SIGNED_IN: &str = "veyra-handoff/1 signed-in";
/// A failure line is this, a space and one failure code.
pub const FAILED_PREFIX: &str = "veyra-handoff/1 failed";
/// The failure codes a game reports, in the contract's order.
pub const FAILURE_CODES: [&str; 6] = [
    "no_launch_code",
    "invalid_launch_code",
    "sign_in_refused",
    "backend_unreachable",
    "bad_answer",
    "misconfigured",
];

/// What the game's standard output gave.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum GameOutput {
    Line(String),
    /// The game closed its output: it exited.
    Closed,
}

/// Where the handshake is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Stage {
    /// The game is starting; it has not yet asked for its code.
    WaitingForGame,
    /// The launcher is asking the backend for the code.
    IssuingCode,
    /// The code was written; the game is signing in.
    WaitingForSignIn,
}

/// How long each stage may take.
#[derive(Debug, Clone, Copy)]
pub struct Timing {
    pub await_ready: Duration,
    pub await_sign_in: Duration,
}

#[derive(Debug)]
pub enum HandshakeError {
    /// The game did not reach the next line in time.
    TimedOut(Stage),
    /// The game's output ended: it exited.
    GameStopped(Stage),
    /// The backend gave no launch code.
    NoLaunchCode(BackendError),
    /// The code could not be written to the game.
    CouldNotWrite(io::ErrorKind),
    /// The game said why it could not sign in: one of [`FAILURE_CODES`], or another word.
    SignInFailed(String),
}

impl fmt::Display for HandshakeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::TimedOut(Stage::WaitingForGame) => f.write_str("the game did not start in time"),
            Self::TimedOut(_) => f.write_str("the game did not sign in in time"),
            Self::GameStopped(Stage::WaitingForGame) => f.write_str("the game closed before it could sign in"),
            Self::GameStopped(_) => f.write_str("the game closed while signing in"),
            Self::NoLaunchCode(error) => write!(f, "no launch code: {error}"),
            Self::CouldNotWrite(kind) => write!(f, "the launch code could not be given to the game ({kind})"),
            Self::SignInFailed(code) => write!(f, "the game could not sign in: {}", describe_failure(code)),
        }
    }
}

impl std::error::Error for HandshakeError {}

/// A game's failure code, as a player reads it.
pub fn describe_failure(code: &str) -> String {
    match code {
        "no_launch_code" => "it received no launch code".to_string(),
        "invalid_launch_code" => "it received something that is not a launch code".to_string(),
        "sign_in_refused" => "Veyra's services refused its launch code".to_string(),
        "backend_unreachable" => "it could not reach Veyra's services".to_string(),
        "bad_answer" => "it did not understand Veyra's services".to_string(),
        "misconfigured" => "its own settings are wrong".to_string(),
        other => format!("it reported {other}"),
    }
}

/// Runs the handshake with a started game. `output` carries its standard output, line by line;
/// `input` is its standard input, closed once the code is written. `issue` asks the backend for a
/// code and is called at most once, and only after the game asked. `progress` hears each stage.
pub fn run(
    output: &Receiver<GameOutput>,
    input: Box<dyn Write + Send>,
    timing: Timing,
    issue: &mut dyn FnMut() -> Result<Secret, BackendError>,
    progress: &mut dyn FnMut(Stage),
) -> Result<(), HandshakeError> {
    let mut input = Some(input);
    let mut stage = Stage::WaitingForGame;
    progress(stage);
    let mut deadline = Instant::now() + timing.await_ready;
    loop {
        let line = match output.recv_timeout(deadline.saturating_duration_since(Instant::now())) {
            Ok(GameOutput::Line(line)) => line,
            Ok(GameOutput::Closed) | Err(RecvTimeoutError::Disconnected) => return Err(HandshakeError::GameStopped(stage)),
            Err(RecvTimeoutError::Timeout) => return Err(HandshakeError::TimedOut(stage)),
        };
        let line = line.trim_end_matches(['\r', '\n']);
        if line == AWAITING_LAUNCH_CODE && stage == Stage::WaitingForGame {
            stage = Stage::IssuingCode;
            progress(stage);
            let mut writer = input.take().expect("the code is written once");
            let code = issue().map_err(HandshakeError::NoLaunchCode)?;
            writer
                .write_all(code.expose().as_bytes())
                .and_then(|()| writer.write_all(b"\n"))
                .and_then(|()| writer.flush())
                .map_err(|error| HandshakeError::CouldNotWrite(error.kind()))?;
            // Closing standard input tells the game nothing more is coming.
            drop(writer);
            stage = Stage::WaitingForSignIn;
            progress(stage);
            deadline = Instant::now() + timing.await_sign_in;
        } else if line == SIGNED_IN && stage == Stage::WaitingForSignIn {
            return Ok(());
        } else if let Some(code) = line.strip_prefix(FAILED_PREFIX).and_then(|rest| rest.strip_prefix(' ')) {
            return Err(HandshakeError::SignInFailed(code.to_string()));
        }
        // Anything else is not part of the handshake.
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::mpsc::{channel, Sender};
    use std::sync::{Arc, Mutex};
    use std::thread;

    /// What the fake game's standard input received, and whether it was closed.
    #[derive(Default)]
    struct Received {
        bytes: Vec<u8>,
        closed: bool,
    }

    struct FakeInput(Arc<Mutex<Received>>);

    impl Write for FakeInput {
        fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
            self.0.lock().unwrap().bytes.extend_from_slice(buf);
            Ok(buf.len())
        }
        fn flush(&mut self) -> io::Result<()> {
            Ok(())
        }
    }

    impl Drop for FakeInput {
        fn drop(&mut self) {
            self.0.lock().unwrap().closed = true;
        }
    }

    fn code() -> Secret {
        Secret::new(format!("vlc_{}", "A".repeat(43)))
    }

    fn timing() -> Timing {
        Timing {
            await_ready: Duration::from_secs(5),
            await_sign_in: Duration::from_secs(5),
        }
    }

    fn say(game: &Sender<GameOutput>, line: &str) {
        game.send(GameOutput::Line(line.to_string())).unwrap();
    }

    #[test]
    fn the_code_is_issued_only_once_the_game_asks() {
        let (game, output) = channel();
        let received = Arc::new(Mutex::new(Received::default()));
        let issued = Arc::new(Mutex::new(0));
        let (launcher_received, launcher_issued) = (received.clone(), issued.clone());
        let launcher = thread::spawn(move || {
            let mut issue = || {
                *launcher_issued.lock().unwrap() += 1;
                Ok(code())
            };
            run(&output, Box::new(FakeInput(launcher_received)), timing(), &mut issue, &mut |_| {})
        });
        say(&game, "LogInit: Display: the engine is starting");
        say(&game, "veyra-handoff/1 signed-in");
        thread::sleep(Duration::from_millis(100));
        assert_eq!(*issued.lock().unwrap(), 0, "nothing is issued before the game asks");
        say(&game, AWAITING_LAUNCH_CODE);
        thread::sleep(Duration::from_millis(100));
        assert_eq!(*issued.lock().unwrap(), 1);
        {
            let received = received.lock().unwrap();
            assert_eq!(received.bytes, format!("{}\n", code().expose()).into_bytes());
            assert!(received.closed, "standard input is closed after the code");
        }
        say(&game, AWAITING_LAUNCH_CODE);
        say(&game, SIGNED_IN);
        assert!(launcher.join().unwrap().is_ok());
        assert_eq!(*issued.lock().unwrap(), 1, "one code, however often the game asks");
    }

    #[test]
    fn a_slow_start_still_signs_in() {
        let (game, output) = channel();
        let started = Instant::now();
        let asked_at = Arc::new(Mutex::new(None));
        let launcher_asked_at = asked_at.clone();
        let launcher = thread::spawn(move || {
            let mut issue = || {
                *launcher_asked_at.lock().unwrap() = Some(Instant::now());
                Ok(code())
            };
            let timing = Timing {
                await_ready: Duration::from_secs(3),
                await_sign_in: Duration::from_millis(500),
            };
            run(&output, Box::new(FakeInput(Arc::default())), timing, &mut issue, &mut |_| {})
        });
        // Longer than the sign-in wait: the code's clock starts only when the game asks.
        thread::sleep(Duration::from_millis(800));
        say(&game, AWAITING_LAUNCH_CODE);
        say(&game, SIGNED_IN);
        assert!(launcher.join().unwrap().is_ok());
        assert!(asked_at.lock().unwrap().unwrap() - started >= Duration::from_millis(800));
    }

    #[test]
    fn a_failure_line_names_the_games_reason() {
        let (game, output) = channel();
        say(&game, AWAITING_LAUNCH_CODE);
        say(&game, "veyra-handoff/1 failed sign_in_refused");
        let result = run(&output, Box::new(FakeInput(Arc::default())), timing(), &mut || Ok(code()), &mut |_| {});
        let error = result.expect_err("the game failed");
        assert!(matches!(&error, HandshakeError::SignInFailed(code) if code == "sign_in_refused"));
        assert_eq!(error.to_string(), "the game could not sign in: Veyra's services refused its launch code");
    }

    #[test]
    fn a_game_that_exits_early_ends_the_handshake() {
        let (game, output) = channel();
        say(&game, "LogInit: Display: starting");
        game.send(GameOutput::Closed).unwrap();
        let mut issued = false;
        let result = run(
            &output,
            Box::new(FakeInput(Arc::default())),
            timing(),
            &mut || {
                issued = true;
                Ok(code())
            },
            &mut |_| {},
        );
        assert!(matches!(result, Err(HandshakeError::GameStopped(Stage::WaitingForGame))));
        assert!(!issued);
    }

    #[test]
    fn each_stage_has_its_own_deadline() {
        let (_game, output) = channel::<GameOutput>();
        let quick = Timing {
            await_ready: Duration::from_millis(50),
            await_sign_in: Duration::from_secs(5),
        };
        let result = run(&output, Box::new(FakeInput(Arc::default())), quick, &mut || Ok(code()), &mut |_| {});
        assert!(matches!(result, Err(HandshakeError::TimedOut(Stage::WaitingForGame))));

        let (game, output) = channel();
        say(&game, AWAITING_LAUNCH_CODE);
        let quick = Timing {
            await_ready: Duration::from_secs(5),
            await_sign_in: Duration::from_millis(50),
        };
        let mut stages = Vec::new();
        let result = run(&output, Box::new(FakeInput(Arc::default())), quick, &mut || Ok(code()), &mut |stage| {
            stages.push(stage)
        });
        assert!(matches!(result, Err(HandshakeError::TimedOut(Stage::WaitingForSignIn))));
        assert_eq!(stages, [Stage::WaitingForGame, Stage::IssuingCode, Stage::WaitingForSignIn]);
    }

    #[test]
    fn a_refused_code_request_is_reported_without_writing() {
        let (game, output) = channel();
        say(&game, AWAITING_LAUNCH_CODE);
        let received = Arc::new(Mutex::new(Received::default()));
        let refused = BackendError::Refused {
            status: 401,
            code: "invalid_credentials".to_string(),
        };
        let result = run(
            &output,
            Box::new(FakeInput(received.clone())),
            timing(),
            &mut || Err(refused.clone()),
            &mut |_| {},
        );
        assert!(matches!(result, Err(HandshakeError::NoLaunchCode(_))));
        let received = received.lock().unwrap();
        assert!(received.bytes.is_empty() && received.closed, "the game's input is closed with nothing on it");
    }

    #[test]
    fn the_lines_are_the_contracts() {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../Game/Source/VeyraServices/Contracts/LaunchHandshake.json");
        let contract: serde_json::Value = serde_json::from_str(&std::fs::read_to_string(&path).expect("read the contract")).expect("the contract is JSON");
        assert_eq!(contract["awaitingLaunchCode"], AWAITING_LAUNCH_CODE);
        assert_eq!(contract["signedIn"], SIGNED_IN);
        assert_eq!(contract["failedPrefix"], FAILED_PREFIX);
        let codes: Vec<&str> = contract["failureCodes"]
            .as_array()
            .expect("a list")
            .iter()
            .map(|code| code.as_str().expect("a word"))
            .collect();
        assert_eq!(codes, FAILURE_CODES);
    }
}
