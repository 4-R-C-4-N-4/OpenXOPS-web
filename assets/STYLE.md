# Free Data Pack — Style Bible

Every art prompt starts with the **Preamble** below (artgen.py copies everything
between the `<!-- preamble -->` markers verbatim). The rest of this file is the
rationale for humans curating candidates.

<!-- preamble:begin -->
Art direction: an original texture for a small, low-poly, early-2000s freeware
PC first-person shooter. The look is a modest, slightly painterly, photo-sourced
game texture of that era: low resolution in feel, soft detail, mild noise and
grain, no ultra-sharp micro-detail, no modern PBR or hyper-realistic rendering.
Palette: muted and slightly desaturated, earthy and industrial greys, browns,
olives, dull reds and blues; no neon, no saturated rainbow colours.
Lighting: flat, even and neutral, as if shot under an overcast sky; no strong
baked shadows, no highlights or specular glare, no directional light gradient,
no vignette, no ambient-occlusion darkening at the borders.
Content rules: absolutely no text, letters, numbers, signage, logos, symbols,
watermarks, signatures, brand names or trademarks of any kind. No people, no
faces. The design must be wholly original and must not imitate any existing
game, film or product.
<!-- preamble:end -->

## Rationale

- **Era fit.** Textures are shown at 256×256 or smaller, quantised to 256
  colours and magnified by bilinear filtering on low-poly geometry. Fine detail
  turns to noise. Medium-scale shapes (bricks, planks, panels) read well.
- **Flat lighting.** The engine applies its own lighting and textures tile
  across large walls and floors. Baked shadows and gradients repeat visibly
  and clash with the scene's light.
- **Muted palette.** It survives 8-bit quantisation with little banding and
  keeps maps cohesive when different tiles sit side by side.
- **Clean-room.** Prompts never name or describe any existing game, and no
  reference images are passed to the generator (FREE-DATA-PACK.md R-1).

## Curation checklist

1. No text, glyph-like marks, logos or watermark-like smudges.
2. Tiles 2×2 without a visible seam, grid or repeating hot-spot.
3. No perspective, no vignette, no light falloff.
4. Reads as the intended material at 256×256 and at 64×64.
5. Colours sit within the muted palette next to the other tiles of the wave.
