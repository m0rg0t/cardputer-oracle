"""Ensure each public page keeps text, media and language navigation together."""
import hashlib
import json
import unittest
from build_site import build, OUT, ROOT, LANGUAGES, HTML_LANG, EXAMPLES, GUIDES, validate_links
from languages import deck_path

class SiteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def test_matching_locales_and_assets(self):
        for lang in LANGUAGES:
            page=(OUT/lang/'index.html').read_text()
            self.assertIn(f'lang="{HTML_LANG[lang]}"',page)
            self.assertIn('data-language-entry="explicit"',page)
            cards={c['id']:c for c in json.loads(deck_path(lang).read_text())['cards']}
            for ident,_ in EXAMPLES:
                import html
                self.assertIn(html.escape(cards[ident]['text']),page)
                self.assertIn(f'{ident}-{lang}.wav',page)
                original=ROOT/'sdcard'/cards[ident]['audio'].lstrip('/')
                self.assertEqual((OUT/f'assets/{ident}-{lang}.wav').read_bytes(),original.read_bytes())
            for kind in ('home','result'):
                self.assertIn(f'screen-{kind}-{lang}.png',page)
            self.assertIn(f'receipt-tea-{lang}.png',page)
            self.assertIn(GUIDES[lang],page)
            for other in LANGUAGES:
                self.assertIn(f'href="../{other}/"',page)
                if other!=lang:
                    self.assertNotIn(f'screen-home-{other}.png',page)
                    self.assertNotIn(f'receipt-tea-{other}.png',page)
            self.assertEqual(page.count('aria-current="page"'),1)
            self.assertNotIn('autoplay',page)

    def test_entry_and_all_relative_links(self):
        root=(OUT/'index.html').read_text()
        self.assertIn('data-language-entry="auto"',root)
        self.assertIn('<html lang="en"',root)
        self.assertIn('data-language="zh"',root) # No-JS users still have working links.
        validate_links(OUT)
        self.assertFalse((OUT/'ORACLE').exists())
        self.assertFalse(any(p.name=='config.json' for p in OUT.rglob('*')))

    def test_media_manifest(self):
        manifest=json.loads((ROOT/'docs/media/site-media.json').read_text())
        self.assertEqual(manifest['version'],(ROOT/'VERSION').read_text().strip())
        for items in manifest['languages'].values():
            for item in items.values():
                self.assertEqual(hashlib.sha256((OUT/'assets'/item['file']).read_bytes()).hexdigest(),item['sha256'])

if __name__=='__main__': unittest.main()
