#!/usr/bin/env python3
"""Export generated artwork to device JPEGs and assign it to the source deck."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'assets/artwork.json').read_text())
deck_path = ROOT / 'content/deck.json'
deck = json.loads(deck_path.read_text())
assignments = {}
exports = []
for item in manifest['images']:
    device_path = f'/ORACLE/images/{item["id"]}.jpg'
    for card_id in item['cards']:
        if card_id in assignments:
            raise ValueError(f'Duplicate artwork assignment: {card_id}')
        assignments[card_id] = device_path
    source = ROOT / 'assets' / item['source']
    with Image.open(source) as original:
        if original.width != original.height:
            raise ValueError(f'Expected square artwork: {source}')
        exports.append((item, original.convert('RGB').resize((92, 92), Image.Resampling.LANCZOS)))

if set(assignments) != {card['id'] for card in deck['cards']}:
    raise ValueError('Artwork assignments must cover the source deck exactly')

for item, small in exports:
    dest = ROOT / 'sdcard/ORACLE/images' / f'{item["id"]}.jpg'
    dest.parent.mkdir(parents=True, exist_ok=True)
    # Preserve the already shipped original cat JPEG.
    if item['id'] != 'oracle' or not dest.exists():
        small.save(dest, 'JPEG', quality=94, subsampling=0, progressive=False)
    with Image.open(dest) as ready:
        if ready.size != (92, 92) or ready.mode != 'RGB' or ready.info.get('progressive'):
            raise ValueError(f'Invalid device JPEG: {dest}')
        ready.verify()
    if dest.stat().st_size >= 65536:
        raise ValueError(f'Device JPEG too large: {dest}')
    print(f'{dest.name}: {dest.stat().st_size} bytes, {len(item["cards"])} cards')

for card in deck['cards']:
    card['image'] = assignments[card['id']]
deck_path.write_text(json.dumps(deck, ensure_ascii=False, indent=2) + '\n')

# Compare the actual decoded device JPEGs at 1x and 3x, without altering originals.
cols, cell_w, cell_h = 4, 330, 405
sheet = Image.new('RGB', (cols * cell_w, ((len(exports) + cols - 1) // cols) * cell_h), '#100e1b')
draw = ImageDraw.Draw(sheet)
font_path = Path('/System/Library/Fonts/Supplemental/Arial.ttf')
font = ImageFont.truetype(str(font_path), 19) if font_path.exists() else ImageFont.load_default()
for i, (item, _) in enumerate(exports):
    x, y = (i % cols) * cell_w + 16, (i // cols) * cell_h + 12
    with Image.open(ROOT / 'sdcard/ORACLE/images' / f'{item["id"]}.jpg') as jpeg:
        small = jpeg.convert('RGB')
        sheet.paste(small.resize((276, 276), Image.Resampling.NEAREST), (x, y))
        sheet.paste(small, (x, y + 304))
    draw.text((x, y + 278), item['title'], font=font, fill='#eac57b')
    draw.text((x + 105, y + 320), '92 x 92', font=font, fill='#b2a3bb')
sheet.save(ROOT / 'docs/artwork-overview.png')
print(f'Assigned {len(exports)} images to {len(deck["cards"])} cards')
