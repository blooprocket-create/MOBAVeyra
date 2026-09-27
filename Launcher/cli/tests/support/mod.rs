//! A fake backend for the launcher's tests: the three routes a launcher uses, served over plain HTTP
//! on a local port, recording each request and when it came.

use std::io::{BufRead, BufReader, Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

/// A fixture launcher session and launch code, in the backend's formats. Not credentials.
pub fn launcher_session() -> String {
    format!("vls_{}", "S".repeat(43))
}

pub fn launch_code() -> String {
    format!("vlc_{}", "C".repeat(43))
}

/// One request the backend received.
#[derive(Debug, Clone)]
pub struct Received {
    pub route: String,
    pub authorization: Option<String>,
    pub at: Instant,
}

pub struct FakeBackend {
    pub url: String,
    pub requests: Arc<Mutex<Vec<Received>>>,
}

impl FakeBackend {
    pub fn start() -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").expect("bind a local port");
        let url = format!("http://{}", listener.local_addr().expect("a local address"));
        let requests = Arc::new(Mutex::new(Vec::new()));
        let recorded = requests.clone();
        thread::spawn(move || {
            for stream in listener.incoming().flatten() {
                serve(stream, &recorded);
            }
        });
        Self { url, requests }
    }

    pub fn routes(&self) -> Vec<String> {
        self.requests.lock().unwrap().iter().map(|request| request.route.clone()).collect()
    }

    pub fn time_of(&self, route: &str) -> Option<Instant> {
        self.requests
            .lock()
            .unwrap()
            .iter()
            .find(|request| request.route == route)
            .map(|request| request.at)
    }
}

fn serve(stream: TcpStream, recorded: &Mutex<Vec<Received>>) {
    let mut reader = BufReader::new(stream.try_clone().expect("clone the stream"));
    let mut request_line = String::new();
    if reader.read_line(&mut request_line).is_err() {
        return;
    }
    let mut content_length = 0;
    let mut authorization = None;
    loop {
        let mut header = String::new();
        if reader.read_line(&mut header).is_err() || header.trim().is_empty() {
            break;
        }
        let (name, value) = header.split_once(':').unwrap_or((&header, ""));
        match name.trim().to_ascii_lowercase().as_str() {
            "content-length" => content_length = value.trim().parse().unwrap_or(0),
            "authorization" => authorization = Some(value.trim().to_string()),
            _ => {}
        }
    }
    let mut body = vec![0; content_length];
    let _ = reader.read_exact(&mut body);
    let body: serde_json::Value = serde_json::from_slice(&body).unwrap_or(serde_json::Value::Null);
    let mut parts = request_line.split_whitespace();
    let route = format!("{} {}", parts.next().unwrap_or(""), parts.next().unwrap_or(""));
    recorded.lock().unwrap().push(Received {
        route: route.clone(),
        authorization: authorization.clone(),
        at: Instant::now(),
    });

    let (status, answer) = match route.as_str() {
        "GET /v1/dev/accounts" => (
            200,
            serde_json::json!({ "accounts": [{ "displayName": "DevOne" }, { "displayName": "DevTwo" }] }),
        ),
        "POST /v1/dev/login" if body["accountName"] == "DevOne" => (
            200,
            serde_json::json!({ "token": launcher_session(), "expiresAt": "2026-09-27T12:00:00Z", "account": { "id": "acc-1", "displayName": "DevOne" } }),
        ),
        "POST /v1/dev/login" => (404, serde_json::json!({ "error": "account_not_found" })),
        "POST /v1/launch-codes" if authorization == Some(format!("Bearer {}", launcher_session())) && body["buildVersion"] == "0.1.0" => {
            (200, serde_json::json!({ "token": launch_code(), "expiresAt": "2026-09-27T12:00:20Z" }))
        }
        "POST /v1/launch-codes" => (401, serde_json::json!({ "error": "invalid_credentials" })),
        _ => (404, serde_json::json!({ "error": "not_found" })),
    };
    let text = answer.to_string();
    let mut stream = stream;
    let _ = write!(
        stream,
        "HTTP/1.1 {status} X\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{text}",
        text.len()
    );
}
