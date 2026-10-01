//! Firebase Authentication, the identity provider players register and sign in with (ADR-038). The
//! launcher speaks Firebase's REST API itself, from Rust over rustls like every other request, and
//! keeps only the ID token it is given: a short-lived proof of who signed in, which it trades with
//! Veyra's backend for a Veyra session. Passwords go only to Firebase, and nothing here is written
//! to disk or shown in a message.

use crate::secret::Secret;
use serde::Deserialize;
use std::fmt;
use std::time::Duration;
use ureq::Agent;

/// The longest ID token accepted; real ones are about 1 KB (the backend's limit is 8 KB).
const MAX_ID_TOKEN_LENGTH: usize = 8 * 1024;

/// Why Firebase did not sign someone in, register them or send a reset email.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FirebaseError {
    /// No answer: Firebase is unreachable, or the request timed out.
    Unreachable(String),
    /// Firebase refused, with its reason.
    Refused(Refusal),
    /// The answer was not what the route returns.
    BadAnswer(String),
}

/// Firebase's reasons for refusing, as the launcher tells them apart.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Refusal {
    EmailExists,
    WrongEmailOrPassword,
    WeakPassword,
    InvalidEmail,
    MissingPassword,
    TooManyAttempts,
    UserDisabled,
    /// Email and password sign-in is switched off in the Firebase project.
    NotEnabled,
    /// The launcher's API key is wrong or restricted.
    BadApiKey,
    /// Anything else: Firebase's code, a word in capitals.
    Other(String),
}

impl Refusal {
    fn from_message(message: &str) -> Self {
        // Firebase's message is a code, sometimes followed by " : " and an explanation.
        let code = message.split(" : ").next().unwrap_or_default().trim();
        match code {
            "EMAIL_EXISTS" => Self::EmailExists,
            "INVALID_LOGIN_CREDENTIALS" | "INVALID_PASSWORD" | "EMAIL_NOT_FOUND" => Self::WrongEmailOrPassword,
            "WEAK_PASSWORD" => Self::WeakPassword,
            "INVALID_EMAIL" | "MISSING_EMAIL" => Self::InvalidEmail,
            "MISSING_PASSWORD" => Self::MissingPassword,
            "TOO_MANY_ATTEMPTS_TRY_LATER" => Self::TooManyAttempts,
            "USER_DISABLED" => Self::UserDisabled,
            "OPERATION_NOT_ALLOWED" | "PASSWORD_LOGIN_DISABLED" => Self::NotEnabled,
            _ if code.starts_with("API_KEY") || code == "INVALID_API_KEY" => Self::BadApiKey,
            _ if !code.is_empty() && code.bytes().all(|byte| byte.is_ascii_uppercase() || byte == b'_') => Self::Other(code.to_string()),
            _ => Self::Other(String::new()),
        }
    }
}

impl fmt::Display for FirebaseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Unreachable(_) => f.write_str("Veyra's sign-in service did not answer. Check your connection and try again"),
            Self::BadAnswer(_) => f.write_str("Veyra's sign-in service gave an answer the launcher does not understand"),
            Self::Refused(refusal) => f.write_str(match refusal {
                Refusal::EmailExists => "An account already uses that email. Sign in instead",
                Refusal::WrongEmailOrPassword => "That email and password do not match an account",
                Refusal::WeakPassword => "Choose a password of at least 6 characters",
                Refusal::InvalidEmail => "Enter a valid email address",
                Refusal::MissingPassword => "Enter your password",
                Refusal::TooManyAttempts => "Too many attempts. Wait a few minutes and try again",
                Refusal::UserDisabled => "This account has been disabled",
                Refusal::NotEnabled => "Signing in with email is not switched on for Veyra yet",
                Refusal::BadApiKey => "The launcher's sign-in settings are wrong; reinstall the launcher",
                Refusal::Other(_) => "Veyra's sign-in service refused. Try again later",
            }),
        }
    }
}

impl std::error::Error for FirebaseError {}

/// Firebase Authentication for one project, reached with its web API key. The key identifies the
/// project to Firebase; it is not a secret and grants nothing on its own.
pub struct Firebase {
    base_url: String,
    api_key: String,
    agent: Agent,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct TokenAnswer {
    id_token: String,
}

#[derive(Deserialize)]
struct ErrorAnswer {
    error: ErrorBody,
}

#[derive(Deserialize)]
struct ErrorBody {
    #[serde(default)]
    message: String,
}

impl Firebase {
    /// `base_url` is Firebase's Identity Toolkit, https://identitytoolkit.googleapis.com.
    pub fn new(base_url: &str, api_key: &str, timeout: Duration) -> Self {
        let config = Agent::config_builder().timeout_global(Some(timeout)).http_status_as_error(false).build();
        Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            api_key: api_key.to_string(),
            agent: Agent::new_with_config(config),
        }
    }

    /// Creates a Firebase user with this email and password; returns its ID token.
    pub fn sign_up(&self, email: &str, password: &str) -> Result<Secret, FirebaseError> {
        self.token("accounts:signUp", email, password)
    }

    /// Signs in with this email and password; returns the user's ID token.
    pub fn sign_in(&self, email: &str, password: &str) -> Result<Secret, FirebaseError> {
        self.token("accounts:signInWithPassword", email, password)
    }

    /// Asks Firebase to email a password-reset link. Firebase answers the same whether or not the
    /// email has an account (with email enumeration protection on), so this says nothing about it.
    pub fn send_password_reset(&self, email: &str) -> Result<(), FirebaseError> {
        let body = serde_json::json!({ "requestType": "PASSWORD_RESET", "email": email });
        let _: serde_json::Value = self.post("accounts:sendOobCode", &body)?;
        Ok(())
    }

    fn token(&self, route: &str, email: &str, password: &str) -> Result<Secret, FirebaseError> {
        let body = serde_json::json!({ "email": email, "password": password, "returnSecureToken": true });
        let answer: TokenAnswer = self.post(route, &body)?;
        if !is_id_token(&answer.id_token) {
            return Err(FirebaseError::BadAnswer("the ID token is not in the expected format".to_string()));
        }
        Ok(Secret::new(answer.id_token))
    }

    fn post<T: serde::de::DeserializeOwned>(&self, route: &str, body: &serde_json::Value) -> Result<T, FirebaseError> {
        let url = format!("{}/v1/{route}?key={}", self.base_url, self.api_key);
        let result = self.agent.post(url).header("Accept", "application/json").send_json(body);
        // The request's error may quote its URL, which holds only the API key; never its body.
        let mut response = result.map_err(|error| FirebaseError::Unreachable(error.to_string()))?;
        let status = response.status().as_u16();
        if !(200..300).contains(&status) {
            let message = response
                .body_mut()
                .read_json::<ErrorAnswer>()
                .map(|answer| answer.error.message)
                .unwrap_or_default();
            return Err(FirebaseError::Refused(Refusal::from_message(&message)));
        }
        // The body is never quoted: it holds tokens.
        response
            .body_mut()
            .read_json::<T>()
            .map_err(|_| FirebaseError::BadAnswer("the answer is not the JSON this route returns".to_string()))
    }
}

/// Whether `text` looks like a JWT: three base64url parts, within the length the backend accepts.
fn is_id_token(text: &str) -> bool {
    let parts: Vec<&str> = text.split('.').collect();
    text.len() <= MAX_ID_TOKEN_LENGTH
        && parts.len() == 3
        && parts
            .iter()
            .all(|part| !part.is_empty() && part.bytes().all(|byte| byte.is_ascii_alphanumeric() || byte == b'-' || byte == b'_'))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn firebase_messages_become_refusals() {
        assert_eq!(Refusal::from_message("EMAIL_EXISTS"), Refusal::EmailExists);
        assert_eq!(Refusal::from_message("INVALID_LOGIN_CREDENTIALS"), Refusal::WrongEmailOrPassword);
        assert_eq!(Refusal::from_message("EMAIL_NOT_FOUND"), Refusal::WrongEmailOrPassword);
        assert_eq!(
            Refusal::from_message("WEAK_PASSWORD : Password should be at least 6 characters"),
            Refusal::WeakPassword
        );
        assert_eq!(Refusal::from_message("TOO_MANY_ATTEMPTS_TRY_LATER"), Refusal::TooManyAttempts);
        assert_eq!(Refusal::from_message("API_KEY_INVALID"), Refusal::BadApiKey);
        assert_eq!(Refusal::from_message("SOMETHING_NEW"), Refusal::Other("SOMETHING_NEW".to_string()));
        // Prose is never carried into a message.
        assert_eq!(Refusal::from_message("<html> oops"), Refusal::Other(String::new()));
    }

    #[test]
    fn a_wrong_password_and_an_unknown_email_read_the_same() {
        let wrong = FirebaseError::Refused(Refusal::from_message("INVALID_PASSWORD")).to_string();
        let unknown = FirebaseError::Refused(Refusal::from_message("EMAIL_NOT_FOUND")).to_string();
        assert_eq!(wrong, unknown);
    }

    #[test]
    fn id_tokens_are_jwts() {
        assert!(is_id_token("eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ4In0.c2ln"));
        assert!(!is_id_token("not-a-token"));
        assert!(!is_id_token("a..c"));
        assert!(!is_id_token("a.b.c d"));
        assert!(!is_id_token(&format!("a.{}.c", "b".repeat(MAX_ID_TOKEN_LENGTH))));
    }
}
