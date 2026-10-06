//! `veyra-fake-setup`: a stand-in for Veyra Setup, for testing a launcher that updates itself (ADR-022
//! §11) without installing anything. It writes the switches it was started with, space-separated, to
//! a file beside itself named `<its own file name>.args`, and exits.

use std::fs;

fn main() {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    let me = std::env::current_exe().expect("its own path");
    let mut record = me.clone().into_os_string();
    record.push(".args");
    fs::write(record, arguments.join(" ")).expect("write the switches");
}
