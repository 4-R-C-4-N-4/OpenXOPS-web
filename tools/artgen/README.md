# tools/artgen — Codex image-gen art pipeline

Turns a slot spec + `assets/STYLE.md` + `assets/prompts/<class>.md` into a
game-ready, provenance-logged texture. Spec: docs/FREE-DATA-PACK.md §5.
Python 3 stdlib + Pillow; needs the Codex CLI (`image_generation` feature,
ChatGPT login) and must run inside this git repo.

```sh
A=tools/artgen/artgen.py
python3 $A gen assets/batches/w1a_tiles.json --n 1   # generate + post-process (resumable)
python3 $A gen grass --n 2                           # add candidate #2 for one slot (retry)
python3 $A gen <batch|slot> --dry-run                # print prompts/commands only
python3 $A gen <batch|slot> --force-post             # re-run post-processing, no Codex
python3 $A sheet grass                               # assets/work/.../grass/sheet.png
python3 $A pick grass 1                              # -> assets/pack/<path> + provenance.jsonl
python3 $A status [batch]                            # candidates, seam metrics, pick state
python3 $A preview assets/batches/w1a_tiles.json docs/img/w1a_tiles_preview.png
python3 $A verify assets/pack/data/tiles_w1/*.bmp    # uncompressed 8-bit BMP check
```

## Slots

A slot is a pack path (`data/tiles_w1/grass.bmp`) or a unique bare name
(`grass`). Specs come from `assets/batches/*.json` (`defaults` + `slots`) and,
if present, `assets/manifest.json` (§4 field names: `path kind class ref_size
ref_format alpha tileable summary`). Batch entries override manifest ones.

## What `gen` does per candidate i

1. Prompt = STYLE.md preamble + class template with `{summary}` filled in.
   No reference images, no game names (R-1).
2. One `codex exec --sandbox workspace-write --json` call at a time (R-2),
   stdin from /dev/null, `timeout $CODEX_TIMEOUT_S`. Codex is told to copy the
   PNG to `assets/work/<path-without-ext>/raw_<i>.png`. If it doesn't, the
   PNG is recovered from the event log / last message, or the newest file in
   `~/.codex/generated_images` created after the call started. Raw images
   whose sha256 matches an earlier candidate are rejected.
   Every call is logged to `assets/work/artgen.log` (duration, exit code).
3. Lanczos resize to `ref_size`.
4. Tileables: seam check (below). If flagged, auto-heal: blend the image with
   its half-offset copy through a smoothstep border mask (borders come from
   the offset copy, which is continuous across the wrap; centre from the
   original), then re-measure.
5. Quantise to 256 colours (median cut palette, Floyd–Steinberg dither) and
   write an **uncompressed** bottom-up 8-bit BMP with a full 256-entry palette
   (own writer, not ImageMagick, so no RLE8 risk — F-6). Verified by header:
   `bpp==8`, `biCompression==0` (offset 30), size == 66,614 bytes at 256².
6. Writes `<i>.bmp`, `<i>_256.png`, `<i>_tiled.png` (2×2), `<i>.json` (meta,
   prompt, seam metrics) and `sheet.png` (all candidates, 2×2 each).

Resumable: an existing `raw_<i>.png` is never regenerated; post-processing
re-runs only if its output is missing (or `--force-post`).

## Seam metric

On the 256² image: `edge_h` = mean abs RGB diff between column 0 and column
W-1 (neighbours after wrapping), `edge_v` the same for rows; `interior_h/v` =
mean abs diff between all adjacent interior columns/rows. **`excess` =
max(edge_h − interior_h, edge_v − interior_v)**, 0–255 scale; `ratio` is the
same as a quotient. A candidate is flagged when `excess > SEAM_MAX`.

Tuning (`SEAM_MAX`, default 12.0), from Wave 1a (13 real Codex candidates):

| tile | excess (raw → final) | note |
|---|---|---|
| ceramic_floor_tile, metal_grate, dirt | −1.2 … −0.1 | seamless |
| white_plaster, wood_floor, grass, asphalt | 0.8 … 2.2 | seamless |
| concrete_wall, office_carpet, red_brick #2 | 4.2 … 4.9 | seamless by eye |
| steel_plate | 8.2 | panel border sits on the edge, by design |
| red_brick #1 | 18.0 → 4.8 (healed) | courses misaligned at wrap; visible seam |
| wooden_crate | 36.7 (`seam_heal:false`) | framed crate face, repeats as stacked crates |

Visually seamless tiles stay below ~8; a real seam starts around 18. 12 sits
in the gap. Healing works on stochastic and brick-like textures (faint
ghosting of joints near the border) but destroys framed designs. For those,
set `"seam_heal": false` on the slot: the flag is still recorded. Prefer
regenerating structured tiles (bricks, tiles) over healing them.

## Pick / provenance

`pick <slot> <i>` re-verifies the BMP, copies it to `assets/pack/<path>`, and
writes one line per path to `assets/provenance.jsonl` (a re-pick replaces the
line): `path sha256 generator prompt license author date curated_by(null)
seam_metric candidate source_png_sha256`. If `assets/manifest.json` has the
slot, its status becomes `curated`. `curated_by` is set by a human later.

`assets/work/` is git-ignored (R-4); only picks + provenance are committed.

Config via env: `ARTGEN_CANDIDATES SEAM_MAX QUANT_COLORS CODEX_TIMEOUT_S
PACK_ROOT WORK_ROOT PROVENANCE`.

Not yet implemented: `ref_format` other than `bmp8`, `alpha` other than
`none` (DDS writer, black-key) — needed for Wave 1 effects/HUD.
