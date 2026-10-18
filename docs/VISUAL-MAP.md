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
