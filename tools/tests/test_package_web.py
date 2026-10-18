import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('package_web', Path(__file__).resolve().parents[1]/'package_web.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)

class WebPackageTests(unittest.TestCase):
    def test_fingerprint_covers_both_engine_files_and_preserves_old_builds(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            template = root/'template.html'
            template.write_text('__ENGINE_SCRIPT__ __ENGINE_WASM__')
            (root/'openxops.js').write_bytes(b'engine-v1')
            (root/'openxops.wasm').write_bytes(b'wasm-v1')
            first = packager.package(root, template)
            self.assertEqual(first, packager.package(root, template))
            self.assertIn(f'openxops.{first}.js', (root/'index.html').read_text())
            (root/'openxops.js').write_bytes(b'engine-v2')
            second = packager.package(root, template)
            self.assertNotEqual(first, second)
            (root/'openxops.wasm').write_bytes(b'wasm-v2')
            third = packager.package(root, template)
            self.assertNotEqual(second, third)
            html = (root/'index.html').read_text()
            self.assertNotIn('__ENGINE_', html)
            self.assertIn(f'openxops.{third}.js', html)
            self.assertIn(f'openxops.{third}.wasm', html)
            self.assertEqual((root/f'openxops.{first}.js').read_bytes(), b'engine-v1')
            self.assertEqual((root/f'openxops.{first}.wasm').read_bytes(), b'wasm-v1')

    def test_invalid_template_does_not_replace_working_page(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            (root/'openxops.js').write_bytes(b'js')
            (root/'openxops.wasm').write_bytes(b'wasm')
            (root/'index.html').write_text('working page')
            template = root/'template.html'
            template.write_text('missing tokens')
            with self.assertRaises(ValueError):
                packager.package(root, template)
            self.assertEqual((root/'index.html').read_text(), 'working page')
