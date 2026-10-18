import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path

SCRIPT = Path(__file__).parents[1] / 'audit_original_assets.py'
spec = importlib.util.spec_from_file_location('asset_audit', SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AuditTests(unittest.TestCase):
    def test_weapon_table_literals_and_normalization(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'parameter.cpp'
            source.write_text(r'''Weapon[0].name = "NONE"; Weapon[0].model = ""; Weapon[0].texture = "";
Weapon[1].name = "Test"; Weapon[1].model = ".\\data\\model\\gun.x"; Weapon[1].texture = "./data//gun.bmp";
''', encoding='utf-8')
            archive = root / 'game.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('data/model/gun.x', b'x')
                z.writestr('data/gun.bmp', b'x')
            report = audit.audit(archive, [source])
            self.assertEqual(len(report['weapons']), 1)
            self.assertEqual(report['weapons'][0]['model']['status'], 'present')
            self.assertEqual(report['weapons'][0]['texture']['status'], 'present')
            self.assertEqual(report['weapons'][0]['texture']['format'], 'bmp')

    def test_missing_empty_decode_and_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); source = root / 'resource.cpp'
            source.write_text(r'''LoadModel("./data/missing.x"); LoadTexture("data/empty.bmp"); LoadSound("../escape.wav");''', encoding='utf-8')
            archive = root / 'game.zip'
            with zipfile.ZipFile(archive, 'w') as z: z.writestr('data/empty.bmp', b'')
            refs = audit.audit(archive, [source])['references']
            self.assertEqual([r['status'] for r in refs], ['missing', 'present', 'unverified'])

    def test_source_double_backslash_cstring(self):
        self.assertEqual(audit.c_string(r'"data\\model\\a.x"'), 'data\\model\\a.x')


if __name__ == '__main__':
    unittest.main()
