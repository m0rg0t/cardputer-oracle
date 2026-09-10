#!/usr/bin/env python3
"""Convert actual firmware-rendered PPM captures into a review sheet."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
files=sorted((ROOT/'docs/screens').glob('*.ppm'))
sheet=Image.new('RGB',(1020,max(1,(len(files)+1)//2)*328),(16,14,27)); d=ImageDraw.Draw(sheet)
for i,p in enumerate(files):
    im=Image.open(p).convert('RGB');im.save(p.with_suffix('.png'))
    x=20+(i%2)*500;y=20+(i//2)*328
    d.text((x,y),p.stem,fill=(234,197,123))
    sheet.paste(im.resize((480,270),Image.Resampling.NEAREST),(x,y+22))
sheet.save(ROOT/'docs/screens-overview.png')
print(ROOT/'docs/screens-overview.png')
