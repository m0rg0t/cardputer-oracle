import json
import struct
import unittest
import zlib
from chinese_font import ROOT, glyphs, wide, BitmapFont

class ChineseFontTests(unittest.TestCase):
    def test_book_matches_source_and_covers_deck(self):
        data=(ROOT/'sdcard/ORACLE/fonts/chinese12.bin').read_bytes()
        magic,count,crc=struct.unpack('<4sII',data[:12])
        self.assertEqual(magic,b'OCF1')
        self.assertEqual(len(data),12+26*count)
        self.assertEqual(crc,zlib.crc32(data[12:]))
        records={}
        for offset in range(12,len(data),26):
            cp,*rows=struct.unpack('<13H',data[offset:offset+26])
            self.assertNotIn(cp,records)
            self.assertEqual(rows,glyphs()[cp][1])
            self.assertEqual(glyphs()[cp][0],12)
            records[cp]=rows
        self.assertEqual(list(records),sorted(records))
        for card in json.loads((ROOT/'content/deck.zh.json').read_text())['cards']:
            for char in card['title']+card['text']:
                if wide(ord(char)): self.assertIn(ord(char),records)
                elif char!='\n': self.assertIn(ord(char),glyphs())
        self.assertEqual(BitmapFont(2).getlength('茶A茶'),60)

if __name__=='__main__': unittest.main()
