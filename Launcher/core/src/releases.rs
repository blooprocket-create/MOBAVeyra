//! The launcher's conversation with a release store over HTTP (ADR-022 §3): a channel's current
//! release, its manifest and its chunks. Everything fetched is bounded in size and checked: the
//! manifest against the hash the channel names, and each chunk (by `release::decode_chunk`) against
//! the hash the manifest names.

use crate::install::{ChunkStore, Target};
use crate::release::{self, Channel, ReleaseChunk};
use std::fmt;
use std::time::Duration;
use ureq::Agent;

/// Why a release store did not give what was asked.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ReleaseError {
    /// No answer: the server is down or unreachable, or the request timed out.
    Unreachable(String),
    /// The server has no such file (HTTP 404): no release on the channel, or a store half copied.
    Missing(String),
    /// Any other error status.
    Refused { what: String, status: u16 },
    /// An answer that is not what the store should hold.
    Invalid { what: String, problem: String },
}

impl fmt::Display for ReleaseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Unreachable(reason) => write!(f, "Veyra's download server did not answer ({reason})"),
            Self::Missing(what) => write!(f, "Veyra's download server has no {what}"),
            Self::Refused { what, status } => write!(f, "Veyra's download server refused the {what} (HTTP {status})"),
            Self::Invalid { what, problem } => write!(f, "Veyra's download server sent a bad {what}: {problem}"),
        }
    }
}

impl std::error::Error for ReleaseError {}

/// A release store at one base URL.
pub struct ReleaseServer {
    base_url: String,
    agent: Agent,
}

impl ReleaseServer {
    /// `timeout` bounds each request, from connecting to the last byte.
    pub fn new(base_url: &str, timeout: Duration) -> Self {
        let config = Agent::config_builder().timeout_global(Some(timeout)).http_status_as_error(false).build();
        Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            agent: Agent::new_with_config(config),
        }
    }

    /// The channel's current release: its channel file, then the manifest it names.
    pub fn current(&self, channel: &str) -> Result<Target, ReleaseError> {
        let channel_file = self.channel(channel)?;
        let what = "release manifest";
        let bytes = self.get(&release::manifest_object(&channel_file.manifest), what, release::MAX_MANIFEST_BYTES)?;
        if release::sha256_hex(&bytes) != channel_file.manifest {
            return Err(ReleaseError::Invalid {
                what: what.to_string(),
                problem: "it is not the manifest the channel names".to_string(),
            });
        }
        let manifest = release::parse_manifest(&bytes).map_err(|problems| ReleaseError::Invalid {
            what: what.to_string(),
            problem: problems.join("; "),
        })?;
        if manifest.build_version != channel_file.build_version {
            return Err(ReleaseError::Invalid {
                what: what.to_string(),
                problem: format!("it is build {}, and the channel says {}", manifest.build_version, channel_file.build_version),
            });
        }
        Ok(Target {
            manifest_hash: channel_file.manifest,
            manifest,
        })
    }

    /// The channel file alone.
    pub fn channel(&self, channel: &str) -> Result<Channel, ReleaseError> {
        let what = format!("channel {channel}");
        let bytes = self.get(&release::channel_object(channel), &what, release::MAX_CHANNEL_BYTES)?;
        release::parse_channel(&bytes).map_err(|problems| ReleaseError::Invalid {
            what,
            problem: problems.join("; "),
        })
    }

    /// The bytes at `object`, refused if longer than `limit`.
    fn get(&self, object: &str, what: &str, limit: u64) -> Result<Vec<u8>, ReleaseError> {
        let url = format!("{}/{object}", self.base_url);
        let mut response = self.agent.get(&url).call().map_err(|error| ReleaseError::Unreachable(error.to_string()))?;
        match response.status().as_u16() {
            200 => {}
            404 => return Err(ReleaseError::Missing(what.to_string())),
            status => {
                return Err(ReleaseError::Refused {
                    what: what.to_string(),
                    status,
                })
            }
        }
        let too_long = || ReleaseError::Invalid {
            what: what.to_string(),
            problem: format!("it is longer than {limit} bytes"),
        };
        // ureq's limit refuses any read past it, even the one that finds the end, so it is one byte
        // more; a body of exactly one byte more is then refused here.
        let bytes = response
            .body_mut()
            .with_config()
            .limit(limit.saturating_add(1))
            .read_to_vec()
            .map_err(|error| match error {
                ureq::Error::BodyExceedsLimit(_) => too_long(),
                other => ReleaseError::Unreachable(other.to_string()),
            })?;
        if bytes.len() as u64 > limit {
            return Err(too_long());
        }
        Ok(bytes)
    }
}

impl ChunkStore for ReleaseServer {
    fn fetch(&self, chunk: &ReleaseChunk) -> Result<Vec<u8>, String> {
        let stored = self
            .get(&release::chunk_object(&chunk.hash), "chunk", chunk.stored_size)
            .map_err(|error| error.to_string())?;
        Ok(stored)
    }
}
