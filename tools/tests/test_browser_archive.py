"""Preflight rejects engine/source archives before main; retry stays usable.
Synthetic ZIPs only. Requires a C++17 compiler and system zlib development files.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
REQUIRED = ('data/char.dds', 'data/model/up0.x', 'data/map10/temp.bd1', 'data/sound/bang.wav')

@unittest.skipUnless(shutil.which('g++'), 'needs g++ and zlib')
class ArchivePreflightTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR'))
        cls.root = Path(cls.tmp.name)
        harness = cls.root / 'preflight.cpp'
        harness.write_text('''#include <cstdio>
int openxops_prepare_archive_path(const char*);
extern "C" const char* openxops_archive_error();
int main(int argc,char**argv){for(int i=1;i<argc;++i){
 int code=openxops_prepare_archive_path(argv[i]);
 printf("%d:%s\\n",code,openxops_archive_error());}return 0;}
''')
        cls.exe = cls.root / 'preflight'
        subprocess.run(['g++', '-std=c++17', str(harness),
            str(ROOT/'src/platform/vfs.cpp'),
            str(ROOT/'src/platform/portable/browser-archive.cpp'),
            '-lz', '-o', str(cls.exe)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def archive(self, name, files):
        path = self.root / name
        with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as z:
            for key in files:
                z.writestr(key, b'original synthetic test bytes')
        return path

    def invoke(self, *paths):
        p = subprocess.run([str(self.exe), *map(str, paths)], check=True,
                           capture_output=True, text=True)
        return p.stdout.splitlines()

    def test_wrong_archive_then_valid_archive_without_restart(self):
        bad = self.archive('source.zip', ['OpenXOPS-source/main.cpp'])
        good = self.archive('game.zip', REQUIRED)
        lines = self.invoke(bad, good)
        self.assertTrue(lines[0].startswith('2:'))
        self.assertIn('source code', lines[0])
        self.assertEqual(lines[1], '0:')

    def test_common_wrapper_accepted(self):
        wrapped = self.archive('wrapped.zip', ['xops096/'+p for p in REQUIRED])
        self.assertEqual(self.invoke(wrapped), ['0:'])

    def test_corrupt_zip_rejected(self):
        path = self.root/'damaged.zip'
        path.write_bytes(b'not a ZIP')
        self.assertTrue(self.invoke(path)[0].startswith('1:Cannot read this ZIP'))

if __name__ == '__main__':
    unittest.main()
