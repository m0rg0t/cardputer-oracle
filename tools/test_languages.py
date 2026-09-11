"""Language invariants that keep screen text, recorded speech and paper in sync."""
import json
import re
import unittest
import wave
from languages import ROOT, LANGUAGES, deck_path, firmware_env
from prepare_pack import validate
from prepare_receipts import render, encode, validate_receipt

class LanguageTests(unittest.TestCase):
    def test_same_cards_and_themes(self):
        decks=[json.loads(deck_path(lang).read_text()) for lang in LANGUAGES]
        for lang,deck in zip(LANGUAGES,decks):
            self.assertEqual(deck['language'],lang)
            cards=validate(deck,require_media=True)
            self.assertEqual(len(cards),120)
            self.assertEqual(sum(c['topic']=='beer' for c in cards),30)
            self.assertEqual(sum(c['topic']=='tea' for c in cards),30)
        for deck in decks[1:]:
            self.assertEqual([(c['id'],c['topic'],c['image'],c['motif']) for c in decks[0]['cards']],
                             [(c['id'],c['topic'],c['image'],c['motif']) for c in deck['cards']])

    def test_speech_matches_current_text_and_language(self):
        for lang in LANGUAGES:
            for card in json.loads(deck_path(lang).read_text())['cards']:
                with self.subTest(lang=lang,card=card['id']):
                    path=ROOT/'sdcard'/card['audio'].lstrip('/')
                    meta=json.loads(path.with_suffix('.json').read_text())
                    self.assertEqual(meta['text'],card['text'])
                    self.assertEqual(meta.get('language','ru'),lang)
                    self.assertEqual('/audio/en/' in card['audio'],lang=='en')
                    self.assertEqual('/audio/zh/' in card['audio'],lang=='zh')
                    with wave.open(str(path),'rb') as wav:
                        self.assertEqual((wav.getnchannels(),wav.getsampwidth(),wav.getframerate()),(1,2,16000))
                        self.assertGreater(wav.getnframes(),32000)

    def test_english_and_universal_content(self):
        for lang in LANGUAGES:
            for card in json.loads(deck_path(lang).read_text())['cards']:
                text=card['title']+' '+card['text']
                if lang=='en': self.assertTrue(text.isascii(),card['id'])
                self.assertIsNone(re.search(r'доклад|выступлен|слайд|lecture|presentation|keynote',text,re.I),card['id'])

    def test_receipts_fit_and_bind_the_translated_text(self):
        for lang in LANGUAGES:
            for card in json.loads(deck_path(lang).read_text())['cards']:
                with self.subTest(lang=lang,card=card['id']):
                    data=encode(render(card,lang),card)
                    self.assertLessEqual(validate_receipt(data,card),1024)
                    with self.assertRaises(ValueError):
                        validate_receipt(data,{**card,'text':card['text']+'!'})

    def test_separate_build_targets(self):
        self.assertEqual(len({firmware_env(lang) for lang in LANGUAGES}),3)
        with self.assertRaises(ValueError): firmware_env('de')

if __name__=='__main__': unittest.main()
