#!/usr/bin/env python3
"""Build only allowlisted public assets for GitHub Pages; Python stdlib only."""
from pathlib import Path
from collections import Counter
from html.parser import HTMLParser
from urllib.parse import urlsplit, unquote
import argparse
import html
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/site'
EXAMPLES = [('it_demo', 'Айти и юмор', 'humor_it'),
            ('beer_long_title', 'Пивная', 'beer'),
            ('tea_last_cookie', 'Чайная', 'tea')]


def validate_links(directory):
    class Links(HTMLParser):
        def __init__(self):
            super().__init__(); self.urls = []; self.ids = set()
        def handle_starttag(self, tag, attrs):
            a = dict(attrs)
            if 'id' in a: self.ids.add(a['id'])
            for key in ('href', 'src'):
                if key in a: self.urls.append(a[key])
    page = directory / 'index.html'
    parser = Links(); parser.feed(page.read_text())
    for url in parser.urls:
        parts = urlsplit(url)
        if parts.scheme or parts.netloc: continue
        if parts.path:
            path = (directory / unquote(parts.path)).resolve()
            if not path.is_relative_to(directory.resolve()) or not path.is_file():
                raise ValueError(f'Missing/unsafe site asset: {url}')
        elif parts.fragment and parts.fragment not in parser.ids:
            raise ValueError(f'Missing section: {url}')
    if '@@' in page.read_text(): raise ValueError('Unresolved template value')


def build(repository=''):
    if repository and not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
        raise ValueError('Use --repository OWNER/REPOSITORY')
    OUT.mkdir(parents=True, exist_ok=True)
    if OUT.is_symlink() or OUT.resolve() != ROOT / 'build/site':
        raise ValueError('Unsafe build destination')
    for p in OUT.iterdir():
        if p.is_dir() and not p.is_symlink(): shutil.rmtree(p)
        else: p.unlink()
    (OUT / 'assets').mkdir()
    deck = json.loads((ROOT / 'content/deck.json').read_text())['cards']
    cards = {c['id']: c for c in deck}
    counts = Counter(c['topic'] if c['topic'] in ('beer', 'tea') else 'humor_it' for c in deck)
    art = {i['id']: i['source'] for i in json.loads((ROOT / 'assets/artwork.json').read_text())['images']}
    def copy(source, name):
        if source.is_symlink() or not source.resolve().is_relative_to(ROOT):
            raise ValueError('Unsafe asset')
        if Path(name).name != name: raise ValueError('Unsafe asset filename')
        shutil.copyfile(source, OUT / 'assets' / name)
    copy(ROOT / 'assets/oracle-cat.png', 'oracle-cat.png')
    copy(ROOT / 'docs/media/screen-home.png', 'screen-home.png')
    copy(ROOT / 'docs/media/receipt-tea.png', 'receipt-tea.png')
    for guide in ('INSTALL.en.md', 'INSTALL.md', 'INSTALL.zh.md'):
        copy(ROOT / 'docs' / guide, guide)
    examples = []
    for ident, label, theme in EXAMPLES:
        card = cards[ident]; image = art[Path(card['image']).stem]
        copy(ROOT / 'assets' / image, image)
        audio = f'{ident}.wav'; copy(ROOT / 'sdcard/ORACLE/audio' / audio, audio)
        examples.append(f'''<article class="example">
<img src="assets/{html.escape(image)}" alt="{html.escape(label)}: иллюстрация из колоды" width="1254" height="1254" loading="lazy">
<div class="example-copy"><p class="eyebrow">{label} · {counts[theme]} карточек</p><h3>{html.escape(card['title'])}</h3>
<blockquote>{html.escape(card['text'])}</blockquote><details><summary>Послушать предсказание</summary>
<audio controls preload="none" aria-label="Озвучка: {html.escape(card['title'])}"><source src="assets/{audio}" type="audio/wav"><a href="assets/{audio}">Скачать запись</a></audio></details></div></article>''')
    page = (ROOT / 'site/index.html').read_text()
    values = {'VERSION': (ROOT / 'VERSION').read_text().strip(), 'TOTAL': str(len(deck)), 'EXAMPLES': '\n'.join(examples),
              'REPOSITORY_LINK': f'<a class="button" href="https://github.com/{repository}">Открыть код на GitHub ↗</a>' if repository else ''}
    for key, value in values.items(): page = page.replace('@@' + key + '@@', value)
    (OUT / 'index.html').write_text(page)
    shutil.copyfile(ROOT / 'site/style.css', OUT / 'style.css')
    (OUT / '.nojekyll').touch()
    validate_links(OUT)
    print(f'Built {OUT}: {sum(1 for p in OUT.rglob("*") if p.is_file())} public files; local links verified')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repository', default='', help='Optional OWNER/REPOSITORY for the GitHub link')
    build(p.parse_args().repository)
