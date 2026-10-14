"""Split generated sheets and export Veyra's 256 px item source icons.

Run with the bundled Python (Pillow required). Inputs and exact prompts are
recorded in generation.json. This only crops/resizes; artwork uses image_gen.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageStat

HERE = Path(__file__).resolve().parent
ART = HERE.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore-previous-pass", action="store_true",
                        help="explicitly restore the superseded 27-icon pass")
    args = parser.parse_args()
    if (ART / "Toon" / "generation.json").exists() and not args.restore_previous_pass:
        raise SystemExit("Superseded pass: use Toon/export_icons.py. Explicit rollback requires --restore-previous-pass.")
    record = json.loads((HERE / "generation.json").read_text(encoding="utf-8"))
    preview = Image.new("RGB", (9 * 160, 3 * 186), (27, 28, 32))
    draw = ImageDraw.Draw(preview)
    for batch_index, batch in enumerate(record["batches"]):
        sheet = Image.open(HERE / batch["sheet"]).convert("RGB")
        assert sheet.width == sheet.height, "Expected a square sheet"
        # Generated grids may have slightly unequal cells. Locate the dark
        # separators close to each third instead of cutting through adjacent art.
        def boundaries(axis):
            length = sheet.width if axis == "x" else sheet.height
            result = [0]
            for fraction in (1 / 3, 2 / 3):
                candidates = range(round(length * (fraction - 0.035)), round(length * (fraction + 0.035)))
                def darkness(position):
                    strip = sheet.crop((position, 0, position + 1, sheet.height) if axis == "x"
                                       else (0, position, sheet.width, position + 1))
                    return sum(ImageStat.Stat(strip).mean)
                result.append(min(candidates, key=darkness))
            return result + [length]
        xs, ys = boundaries("x"), boundaries("y")
        batch["crop_boundaries"] = {"x": xs, "y": ys}
        for index, item in enumerate(batch["items"]):
            col, row = index % 3, index // 3
            box = (xs[col] + 2, ys[row] + 2, xs[col + 1] - 2, ys[row + 1] - 2)
            icon = sheet.crop(box).resize((256, 256), Image.Resampling.LANCZOS)
            path = ART / item["folder"] / f'T_{item["id"]}_Icon.png'
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists():
                backup = HERE / "Before" / f'{item["id"]}.png'
                backup.parent.mkdir(parents=True, exist_ok=True)
                if not backup.exists():
                    backup.write_bytes(path.read_bytes())
            icon.save(path)
            item["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            item["file"] = path.relative_to(ART).as_posix()
            x, y = index * 160, batch_index * 186
            preview.paste(icon.resize((144, 144), Image.Resampling.LANCZOS), (x + 8, y))
            draw.text((x + 4, y + 148), item["id"].replace("_", " "), fill="white")
    preview.save(HERE / "contact_sheet.jpg", quality=95)
    (HERE / "generation.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("Exported 27 opaque 256 x 256 item icons and contact sheet.")


if __name__ == "__main__":
    main()
