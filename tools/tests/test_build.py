"""Determinism and coverage tests for tools/manifest/build.py."""
import importlib.util
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "manifest_build", os.path.normpath(os.path.join(HERE, "..", "manifest", "build.py")))
build = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(build)


class TestBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc, _ = build.build()

    def test_deterministic(self):
        doc2, _ = build.build()
        self.assertEqual(build.render(self.doc), build.render(doc2))

    def test_hard_literal_count(self):
        st = self.doc["stats"]
        self.assertEqual(st["hard_literals_distinct"], 152)
        self.assertEqual(st["hard_literals_by_category"]["model"], 92)
        self.assertEqual(st["hard_literals_by_category"]["article"], 17)
        self.assertEqual(st["hard_literals_by_category"]["sound"], 14)

    def test_standard_missions(self):
        txt = [s for s in self.doc["slots"] if s["kind"] == "mission" and s["path"].endswith(".txt")]
        self.assertEqual(len(txt), 58)

    def test_schema_values(self):
        kinds = {"texture", "model", "sound", "map", "points", "mission", "dir"}
        classes = {"hud", "effect", "skin", "weapon_skin", "article_skin", "sky", "map_tile",
                   "briefing", "model", "sfx", "other"}
        for s in self.doc["slots"]:
            self.assertIn(s["kind"], kinds, s["path"])
            self.assertIn(s["class"], classes, s["path"])
            self.assertIn(s["priority"], {"P0", "P1", "P2"}, s["path"])
            self.assertIn(s["status"], {"missing", "generated", "curated", "final"}, s["path"])
            self.assertTrue(s["source_refs"], s["path"])
            self.assertEqual(s["path"], s["path"].replace("\\", "/"))


if __name__ == "__main__":
    unittest.main()
