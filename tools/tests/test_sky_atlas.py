"""Visual skybox contract: six faces, matched corners, no atlas edge bleeding."""
from collections import defaultdict
from pathlib import Path
import unittest
from PIL import Image
from tools.modelgen.generators import sky

ROOT=Path(__file__).resolve().parents[2]

class SkyAtlasTests(unittest.TestCase):
    def test_shared_sky_vertices_have_matching_colors(self):
        mesh,atlas=sky()
        self.assertEqual(len(mesh.faces),6)
        image=Image.open(ROOT/'assets/pack/data/sky/sky1.bmp').convert('RGB')
        colors=defaultdict(list)
        for face in mesh.faces:
            for index in face:
                pos=tuple(round(v,5) for v in mesh.verts[index])
                u,v=mesh.uvs[index]
                colors[pos].append(image.getpixel((min(255,int(u*256)),min(255,int(v*256)))))
        for pos,values in colors.items():
            for channel in range(3):
                self.assertLessEqual(max(v[channel] for v in values)-min(v[channel] for v in values),1,(pos,values))

    def test_sky_uvs_use_texel_centers_not_neighboring_faces(self):
        mesh,_=sky()
        for u,v in mesh.uvs:
            self.assertAlmostEqual((u*256)%1,.5)
            self.assertAlmostEqual((v*256)%1,.5)
