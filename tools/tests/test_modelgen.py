"""Tests for tools/modelgen (Phase B3).

Validates generated .x files against the actual grammar subset
tools/contract/check_x.py emulates from src/d3dgraphics-opengl.cpp (see
docs/ASSET-CONTRACT.md section 4), plus modelgen-specific invariants:
determinism, poly budget, UV range, and full P0 slot coverage.
"""
import importlib
import os
import sys
import tempfile
import shutil
import unittest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.modelgen import generators, primitives  # noqa: E402
from tools.modelgen.specs import SPECS, P0_KEYS  # noqa: E402
from tools.modelgen.xwriter import Mesh, write_x, render_x, fmt  # noqa: E402
from tools.modelgen.uv_atlas import Atlas, render_uv_template  # noqa: E402
from tools.modelgen import __main__ as modelgen_main  # noqa: E402

CONTRACT_DIR = os.path.join(REPO_ROOT, "tools", "contract")
sys.path.insert(0, CONTRACT_DIR)
import check_x  # noqa: E402

MAX_POLYS = 139  # FREE-DATA-PACK.md F-4


class XWriterTests(unittest.TestCase):
    def test_header_exact(self):
        m = Mesh()
        m.add_vertex((0, 0, 0), (0, 0))
        m.add_vertex((1, 0, 0), (1, 0))
        m.add_vertex((1, 1, 0), (1, 1))
        m.add_face((0, 1, 2))
        text = render_x("Tri", [m])
        self.assertTrue(text.startswith("xof 0302txt"))
        self.assertIn("Mesh Tri {", text)
        self.assertTrue(text.splitlines()[0][:11] == "xof 0302txt")

    def test_mesh_textcoords_one_space_indent(self):
        m = Mesh()
        m.add_vertex((0, 0, 0), (0, 0))
        m.add_vertex((1, 0, 0), (1, 0))
        m.add_vertex((1, 1, 0), (1, 1))
        m.add_face((0, 1, 2))
        text = render_x("Tri", [m])
        self.assertIn("\n MeshTextureCoords {\n", text)
        self.assertNotIn("\nMeshTextureCoords", text)  # must not be column 0

    def test_float_format_deterministic_and_no_negzero(self):
        self.assertEqual(fmt(-0.0), "0.000000")
        self.assertEqual(fmt(1), "1.000000")
        self.assertEqual(fmt(1.23456789), "1.234568")

    def test_face_count_vertex_count_lines_present(self):
        m = Mesh()
        for i in range(4):
            m.add_vertex((i, 0, 0), (0, 0))
        m.add_face((0, 1, 2, 3))
        text = render_x("Quad", [m])
        self.assertIn("4;\n", text)  # vertex count
        self.assertIn("1;\n", text)  # face count


class CheckXGrammarTests(unittest.TestCase):
    """Run every generated model through the engine-faithful parser."""

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="modelgen-test-")
        cls.results = modelgen_main.run("P0", os.path.join(cls.tmpdir, "model"),
                                         provenance_path=os.path.join(cls.tmpdir, "prov.jsonl"),
                                         uv_dir=os.path.join(cls.tmpdir, "uv"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def test_every_file_parses_with_no_errors(self):
        for key, path, polys in self.results:
            meshes, errors, warns = check_x.parse(path)
            self.assertEqual(errors, [], f"{key} ({path}) failed check_x: {errors}")
            self.assertGreater(len(meshes), 0, f"{key}: no mesh parsed")

    def test_poly_budget(self):
        for key, path, polys in self.results:
            self.assertLessEqual(polys, MAX_POLYS, f"{key}: {polys} polys exceeds F-4 budget {MAX_POLYS}")
            self.assertGreater(polys, 0, f"{key}: empty model")

    def test_uvs_in_unit_range(self):
        for key, path, polys in self.results:
            meshes, errors, warns = check_x.parse(path)
            for mesh in meshes:
                self.assertIsNotNone(mesh["uvs"], f"{key}: missing MeshTextureCoords")
                for u, v in mesh["uvs"]:
                    self.assertGreaterEqual(u, -1e-9, key)
                    self.assertLessEqual(u, 1 + 1e-9, key)
                    self.assertGreaterEqual(v, -1e-9, key)
                    self.assertLessEqual(v, 1 + 1e-9, key)

    def test_uv_count_matches_vertex_count(self):
        for key, path, polys in self.results:
            meshes, errors, warns = check_x.parse(path)
            for mesh in meshes:
                self.assertEqual(len(mesh["uvs"]), len(mesh["verts"]), key)

    def test_face_sizes_are_tri_or_quad(self):
        for key, path, polys in self.results:
            meshes, errors, warns = check_x.parse(path)
            for mesh in meshes:
                for f in mesh["faces"]:
                    self.assertIn(len(f), (3, 4), key)


class DeterminismTests(unittest.TestCase):
    def test_same_spec_twice_byte_identical(self):
        for key in P0_KEYS[:6]:
            spec = SPECS[key]
            fn = generators.GENERATORS[spec.generator]
            mesh1, _ = fn(**spec.gen_args)
            mesh2, _ = fn(**spec.gen_args)
            text1 = render_x(key, [mesh1])
            text2 = render_x(key, [mesh2])
            self.assertEqual(text1, text2, key)

    def test_cli_run_twice_identical_files(self):
        d1 = tempfile.mkdtemp(prefix="modelgen-det1-")
        d2 = tempfile.mkdtemp(prefix="modelgen-det2-")
        try:
            r1 = modelgen_main.run("P0", os.path.join(d1, "model"),
                                    provenance_path=os.path.join(d1, "p.jsonl"),
                                    uv_dir=os.path.join(d1, "uv"))
            r2 = modelgen_main.run("P0", os.path.join(d2, "model"),
                                    provenance_path=os.path.join(d2, "p.jsonl"),
                                    uv_dir=os.path.join(d2, "uv"))
            self.assertEqual(len(r1), len(r2))
            for (k1, p1, _), (k2, p2, _) in zip(sorted(r1), sorted(r2)):
                self.assertEqual(k1, k2)
                with open(p1, "rb") as a, open(p2, "rb") as b:
                    self.assertEqual(a.read(), b.read(), k1)
        finally:
            shutil.rmtree(d1, ignore_errors=True)
            shutil.rmtree(d2, ignore_errors=True)


class CoverageTests(unittest.TestCase):
    def test_every_manifest_p0_model_slot_has_a_spec(self):
        import json
        manifest_path = os.path.join(REPO_ROOT, "assets", "manifest.json")
        with open(manifest_path) as fh:
            manifest = json.load(fh)
        p0_model_slots = {
            s["path"] for s in manifest["slots"]
            if s["kind"] == "model" and s.get("priority") == "P0"
        }
        covered = set()
        for key in P0_KEYS:
            covered.add("data/" + SPECS[key].slot)
        # The addon uses its own generator; format and determinism coverage
        # lives in test_ufo_assets rather than the classic modelgen registry.
        from tools.ufogen.weapons import UFO_WEAPON_SPECS
        covered.update("data/model/weapon/" + key + ".x" for key in UFO_WEAPON_SPECS)
        covered.update({"data/model/cow.x", "data/model/laser.x",
                        "data/model/tube.x", "data/model/weapon/rock.x"})  # tools/escapegen
        missing = p0_model_slots - covered
        self.assertEqual(missing, set(), f"P0 manifest model slots with no modelgen spec: {sorted(missing)}")


class MorphCompatibilityTests(unittest.TestCase):
    """ASSET-CONTRACT 4.3: morph partners need identical vertex/face
    topology; only positions may differ."""

    def _check_group(self, keys):
        tmpdir = tempfile.mkdtemp(prefix="modelgen-morph-")
        try:
            paths = []
            for key in keys:
                spec = SPECS[key]
                fn = generators.GENERATORS[spec.generator]
                mesh, _ = fn(**spec.gen_args)
                path = os.path.join(tmpdir, f"{key}.x")
                write_x(path, key, [mesh])
                paths.append(path)
            base_meshes, errs, _ = check_x.parse(paths[0])
            base_faces = [f for m in base_meshes for f in m["faces"]]
            base_nv = sum(len(m["verts"]) for m in base_meshes)
            for p in paths[1:]:
                meshes, errs, _ = check_x.parse(p)
                faces = [f for m in meshes for f in m["faces"]]
                nv = sum(len(m["verts"]) for m in meshes)
                self.assertEqual(nv, base_nv, p)
                self.assertEqual(faces, base_faces, p)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

    def test_walk_cycle_topology_matches(self):
        self._check_group(["walk01", "walk02", "walk03", "walk04"])

    def test_run_cycle_topology_matches(self):
        self._check_group(["run01", "run02", "run03", "run04", "run05", "run06"])


class PrimitiveWindingTests(unittest.TestCase):
    """Per-primitive winding must satisfy the engine's 'outward' rule
    (docs/ASSET-CONTRACT.md 4.2); check_x.py --closed only validates this
    correctly for a single convex solid, so test primitives in isolation."""

    def test_box_is_closed_and_outward(self):
        uv = {k: (0, 0, 1, 1) for k in ("front", "back", "left", "right", "top", "bottom")}
        m = primitives.box((0, 0, 0), (1, 2, 1), uv)
        bad = check_x.check_winding({"verts": m.verts, "faces": m.faces})
        self.assertEqual(bad, [])

    def test_tapered_box_with_z_axis_is_a_solid(self):
        uv = {k: (0, 0, 1, 1) for k in ("front", "back", "left", "right", "top", "bottom")}
        m = primitives.tapered_box((0, 0, 0), (1, 1), (0, 0, -1), (0.8, 0.8), uv)
        self.assertGreater(max(v[2] for v in m.verts) - min(v[2] for v in m.verts), 0.9)
        self.assertEqual(check_x.check_winding({"verts": m.verts, "faces": m.faces}), [])
        self.assertGreater(abs(sum(
            m.verts[f[0]][0] * (m.verts[f[1]][1] * m.verts[f[2]][2] - m.verts[f[1]][2] * m.verts[f[2]][1])
            + m.verts[f[1]][0] * (m.verts[f[2]][1] * m.verts[f[3]][2] - m.verts[f[2]][2] * m.verts[f[3]][1])
            + m.verts[f[2]][0] * (m.verts[f[3]][1] * m.verts[f[0]][2] - m.verts[f[3]][2] * m.verts[f[0]][1])
            + m.verts[f[3]][0] * (m.verts[f[0]][1] * m.verts[f[1]][2] - m.verts[f[0]][2] * m.verts[f[1]][1])
            for f in m.faces
        )), 0)

    def test_cylinder_is_closed_and_outward(self):
        m = primitives.cylinder((0, 0, 0), radius=1.0, height=2.0, segments=8,
                                 side_uv=(0, 0, 1, 1))
        bad = check_x.check_winding({"verts": m.verts, "faces": m.faces})
        self.assertEqual(bad, [])

    def test_flip_mesh_inverts_sky_box_for_inside_view(self):
        uv = {k: (0, 0, 1, 1) for k in ("front", "back", "left", "right", "top")}
        m = primitives.box((0, 0, 0), (10, 10, 10), uv)
        bad_outward = check_x.check_winding({"verts": m.verts, "faces": m.faces})
        self.assertEqual(bad_outward, [])
        primitives.flip_mesh(m)
        bad_after_flip = check_x.check_winding({"verts": m.verts, "faces": m.faces})
        self.assertEqual(len(bad_after_flip), len(m.faces))


class UVAtlasTests(unittest.TestCase):
    def test_pack_rects_within_bounds_and_nonoverlapping_rows(self):
        a = Atlas(128)
        a.pack([("a", 32, 16), ("b", 32, 16), ("c", 96, 16)])
        for r in a.rects.values():
            self.assertLessEqual(r.x1, 128)
            self.assertLessEqual(r.y1, 128)

    def test_pack_overflow_raises(self):
        a = Atlas(32)
        with self.assertRaises(ValueError):
            a.pack([("x", 64, 16)])

    def test_render_uv_template_writes_a_file(self):
        a = Atlas(64)
        a.pack([("x", 32, 32)])
        tmpdir = tempfile.mkdtemp(prefix="modelgen-uv-")
        try:
            out = os.path.join(tmpdir, "t.png")
            render_uv_template(a, out)
            self.assertTrue(os.path.isfile(out))
            self.assertGreater(os.path.getsize(out), 0)
            with open(out, "rb") as fh:
                self.assertEqual(fh.read(8), b"\x89PNG\r\n\x1a\n")
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
