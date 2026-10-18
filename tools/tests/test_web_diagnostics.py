import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('stage_pages_diagnostics', ROOT/'tools/stage_pages.py')
staging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(staging)

class DiagnosticBuildTests(unittest.TestCase):
    def test_staging_rejects_qa_exports_before_overwriting_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);build=base/'build';build.mkdir();dest=base/'public';dest.mkdir()
            marker=dest/'keep.txt';marker.write_text('existing public files')
            stem='openxops.'+'a'*16
            (build/'index.html').write_text(f'<script src="{stem}.js"></script>\n\'{stem}.wasm\'')
            (build/(stem+'.js')).write_text('Module._qa_reticle_pixels = diagnostic;')
            with self.assertRaisesRegex(ValueError,'diagnostic/test-hook'):
                staging.stage(build,dest)
            self.assertEqual(marker.read_text(),'existing public files')

    def test_diagnostics_are_explicitly_opt_in(self):
        script=(ROOT/'tools/build-playable-web.sh').read_text()
        self.assertIn('${OPENXOPS_TEST_HOOKS:-0}',script)
        self.assertIn('tests/browser/qa_hooks.cpp',script)

if __name__=='__main__':unittest.main()
