import struct
import tempfile
import unittest
from pathlib import Path
from PIL import Image
import artgen
import dds


class DDSTests(unittest.TestCase):
    def test_known_pixels(self):
        pixels = bytes((255, 0, 0, 255, 0, 255, 0, 128,
                        0, 0, 255, 0, 16, 32, 48, 64))
        for fmt, payload, expected in (
            ('dds_argb8', bytes.fromhex('0000ffff00ff0080ff00000030201040'), pixels),
            ('dds_argb4', bytes.fromhex('00fff0800f002341'),
             bytes((240,0,0,240,0,240,0,128,0,0,240,0,16,32,48,64))),
        ):
            # Independent byte-level expectations, including engine nibble<<4.
            data = dds.header(2, 2, fmt) + dds.encode(pixels, 2, 2, fmt)
            self.assertEqual(data[128:], payload)
            self.assertEqual(dds.engine_decode(data), (2, 2, expected))
            self.assertEqual(struct.unpack_from('<I', data, 0x70)[0], 0)

    def test_scope_alpha(self):
        from scope import scope_image
        im = scope_image()
        self.assertEqual(im.getpixel((64,64)), (0,0,0,0))
        self.assertEqual(im.getpixel((0,0)), (0,0,0,255))
        self.assertTrue(any(0 < a < 255 for a in im.getchannel('A').get_flattened_data()))
        self.assertTrue(all(rgb == (0,0,0) for rgb in im.convert('RGB').get_flattened_data()))

    def test_candidate_sheet_composites_rgba_over_checker(self):
        from unittest.mock import patch
        import json
        with tempfile.TemporaryDirectory() as td:
            slot = {'path': 'test.dds'}
            tiled = Path(td) / '1_tiled.png'
            Image.new('RGBA', (32, 32), (255, 0, 0, 128)).save(tiled)
            with patch.object(artgen, 'work_dir', return_value=td), \
                 patch.object(artgen, 'candidates', return_value=[1]), \
                 patch.object(artgen, '_read_meta', return_value={}):
                output = artgen.make_sheet(slot, thumb=32)
            preview = Image.open(output).convert('RGB')
            self.assertEqual(preview.getpixel((24, 24)), (245, 117, 117))

    def test_pipeline_and_pick(self):
        from unittest.mock import patch
        import json
        for fmt in ('dds_argb8', 'dds_argb4', 'bmp8'):
            with self.subTest(fmt=fmt), tempfile.TemporaryDirectory() as td:
                root = Path(td)
                slot = dict(path='data/test.'+('bmp' if fmt == 'bmp8' else 'dds'),
                            ref_format=fmt, ref_size=[2,2], alpha='none' if fmt == 'bmp8' else 'channel')
                with patch.multiple(artgen, WORK_ROOT=str(root/'work'), PACK_ROOT=str(root/'pack'),
                                    PROVENANCE=str(root/'provenance.jsonl'), MANIFEST=str(root/'absent'),
                                    LOG=str(root/'log')):
                    wd = Path(artgen.work_dir(slot)); wd.mkdir(parents=True)
                    im = Image.new('RGBA', (2,2)); im.putdata([(255,0,0,255),(255,0,0,64),(0,0,0,0),(0,255,0,255)])
                    im.save(wd/'raw_1.png')
                    (wd/'1.json').write_text(json.dumps(dict(prompt='test', raw_sha256='test')))
                    meta = artgen.postprocess(slot, 1)
                    artgen.pick(slot, 1)
                    dst = root/'pack'/slot['path']
                    self.assertEqual(artgen.verify_texture(dst, slot), [])
                    if fmt != 'bmp8':
                        _,_,pixels = dds.engine_load(dst)
                        self.assertEqual(pixels[8:12], bytes(4))
                        self.assertTrue(0 < pixels[7] < 255)
                    else:
                        self.assertIn('bmp_sha256', meta)
                        self.assertEqual(Image.open(dst).getpixel((0,0)), Image.open(wd/'1.bmp').getpixel((0,0)))

    def test_rgb_pipeline_extraction(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as td:
            slot = dict(path='effect.dds', ref_format='dds_argb8', ref_size=[2,2], alpha_source='black')
            raw = Path(td)/'raw.png'
            im = Image.new('RGB', (2,2)); im.putdata([(0,0,0),(64,32,0),(255,255,255),(1,0,0)])
            im.save(raw)
            with patch.object(artgen, 'work_dir', return_value=td):
                artgen.postprocess_dds(slot, 1, raw, {})
            self.assertEqual(dds.engine_load(Path(td)/'1.dds')[2][:8],
                             bytes((0,0,0,0,255,128,0,64)))

    def test_black_extraction(self):
        im = Image.new('RGB', (3, 1))
        im.putdata([(0,0,0), (64,32,0), (255,128,0)])
        result = artgen.extract_black_alpha(im)
        self.assertEqual(list(result.get_flattened_data()), [(0,0,0,0),(255,128,0,64),(255,128,0,255)])

    def test_engine_decode_rejects_nonpositive_dimensions_and_truncated_pixels(self):
        valid = dds.header(1, 1, 'dds_argb8') + bytes((1, 2, 3, 4))
        for width, height in ((0, 1), (1, 0)):
            with self.subTest(width=width, height=height):
                malformed = bytearray(valid)
                struct.pack_into('<i', malformed, 0x10, width)
                struct.pack_into('<i', malformed, 0x0C, height)
                with self.assertRaisesRegex(ValueError, 'non-positive dimensions'):
                    dds.engine_decode(bytes(malformed))
        with self.assertRaisesRegex(ValueError, 'truncated pixel data'):
            dds.engine_decode(valid[:-1])

    def test_short_dds_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td)/'bad.dds'; p.write_bytes(b'DDS ')
            self.assertTrue(dds.verify_dds(p))


if __name__ == '__main__':
    unittest.main()
