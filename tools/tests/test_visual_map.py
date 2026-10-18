"""Free-demo scale, valid sky and world-density regressions (no original assets)."""
import json
from pathlib import Path
import unittest
from tools.mapgen import author, bd1

ROOT = Path(__file__).resolve().parents[2]

class VisualMapTests(unittest.TestCase):
    def test_world_density_is_per_face_not_one_stretched_tile(self):
        doc = {'textures':['wall.bmp'],'blocks':[{'min':[0,0,0],'max':[64,32,96],
               'texture':'wall.bmp','texture_world_units':32}],'points':[]}
        result=bd1.loads(author.build(doc).bd1_bytes)
        block=result.blocks[0]
        for index,(u,v) in enumerate([(2,3),(2,3),(2,1),(3,1),(2,1),(3,1)]):
            self.assertAlmostEqual(max(block.faces[index].u)-min(block.faces[index].u),u)
            self.assertAlmostEqual(max(block.faces[index].v)-min(block.faces[index].v),v)

    def test_demo_is_human_scale_and_menu_copies_match(self):
        doc=json.loads((ROOT/'tools/mapgen/examples/map10_demo.json').read_text())
        self.assertGreaterEqual(max(b['max'][0]-b['min'][0] for b in doc['blocks']),200)
        self.assertGreaterEqual(max(b['max'][1]-b['min'][1] for b in doc['blocks']),30)
        result=author.build(doc)
        for slot in ('data/map10/temp.bd1','data/menu/temp.bd1'):
            self.assertEqual((ROOT/'assets/pack'/slot).read_bytes(),result.bd1_bytes)
        for slot in ('data/map10/op.pd1','data/menu/demo.pd1'):
            self.assertEqual((ROOT/'assets/pack'/slot).read_bytes(),result.pd1_bytes)

    def test_starter_uses_existing_valid_sky(self):
        sky=int((ROOT/'assets/pack/data/starter/start.txt').read_text().splitlines()[2])
        self.assertIn(sky,range(1,6))
        self.assertTrue((ROOT/f'assets/pack/data/sky/sky{sky}.bmp').is_file())

    def test_density_rejects_zero_negative_and_nonfinite_values(self):
        for units in (0,-1,float('nan'),float('inf')):
            doc={'textures':['wall.bmp'],'blocks':[{'min':[0,0,0],'max':[64,32,96],
                 'texture':'wall.bmp','texture_world_units':units}],'points':[]}
            with self.assertRaises(author.AuthorError):author.build(doc)
