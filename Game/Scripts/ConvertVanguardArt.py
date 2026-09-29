"""Converts each playable Vanguard's hero illustration to PNG for the Vanguard art commandlet.

The engine decodes no WebP, which is how ConceptArt/Vanguards stores the heroes, so
BuildVanguardArt.ps1 runs this first. The playable Vanguards are the ones Game/Tuning/Vanguards.json
marks "Playable"; each one's ConceptArt/Vanguards/<id>/hero.(webp|png|jpg) becomes <out>/<id>.png.

Usage: python ConvertVanguardArt.py <repository root> <output folder>
"""

import json
import pathlib
import sys

from PIL import Image


def main() -> int:
    repository = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    for stale in output.glob('*.png'):
        stale.unlink()
    tuning = json.loads((repository / 'Game' / 'Tuning' / 'Vanguards.json').read_text(encoding='utf-8'))
    missing = []
    for vanguard, definition in sorted(tuning['vanguards'].items()):
        if definition.get('availability') != 'Playable':
            continue
        heroes = [repository / 'ConceptArt' / 'Vanguards' / vanguard / f'hero.{extension}' for extension in ('webp', 'png', 'jpg')]
        hero = next((path for path in heroes if path.exists()), None)
        if hero is None:
            missing.append(vanguard)
            continue
        Image.open(hero).convert('RGB').save(output / f'{vanguard}.png')
        print(f'{vanguard}: {hero.relative_to(repository)}')
    if missing:
        print(f'No hero illustration for: {", ".join(missing)}')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
