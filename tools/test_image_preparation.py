"""Exercise the real Pillow export script using generated artwork only."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

SCRIPT = Path(__file__).with_name('prepare_images.py')


class ImagePreparationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='oracle-artwork-test-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in ('tools', 'assets', 'content', 'docs', 'sdcard/ORACLE/images'):
            (self.root/name).mkdir(parents=True)
        shutil.copyfile(SCRIPT, self.root/'tools/prepare_images.py')
        Image.new('RGBA', (128, 128), (40, 80, 120, 128)).save(self.root/'assets/input.png')
        self.manifest = {'images': [{'id': 'fixture', 'source': 'input.png',
                                   'cards': ['card-1'], 'title': 'Synthetic fixture'}]}
        self.deck = {'cards': [{'id': 'card-1'}]}

    def run_export(self):
        (self.root/'assets/artwork.json').write_text(json.dumps(self.manifest))
        (self.root/'content/deck.json').write_text(json.dumps(self.deck))
        return subprocess.run([sys.executable, str(self.root/'tools/prepare_images.py')],
                              cwd=self.root, text=True, capture_output=True)

    def test_exports_baseline_rgb_jpeg_and_assigns_card(self):
        result = self.run_export()
        self.assertEqual(result.returncode, 0, result.stderr)
        image_path = self.root/'sdcard/ORACLE/images/fixture.jpg'
        with Image.open(image_path) as image:
            self.assertEqual(image.size, (92, 92))
            self.assertEqual(image.mode, 'RGB')
            self.assertFalse(image.info.get('progressive', False))
            image.verify()
        self.assertLess(image_path.stat().st_size, 65536)
        card = json.loads((self.root/'content/deck.json').read_text())['cards'][0]
        self.assertEqual(card['image'], '/ORACLE/images/fixture.jpg')
        self.assertTrue((self.root/'docs/artwork-overview.png').is_file())

    def test_rejects_non_square_artwork_without_changing_deck(self):
        Image.new('RGB', (128, 64)).save(self.root/'assets/input.png')
        result = self.run_export()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Expected square artwork', result.stderr)
        self.assertEqual(json.loads((self.root/'content/deck.json').read_text()), self.deck)

    def test_rejects_duplicate_assignments(self):
        self.manifest['images'].append(dict(self.manifest['images'][0], id='other'))
        result = self.run_export()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Duplicate artwork assignment', result.stderr)

    def test_preserves_existing_original_cat_jpeg(self):
        self.manifest['images'][0]['id'] = 'oracle'
        original = self.root/'sdcard/ORACLE/images/oracle.jpg'
        Image.new('RGB', (92, 92), 'orange').save(original, 'JPEG')
        before = original.read_bytes()
        result = self.run_export()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(original.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
