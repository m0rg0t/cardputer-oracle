#!/usr/bin/env python3
"""Build three static language pages using only allowlisted public assets; stdlib only."""
from pathlib import Path
from collections import Counter
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote
import argparse
import hashlib
import html
import json
import re
import shutil
import struct
from languages import LANGUAGES, deck_path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/site'
EXAMPLES = [('it_demo','humor_it'),('beer_long_title','beer'),('tea_last_cookie','tea')]
HTML_LANG = {'en':'en','ru':'ru','zh':'zh-Hans'}
LABELS = {'en':'English','ru':'Русский','zh':'简体中文'}
GUIDES = {'en':'INSTALL.en.md','ru':'INSTALL.md','zh':'INSTALL.zh.md'}

def validate_links(directory):
    class Links(HTMLParser):
        def __init__(self):
            super().__init__(); self.urls=[]; self.ids=set()
        def handle_starttag(self, tag, attrs):
            values=dict(attrs)
            if 'id' in values: self.ids.add(values['id'])
            for key in ('href','src'):
                if key in values: self.urls.append(values[key])
    pages={}
    for page in directory.rglob('*.html'):
        parser=Links(); parser.feed(page.read_text()); pages[page.resolve()]=parser
        if '@@' in page.read_text(): raise ValueError('Unresolved template value: '+str(page))
    for page,parser in pages.items():
        for url in parser.urls:
            parts=urlsplit(url)
            if parts.scheme or parts.netloc: continue
            target=(page.parent/unquote(parts.path)).resolve() if parts.path else page
            if target.is_dir(): target=target/'index.html'
            if not target.is_relative_to(directory.resolve()) or not target.is_file():
                raise ValueError(f'Missing/unsafe site asset: {url} in {page}')
            if parts.fragment and target in pages and parts.fragment not in pages[target].ids:
                raise ValueError(f'Missing section: {url}')

def build(repository='m0rg0t/cardputer-oracle'):
    if repository and not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',repository):
        raise ValueError('Use --repository OWNER/REPOSITORY')
    OUT.mkdir(parents=True,exist_ok=True)
    if OUT.is_symlink() or OUT.resolve()!=ROOT/'build/site': raise ValueError('Unsafe build destination')
    for path in OUT.iterdir():
        if path.is_dir() and not path.is_symlink(): shutil.rmtree(path)
        else: path.unlink()
    (OUT/'assets').mkdir()
    def copy(source,name):
        if source.is_symlink() or not source.resolve().is_relative_to(ROOT) or Path(name).name!=name:
            raise ValueError('Unsafe public asset')
        shutil.copyfile(source,OUT/'assets'/name)
    version=(ROOT/'VERSION').read_text().strip()
    media=json.loads((ROOT/'docs/media/site-media.json').read_text())
    if media['version']!=version: raise ValueError('Refresh the screenshots for this firmware version')
    artwork=json.loads((ROOT/'assets/artwork.json').read_text())['images']
    art={item['id']:item['source'] for item in artwork}
    copy(ROOT/'assets/oracle-cat.png','oracle-cat.png')
    template=(ROOT/'site/index.html').read_text()
    expected_keys=None
    for lang in LANGUAGES:
        strings=json.loads((ROOT/f'site/locales/{lang}.json').read_text())
        if expected_keys is None: expected_keys=set(strings)
        if set(strings)!=expected_keys: raise ValueError('Incomplete site translation: '+lang)
        deck=json.loads(deck_path(lang).read_text())
        if deck.get('language')!=lang: raise ValueError('Wrong-language source deck')
        cards={card['id']:card for card in deck['cards']}
        counts=Counter(card['topic'] if card['topic'] in ('beer','tea') else 'humor_it' for card in deck['cards'])
        for kind in ('home','result','receipt'):
            entry=media['languages'][lang][kind]
            expected=f'receipt-tea-{lang}.png' if kind=='receipt' else f'screen-{kind}-{lang}.png'
            if entry['file']!=expected: raise ValueError('Screenshot language mismatch')
            source=ROOT/'docs/media'/expected
            if hashlib.sha256(source.read_bytes()).hexdigest()!=entry['sha256']:
                raise ValueError('Stale screenshot manifest: '+expected)
            copy(source,expected)
        copy(ROOT/'docs'/GUIDES[lang],GUIDES[lang])
        examples=[]
        for i,(ident,theme) in enumerate(EXAMPLES):
            card=cards[ident]; image=art[Path(card['image']).stem]
            copy(ROOT/'assets'/image,image)
            source=ROOT/'sdcard'/card['audio'].lstrip('/')
            voice=json.loads(source.with_suffix('.json').read_text())
            if voice.get('language','ru')!=lang or voice['text']!=card['text']:
                raise ValueError('Stale/wrong-language example narration: '+ident)
            audio=f'{ident}-{lang}.wav'; copy(source,audio)
            examples.append((card,image,audio,strings['THEMES'][i],counts[theme]))
        for entry in (False,True) if lang=='en' else (False,):
            base='' if entry else '../'
            def esc(value): return html.escape(str(value))
            values={key:esc(value) for key,value in strings.items() if isinstance(value,str)}
            for key in ('HEADLINE','EXAMPLES_TITLE','HOW_TITLE','BUILD_TITLE','FOOTER'):
                values[key]=values[key].replace('\n','<br>')
            values.update(BASE=base,LANG=lang,HTML_LANG=HTML_LANG[lang],ENTRY='auto' if entry else 'explicit',
                VERSION=esc(version),TOTAL=str(len(cards)),ART_COUNT=str(len(artwork)),GUIDE_FILE=GUIDES[lang],
                BUILD_COMMAND='python tools/build_release.py'+('' if lang=='en' else ' --language '+lang),
                RECEIPT_HEIGHT=str(struct.unpack('>I',(OUT/f'assets/receipt-tea-{lang}.png').read_bytes()[20:24])[0]))
            values['SWITCHER']=''.join(f'<a href="{base}{other}/" lang="{HTML_LANG[other]}" hreflang="{HTML_LANG[other]}" data-language="{other}"'+(' aria-current="page"' if other==lang else '')+f'>{LABELS[other]}</a>' for other in LANGUAGES)
            values['ALTERNATES']=''.join(f'<link rel="alternate" hreflang="{HTML_LANG[other]}" href="{base}{other}/">' for other in LANGUAGES)+f'<link rel="alternate" hreflang="x-default" href="{base}index.html">'
            values['STEPS']=''.join(f'<li><h3>{esc(title)}</h3><p>{esc(body)}</p></li>' for title,body in strings['STEPS'])
            values['REPOSITORY_LINK']=f'<a class="button" href="https://github.com/{repository}">{esc(strings["REPOSITORY_LABEL"])} ↗</a>' if repository else ''
            values['EXAMPLES']=''.join(f'''<article class="example">
<img src="{base}assets/{esc(image)}" alt="{esc(label)}: {esc(strings['ART_ALT'])}" width="1254" height="1254" loading="lazy">
<div class="example-copy"><p class="eyebrow">{esc(label)} · {count} {esc(strings['CARDS_LABEL'])}</p><h3>{esc(card['title'])}</h3>
<blockquote>{esc(card['text'])}</blockquote><details><summary>{esc(strings['LISTEN'])}</summary>
<audio controls preload="none" aria-label="{esc(strings['AUDIO_LABEL'])}: {esc(card['title'])}"><source src="{base}assets/{audio}" type="audio/wav"><a href="{base}assets/{audio}">{esc(strings['DOWNLOAD_AUDIO'])}</a></audio></details></div></article>''' for card,image,audio,label,count in examples)
            page=re.sub(r'@@([A-Z_]+)@@',lambda match:values[match[1]],template)
            destination=OUT if entry else OUT/lang
            destination.mkdir(exist_ok=True)
            (destination/'index.html').write_text(page)
    for name in ('style.css','language.js'): shutil.copyfile(ROOT/'site'/name,OUT/name)
    (OUT/'.nojekyll').touch()
    validate_links(OUT)
    print(f'Built {OUT}: EN/RU/ZH + automatic entry; {sum(p.is_file() for p in OUT.rglob("*"))} public files; links verified')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--repository',default='m0rg0t/cardputer-oracle',help='OWNER/REPOSITORY for source link')
    build(parser.parse_args().repository)
