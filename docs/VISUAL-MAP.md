# Free-map visual corrections

The original free demo was authored with a 36-unit-wide ground plane and walls
5 units high while engine humans are 20 units tall. Its geometry and points are
now scaled by eight, retaining the original layout. `data/map10/` supplies the
opening; its authored binaries are also copied to `data/menu/` for the menu.
The menu camera remains the original player-relative offset; no engine camera
or classic-data code is changed.

The starter briefing now selects sky 1. `ResourceManager::LoadSkyModelTexture`
accepts 1..5 and previously rejected its sky 0, leaving a black background.
Parent browser inspection confirmed the generated clouds now render. Visible
skybox face seams remain a separate quality issue, not declared solved here.

## Texture density

Box entries in mapgen JSON can set `texture_world_units: 32`: each texture tile
covers 32 world units in each face direction. Top/bottom map X/Z extent;
front/back map X/Y; left/right map Z/Y. This takes precedence over `uv_scale`
for that block. Values must be positive and finite. Without the property,
legacy `uv_scale` behavior is unchanged. Starter and demo tileable surfaces use
this density rather than stretching one tile over the entire courtyard. Framed
crate faces keep one full image per face rather than cropping their borders.

`tools/tests/test_visual_map.py` covers per-face density, invalid inputs,
source/binary synchronization, demo dimensions and an existing valid sky.
Map-loader tests and production WASM build pass. Parent viewed the menu and
starter in an isolated local browser with no archive. Character appearance is
NOT validated by this branch: skin and mesh corrections are separate streams.
Do not deploy this component alone as a fix for all reported geometry defects.

## Sky

`data/sky/sky1.bmp`..`sky5.bmp` are generated procedurally by
`tools/artgen/skybox.py`, not painted or AI-image-generated: for every texel
of the 6-face, 64px-per-face atlas it computes the ray direction from the box
centre through that point on `data/sky/sky.x` and colours it from a smooth
function of that direction (vertical gradient + soft fbm cloud noise). Two
texels on the same physical cube edge always share the same direction, so
they always get the same colour — the atlas is seam-free by construction,
not by post-hoc touch-up. Each sky's horizon colour (`fog`, the colour at
`elev == 0`) matches the engine's fog colour for that sky number, so the
skybox blends into distance fog instead of showing a horizon line.
`tools/tests/test_sky_atlas.py` enforces both properties (shared-vertex
colour match, texel-centre UVs) for every `sky<N>.bmp` present (1-5).

Regenerate all five with:

```sh
python3 -m tools.artgen.skybox --numbers 1,2,3,4,5
```

Pass `--out <dir>` to write elsewhere, or a shorter `--numbers` list to
refresh only some.
