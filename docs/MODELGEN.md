# Modelgen: procedural low-poly `.x` model generator

`tools/modelgen` emits the OpenXOPS `xof 0302txt` model subset described in
`docs/ASSET-CONTRACT.md` section 4, for every P0 model slot in
`assets/manifest.json`. It is a clean-room generator: everything here comes
from the BSD engine source (`src/d3dgraphics-opengl.cpp` loader,
`src/object.cpp` / `src/resource.cpp` transforms, `src/parameter.cpp`
weapon/human tables) plus `docs/ASSET-CONTRACT.md`. No original model was
opened while writing it.

## Why this shape

- **Grammar.** `xwriter.py` writes exactly the subset
  `tools/contract/check_x.py` emulates from the GL loader
  (`d3dgraphics-opengl.cpp:468-721`): one vertex/face/UV per line, a column-0
  `Mesh`, a single blank separator line between vertices and the face
  count, a one-space-indented ` MeshTextureCoords`, triangle/quad faces
  only, and a trivial single-material `MeshMaterialList` (contract item 9,
  keeps the D3D per-material draw path safe).
- **Winding.** `primitives.py` builds every quad/triangle so the ordinary
  cross product `(v1-v0) x (v2-v0)` points away from the shape's own
  centre, per ASSET-CONTRACT 4.2. `fix_winding` does this automatically;
  `flip_mesh` inverts it for shapes drawn from the inside (the sky box, §7).
- **Axes/scale/origin.** Humans are centred on `x=z=0`, face `-Z`
  (ASSET-CONTRACT 4.2/5.2), and are built against the file-space limits
  derived in ASSET-CONTRACT 5.3 from the bullet collision cylinders
  (`objectmanager.cpp:755-787`) and the render-time scale/translate in
  `object.cpp:2154-2205`. Weapon origin sits at the grip
  (ASSET-CONTRACT 6.1), barrel points `-Z`. The bullet's nose points `-Z`
  (`parameter.cpp:1434-1436`, `object.cpp:2897`). The sky box is visible
  from the inside and kept within ~50 file units (ASSET-CONTRACT 7).
  `specs.py` carries the exact `file:line` citation for every one of
  these numbers.
- **Morph compatibility.** `walk01-04` and `run01-06` are built from the
  *same* code path (`human_legs`), varying only a rotation angle, so their
  vertex/face lists are identical in count and order across the group —
  required by `MorphingModel` (ASSET-CONTRACT 4.3). Checked in
  `tools/tests/test_modelgen.py::MorphCompatibilityTests`.
- **Shared UV layout.** All human parts (`up0-5`, `arm00-02`, `leg`,
  `walk*`, `run*`) draw from one 256x256 atlas (`generators.HUMAN_ATLAS`),
  per ASSET-CONTRACT 5.3's recommendation that any skin should fit any
  part.
- **Tapered cross-sections.** `tapered_box` builds its cross-section
  perpendicular to whichever axis it tapers along (not always Y), so a
  limb segment tapers cleanly along its own length regardless of which
  way it is oriented in the body.
- **Arm pose.** `human_arms` builds each arm as a shoulder -> elbow ->
  wrist chain that reaches to the weapon grip point from `specs.py`
  (ASSET-CONTRACT 6.1), rather than a single straight segment, so the
  held weapon's grip lines up with the hand across every `up*`/`weapon`
  pair.
- **Neck and feet.** `human_upper` inserts a neck segment between torso
  and head instead of the head floating directly off the torso top, and
  `human_legs` keeps the foot base flush with `y=0` (the ground plane,
  ASSET-CONTRACT 5.3) at every walk/run phase instead of letting it dip
  or float.
- **Weapon length.** Each weapon's `.x` length is derived from its
  `specs.py` grip-to-muzzle vector (ASSET-CONTRACT 6.1) rather than a
  fixed box size, so the generated mesh ends exactly at its own muzzle
  anchor.
- **Sky box UVs.** `sky()`'s six faces each get texel-centre-inset UVs
  (offset by half a texel inward, see `tools/artgen/skybox.py`'s header
  comment) rather than UVs that touch `0`/`1` exactly, so a face never
  samples a neighbouring face's texels across the atlas seam.

## Layout

```
tools/modelgen/
  xwriter.py      Mesh dataclass + text .x writer (deterministic floats)
  primitives.py   box, tapered_box, cylinder, merge, flip_mesh, rotate_x_about
  uv_atlas.py     Atlas (deterministic shelf packer) + PNG wireframe export
  specs.py        ModelSpec table: every P0 slot, its generator + args, source refs
  generators.py   one function per model family (human up/arm/leg, weapon, bullet, sky)
  __main__.py     CLI
```

## CLI

```
python3 -m tools.modelgen --priority P0 -o assets/pack/data/model
```

- `-o` should point at `assets/pack/data/model`; the sky slot
  (`sky/sky.x`) is resolved against its parent `assets/pack/data`
  automatically.
- `--priority P0|P1|ALL`, or `--only KEY [KEY ...]` to regenerate specific
  slots (see `specs.SPECS` keys, e.g. `up0`, `arm01`, `walk03`, `de`).
- Writes one `.x` file per slot, one UV wireframe PNG per slot under
  `assets/work/uv/<key>.png` (own diagram only, for an image-gen pass to
  paint a skin against), and appends one line per file to
  `assets/provenance.jsonl` (license `CC0-1.0`, `generator: tools/modelgen`).

## Validating output

```
python3 tools/contract/check_x.py assets/pack/data/model/up0.x
python3 tools/contract/check_x.py assets/pack/data/model/walk01.x --morph assets/pack/data/model/walk02.x
python3 tools/validate_pack.py assets/pack
```

`check_x.py --closed` only validates winding correctly for a single convex
solid (it compares against one whole-mesh centroid — its own docstring
says as much). Composite models here are unions of several convex
primitives, so run `--closed` against a single primitive in isolation
(see `PrimitiveWindingTests` in the test file) rather than a composite
`.x` file.

## Tests

`tools/tests/test_modelgen.py` covers:

- exact header/grammar shape (`xof 0302txt`, one-space `MeshTextureCoords` indent)
- every generated P0 file parses with zero errors through
  `tools/contract/check_x.py` (the same line-by-line emulation of the GL
  loader used elsewhere in this repo — this *is* the "engine's actual
  parsing code" in Python form; see Limitation below)
- poly budget (<=139, FREE-DATA-PACK F-4) and non-empty mesh
- UV coordinates in `[0,1]` and UV count == vertex count
- byte-for-byte determinism, both for a single spec and for two full CLI runs
- every P0 model slot in `assets/manifest.json` has a modelgen spec
  (`CoverageTests`)
- walk/run morph groups share identical topology (`MorphCompatibilityTests`)
- per-primitive winding is outward (box, cylinder) and `flip_mesh` inverts it
- UV atlas packer stays in bounds and raises on overflow

Run: `python3 -m unittest discover -s tools/tests`.

### Limitation: no C++ load harness

The task asked for a `g++` harness that runs the engine's actual `.x`
parsing code when cheap. `LoadModel` in `src/d3dgraphics-opengl.cpp` is not
factored into a standalone function — it is inlined in a ~250-line method
tangled with the rest of `D3DGraphics` (texture/material state, the
`MAX_MODEL` table, etc.), and this task's ownership boundary excludes
editing engine source to extract it. Building a harness would mean either
copying the parsing logic out of the engine file (duplicating the same
risk a hand-written Python port has) or linking the whole
`D3DGraphics`/GL stack just to call one method, well past "cheap".

Instead this project already has `tools/contract/check_x.py`, a
line-accurate emulation of the exact same function (same line numbers
cited throughout), written and reviewed as the project's engine-fidelity
reference for `.x` files. `test_modelgen.py` runs every generated file
through it. If the engine source is later refactored to expose `LoadModel`
as a free function, a real `g++`/CDT harness can link against it directly
and replace this limitation note.

## Known simplifications (not contract violations)

- Hand/finger detail, grip-specific arm poses and per-weapon part counts
  are simplified to boxes/cylinders — "simple but recognisable
  silhouettes", as scoped, not visual fidelity to the original models.
- Weapon stock/magazine presence and lengths are categorised by weapon
  `kind` (pistol/smg/rifle/grenade) rather than modelled per weapon ID;
  muzzle/grip placement still follows the per-weapon numbers in `specs.py`.
