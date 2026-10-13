//! What the launcher reads from Veyra's website (ADR-081): the public servers' daily hours and each
//! game build's patch notes, from the `launcher.json` the website's build publishes from
//! `Website/site/site.json` and `Website/site/patch-notes.json`. The website stays up when the
//! servers are down, so the launcher can still say when they next open.
//!
//! The website validates the feed strictly when it builds; the launcher checks only what it relies on
//! and ignores fields it does not know, so the website can add to schema 1 without breaking older
//! launchers.

use serde::{Deserialize, Serialize};
use std::time::Duration;
use ureq::Agent;

/// The feed format this launcher reads.
pub const FEED_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Feed {
    pub schema_version: u32,
    pub server_hours: ServerHours,
    /// Newest first.
    pub patch_notes: Vec<PatchNote>,
}

/// When the public servers run each day, on the host's clock: `opens` and `closes` as "HH:MM" in
/// `time_zone`, an IANA zone such as America/New_York. A session may run past midnight.
#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ServerHours {
    pub time_zone: String,
    pub opens: String,
    pub closes: String,
}

/// What one game build changed, in words a player reads.
#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PatchNote {
    pub version: String,
    pub date: String,
    pub title: String,
    pub changes: Vec<String>,
}

/// Fetches and checks the feed at `feed_url`. The error says why, without quoting the answer.
pub fn fetch(feed_url: &str, timeout: Duration) -> Result<Feed, String> {
    let config = Agent::config_builder().timeout_global(Some(timeout)).http_status_as_error(false).build();
    let mut response = Agent::new_with_config(config)
        .get(feed_url)
        .header("Accept", "application/json")
        .call()
        .map_err(|error| format!("the website did not answer ({error})"))?;
    let status = response.status().as_u16();
    if !(200..300).contains(&status) {
        return Err(format!("the website refused (HTTP {status})"));
    }
    let text = response
        .body_mut()
        .read_to_string()
        .map_err(|error| format!("the website's answer could not be read ({error})"))?;
    parse(&text)
}

/// Parses and checks a feed's text.
pub fn parse(text: &str) -> Result<Feed, String> {
    let feed: Feed = serde_json::from_str(text).map_err(|error| format!("the website's feed is not one the launcher reads: {error}"))?;
    if feed.schema_version != FEED_SCHEMA_VERSION {
        return Err(format!(
            "the website's feed is schema {}, and this launcher reads {FEED_SCHEMA_VERSION}",
            feed.schema_version
        ));
    }
    let hours = &feed.server_hours;
    if !is_time_zone(&hours.time_zone) || !is_clock(&hours.opens) || !is_clock(&hours.closes) {
        return Err("the website's server hours are not a time zone and two HH:MM times".to_string());
    }
    let blank = |text: &str| text.trim().is_empty();
    if feed
        .patch_notes
        .iter()
        .any(|note| blank(&note.version) || blank(&note.title) || note.changes.is_empty() || note.changes.iter().any(|change| blank(change)))
    {
        return Err("a patch note in the website's feed has no version, title or changes".to_string());
    }
    Ok(feed)
}

/// An IANA zone's shape, such as America/New_York: the window's own clock checks the zone itself.
fn is_time_zone(text: &str) -> bool {
    text.contains('/')
        && !text.starts_with('/')
        && !text.ends_with('/')
        && text
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'/' | b'_' | b'-' | b'+'))
}

/// "HH:MM" on a 24-hour clock.
fn is_clock(text: &str) -> bool {
    let Some((hours, minutes)) = text.split_once(':') else {
        return false;
    };
    let number = |part: &str, below: u32| part.len() == 2 && part.parse::<u32>().is_ok_and(|value| value < below);
    number(hours, 24) && number(minutes, 60)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_server::serve_once;

    const FEED: &str = r#"{
        "schemaVersion": 1,
        "serverHours": { "timeZone": "America/New_York", "opens": "22:00", "closes": "23:30" },
        "patchNotes": [
            { "version": "0.2.7", "date": "2026-10-13", "title": "Smoother movement", "changes": ["Vanguards glide."] }
        ]
    }"#;

    #[test]
    fn reads_the_hours_and_the_notes() {
        let feed = parse(FEED).expect("valid");
        assert_eq!(feed.server_hours.opens, "22:00");
        assert_eq!(feed.patch_notes[0].version, "0.2.7");
        assert_eq!(feed.patch_notes[0].changes, ["Vanguards glide."]);
    }

    #[test]
    fn ignores_what_it_does_not_know_so_the_website_can_add_to_it() {
        let added = FEED.replace(r#""schemaVersion": 1,"#, r#""schemaVersion": 1, "events": [],"#);
        assert!(parse(&added).is_ok());
    }

    #[test]
    fn rejects_what_it_relies_on_when_wrong() {
        for (from, to) in [
            (r#""schemaVersion": 1"#, r#""schemaVersion": 2"#),
            ("America/New_York", "Eastern"),
            (r#""22:00""#, r#""10pm""#),
            (r#""23:30""#, r#""24:00""#),
            (r#"["Vanguards glide."]"#, "[]"),
            (r#""Smoother movement""#, r#"" ""#),
        ] {
            assert!(parse(&FEED.replace(from, to)).is_err(), "{to}");
        }
        assert!(parse("<html>").is_err());
    }

    #[test]
    fn fetches_the_feed_and_names_a_refusal() {
        let feed_at = |status: &str, body: &'static str| format!("{}/launcher.json", serve_once(status, body));
        let feed = fetch(&feed_at("200 OK", FEED), Duration::from_secs(5)).expect("fetched");
        assert_eq!(feed.server_hours.time_zone, "America/New_York");
        let refused = fetch(&feed_at("404 Not Found", "{}"), Duration::from_secs(5)).expect_err("refused");
        assert!(refused.contains("HTTP 404"), "{refused}");
    }
}
