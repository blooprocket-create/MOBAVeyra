from pathlib import Path
from PIL import Image, ImageDraw
HERE = Path(__file__).resolve().parent
ids = sorted(p.stem for p in HERE.glob('*.png') if not p.stem.startswith('before_'))
sheet = Image.new('RGB', (960, 780), '#202226')
draw = ImageDraw.Draw(sheet)
for n, item in enumerate(ids):
    x, y = n % 4 * 240, n // 4 * 260
    im = Image.open(HERE / (item + '.png')).convert('RGB')
    sheet.paste(im.resize((176,176)), (x,y))
    sheet.paste(im.resize((32,32)), (x+182,y+25))
    sheet.paste(im.convert('L').convert('RGB').resize((48,48)), (x+182,y+75))
    old = Image.open(HERE / ('before_' + item + '.png')).convert('RGB')
    sheet.paste(old.resize((48,48)), (x+182,y+132))
    draw.text((x,y+183),item,fill='white')
    draw.text((x,y+203),'new / 32px / gray / old',fill='#abb3bf')
sheet.save(HERE / 'comparison.jpg', quality=95)
