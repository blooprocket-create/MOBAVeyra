"""Review available sources without changing any game-facing icon."""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent


def main():
    record = json.loads((HERE / "generation.json").read_text(encoding="utf-8"))
    image = Image.new("RGB", (8 * 176, 8 * 164), (27, 28, 32))
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 12)
    count = 0
    for index, item in enumerate(record["items"]):
        source = HERE / "Sources" / f'{item["id"]}.png'
        x, y = index % 8 * 176, index // 8 * 164
        if source.exists():
            icon = Image.open(source).convert("RGB")
            image.paste(icon.resize((112, 112), Image.Resampling.LANCZOS), (x + 4, y + 4))
            image.paste(icon.resize((48, 48), Image.Resampling.LANCZOS), (x + 122, y + 4))
            image.paste(icon.convert("L").convert("RGB").resize((48, 48), Image.Resampling.LANCZOS), (x + 122, y + 64))
            count += 1
        draw.text((x + 4, y + 120), item["id"].replace("_", " "), font=font, fill="white")
    image.save(HERE / "source_review.jpg", quality=95)
    print(f"Reviewed source sheet: {count}/64 images available.")


if __name__ == "__main__":
    main()
