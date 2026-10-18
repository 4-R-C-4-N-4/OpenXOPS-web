"""Proportions and attachment checks in the engine's assembled world units."""
import unittest
from tools.modelgen import generators as g

class HumanAssemblyTests(unittest.TestCase):
    def test_standing_soles_match_ground_transform(self):
        mesh,_=g.human_legs(phase=0)
        floor=min(v[1] for v in mesh.verts)*9-.2
        self.assertAlmostEqual(floor,0,delta=.02)

    def test_weapon_forward_extent_matches_engine_muzzle_anchor(self):
        from tools.modelgen.specs import WEAPON_SPECS
        for name,spec in WEAPON_SPECS.items():
            if spec['kind']=='grenade':continue
            mesh,_=g.weapon(**spec)
            self.assertAlmostEqual(min(v[2] for v in mesh.verts),spec['muzzle'][2],delta=1e-5,msg=name)

    def test_head_is_not_an_elongated_pillar(self):
        mesh,_=g.human_upper()
        head=[p for p in mesh.verts if abs(abs(p[0])-g.HEAD_HALF_W)<1e-8]
        width=max(p[0] for p in head)-min(p[0] for p in head)
        height=max(p[1] for p in head)-min(p[1] for p in head)
        self.assertLessEqual(height/width,1.25)

    def test_upper_body_has_a_clothed_human_width(self):
        mesh,_=g.human_upper()
        torso=mesh.verts[:24]
        width=(max(p[0] for p in torso)-min(p[0] for p in torso))*9.4
        self.assertGreaterEqual(width,4.7)
        self.assertLessEqual(width,5.6)

    def test_arm_shoulders_are_outside_and_attached_to_torso(self):
        mesh,_=g.human_arms('twohand')
        width=max(p[0] for p in mesh.verts)-min(p[0] for p in mesh.verts)
        self.assertGreaterEqual(width*9,5.0)
        self.assertLessEqual(width*9,7.0)

    def test_twohand_wrist_reaches_rifle_grip(self):
        mesh,_=g.human_arms('twohand')
        # Right hand is the last box (left/right each upper+forearm+hand).
        hand=mesh.verts[-24:]
        center=tuple((max(p[k] for p in hand)+min(p[k] for p in hand))/2 for k in range(3))
        # M4 grip translated from parameter.cpp mx/my/mz and arm scale=9.
        target=(.9/9,.6/9,-3.7/9)
        distance=sum((center[k]-target[k])**2 for k in range(3))**.5*9
        self.assertLess(distance,1.0)
