//! Builds the launcher's Windows resources with tauri-build. Tauri needs an icon file; the launcher's
//! is drawn here, from code, into the git-ignored `icons/` folder, so no binary icon is committed
//! (ADR-006 §6: reviewable text).

use std::fs;
use std::path::Path;

/// The icon's colours, as the launcher's style sheet uses them (ui/launcher.css): brass on a smoked
/// surface.
const BACKGROUND: [u8; 4] = [0x18, 0x1d, 0x29, 0xff];
const ACCENT: [u8; 4] = [0xd9, 0xb4, 0x77, 0xff];

/// The sizes Windows asks for: the taskbar and window corner, and large views.
const SIZES: [u32; 2] = [32, 256];

fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").expect("cargo sets CARGO_MANIFEST_DIR");
    sync_splash_art(Path::new(&manifest_dir));
    // capabilities/window.json grants the window only what its own title bar needs: dragging,
    // minimizing and closing. Everything else goes through the app's own commands.
    let icon = Path::new(&manifest_dir).join("icons/icon.ico");
    fs::create_dir_all(icon.parent().expect("the icon has a folder")).expect("create icons/");
    // Written only when it differs: tauri-build watches the icon, so rewriting it would rebuild the
    // launcher on every build.
    let bytes = ico(&SIZES);
    if fs::read(&icon).ok().as_deref() != Some(bytes.as_slice()) {
        fs::write(&icon, &bytes).expect("write icons/icon.ico");
    }
    println!("cargo:rerun-if-changed=build.rs");
    tauri_build::build();
}

/// Bundle the current canonical illustrations instead of retaining stale launcher copies.
fn sync_splash_art(manifest_dir: &Path) {
    let art_dir = manifest_dir.join("../ui/art");
    let source_dir = manifest_dir.join("../../ConceptArt/Vanguards");
    println!("cargo:rerun-if-changed={}", art_dir.display());
    for entry in fs::read_dir(&art_dir).expect("read launcher art directory") {
        let destination = entry.expect("read launcher art entry").path();
        if destination.extension().and_then(|extension| extension.to_str()) != Some("webp") {
            continue;
        }
        let source = source_dir.join(destination.file_stem().expect("art has a Vanguard ID")).join("hero.webp");
        println!("cargo:rerun-if-changed={}", source.display());
        let bytes = fs::read(&source).unwrap_or_else(|error| panic!("read canonical splash {}: {error}", source.display()));
        if fs::read(&destination).ok().as_deref() != Some(bytes.as_slice()) {
            fs::write(&destination, bytes).expect("sync launcher splash art");
        }
    }
}

/// An ICO file holding one 32-bit image per size: a "V" in the accent colour on the background.
fn ico(sizes: &[u32]) -> Vec<u8> {
    const DIRECTORY_BYTES: u32 = 6;
    const ENTRY_BYTES: u32 = 16;
    let images: Vec<Vec<u8>> = sizes.iter().map(|&size| bitmap(size)).collect();
    let mut out = Vec::new();
    // ICONDIR: reserved, type 1 (icon), count.
    out.extend_from_slice(&0u16.to_le_bytes());
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&(sizes.len() as u16).to_le_bytes());
    let mut offset = DIRECTORY_BYTES + ENTRY_BYTES * sizes.len() as u32;
    for (&size, image) in sizes.iter().zip(&images) {
        // ICONDIRENTRY: a width or height of 256 is written as 0.
        let side = if size >= 256 { 0u8 } else { size as u8 };
        out.extend_from_slice(&[side, side, 0, 0]);
        out.extend_from_slice(&1u16.to_le_bytes());
        out.extend_from_slice(&32u16.to_le_bytes());
        out.extend_from_slice(&(image.len() as u32).to_le_bytes());
        out.extend_from_slice(&offset.to_le_bytes());
        offset += image.len() as u32;
    }
    for image in images {
        out.extend_from_slice(&image);
    }
    out
}

/// A 32-bit bottom-up device-independent bitmap with its AND mask, as ICO stores images.
fn bitmap(size: u32) -> Vec<u8> {
    const HEADER_BYTES: u32 = 40;
    let mask_row_bytes = size.div_ceil(32) * 4;
    let mut out = Vec::new();
    // BITMAPINFOHEADER: the height counts the colour image and its mask.
    out.extend_from_slice(&HEADER_BYTES.to_le_bytes());
    out.extend_from_slice(&(size as i32).to_le_bytes());
    out.extend_from_slice(&((size * 2) as i32).to_le_bytes());
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&32u16.to_le_bytes());
    out.extend_from_slice(&[0u8; 24]);
    for row in (0..size).rev() {
        for column in 0..size {
            let [r, g, b, a] = if in_v(column, row, size) { ACCENT } else { BACKGROUND };
            out.extend_from_slice(&[b, g, r, a]);
        }
    }
    // Every pixel opaque: the alpha channel shapes it.
    out.extend(std::iter::repeat_n(0u8, (mask_row_bytes * size) as usize));
    out
}

/// Whether a pixel lies on the "V": two strokes from the top corners to the bottom centre.
fn in_v(column: u32, row: u32, size: u32) -> bool {
    let x = (column as f32 + 0.5) / size as f32;
    let y = (row as f32 + 0.5) / size as f32;
    let (top, bottom, margin, half_stroke) = (0.22, 0.8, 0.2, 0.07);
    if !(top..=bottom).contains(&y) {
        return false;
    }
    let along = (y - top) / (bottom - top);
    let left = margin + along * (0.5 - margin);
    let right = 1.0 - left;
    (x - left).abs() <= half_stroke || (x - right).abs() <= half_stroke
}
