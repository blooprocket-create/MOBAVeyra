"""Export retained individual image_gen sources to Veyra's UI icon contract.

Run with Python and Pillow after every source listed in generation.json exists.
This performs deterministic resizing, preview layout and validation only.
"""
import hashlib
import json
import shutil
import time
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ART = HERE.parent
ROOT = ART.parent.parent


def label_font(root: Path, size: int) -> ImageFont.FreeTypeFont:
    """The review sheets' label font: the project's own Inter (OFL), so the export runs on any machine."""
    path = root / "Game/Content/Veyra/Fonts/Inter/Inter-Regular.ttf"
    try:
        return ImageFont.truetype(str(path), size)
    except OSError as error:
        raise SystemExit(f"Cannot open the label font {path} ({error}); run `git lfs pull --include Game/Content/Veyra/Fonts` first.")


def atomic_save(image, target, **options):
    temporary = target.with_name(target.name + ".new")
    image.save(temporary, format=Image.registered_extensions()[target.suffix], **options)
    for attempt in range(10):
        try:
            temporary.replace(target)
            return
        except OSError:
            if attempt == 9:
                raise
            time.sleep(0.3)


def main():
    record = json.loads((HERE / "generation.json").read_text(encoding="utf-8"))
    if (HERE / "repairs.json").exists():
        record["repairs"] = json.loads((HERE / "repairs.json").read_text(encoding="utf-8"))
    catalog = json.loads((ROOT / "Game/Tuning/Items.json").read_text(encoding="utf-8"))["items"]
    assert set(catalog) == {item["id"] for item in record["items"]}, "Catalog coverage differs"
    assert len(record["items"]) == 64
    # Preflight every input before changing any shipping source art.
    for item in record["items"]:
        source = HERE / "Sources" / f'{item["id"]}.png'
        with Image.open(source) as image:
            assert image.width == image.height and image.width >= 512, str(source)
        targets = list(ART.glob(f'*/T_{item["id"]}_Icon.png'))
        assert len(targets) == 1, item["id"]
        item["file"] = targets[0].relative_to(ART).as_posix()

    font = label_font(ROOT, 13)
    font_small = label_font(ROOT, 10)
    preview = Image.new("RGB", (8 * 176, 8 * 190), (27, 28, 32))
    thumbnail = Image.new("RGB", (8 * 176, 8 * 106), (27, 28, 32))
    old_preview = Image.new("RGB", preview.size, (27, 28, 32))
    draw = ImageDraw.Draw(preview)
    draw_small = ImageDraw.Draw(thumbnail)
    draw_old = ImageDraw.Draw(old_preview)
    for index, item in enumerate(record["items"]):
        target = ART / item["file"]
        old = HERE / "Before" / f'{item["id"]}.png'
        old.parent.mkdir(parents=True, exist_ok=True)
        if not old.exists():
            old.write_bytes(target.read_bytes())
        icon = Image.open(HERE / "Sources" / f'{item["id"]}.png').convert("RGB")
        icon = icon.resize((256, 256), Image.Resampling.LANCZOS)
        # Atomic replacement avoids truncating an icon while an editor/indexer
        # reads it. Windows scanners may briefly hold either file open.
        atomic_save(icon, target)
        item["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
        item["source_sha256"] = hashlib.sha256((HERE / "Sources" / f'{item["id"]}.png').read_bytes()).hexdigest()
        name = item["id"].replace("_", " ")
        x, y = index % 8 * 176, index // 8 * 190
        preview.paste(icon.resize((160, 160), Image.Resampling.LANCZOS), (x + 8, y))
        old_preview.paste(Image.open(old).convert("RGB").resize((160, 160), Image.Resampling.LANCZOS), (x + 8, y))
        draw.text((x + 4, y + 162), name, font=font, fill="white")
        draw_old.text((x + 4, y + 162), name, font=font, fill="white")
        sy = index // 8 * 106
        thumbnail.paste(icon.resize((48, 48), Image.Resampling.LANCZOS), (x + 8, sy + 4))
        thumbnail.paste(icon.resize((32, 32), Image.Resampling.LANCZOS), (x + 66, sy + 12))
        thumbnail.paste(icon.convert("L").convert("RGB").resize((48, 48), Image.Resampling.LANCZOS), (x + 108, sy + 4))
        draw_small.text((x + 4, sy + 57), name, font=font, fill="white")
        draw_small.text((x + 8, sy + 78), "48px     32px       gray", font=font_small, fill=(170, 180, 190))
    atomic_save(preview, HERE / "contact_sheet.jpg", quality=95)
    legacy_index = HERE / "Before" / "original_contact_sheet.jpg"
    if (ART / "contact_sheet.jpg").exists() and not legacy_index.exists():
        shutil.copyfile(ART / "contact_sheet.jpg", legacy_index)
    atomic_save(preview, ART / "contact_sheet.jpg", quality=95)
    atomic_save(old_preview, HERE / "before_contact_sheet.jpg", quality=95)
    atomic_save(thumbnail, HERE / "readability_48_32_gray.png")
    (HERE / "generation.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("Exported and validated all 64 opaque RGB 256 x 256 icons.")


if __name__ == "__main__":
    main()
