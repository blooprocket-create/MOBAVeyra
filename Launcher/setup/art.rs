//! `veyra-setup-art`: draws Veyra Setup's bitmaps from a Vanguard's splash art (ADR-022 §2), so no
//! bitmap is committed. `Launcher/Package.ps1` runs it before compiling Setup.
//!
//!     veyra-setup-art --config <Launcher/setup/art.json> --out <folder>
//!
//! The configuration names the art and frames each picture: the point of the art to show (`focus`,
//! as fractions across and down), where in the picture it goes (`anchor`), and how much closer than
//! the whole art the picture looks (`zoom`). It writes `welcome.bmp`, the tall picture beside Setup's
//! first and last pages, drawn at twice the size NSIS lays it out at so it stays sharp when Windows
//! scales Setup up; and `header.bmp`, the strip at the top right of the pages between, at exactly its
//! size, since NSIS stretches a header crudely.

use image::imageops::{self, FilterType};
use image::{Rgb, RgbImage};
use serde::Deserialize;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

/// The configuration format this tool reads.
const SCHEMA_VERSION: u32 = 1;

/// The launcher's ink, the darkest colour of its palette (ui/launcher.css `--ink`).
const INK: [u8; 3] = [0x07, 0x09, 0x0e];
/// The launcher's Flux-teal accent (ui/launcher.css `--accent`).
const ACCENT: [u8; 3] = [0x4f, 0xd8, 0xcf];

/// NSIS Modern UI's welcome and finish picture is 164 × 314 at 100 % scale; this is twice that.
const WELCOME: (u32, u32) = (328, 628);
/// NSIS Modern UI's header picture, at 100 % scale.
const HEADER: (u32, u32) = (150, 57);

/// Windows scales the welcome picture down by picking pixels, so it is softened first by this much
/// (a Gaussian's sigma, in pixels of the drawn bitmap): about the pixel it may skip.
const SOFTEN: f32 = 0.9;

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct ArtConfig {
    schema_version: u32,
    /// The splash art, relative to the configuration file.
    art: String,
    welcome: Framing,
    header: Framing,
}

#[derive(Clone, Copy, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct Framing {
    focus: Point,
    anchor: Point,
    zoom: f32,
}

/// A point as fractions of a picture: 0 is the left or top, 1 the right or bottom.
#[derive(Clone, Copy, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct Point {
    across: f32,
    down: f32,
}

fn load(path: &Path) -> Result<ArtConfig, String> {
    let text = std::fs::read_to_string(path).map_err(|error| format!("{} could not be read: {error}", path.display()))?;
    let config: ArtConfig = serde_json::from_str(&text).map_err(|error| format!("{} is not Setup's art configuration: {error}", path.display()))?;
    let mut problems = Vec::new();
    if config.schema_version != SCHEMA_VERSION {
        problems.push(format!("schemaVersion must be {SCHEMA_VERSION}"));
    }
    for (name, framing) in [("welcome", config.welcome), ("header", config.header)] {
        let fractions = [framing.focus.across, framing.focus.down, framing.anchor.across, framing.anchor.down];
        if !fractions.iter().all(|value| (0.0..=1.0).contains(value)) {
            problems.push(format!("{name}'s focus and anchor must be fractions from 0 to 1"));
        }
        if !(framing.zoom.is_finite() && framing.zoom >= 1.0) {
            problems.push(format!("{name}.zoom must be at least 1"));
        }
    }
    if problems.is_empty() {
        Ok(config)
    } else {
        Err(format!("{} cannot be used: {}", path.display(), problems.join("; ")))
    }
}

fn main() -> ExitCode {
    let mut arguments = std::env::args().skip(1);
    let (mut config, mut out) = (None, None);
    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "--config" => config = arguments.next().map(PathBuf::from),
            "--out" => out = arguments.next().map(PathBuf::from),
            _ => config = None,
        }
    }
    let (Some(config_path), Some(out)) = (config, out) else {
        eprintln!("usage: veyra-setup-art --config <art.json> --out <folder>");
        return ExitCode::from(2);
    };
    let config = match load(&config_path) {
        Ok(config) => config,
        Err(problem) => {
            eprintln!("veyra-setup-art: {problem}");
            return ExitCode::from(2);
        }
    };
    let art = config_path.parent().unwrap_or(Path::new(".")).join(&config.art);
    let source = match image::open(&art) {
        Ok(source) => source.to_rgb8(),
        Err(error) => {
            eprintln!("veyra-setup-art: {} could not be read: {error}", art.display());
            return ExitCode::FAILURE;
        }
    };
    if let Err(error) = std::fs::create_dir_all(&out) {
        eprintln!("veyra-setup-art: {} could not be made: {error}", out.display());
        return ExitCode::FAILURE;
    }
    for (name, picture) in [
        ("welcome.bmp", welcome(&source, config.welcome)),
        ("header.bmp", header(&source, config.header)),
    ] {
        let path = out.join(name);
        if let Err(error) = picture.save(&path) {
            eprintln!("veyra-setup-art: {} could not be written: {error}", path.display());
            return ExitCode::FAILURE;
        }
    }
    ExitCode::SUCCESS
}

/// The part of `source` with the shape of `size`, as large as fits divided by the framing's zoom,
/// placed so its focus lands on its anchor (as near as the art's edges allow), scaled to `size`.
fn frame(source: &RgbImage, size: (u32, u32), framing: Framing) -> RgbImage {
    let (width, height) = source.dimensions();
    let scale = (width as f32 / size.0 as f32).min(height as f32 / size.1 as f32) / framing.zoom;
    let (crop_width, crop_height) = ((size.0 as f32 * scale) as u32, (size.1 as f32 * scale) as u32);
    let place = |focus: f32, anchor: f32, whole: u32, part: u32| -> u32 { ((whole as f32 * focus - part as f32 * anchor).max(0.0) as u32).min(whole - part) };
    let left = place(framing.focus.across, framing.anchor.across, width, crop_width);
    let top = place(framing.focus.down, framing.anchor.down, height, crop_height);
    let cropped = imageops::crop_imm(source, left, top, crop_width, crop_height).to_image();
    imageops::resize(&cropped, size.0, size.1, FilterType::Lanczos3)
}

/// `colour` over `pixel` by `amount` (0 keeps the pixel, 1 is the colour).
fn blend(pixel: &mut Rgb<u8>, colour: [u8; 3], amount: f32) {
    let amount = amount.clamp(0.0, 1.0);
    for (channel, target) in pixel.0.iter_mut().zip(colour) {
        *channel = (*channel as f32 * (1.0 - amount) + target as f32 * amount).round() as u8;
    }
}

/// How far `at` (0 to 1) has come from `start` to `end`, eased: 0 before, 1 after.
fn ramp(at: f32, start: f32, end: f32) -> f32 {
    let t = ((at - start) / (end - start)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

/// The Vanguard, full height, sinking into ink toward the foot, with a line of Flux down its right
/// edge where the page's text begins.
fn welcome(source: &RgbImage, framing: Framing) -> RgbImage {
    let mut picture = imageops::blur(&frame(source, WELCOME, framing), SOFTEN);
    let (width, height) = picture.dimensions();
    let line = width / 160 + 1;
    for (x, y, pixel) in picture.enumerate_pixels_mut() {
        let down = y as f32 / height as f32;
        blend(pixel, INK, 0.1 + 0.85 * ramp(down, 0.6, 1.0));
        if x >= width - line {
            blend(pixel, ACCENT, 0.85 * (1.0 - ramp(down, 0.2, 0.95)));
        }
    }
    picture
}

/// The Vanguard's face emerging from ink at the left, as the launcher's art meets its card.
fn header(source: &RgbImage, framing: Framing) -> RgbImage {
    let mut picture = frame(source, HEADER, framing);
    let (width, _) = picture.dimensions();
    for (x, _, pixel) in picture.enumerate_pixels_mut() {
        blend(pixel, INK, 0.9 * (1.0 - ramp(x as f32 / width as f32, 0.05, 0.55)));
    }
    picture
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_committed_configuration_frames_existing_art() {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("art.json");
        let config = load(&path).expect("setup/art.json");
        assert!(path.parent().unwrap().join(&config.art).is_file(), "{} exists", config.art);
    }

    #[test]
    fn the_focus_lands_on_the_anchor_unless_an_edge_is_in_the_way() {
        // A 100 × 100 art, black but for one white pixel at (70, 20).
        let mut art = RgbImage::new(100, 100);
        art.put_pixel(70, 20, Rgb([255, 255, 255]));
        let framing = Framing {
            focus: Point { across: 0.705, down: 0.205 },
            anchor: Point { across: 0.5, down: 0.5 },
            zoom: 5.0,
        };
        let brightest = |picture: &RgbImage| {
            picture
                .enumerate_pixels()
                .max_by_key(|(_, _, pixel)| pixel.0[0])
                .map(|(x, y, _)| (x, y))
                .unwrap()
        };
        assert_eq!(brightest(&frame(&art, (20, 20), framing)), (10, 10));
        // Near the top edge, the picture stops at the edge rather than showing past it.
        let top = Framing {
            focus: Point { across: 0.705, down: 0.0 },
            ..framing
        };
        assert_eq!(frame(&art, (20, 20), top).dimensions(), (20, 20));
    }
}
