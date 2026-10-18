# Human skin atlas generation

`python -m tools.artgen.human_skins` builds the two active free-pack character skins from the retained original Codex image-generation source `assets/sources/human-olive-fabric.png`. The source is a 1254×1254 RGB olive woven-fabric material swatch, generated with Codex CLI image_generation, prompt retained below, SHA-256 `54cb5c2091f23a416a7f238bb8be9805dcb496cde18f122bf5532954d0387ab9`.

The generator reads the frozen `HUMAN_ATLAS.rects` from `tools/modelgen/generators.py`; it crops/resizes the source separately to each named rectangle and applies deterministic fabric tinting plus face details on `head_front`. No atlas geometry or UV rectangles are changed. It emits opaque, uncompressed, 256×256 8-bit BMPs: `soldier_green.bmp` (olive uniform shared by starter player and enemies of Human type 2) and `soldier_black.bmp` (dark uniform used by map10_demo Human types 0 and 1). Human type 2 shares its texture between player and enemies by engine parameter mapping; they are not team-colored separately.

Generation prompt: “Create ONE original hand-painted material swatch image, no game assets or references: a richly textured olive drab woven military cotton fabric with subtle irregular fibers, soft muted variation, flat evenly lit diffuse material study, no objects, no clothing, no seams, no text, no symbols, no faces.” License declaration: CC0-1.0, generated original material; source PNG is retained for reproducibility.

Run tests with `python -m unittest tools.tests.test_human_skins` and regenerate from source with `python -m tools.artgen.human_skins`. This addresses missing/fallback textures only; the known flat-limb geometry issue is independent and not hidden or corrected by the skins.
