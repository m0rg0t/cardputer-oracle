#!/usr/bin/env python3
"""Prepare bounded 384-dot monochrome BQ95 receipts from the active SD catalog.

ORP1: magic, u16 width/height, u32 raster length/CRC32/content CRC32, then
48 bytes per row, black=1, least significant bit first. All integers little endian.
The content checksum binds the printed title/text/image path to the shown card.
"""
from pathlib import Path
import hashlib
import json
import os
import struct
import zlib
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
WIDTH, MAX_HEIGHT = 384, 1024
ART_SIZE = 336
def font_path(bold=False):
    configured = os.environ.get('ORACLE_FONT_BOLD' if bold else 'ORACLE_FONT_REGULAR')
    if configured:
        if not Path(configured).is_file(): raise ValueError('Configured receipt font does not exist')
        return configured
    name = 'Arial Bold.ttf' if bold else 'Arial.ttf'
    dejavu = 'DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf'
    for path in [Path('/System/Library/Fonts/Supplemental') / name,
                 Path('/usr/share/fonts/truetype/dejavu') / dejavu]:
        if path.is_file(): return str(path)
    raise ValueError('Install DejaVu Sans or set ORACLE_FONT_REGULAR / ORACLE_FONT_BOLD')

def content_crc(card):
    return zlib.crc32((card['title']+'\n'+card['text']+'\n'+card['image']).encode())

def validate_receipt(data, card):
    if len(data)<20:
        raise ValueError('Truncated receipt header')
    magic, width, height, size, raster_crc, card_crc = struct.unpack('<4sHHIII', data[:20])
    if magic!=b'ORP1' or width!=WIDTH or not 1<=height<=MAX_HEIGHT or size!=height*48 or len(data)!=size+20:
        raise ValueError('Invalid receipt dimensions/length')
    if raster_crc!=zlib.crc32(data[20:]) or card_crc!=content_crc(card):
        raise ValueError('Receipt checksum mismatch or stale card text')
    return height

def wrapped(text, font, width):
    lines=[]
    for paragraph in text.splitlines():
        line=''
        for word in paragraph.split():
            candidate=f'{line} {word}'.strip()
            if font.getlength(candidate)>width and line:
                lines.append(line); line=''
            # Long words are split at a character, so no text can run off paper.
            for character in ((' ' if line else '')+word):
                if font.getlength(line+character)>width:
                    lines.append(line); line=''
                line+=character
        lines.append(line)
    return lines

def render(card):
    regular=ImageFont.truetype(font_path(),23)
    title=ImageFont.truetype(font_path(True),25)
    small=ImageFont.truetype(font_path(),16)
    banner=ImageFont.truetype(font_path(True),24)
    theme={'life':'АЙТИ И ЮМОР','it':'АЙТИ И ЮМОР','beer':'ПИВНАЯ','tea':'ЧАЙНАЯ'}[card['topic']]
    title_lines=wrapped(card['title'],title,336)
    body_lines=wrapped(card['text'],regular,336)
    art_extra=ART_SIZE-112
    body_y=196+art_extra+len(title_lines)*30
    footer_y=body_y+len(body_lines)*29+20
    height=footer_y+68
    if height>MAX_HEIGHT:
        raise ValueError(f'{card["id"]}: receipt too tall ({height})')
    canvas=Image.new('L',(WIDTH,height),255)
    draw=ImageDraw.Draw(canvas)
    draw.rounded_rectangle((8,8,375,height-9),radius=12,outline=0,width=2)
    draw.text((192,22),'БЮРО ПРОГНОЗОВ',font=banner,anchor='mt',fill=0)
    # Invert luminance for low-ink thermal delivery of the dark screen artwork.
    # This is a format derivative; full-resolution generated originals are intact.
    manifest=json.loads((ROOT/'assets/artwork.json').read_text())
    original=next(item['source'] for item in manifest['images'] if card['image']==f'/ORACLE/images/{item["id"]}.jpg')
    with Image.open(ROOT/'assets'/original) as source:
        artwork=ImageOps.invert(ImageOps.autocontrast(source.convert('L'))).resize((ART_SIZE,ART_SIZE),Image.Resampling.LANCZOS)
        canvas.paste(artwork,((WIDTH-ART_SIZE)//2,57))
    draw.text((192,174+art_extra),theme,font=small,anchor='mt',fill=0)
    for i,line in enumerate(title_lines):
        draw.text((192,196+art_extra+i*30),line,font=title,anchor='mt',fill=0)
    for i,line in enumerate(body_lines):
        draw.text((24,body_y+i*29),line,font=regular,anchor='lt',fill=0)
    draw.line((24,footer_y-6,359,footer_y-6),fill=0,width=1)
    draw.text((192,footer_y+4),'Звёзды старались. Гарантий нет.',font=small,anchor='mt',fill=0)
    draw.text((192,footer_y+28),'ORACLE / '+card['id'],font=small,anchor='mt',fill=0)
    return canvas.convert('1',dither=Image.Dither.FLOYDSTEINBERG)

def encode(image, card):
    pixels=image.load(); data=bytearray(image.height*48)
    for y in range(image.height):
        for x in range(WIDTH):
            if not pixels[x,y]: data[y*48+x//8]|=1<<(x%8)
    header=struct.pack('<4sHHIII',b'ORP1',WIDTH,image.height,len(data),zlib.crc32(data),content_crc(card))
    result=header+data
    validate_receipt(result,card)
    return result

def main():
    index=json.loads((ROOT/'sdcard/ORACLE/index.json').read_text())
    out=ROOT/'sdcard/ORACLE/print'; out.mkdir(exist_ok=True)
    previews=ROOT/'docs/printed-predictions'; previews.mkdir(exist_ok=True)
    manifest=[]
    for ident in index['ids']:
        card=json.loads((ROOT/'sdcard/ORACLE/cards'/f'{ident}.json').read_text())
        image=render(card); data=encode(image,card)
        (out/f'{ident}.bin').write_bytes(data)
        image.save(previews/f'{ident}.png')
        manifest.append({'id':ident,'width':WIDTH,'height':image.height,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    (ROOT/'docs/receipt-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    selected=['coat','beer_oracle','tea_ceremony']
    selected=[ident if (previews/f'{ident}.png').exists() else next(i for i in index['ids'] if i.startswith('beer')) for ident in selected]
    images=[Image.open(previews/f'{ident}.png').convert('RGB') for ident in selected]
    sheet=Image.new('RGB',(24+len(images)*408,max(im.height for im in images)+48),(33,28,50))
    for i,im in enumerate(images): sheet.paste(im,(24+i*408,24))
    sheet.save(ROOT/'docs/prediction-receipts.png')
    print(f'{len(manifest)} receipts; height {min(x["height"] for x in manifest)}..{max(x["height"] for x in manifest)}, total {sum(x["bytes"] for x in manifest):,} bytes')

if __name__=='__main__': main()
