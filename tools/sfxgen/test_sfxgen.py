"""Run with python3 -m unittest discover -s tools/sfxgen -v."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[2]
SOUNDS = ROOT / 'assets/pack/data/sound'
NAMES = {'bang', 'bang1', 'bang2', 'bang3', 'bang4', 'bang5', 'ka', 'can',
         'dan', 'hit1', 'hit2', 'hit3', 'hyu', 'cco'}


class SoundPackTests(unittest.TestCase):
    def test_headers_and_levels(self):
        self.assertEqual({p.stem for p in SOUNDS.glob('*.wav')}, NAMES)
        for name in sorted(NAMES):
            with self.subTest(name=name):
                path = SOUNDS / (name + '.wav')
                data = path.read_bytes()
                self.assertEqual(data[:4], b'RIFF')
                self.assertEqual(data[8:16], b'WAVEfmt ')
                self.assertEqual(struct.unpack_from('<I', data, 4)[0], len(data)-8)
                self.assertEqual(struct.unpack_from('<IHHIIHH', data, 16),
                                 (16, 1, 1, 22050, 44100, 2, 16))
                self.assertEqual(data[36:40], b'data')
                self.assertEqual(struct.unpack_from('<I', data, 40)[0], len(data)-44)
                with wave.open(str(path)) as wav:
                    self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (1, 2, 22050))
                    samples = struct.unpack('<%dh' % wav.getnframes(), wav.readframes(wav.getnframes()))
                self.assertGreater(max(abs(x) for x in samples), 1000)
                self.assertLess(max(abs(x) for x in samples), 32767)
                self.assertEqual(samples[-1], 0)

    def test_regeneration_matches_committed_hashes(self):
        with tempfile.TemporaryDirectory() as out:
            subprocess.run([sys.executable, str(ROOT / 'tools/sfxgen/sfxgen.py'), '--out', out, '--seed', '2003'], check=True, capture_output=True)
            self.assertEqual({p.stem for p in Path(out).glob('*.wav')}, NAMES)
            for name in sorted(NAMES):
                self.assertEqual(hashlib.sha256((Path(out)/(name+'.wav')).read_bytes()).hexdigest(),
                                 hashlib.sha256((SOUNDS/(name+'.wav')).read_bytes()).hexdigest(), name)

    def test_manifest_and_provenance(self):
        manifest = json.loads((ROOT/'assets/manifest.json').read_text())
        slots = {s['path']: s for s in manifest['slots'] if s['kind'] == 'sound'}
        records = {r['path']: r for r in map(json.loads, (ROOT/'assets/provenance.jsonl').read_text().splitlines())}
        self.assertEqual(len(slots), 14)
        for name in sorted(NAMES):
            path = 'data/sound/' + name + '.wav'
            with self.subTest(path=path):
                self.assertEqual(slots[path]['status'], 'generated')
                record = records[path]
                self.assertEqual(record['sha256'], hashlib.sha256((SOUNDS/(name+'.wav')).read_bytes()).hexdigest())
                self.assertEqual(record['license'], 'CC0-1.0')
                self.assertIsNone(record['curated_by'])
                for key in ('generator', 'recipe', 'author', 'date'):
                    self.assertTrue(record[key])


if __name__ == '__main__':
    unittest.main()
