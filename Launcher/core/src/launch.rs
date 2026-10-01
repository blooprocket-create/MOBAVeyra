//! Launching the game for a signed-in player (ADR-005 L3–L4, ADR-010 §5, ADR-038): the game
//! started with pipes for its standard input and output, and the launch handshake. On success the
//! game keeps running and the launcher's work is done; on failure the game is stopped, so a retry
//! starts afresh.

use crate::backend::{Backend, BackendError, LauncherSession};
use crate::config::{LoadedConfig, LAUNCH_CODE_SWITCH};
use crate::handshake::{self, GameOutput, HandshakeError, Stage, Timing};
use crate::manifest::GameBuild;
use std::fmt;
use std::io::{self, BufRead as _, BufReader};
use std::path::Path;
use std::process::{Child, Command, Stdio};
use std::sync::mpsc::{channel, Receiver};
use std::thread;

/// Where a launch is, for the launcher's window and log.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LaunchStage {
    SigningIn,
    StartingGame,
    WaitingForGame,
    IssuingCode,
    WaitingForSignIn,
}

impl LaunchStage {
    /// The stage as the player reads it.
    pub fn describe(self) -> &'static str {
        match self {
            Self::SigningIn => "Signing in",
            Self::StartingGame => "Starting Veyra",
            Self::WaitingForGame => "Waiting for Veyra to start",
            Self::IssuingCode => "Handing over your session",
            Self::WaitingForSignIn => "Veyra is signing in",
        }
    }
}

/// A game that signed in. It keeps running; the launcher may close.
#[derive(Debug)]
pub struct Launched {
    pub process_id: u32,
    pub display_name: String,
}

#[derive(Debug)]
pub enum LaunchError {
    SignIn(BackendError),
    StartGame(io::ErrorKind),
    Handshake(HandshakeError),
}

impl fmt::Display for LaunchError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::SignIn(error) => write!(f, "Signing in failed: {error}."),
            Self::StartGame(kind) => write!(f, "Veyra could not be started ({kind})."),
            Self::Handshake(error) => {
                let text = error.to_string();
                let mut chars = text.chars();
                let first = chars.next().map(|c| c.to_uppercase().collect::<String>()).unwrap_or_default();
                write!(f, "{first}{}.", chars.as_str())
            }
        }
    }
}

impl std::error::Error for LaunchError {}

/// A started game: its process, its output line by line, and its standard input until the code is written.
pub struct GameProcess {
    child: Child,
    output: Receiver<GameOutput>,
    input: Option<std::process::ChildStdin>,
}

impl GameProcess {
    pub fn id(&self) -> u32 {
        self.child.id()
    }

    /// Stops the game, if it still runs.
    pub fn stop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// Starts `executable` with the launch-code switch and `arguments`, its standard input and output
/// as pipes and its standard error discarded. Its output is read on a thread of its own.
pub fn start_game(executable: &Path, arguments: &[String]) -> io::Result<GameProcess> {
    keep_own_standard_handles();
    let mut child = Command::new(executable)
        .arg(LAUNCH_CODE_SWITCH)
        .args(arguments)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .spawn()?;
    let input = child.stdin.take();
    let stdout = child.stdout.take().expect("the game's output is piped");
    let (sender, output) = channel();
    thread::Builder::new().name("veyra-game-output".to_string()).spawn(move || {
        let mut reader = BufReader::new(stdout);
        let mut line = Vec::new();
        loop {
            line.clear();
            match reader.read_until(b'\n', &mut line) {
                // The game's own log lines need not be UTF-8; the handshake's are ASCII.
                Ok(read) if read > 0 && sender.send(GameOutput::Line(String::from_utf8_lossy(&line).into_owned())).is_ok() => {}
                // The game closed its output, or the launcher stopped listening.
                _ => break,
            }
        }
        let _ = sender.send(GameOutput::Closed);
    })?;
    Ok(GameProcess { child, output, input })
}

/// Stops the game from inheriting the launcher's own standard handles. Windows hands a child every
/// inheritable handle its parent holds, not just the three it is given, so a game would otherwise
/// keep open the pipes of whoever reads the launcher's output, and they would wait for the game to
/// exit rather than for the launcher (ADR-005 L4: the launcher closes after a successful launch).
#[cfg(windows)]
fn keep_own_standard_handles() {
    use std::ffi::c_void;
    // winbase.h: the standard devices, as GetStdHandle numbers them, and HANDLE_FLAG_INHERIT.
    const STANDARD_HANDLES: [u32; 3] = [-10i32 as u32, -11i32 as u32, -12i32 as u32];
    const HANDLE_FLAG_INHERIT: u32 = 0x1;
    #[link(name = "kernel32")]
    extern "system" {
        fn GetStdHandle(standard_handle: u32) -> *mut c_void;
        fn SetHandleInformation(handle: *mut c_void, mask: u32, flags: u32) -> i32;
    }
    for standard_handle in STANDARD_HANDLES {
        // SAFETY: GetStdHandle takes any value and returns null or INVALID_HANDLE_VALUE when there is
        // no such handle; SetHandleInformation only changes a flag of a handle this process owns.
        unsafe {
            let handle = GetStdHandle(standard_handle);
            if !handle.is_null() && handle as isize != -1 {
                SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
            }
        }
    }
}

#[cfg(not(windows))]
fn keep_own_standard_handles() {
    // Elsewhere a child gets only the descriptors it is given: the standard library opens the rest
    // close-on-exec.
}

/// Signs in as the development account `account`, starts `build` and hands it a launch code. The
/// configuration's game arguments come first, then `extra_arguments`.
pub fn sign_in_and_launch(
    config: &LoadedConfig,
    build: &GameBuild,
    account: &str,
    extra_arguments: &[String],
    progress: &mut dyn FnMut(LaunchStage),
) -> Result<Launched, LaunchError> {
    progress(LaunchStage::SigningIn);
    let backend = Backend::new(&config.config.backend.base_url, config.http_timeout());
    let session = backend.dev_login(account).map_err(LaunchError::SignIn)?;
    launch(config, build, &session, extra_arguments, progress)
}

/// Starts `build` for the player signed in as `session` and hands it a launch code. The
/// configuration's game arguments come first, then `extra_arguments`.
pub fn launch(
    config: &LoadedConfig,
    build: &GameBuild,
    session: &LauncherSession,
    extra_arguments: &[String],
    progress: &mut dyn FnMut(LaunchStage),
) -> Result<Launched, LaunchError> {
    let backend = Backend::new(&config.config.backend.base_url, config.http_timeout());
    progress(LaunchStage::StartingGame);
    let mut arguments = config.config.game.arguments.clone();
    arguments.extend_from_slice(extra_arguments);
    let mut game = start_game(&build.executable, &arguments).map_err(|error| LaunchError::StartGame(error.kind()))?;
    let input = game.input.take().expect("the game's input is piped");
    let timing = Timing {
        await_ready: config.await_ready(),
        await_sign_in: config.await_sign_in(),
    };
    let mut issue = || backend.issue_launch_code(session, &build.version);
    let mut on_stage = |stage: Stage| {
        progress(match stage {
            Stage::WaitingForGame => LaunchStage::WaitingForGame,
            Stage::IssuingCode => LaunchStage::IssuingCode,
            Stage::WaitingForSignIn => LaunchStage::WaitingForSignIn,
        })
    };
    match handshake::run(&game.output, Box::new(input), timing, &mut issue, &mut on_stage) {
        Ok(()) => Ok(Launched {
            process_id: game.id(),
            display_name: session.display_name.clone(),
        }),
        Err(error) => {
            // A game that did not sign in cannot: its code works once. A retry starts a new one.
            game.stop();
            Err(LaunchError::Handshake(error))
        }
    }
}
