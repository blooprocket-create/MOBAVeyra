//! The launcher's conversation with the backend (ADR-005 L3): the development sign-in, and a launch
//! code for the game. Every request goes from Rust over rustls; a credential goes only in the
//! Authorization header, and no error ever quotes a response body.

use crate::secret::{self, Secret};
use serde::Deserialize;
use std::fmt;
use std::time::Duration;
use ureq::Agent;

/// The longest display name the launcher shows.
const MAX_DISPLAY_NAME_LENGTH: usize = 64;

/// The launcher's session: who is signed in. Kept in memory only (ADR-005 L4: no remembered login
/// until an identity provider is chosen).
#[derive(Debug, Clone)]
pub struct LauncherSession {
    token: Secret,
    pub account_id: String,
    pub display_name: String,
}

/// Why a request to the backend did not give what was asked.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BackendError {
    /// No answer: the backend is down or unreachable, or the request timed out.
    Unreachable(String),
    /// The backend answered with an error status and, usually, an error code.
    Refused { status: u16, code: String },
    /// The answer was not what the route returns.
    BadAnswer(String),
}

impl fmt::Display for BackendError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Unreachable(reason) => write!(f, "Veyra's services did not answer ({})", secret::redact(reason)),
            Self::Refused { status, code } if code.is_empty() => write!(f, "Veyra's services refused (HTTP {status})"),
            Self::Refused { status, code } => write!(f, "Veyra's services refused (HTTP {status}, {code})"),
            Self::BadAnswer(problem) => write!(f, "Veyra's services gave an answer the launcher does not understand: {problem}"),
        }
    }
}

impl std::error::Error for BackendError {}

/// The backend at one base URL.
pub struct Backend {
    base_url: String,
    agent: Agent,
}

#[derive(Deserialize)]
struct TokenAnswer {
    token: String,
    account: Option<AccountAnswer>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct AccountAnswer {
    id: String,
    display_name: String,
}

#[derive(Deserialize)]
struct AccountsAnswer {
    accounts: Vec<DevAccountAnswer>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct DevAccountAnswer {
    display_name: String,
}

#[derive(Deserialize)]
struct ErrorAnswer {
    error: String,
}

impl Backend {
    pub fn new(base_url: &str, timeout: Duration) -> Self {
        let config = Agent::config_builder().timeout_global(Some(timeout)).http_status_as_error(false).build();
        Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            agent: Agent::new_with_config(config),
        }
    }

    /// The development accounts to sign in as (a local backend only: it has no passwords).
    pub fn dev_accounts(&self) -> Result<Vec<String>, BackendError> {
        let answer: AccountsAnswer = self.get("/v1/dev/accounts")?;
        let names: Vec<String> = answer.accounts.into_iter().map(|account| account.display_name).collect();
        if names.iter().any(|name| !is_display_name(name)) {
            return Err(BackendError::BadAnswer("an account's name is empty or too long".to_string()));
        }
        Ok(names)
    }

    /// Signs in as a development account (ADR-010 §5: until an identity provider is chosen).
    pub fn dev_login(&self, account: &str) -> Result<LauncherSession, BackendError> {
        let answer: TokenAnswer = self.post("/v1/dev/login", None, &serde_json::json!({ "accountName": account }))?;
        let Some(account) = answer.account else {
            return Err(BackendError::BadAnswer("the sign-in named no account".to_string()));
        };
        if !secret::is_credential(&answer.token, "vls_") || account.id.is_empty() || !is_display_name(&account.display_name) {
            return Err(BackendError::BadAnswer(
                "the sign-in's session or account is not in the expected format".to_string(),
            ));
        }
        Ok(LauncherSession {
            token: Secret::new(answer.token),
            account_id: account.id,
            display_name: account.display_name,
        })
    }

    /// A single-use launch code for `build_version`, which lives only seconds (ADR-005 L3). Ask for
    /// it only once the game is ready to read it (ADR-010 §5).
    pub fn issue_launch_code(&self, session: &LauncherSession, build_version: &str) -> Result<Secret, BackendError> {
        let answer: TokenAnswer = self.post("/v1/launch-codes", Some(&session.token), &serde_json::json!({ "buildVersion": build_version }))?;
        if !secret::is_credential(&answer.token, "vlc_") {
            return Err(BackendError::BadAnswer("the launch code is not in the expected format".to_string()));
        }
        Ok(Secret::new(answer.token))
    }

    fn get<T: serde::de::DeserializeOwned>(&self, path: &str) -> Result<T, BackendError> {
        let request = self.agent.get(format!("{}{path}", self.base_url)).header("Accept", "application/json");
        Self::read(request.call())
    }

    fn post<T: serde::de::DeserializeOwned>(&self, path: &str, credential: Option<&Secret>, body: &serde_json::Value) -> Result<T, BackendError> {
        let mut request = self.agent.post(format!("{}{path}", self.base_url)).header("Accept", "application/json");
        if let Some(credential) = credential {
            request = request.header("Authorization", format!("Bearer {}", credential.expose()));
        }
        Self::read(request.send_json(body))
    }

    fn read<T: serde::de::DeserializeOwned>(result: Result<ureq::http::Response<ureq::Body>, ureq::Error>) -> Result<T, BackendError> {
        let mut response = result.map_err(|error| BackendError::Unreachable(error.to_string()))?;
        let status = response.status().as_u16();
        if !(200..300).contains(&status) {
            let code = response.body_mut().read_json::<ErrorAnswer>().map(|answer| answer.error).unwrap_or_default();
            // An error code is a word; anything else is not shown.
            let code = if code.bytes().all(|byte| byte.is_ascii_lowercase() || byte == b'_') {
                code
            } else {
                String::new()
            };
            return Err(BackendError::Refused { status, code });
        }
        // The body is never quoted: it may hold a credential.
        response
            .body_mut()
            .read_json::<T>()
            .map_err(|_| BackendError::BadAnswer("the answer is not the JSON this route returns".to_string()))
    }
}

fn is_display_name(name: &str) -> bool {
    !name.trim().is_empty() && name.chars().count() <= MAX_DISPLAY_NAME_LENGTH
}
