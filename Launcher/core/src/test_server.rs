//! A one-answer HTTP server on a local port, for the core's tests of its own requests.

use std::io::{Read, Write};
use std::net::TcpListener;
use std::thread;

/// Answers the first request on a fresh local port with `status` and a JSON `body`, and returns the
/// server's base URL, such as http://127.0.0.1:50123.
pub fn serve_once(status: &str, body: &'static str) -> String {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let status = status.to_string();
    thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        let mut request = [0u8; 1024];
        let _ = stream.read(&mut request);
        let answer = format!(
            "HTTP/1.1 {status}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",
            body.len()
        );
        stream.write_all(answer.as_bytes()).unwrap();
    });
    format!("http://{address}")
}
