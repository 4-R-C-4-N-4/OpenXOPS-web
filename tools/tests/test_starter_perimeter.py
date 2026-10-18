"""The starter compound must be fully enclosed: the floor ends at the
perimeter, so any gap in the perimeter wall lets the player walk off the
edge and fall to their death (found in browser QA: backing out of spawn
through the open south gate ended the mission in ~3 s)."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "tools/mapgen/examples/starter_compound.json"


def _covered(intervals, lo, hi):
    pos = lo
    for a, b in sorted(intervals):
        if a > pos + 1e-6:
            return False
        pos = max(pos, b)
    return pos >= hi - 1e-6


class StarterPerimeterTests(unittest.TestCase):
    def setUp(self):
        self.blocks = json.loads(SRC.read_text())["blocks"]
        floor = self.blocks[0]
        self.x0, _, self.z0 = floor["min"]
        self.x1, _, self.z1 = floor["max"]

    def _walls(self, axis, plane, outward):
        """Blocks at least player-height tall touching the given floor edge."""
        out = []
        for b in self.blocks:
            lo, hi = b["min"], b["max"]
            if hi[1] - max(lo[1], 0) < 24:
                continue
            face = lo[axis] if outward > 0 else hi[axis]
            if abs(face - plane) < 1e-6 or (lo[axis] <= plane <= hi[axis]):
                other = 0 if axis == 2 else 2
                out.append((lo[other], hi[other]))
        return out

    def test_every_floor_edge_is_walled(self):
        for name, axis, plane, outward, lo, hi in [
            ("south", 2, self.z1, 1, self.x0, self.x1),
            ("north", 2, self.z0, -1, self.x0, self.x1),
            ("east", 0, self.x1, 1, self.z0, self.z1),
            ("west", 0, self.x0, -1, self.z0, self.z1),
        ]:
            with self.subTest(edge=name):
                self.assertTrue(_covered(self._walls(axis, plane, outward), lo, hi),
                                f"{name} perimeter has a gap")


if __name__ == "__main__":
    unittest.main()
