//! Credentials the launcher holds: its session and the launch codes it issues (ADR-005 L3). They are
//! kept in memory only, and nothing the launcher prints or logs can show one.

use std::fmt;

/// A credential. Its `Debug` and `Display` forms never show it; only [`Secret::expose`] does, for the
/// one place it must go: an HTTP header or the game's standard input.
#[derive(Clone, PartialEq, Eq)]
pub struct Secret(String);

impl Secret {
    pub fn new(value: impl Into<String>) -> Self {
        Self(value.into())
    }

    /// The credential itself. Never format it into a message.
    pub fn expose(&self) -> &str {
        &self.0
    }
}

impl fmt::Debug for Secret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("Secret(<redacted>)")
    }
}

impl fmt::Display for Secret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("<redacted>")
    }
}

/// The prefixes of Veyra's credentials (Backend/internal/secret): launcher sessions, game sessions,
/// launch codes, match-server credentials and join tickets.
pub const CREDENTIAL_PREFIXES: [&str; 5] = ["vls_", "vgs_", "vlc_", "vms_", "vjt_"];

/// The length of a credential after its prefix: the unpadded base64url form of 32 bytes.
pub const CREDENTIAL_BODY_LENGTH: usize = 43;

/// Whether `text` is a credential with `prefix` in the backend's format.
pub fn is_credential(text: &str, prefix: &str) -> bool {
    text.strip_prefix(prefix)
        .is_some_and(|body| body.len() == CREDENTIAL_BODY_LENGTH && body.bytes().all(is_base64url))
}

/// `text` with the secret part of every Veyra credential replaced, so it can be shown or logged.
pub fn redact(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut rest = text;
    while let Some((index, prefix)) = CREDENTIAL_PREFIXES
        .iter()
        .filter_map(|prefix| rest.find(prefix).map(|index| (index, *prefix)))
        .min()
    {
        out.push_str(&rest[..index]);
        out.push_str(prefix);
        out.push_str("<redacted>");
        let after = &rest[index + prefix.len()..];
        let secret_end = after.bytes().position(|byte| !is_base64url(byte)).unwrap_or(after.len());
        rest = &after[secret_end..];
    }
    out.push_str(rest);
    out
}

fn is_base64url(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'-' || byte == b'_'
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_secret_never_formats_itself() {
        let secret = Secret::new(format!("vlc_{}", "A".repeat(43)));
        assert_eq!(format!("{secret}"), "<redacted>");
        assert_eq!(format!("{secret:?}"), "Secret(<redacted>)");
        assert!(secret.expose().starts_with("vlc_"));
    }

    #[test]
    fn redaction_hides_every_credential() {
        let text = format!("session vls_{} then code vlc_{}; and a cut-off vjt_ab.", "a".repeat(43), "B".repeat(43));
        assert_eq!(redact(&text), "session vls_<redacted> then code vlc_<redacted>; and a cut-off vjt_<redacted>.");
        assert_eq!(redact("nothing secret"), "nothing secret");
    }

    #[test]
    fn credentials_have_the_backends_format() {
        assert!(is_credential(&format!("vlc_{}", "A".repeat(43)), "vlc_"));
        assert!(!is_credential(&format!("vlc_{}", "A".repeat(42)), "vlc_"));
        assert!(!is_credential(&format!("vls_{}", "A".repeat(43)), "vlc_"));
        assert!(!is_credential(&format!("vlc_{}+", "A".repeat(42)), "vlc_"));
    }
}
