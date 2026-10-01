//! A player's account in the launcher (ADR-038): registering, signing in and choosing a display
//! name. Firebase proves who the player is; the backend turns that proof into a Veyra launcher
//! session, which is all the rest of the launcher ever holds.
//!
//! Registering is two steps that cannot be one transaction: Firebase creates the user, then the
//! backend creates the Veyra account with the chosen name. If the second step fails (the name was
//! taken in between, or the backend was down), the player is left with a Firebase user and no
//! Veyra account. Signing in then answers [`SignIn::ChooseName`], holding the proof, and
//! [`choose_name`] finishes the account; nothing is lost and no one is stuck.

use crate::backend::{Backend, BackendError, LauncherSession};
use crate::config::LoadedConfig;
use crate::firebase::{Firebase, FirebaseError};
use crate::secret::Secret;
use std::fmt;

/// Display-name rules, as the backend enforces them (Backend/internal/identity; ADR-038 §4,
/// provisional). Checked here first so a bad name never creates a Firebase user.
pub const MIN_DISPLAY_NAME_LENGTH: usize = 3;
pub const MAX_DISPLAY_NAME_LENGTH: usize = 16;

/// What signing in or registering came to.
#[derive(Debug)]
pub enum SignIn {
    /// Signed in: the launcher may launch.
    SignedIn(LauncherSession),
    /// Firebase knows the player, but they have no Veyra account yet. The proof is kept for
    /// [`choose_name`]; it lasts an hour.
    ChooseName(Secret),
}

/// Why a player could not sign in or register. Its `Display` is a sentence for the player.
#[derive(Debug)]
pub enum PlayerError {
    /// This launcher's configuration has no player sign-in.
    NotConfigured,
    InvalidDisplayName,
    DisplayNameTaken,
    Firebase(FirebaseError),
    Backend(BackendError),
}

impl fmt::Display for PlayerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NotConfigured => f.write_str("This launcher is not set up for player accounts"),
            Self::InvalidDisplayName => write!(
                f,
                "A name is {MIN_DISPLAY_NAME_LENGTH} to {MAX_DISPLAY_NAME_LENGTH} letters, digits or underscores"
            ),
            Self::DisplayNameTaken => f.write_str("That name is taken. Choose another"),
            Self::Firebase(error) => error.fmt(f),
            Self::Backend(BackendError::Refused { code, .. }) if code == "already_registered" => {
                f.write_str("This email already has a Veyra account. Sign in instead")
            }
            Self::Backend(BackendError::Refused { code, .. }) if code == "invalid_credentials" => f.write_str("Your sign-in has expired. Sign in again"),
            Self::Backend(BackendError::Refused { code, .. }) if code == "not_found" => f.write_str("Veyra's services do not accept player accounts yet"),
            Self::Backend(error) => error.fmt(f),
        }
    }
}

impl std::error::Error for PlayerError {}

impl From<FirebaseError> for PlayerError {
    fn from(error: FirebaseError) -> Self {
        Self::Firebase(error)
    }
}

impl From<BackendError> for PlayerError {
    fn from(error: BackendError) -> Self {
        match &error {
            BackendError::Refused { code, .. } if code == "display_name_taken" => Self::DisplayNameTaken,
            BackendError::Refused { code, .. } if code == "invalid_display_name" => Self::InvalidDisplayName,
            _ => Self::Backend(error),
        }
    }
}

/// Whether this launcher's configuration offers player sign-in.
pub fn available(config: &LoadedConfig) -> bool {
    config.config.player_login.is_some()
}

/// Whether `name` keeps the display-name rules.
pub fn is_valid_display_name(name: &str) -> bool {
    (MIN_DISPLAY_NAME_LENGTH..=MAX_DISPLAY_NAME_LENGTH).contains(&name.len()) && name.bytes().all(|byte| byte.is_ascii_alphanumeric() || byte == b'_')
}

/// Signs in with an email and password.
pub fn sign_in(config: &LoadedConfig, email: &str, password: &str) -> Result<SignIn, PlayerError> {
    let token = firebase(config)?.sign_in(email.trim(), password)?;
    match backend(config).player_login(&token) {
        Ok(session) => Ok(SignIn::SignedIn(session)),
        Err(BackendError::Refused { code, .. }) if code == "not_registered" => Ok(SignIn::ChooseName(token)),
        Err(error) => Err(error.into()),
    }
}

/// Registers a new player: a Firebase user with this email and password, then their Veyra account
/// named `display_name`. If the second step fails, see the module's notes.
pub fn register(config: &LoadedConfig, email: &str, password: &str, display_name: &str) -> Result<SignIn, PlayerError> {
    if !is_valid_display_name(display_name) {
        return Err(PlayerError::InvalidDisplayName);
    }
    let token = firebase(config)?.sign_up(email.trim(), password)?;
    match backend(config).register(&token, display_name) {
        Ok(session) => Ok(SignIn::SignedIn(session)),
        // The Firebase user exists now; the player picks another name with the same proof.
        Err(BackendError::Refused { code, .. }) if code == "display_name_taken" => Ok(SignIn::ChooseName(token)),
        Err(error) => Err(error.into()),
    }
}

/// Finishes an account Firebase knows but Veyra does not, with the name the player chose.
pub fn choose_name(config: &LoadedConfig, provider_token: &Secret, display_name: &str) -> Result<LauncherSession, PlayerError> {
    if !is_valid_display_name(display_name) {
        return Err(PlayerError::InvalidDisplayName);
    }
    Ok(backend(config).register(provider_token, display_name)?)
}

/// Asks Firebase to email a password-reset link.
pub fn send_password_reset(config: &LoadedConfig, email: &str) -> Result<(), PlayerError> {
    Ok(firebase(config)?.send_password_reset(email.trim())?)
}

fn firebase(config: &LoadedConfig) -> Result<Firebase, PlayerError> {
    let login = config.config.player_login.as_ref().ok_or(PlayerError::NotConfigured)?;
    Ok(Firebase::new(&login.firebase.auth_url, &login.firebase.api_key, config.http_timeout()))
}

fn backend(config: &LoadedConfig) -> Backend {
    Backend::new(&config.config.backend.base_url, config.http_timeout())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_names_follow_the_backends_rules() {
        for good in ["Ember", "abc", "Ember_Wing_12345", "___"] {
            assert!(is_valid_display_name(good), "{good}");
        }
        for bad in ["ab", "Ember_Wing_123456", "Ember Wing", " Ember", "Émber", ""] {
            assert!(!is_valid_display_name(bad), "{bad}");
        }
    }

    #[test]
    fn backend_refusals_read_as_sentences() {
        let refused = |code: &str| BackendError::Refused {
            status: 409,
            code: code.to_string(),
        };
        assert!(matches!(PlayerError::from(refused("display_name_taken")), PlayerError::DisplayNameTaken));
        assert!(matches!(PlayerError::from(refused("invalid_display_name")), PlayerError::InvalidDisplayName));
        assert_eq!(
            PlayerError::from(refused("already_registered")).to_string(),
            "This email already has a Veyra account. Sign in instead"
        );
    }
}
