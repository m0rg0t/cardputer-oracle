#!/usr/bin/env python3
import copy
import json
from pathlib import Path
import unittest
from prepare_pack import ROOT, validate
from prepare_receipts import validate_receipt

class PackTest(unittest.TestCase):
    def setUp(self): self.deck=json.loads((ROOT/'content/deck.json').read_text())
    def test_deck(self):
        cards=validate(self.deck)
        self.assertEqual(len(cards),120)
        self.assertEqual(sum(c['topic']=='beer' for c in cards),30)
        self.assertEqual(sum(c['topic']=='tea' for c in cards),30)
        self.assertEqual(sum(c['topic'] in ('life','it') for c in cards),60)
    def test_unknown_topic(self):
        self.deck['cards'][0]['topic']='unknown'
        with self.assertRaises(ValueError): validate(self.deck)
    def test_duplicate(self):
        self.deck['cards'][1]['id']=self.deck['cards'][0]['id']
        with self.assertRaises(ValueError):validate(self.deck)
    def test_escape_path(self):
        self.deck['cards'][0]['audio']='/ORACLE/../private.wav'
        with self.assertRaises(ValueError):validate(self.deck)
    def test_missing_file(self):
        self.deck['cards'][0]['audio']='/ORACLE/audio/missing.wav'
        with self.assertRaises(ValueError):validate(self.deck)
    def test_unsupported_glyph(self):
        self.deck['cards'][0]['text']='Hello 👁'
        with self.assertRaises(ValueError):validate(self.deck)
    def test_release_pack(self):
        index=json.loads((ROOT/'sdcard/ORACLE/index.json').read_text())
        cards=[json.loads((ROOT/'sdcard/ORACLE/cards'/f'{i}.json').read_text()) for i in index['ids']]
        self.assertCountEqual(index['ids'],[c['id'] for c in self.deck['cards']])
        self.assertEqual(index['themes'],[{'life':0,'it':0,'beer':1,'tea':2}[c['topic']] for c in cards])
        validate({'schema':1,'cards':cards},require_media=True)
        for card in cards:
            meta=json.loads((ROOT/'sdcard'/card['audio'].lstrip('/')).with_suffix('.json').read_text())
            self.assertEqual(meta['text'],card['text'])
    def test_media_not_silence(self):
        import wave, array, math
        for path in (ROOT/'sdcard/ORACLE/audio').rglob('*.wav'):
            with wave.open(str(path),'rb') as f:
                self.assertGreater(f.getnframes()/f.getframerate(),2)
                samples=array.array('h',f.readframes(f.getnframes()))
            peak=max(abs(s) for s in samples)
            rms=math.sqrt(sum(s*s for s in samples)/len(samples))
            self.assertGreater(rms,100)
            self.assertLess(peak,32767)
    def test_receipts(self):
        for card in self.deck['cards']:
            data=(ROOT/'sdcard/ORACLE/print'/f'{card["id"]}.bin').read_bytes()
            self.assertLessEqual(validate_receipt(data,card),1024)
            wrong=copy.deepcopy(card);wrong['text']+='!'
            with self.assertRaises(ValueError):validate_receipt(data,wrong)
            damaged=bytearray(data);damaged[-1]^=1
            with self.assertRaises(ValueError):validate_receipt(damaged,card)
if __name__=='__main__': unittest.main()
