# Asset Contract: what a replacement asset must satisfy

**Project**: OpenXOPS-web
**Document type**: Technical contract (derived from engine source)
**Status**: Draft
**Version**: 0.1
**Date**: 2026-10-17
**Depends on**: docs/FREE-DATA-PACK.md, docs/SCOPE-OF-WORK.md; upstream source at tag `upstream-1.080`

---

## 0. Scope and conventions

This document states what a Free Data Pack file must look like for the
OpenXOPS 1.080 engine to load and draw it, or play it, the same way it handles
the original data. It is derived only from the BSD engine source in `src/`. No
original asset was opened while writing it.

- **Citations.** `file:line` refers to `src/` after `iconv -f CP932 -t UTF-8 | tr -d '\r'`. Line numbers match the
  CRLF originals. Where only one backend is named, it is the OpenGL backend
  (`d3dgraphics-opengl.cpp`, `GRAPHIC_ENGINE == 8`, the port target). The
  Direct3D backend (`d3dgraphics-directx.cpp`) is cited where its behaviour differs.
- **"Derived"** marks a number calculated from source constants (for
  example, inverting a transform). It is not written in the source as such. The
  calculation is shown so it can be checked.
- **"Not determined by source"** means the engine does not constrain the
  value. Each such item is also listed in §10.
- **Coordinates.** "Engine space" is the Direct3D left-handed world: +X right,
  +Y up, +Z forward (`D3DXMatrixLookAtLH`, d3dgraphics-directx.cpp:1354-1357).
  Model files are read in the same space. The GL backend negates X when it loads
  vertices (d3dgraphics-opengl.cpp:535) and when it sets up the camera and
  transforms (d3dgraphics-opengl.cpp:1832, 1861). Model files therefore
  stay in D3D/left-handed form for both backends.
- The texture-space origin is the **top-left**, with v increasing downward (§1.5).

Machine-readable companions in `tools/contract/`:

| File | What it is |
|---|---|
| `char_layout.json` | Glyph grid and every HUD cell of `char.dds` (§2) |
| `render_char_template.py` | Draws a labelled 512×512 layout diagram from the JSON (our own diagram, no game art) |
| `check_x.py` | Emulates the engine's `.x` parser line by line and checks the §4 rules |
| `examples/cube.x` | A minimal textured cube that the parser accepts (§4.4) |

---

## 1. Texture loading

### 1.1 Format detection

`LoadTexture` (d3dgraphics-opengl.cpp:918-1010) first picks a format from the
extension (`.bmp`/`.dds`/`.jpg|.jpeg|.jpe`/`.png`,
d3dgraphics-opengl.cpp:841-876). It then **overrides that choice with the file's
magic bytes** when they match `BM`, `DDS`, `FF D8` or `89 'PNG'`
(d3dgraphics-opengl.cpp:882-911, 943-946). The content therefore decides the
format, not the name. For example, a DDS saved as `sky1.bmp` would be decoded as
DDS.

| Format | Decoder | Available in default build? |
|---|---|---|
| BMP | `LoadBMPTexture` d3dgraphics-opengl.cpp:1048-1220 | yes |
| DDS | `LoadDDSTexture` d3dgraphics-opengl.cpp:1228-1306 | yes |
| JPEG | `LoadJPEGTexture` d3dgraphics-opengl.cpp:1314-1393 | **no**: `ENABLE_OPENGL_LIBJPEGPNG` is commented out (d3dgraphics.h:85), so the stub returns false (d3dgraphics-opengl.cpp:1391-1393) |
| PNG | `LoadPNGTexture` d3dgraphics-opengl.cpp:1402-1551 | **no** (same switch) |

**Contract: ship only BMP and DDS.** PNG/JPEG work only in a non-default build.

Every decoder produces RGBA8. The result is uploaded **at its native size** with
`glTexImage2D(GL_RGBA, w, h, ...)` (d3dgraphics-opengl.cpp:988). Filtering is
`GL_LINEAR` for both magnification and minification, with **no mipmaps**
(d3dgraphics-opengl.cpp:991-992). The texture environment is `GL_MODULATE`
(d3dgraphics-opengl.cpp:995): every texel is multiplied by the vertex colour. No
`GL_TEXTURE_WRAP_*` call exists anywhere in the GL backend, so the GL default
`GL_REPEAT` applies (no call between d3dgraphics-opengl.cpp:982-998). The D3D
backend sets no address mode either, and the D3D default is WRAP.

### 1.2 BMP (`LoadBMPTexture`)

| Rule | Source |
|---|---|
| The engine reads a 54-byte header and checks `BM`. | d3dgraphics-opengl.cpp:1061-1066 |
| Pixel or palette data is read from `14 + biSize`. **`bfOffBits` is ignored**, so the palette or pixels must follow the info header directly. Use `BITMAPINFOHEADER` (biSize 40) with no gap. | d3dgraphics-opengl.cpp:1069, 1075 |
| `biWidth` and `biHeight` are read as-is. **The height must be positive** (bottom-up). Rows are written from `h = height-1` down to 0. | d3dgraphics-opengl.cpp:1070-1071, 1087, 1130, 1160, 1188 |
| `biCompression` is never read. **The file must be uncompressed (BI_RGB)**; RLE produces garbage. | d3dgraphics-opengl.cpp:1068-1072 (not read) |
| Supported bit counts: **4, 8, 24, 32**. 1-bit and 16-bit BMPs are not rejected: the buffer stays uninitialised but the function returns true, giving a garbage texture. | d3dgraphics-opengl.cpp:1080, 1125, 1158, 1186, 1212-1219 |
| 8-bit: **always reads 256 palette entries** (`biClrUsed` ignored). 4-bit: always reads 16. | d3dgraphics-opengl.cpp:1128, 1085 |
| 24-bit row padding is computed as `4 - (width % 4)` instead of from `width*3`. **24-bit BMPs must have a width that is a multiple of 4**, or rows shear. | d3dgraphics-opengl.cpp:1177-1181 |
| 4-bit row padding uses `(width/2) % 4`. Use widths that are multiples of 8. | d3dgraphics-opengl.cpp:1114-1118 |
| **BMP alpha is always 255.** A 32-bit BMP's fourth byte is ignored. | d3dgraphics-opengl.cpp:1096, 1137, 1167, 1195 |

Contract: write **uncompressed 8-bit (256-entry palette) or 24-bit BMPs, with a
positive height, a 40-byte info header and no gap before the data**
(ImageMagick: `BMP3:` plus `-compress None`, as in FREE-DATA-PACK F-6). A BMP is
always fully opaque.

### 1.3 DDS (`LoadDDSTexture`)

| Rule | Source |
|---|---|
| The engine reads 128 bytes (the magic plus a 124-byte header) and checks `"DDS "`. | d3dgraphics-opengl.cpp:1242-1247 |
| Height comes from offset 0x0C, width from 0x10, `dwRGBBitCount` from 0x58, `dwFlags` from 0x08. The value it calls "Caps" comes from **0x70**, which is `dwCaps2`, not `dwCaps` at 0x6C. | d3dgraphics-opengl.cpp:1250-1254 |
| **Only `dwRGBBitCount` 16 or 32 is accepted.** DXT1-5 (FourCC, bit count 0), DX10-header and 24-bit files are rejected and the texture fails to load. | d3dgraphics-opengl.cpp:1256-1259 |
| The pixel-format masks and FourCC are **ignored**. 16-bit data is always decoded as **A4R4G4B4**: byte1 is A:R, byte0 is G:B. 32-bit data is always decoded as **A8R8G8B8**, stored B,G,R,A. | d3dgraphics-opengl.cpp:1273-1284 |
| Consequence: R5G6B5 and A1R5G5B5 files decode with wrong colours. **X8R8G8B8 files are taken to carry real alpha**, so an X byte of 0 makes the texture fully transparent (alpha test, §1.4). | d3dgraphics-opengl.cpp:1283 |
| Pixels are read top-down from byte 128, with no pitch or row padding. | d3dgraphics-opengl.cpp:1268-1271 |
| Mipmaps: if `DDSD_MIPMAPCOUNT` is set in `dwFlags` **and** bit 0x400000 is set in `dwCaps2`, the loader skips 128 bytes of *pixel data* (a bug). Standard files carry `DDSCAPS_MIPMAP` in `dwCaps`, not `dwCaps2`, so the skip does not trigger: the top level is read and the extra levels are ignored. | d3dgraphics-opengl.cpp:1261-1264 |

Contract: **uncompressed DDS, 32-bit A8R8G8B8 or 16-bit A4R4G4B4, no mipmaps,
alpha meaningful in every texel**. A4R4G4B4 matches the reference shape of
`char.dds` (FREE-DATA-PACK F-4). Effects use A8R8G8B8.

### 1.4 Transparency: alpha test, blending, and the BlackTransparent key

Global state for every frame (d3dgraphics-opengl.cpp:1772-1781):
`GL_DEPTH_TEST` on, `GL_CULL_FACE` on, `GL_BLEND` with
`(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`, and `glAlphaFunc(GL_GREATER, 0.0)` with
alpha test on. The D3D backend matches this: SRCALPHA/INVSRCALPHA, alpha test
`GREATER 1/255`, Z-write on (d3dgraphics-directx.cpp:452-471).

- A texel with **alpha 0 is discarded**. Any other alpha is blended **and still
  writes depth** for models and map blocks. Semi-transparent texels on models or
  map tiles therefore sort incorrectly. **Contract: model skins, weapon skins
  and map tiles are opaque** (BMP guarantees this). Use alpha only in
  2D/effect textures.

**BlackTransparent.** Every decoder has a flag that sets alpha to 0 where
R=G=B=0, i.e. exactly `#000000`, after decoding (BMP d3dgraphics-opengl.cpp:1103-1111,
1139-1144, 1169-1174, 1197-1202; DDS d3dgraphics-opengl.cpp:1286-1291). In D3D,
the same flag loads the texture as `D3DFMT_A1R5G5B5` with colour key
`D3DCOLOR_ARGB(255,0,0,0)` (d3dgraphics-directx.cpp:761-770).

**No call site passes `BlackTransparent = true`.** The complete list of
`LoadTexture` calls follows, with the third argument `false` in every one:

| Call site | Path | `texturefont` arg |
|---|---|---|
| d3dgraphics-opengl.cpp:247 (d3dgraphics-directx.cpp:517) | `data\char.dds` (gamemain.cpp:66) | true |
| d3dgraphics-opengl.cpp:2068 (d3dgraphics-directx.cpp:1412) | map tiles `<bd1 dir>\<name>` | false |
| gamemain.cpp:596, 632, 1621, 1668; scene.cpp:172, 182 | `data\title.dds` | false |
| gamemain.cpp:1645, 1650, 1651, 1676, 1681, 1682 | briefing pictures A/B | true |
| resource.cpp:265 | human skins (`HumanTexturePath[]`) | false |
| resource.cpp:349 | weapon skins | false |
| resource.cpp:525, 639 | article skins (built-in / add-on) | false |
| resource.cpp:676 | `bullet.bmp`, grenade skin | false |
| resource.cpp:723 | `data\scope.dds` | false |
| resource.cpp:759 | `data\sky\sky1..5.bmp` | false |
| resource.cpp:858-861 | `data\blood.dds`, `mflash.dds`, `smoke.dds`, `yakkyou.dds` | false |

**Contract: no asset relies on a black colour key.** Anything that needs
transparency (font/HUD, title, scope, effects) must carry an alpha channel,
which means DDS (§1.3). The `texturefont` flag is ignored by GL
(d3dgraphics-opengl.cpp:921). In D3D it only selects 1 mip level instead of 4
(d3dgraphics-directx.cpp:753-758).

### 1.5 UV orientation

The BMP loader stores the file's last row (the top of the image) as row 0
(d3dgraphics-opengl.cpp:1087, 1130). DDS is read top-down
(d3dgraphics-opengl.cpp:1268). GL samples row 0 at t=0. **So (u,v) = (0,0) is the
top-left of the image** in both backends, which is the usual D3D/.x convention.

### 1.6 Sizes and non-power-of-two

GL uploads the native size with no rescale (d3dgraphics-opengl.cpp:985-988).
Desktop GL 1.1 needs power-of-two sizes. WebGL 1 allows NPOT only without
mipmaps and with CLAMP, and the engine relies on REPEAT (§1.1). D3D passes the
file size explicitly to `D3DXCreateTextureFromFileEx`
(d3dgraphics-directx.cpp:762); whether D3DX rounds NPOT up is library
behaviour, not source. **Contract: all textures have power-of-two edges**
(256², 128², 512²; non-square POT is fine). No size is mandated by source except
through UV usage, so §2–§3 give the reference sizes from FREE-DATA-PACK F-4.

Limits: `MAX_TEXTURE 72`, `MAX_MODEL 72` (d3dgraphics.h:49-50), at most
10 distinct human skins per mission (`MAX_LOADHUMANTEXTURE`, resource.h:40), and
at most 10 map textures (datafile.h:36).

---

## 2. `char.dds`: texture font and HUD atlas

### 2.1 Glyph grid (exact)

`Draw2DTextureFontText` (d3dgraphics-opengl.cpp:2707-2786; D3D identical
at d3dgraphics-directx.cpp:2078-2146):

- The cell size is `1/16` of the texture in u and in v (d3dgraphics-opengl.cpp:2727-2728).
  **The grid is 16 columns × 16 rows over the whole texture.** At 512×512 each
  cell is **32×32 px**.
- Character code → cell: the code is taken as an *unsigned* byte
  (`if (w<0) w+=256`). Then `col = code % 16` and `row = code / 16`
  (d3dgraphics-opengl.cpp:2750-2753). All 256 codes are addressable. Code 0
  is never drawn (strlen).
- **The whole cell is drawn** onto a `fontwidth × fontheight` quad, with no
  inset, kerning or per-glyph width (d3dgraphics-opengl.cpp:2755-2766). The
  advance is always `fontwidth`. A space (0x20) is drawn too, so **its cell
  must be fully transparent**.
- The vertex colour modulates the texture (d3dgraphics-opengl.cpp:2742, 995).
  Text is drawn in many colours, with black/grey drop shadows offset by 1–3 px
  (for example gamemain.cpp:803-804, 3158-3159). **Glyphs must be white (or
  light grey), with the glyph shape in alpha**.
- Glyphs are stretched to many sizes and aspect ratios at the 640×480 reference:
  10×14 (gamemain.cpp:3038), 12×16, 16×20, 18×24, 20×26, 22×22, 23×24, 25×26,
  28×32, 32×34, 50×42 (gamemain.cpp:4788), 60×42 (gamemain.cpp:1698). The
  `*Scaling` variants multiply x, y, w and h by `ScreenHeight/480`
  (d3dgraphics-opengl.cpp:2800-2803). Filtering is bilinear without mipmaps,
  so **leave about 1–2 px of transparent margin inside each glyph cell** to avoid
  bleeding into neighbours. HUD frame cells (§2.2) are the exception: they must
  run to the cell edge.
- 2D state: orthographic projection in pixels, depth test off, culling off
  (d3dgraphics-opengl.cpp:2661-2674).

**Character range used.** Engine literals and `sprintf` output use printable
ASCII 0x20–0x7E (menus, "RELOADING", "fps:%.2f", weapon names
parameter.cpp:501-1305, mission names, `[-_-;](mikan)` gamemain.cpp:457).
Mission names come from user `.mif` files (datafile.cpp:885-890), so any byte can
appear there. **Required: all of 0x20–0x7E.** The HUD cells below occupy part of
0xB0–0xD2. In CP932 that range is half-width katakana (the comments call 0xB0
`'ｰ'`, gamemain.cpp:3060). A Japanese mission name would therefore print HUD
pieces, which matches the original's behaviour.

### 2.2 HUD cells sampled from `char.dds`

The engine has **no explicit UV-rect drawing of `char.dds` anywhere**.
`Draw2DTexture` always maps the full 0..1 UV range (d3dgraphics-opengl.cpp:3124-3127).
The radar, the scope reticles, the HP border and the "standard" crosshair lines
use untextured `Draw2DLine`/`Draw2DBox`/`Draw2DCycle`
(gamemain.cpp:2950-2969, 3115-3118, 3181-3193; RenderRadar gamemain.cpp:3384ff;
gui-object.cpp:212-232). Every HUD graphic that does come from `char.dds` is
drawn **as a font character**: `Draw2DTextureFontText` with code bytes
0xB0–0xD2 at 32×32 px.

**27 HUD cells** are used. The rects below are in pixels for a 512×512 texture
(`x = col*32`, `y = row*32`, end-exclusive):

| Code | Cell (col,row) | Rect px [x0,y0,x1,y1) at 512² | Role | Purpose | Draw size / colour | Call site |
|---|---|---|---|---|---|---|
| 0xB0 | (0,11) | [0, 352, 32, 384] | corner | Weapon panel: top-left corner | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3060,3068 |
| 0xB1 | (1,11) | [32, 352, 64, 384] | tile-x | Weapon panel: top edge (repeated x6) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3061-3063,3068 |
| 0xB2 | (2,11) | [64, 352, 96, 384] | corner | Weapon panel: top-right corner | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3064,3068 |
| 0xB3 | (3,11) | [96, 352, 128, 384] | corner | Status panels A and B: top-left cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3045,3047,3052,3054 |
| 0xB4 | (4,11) | [128, 352, 160, 384] | tile-x | Status panels A and B: top edge (panel A x6, panel B x2) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3045-3047,3052-3054 |
| 0xB5 | (5,11) | [160, 352, 192, 384] | corner | Status panel A: top-right cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3046-3047 |
| 0xB6 | (6,11) | [192, 352, 224, 384] | transition | Status panel B: top, transition from B4 run to B7 run | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3052,3054 |
| 0xB7 | (7,11) | [224, 352, 256, 384] | tile-x | Status panel B: top edge, second run (x3) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3052-3054 |
| 0xB8 | (8,11) | [256, 352, 288, 384] | transition | Status panel B: top, second-to-last cell | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3053-3054 |
| 0xB9 | (9,11) | [288, 352, 320, 384] | corner | Status panel B: top-right cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3053-3054 |
| 0xBA | (10,11) | [320, 352, 352, 384] | icon | Ammo readout: icon that replaces 'B' (reserve rounds, nbs-lnbs) | 23×24, rgba(1,1,1,1) | gamemain.cpp:3077-3082 |
| 0xBB | (11,11) | [352, 352, 384, 384] | icon | Ammo readout: icon that replaces 'A' (rounds in magazine, lnbs) | 23×24, rgba(1,1,1,1) | gamemain.cpp:3077-3082 |
| 0xBD | (13,11) | [416, 352, 448, 384] | icon | Crosshair, left bracket; cell centred at (W/2-ErrorRange, H/2) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3195-3196 |
| 0xBE | (14,11) | [448, 352, 480, 384] | icon | Crosshair, right bracket; cell centred at (W/2+ErrorRange, H/2) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3197-3198 |
| 0xC0 | (0,12) | [0, 384, 32, 416] | edge | Weapon panel: middle-left edge | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3069-3072 |
| 0xC1 | (1,12) | [32, 384, 64, 416] | tile-xy | Weapon panel: middle fill (repeated x6) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3069-3072 |
| 0xC2 | (2,12) | [64, 384, 96, 416] | edge | Weapon panel: middle-right edge | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3069-3072 |
| 0xC3 | (3,12) | [96, 384, 128, 416] | corner | Status panels A and B: bottom-left cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3049-3050,3056-3057 |
| 0xC4 | (4,12) | [128, 384, 160, 416] | tile-x | Status panels A and B: bottom edge (panel A x6, panel B x2) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3049-3050,3056-3057 |
| 0xC5 | (5,12) | [160, 384, 192, 416] | corner | Status panel A: bottom-right cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3049-3050 |
| 0xC6 | (6,12) | [192, 384, 224, 416] | transition | Status panel B: bottom, transition from C4 run to C7 run | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3056-3057 |
| 0xC7 | (7,12) | [224, 384, 256, 416] | tile-x | Status panel B: bottom edge, second run (x3) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3056-3057 |
| 0xC8 | (8,12) | [256, 384, 288, 416] | transition | Status panel B: bottom, second-to-last cell | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3056-3057 |
| 0xC9 | (9,12) | [288, 384, 320, 416] | corner | Status panel B: bottom-right cap | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3056-3057 |
| 0xD0 | (0,13) | [0, 416, 32, 448] | corner | Weapon panel: bottom-left corner | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3073-3074 |
| 0xD1 | (1,13) | [32, 416, 64, 448] | tile-x | Weapon panel: bottom edge (repeated x6) | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3073-3074 |
| 0xD2 | (2,13) | [64, 416, 96, 448] | corner | Weapon panel: bottom-right corner | 32×32, rgba(1,1,1,0.5) | gamemain.cpp:3073-3074 |

Unused cells in those rows: 0xBC, 0xBF, 0xCA–0xCF, 0xD3–0xDF.

**Panel assembly** (640×480 reference; `W`, `H` = screen size; these coordinates are *not* scaled):

| Panel | Cell rows | Top-left | Source |
|---|---|---|---|
| Status A (8×2) | `B3 B4 B4 B4 B4 B4 B4 B5` / `C3 C4 C4 C4 C4 C4 C4 C5` | (15, H-105) | gamemain.cpp:3045-3050 |
| Status B (9×2) | `B3 B4 B4 B6 B7 B7 B7 B8 B9` / `C3 C4 C4 C6 C7 C7 C7 C8 C9` | (15, H-55) | gamemain.cpp:3052-3057 |
| Weapon (8×3) | `B0 B1×6 B2` / `C0 C1×6 C2` / `D0 D1×6 D2` | (W-255, H-98) = `HUDA_WEAPON_POS*`, size 8×3 cells | gamemain.cpp:3059-3074, gamemain.h:45-48 |

Panel B's first row (H-55..H-23) overlaps panel A's second row (H-73..H-41) by
18 px. Both are drawn at alpha 0.5, so the overlap is darker.

Text drawn on top of the panels:

- Ammo: `"A%d B%d"` with A→0xBB and B→0xBA, 23×24 at (25, H-96) (gamemain.cpp:3077-3082).
- `STATE` plus `FINE`/`CAUTION`/`DANGER`/`DEAD`, 18×24 at y = H-45, in the HP colour (gamemain.cpp:3085-3100).
- Weapon name, 16×20 at (W-246, H-94), shrunk when longer than 14 chars (gamemain.cpp:3104-3106).

Two 3D weapon models are also drawn inside the weapon panel area (§6.3).

Tiling roles: B1/C1/D1, B4/C4 and B7/C7 repeat horizontally, and C0/C1/C2
repeat vertically when `HUDA_WEAPON_SIZEH > 3`. **Edge-continuous art is
required across these cells**: the left/right/top/bottom borders of a tile cell
must meet its neighbours seamlessly. The **visual design** of each piece (bevel,
icon shape) is not determined by source.

The crosshair cells BD and BE are drawn centred on screen x at `±ErrorRange`
via `Draw2DTextureFontTextCenter` (d3dgraphics-opengl.cpp:2814-2819). Each
cell's centre is the bracket's anchor, and the brackets move outward as accuracy
drops. They are shown only in "standard" crosshair mode (gamemain.cpp:3189-3198).

### 2.3 Template

`python3 tools/contract/render_char_template.py out.png` draws the grid:
ASCII cells are labelled with their hex code and glyph, HUD cells are coloured by
role, and unused cells are hatched. The data comes from `char_layout.json`. The
output is our own diagram, so **do not commit a rendered template that has been
painted over with original art**.

### 2.4 Generator (`tools/artgen/char_atlas.py`)

Produces `assets/pack/data/char.dds` (and `data/title.dds`) deterministically
from §2.1/§2.2 above, reading the same facts from `tools/contract/char_layout.json`
rather than re-deriving them:

- **Glyphs** (0x20–0x7E): rasterised from the public-domain Shinonome 8x16
  JIS X 0201 BDF (`shnm8x16r.bdf`, the same pinned/checksummed tarball
  `tools/fonts/gen_font_atlas.py` downloads — see `assets/fonts/README.md` /
  `SHINONOME-LICENSE.txt`), nearest-neighbour upscaled ×2 (16×32), centred in
  the 32×32 cell with a horizontal margin to satisfy the no-bleed note in
  §2.1. White RGB with the glyph shape carried in alpha; 0x20 (space) is left
  fully transparent, matching the "cell is always drawn as a quad" rule.
  Codes with no ASCII mapping (everything outside 0x20–0x7E) are left
  transparent — §2.1 marks them not required by the engine.
- **HUD cells** (0xB0–0xD2, the 27 cells in `char_layout.json.hud_cells`):
  original Pillow-drawn geometry, since §2.2/Q-2 only fix position, tiling
  role and draw alpha, not appearance. `corner`/`edge`/`tile-x`/`transition`
  roles get a light-top/left, dark-bottom/right bevel on a flat mid-grey fill
  so adjacent tiles read as one continuous beveled panel; `tile-xy` (0xC1,
  the weapon-panel centre fill) is a plain fill so it tiles on all four
  sides. The four `icon` cells get distinct shapes: 0xBA/0xBB (ammo) a
  filled vs. outlined dot, 0xBD/0xBE (crosshair) a `[`/`]` bracket.
- Output format: 512×512 A4R4G4B4 DDS via `tools/artgen/dds.py`
  (`dds_argb4`), matching FREE-DATA-PACK.md F-4 and `assets/manifest.json`'s
  `ref_format` for `data/char.dds`.
- `data/title.dds` (480×80 A8R8G8B8, FREE-DATA-PACK.md F-4 / §3 below) is a
  simple original wordmark built from the same glyph rasters, upscaled ×4,
  transparent background — not derived from any original title art (none
  exists in this pack).
- Deterministic: no RNG, no network beyond the cached/checksummed Shinonome
  tarball; re-running produces byte-identical output (`tools/tests/test_char_atlas.py::test_determinism`).
- PNG previews (checkerboard-composited, matching `tools/artgen/scope.py`'s
  convention) go to `assets/work/char_preview.png` / `title_preview.png`
  (git-ignored). Provenance is appended to `assets/provenance.jsonl`
  (`license: CC0-1.0`).

---

## 3. Other 2D and effect textures

| File | Where drawn | Size drawn | UV | Alpha needed | Source |
|---|---|---|---|---|---|
| `data\title.dds` | Main-menu logo | 480×80 at (20,25), ×H/480 | full 0..1 | yes, transparent background over the 3D menu scene | gamemain.cpp:966 |
| `data\title.dds` | Background of the **briefing and result** screens (`D2Dscene`) | full screen, alpha 0.4, then a black box at 0.75 alpha | full 0..1 | — | scene.cpp:200-201; gamemain.h:207, 305 |
| `data\scope.dds` | Scope overlay | full screen; if aspect > 1.5, a 4:3 box `(H*1.333+1)×H` centred with black bars | full 0..1 | yes, the see-through hole has alpha 0 and the surround is opaque | gamemain.cpp:2935-2947; resource.cpp:723 |
| briefing A (one picture) | briefing | 160×150 at (40,180), ×H/480 | full | none (BMP, opaque) | gamemain.cpp:1704-1706 |
| briefing A+B (two pictures) | briefing | 160×150 at (40,130) and (40,300) | full | none | gamemain.cpp:1708-1714 |

- `title.dds` serves as **both** a 6:1 logo and a 4:3 full-screen background. The
  source draws the same full texture both ways. How to compose one image for
  both uses is open (§10, Q-3).
- Briefing paths: an official mission `.txt` names a base name and the engine
  builds `data\briefing\<name>.bmp` (datafile.cpp:821-838). A `.mif` gives the
  path directly (datafile.cpp:929-935). `"!"` means no picture B, and the
  layout switches to one picture (gamemain.cpp:1643-1652). The picture is
  stretched to a 16:15 aspect (160×150). It loads with `BlackTransparent=false`.
- Scope: `ScopeGunsight` 1 or 2 also draws black reticle lines over the texture
  (gamemain.cpp:2949-2970), so the texture itself should not contain a reticle
  for those scope modes. Which weapon uses which mode: §6, `scopemode` column,
  plus `Scope[]` at parameter.cpp:1820ff.

### 3.1 Effect billboards (`blood`, `smoke`, `mflash`, `yakkyou`)

Geometry: `RenderBoard` draws a **1×1 quad in the local YZ plane**,
`(0,±0.5,±0.5)`, with UV `(0,0)` at (+y, −z) and `(1,1)` at (−y, +z)
(d3dgraphics-opengl.cpp:2306-2309, 2321-2324). It is scaled by `size`, so
**`size` equals the world edge length**. Vertex colour is `(1,1,1,alpha)`
(d3dgraphics-opengl.cpp:2312-2318), and the texture modulates it.

- **Blending is normal alpha blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), not
  additive** (d3dgraphics-opengl.cpp:1776-1777). There is no black key (§1.4).
  **Effect DDS files must have real alpha.** A black background drawn without
  alpha shows as a black square.
- Billboarding: unless `EFFECT_NOBILLBOARD` is set, rx and ry follow the camera
  (object.cpp:3222-3226). `SetWorldTransformEffect` then rolls the quad by
  `rotation_texture` about its normal (d3dgraphics-opengl.cpp:1876-1887).
- **Every spawn uses a random roll in 10° steps** (`DegreeToRadian(10)*GetRand(36)`),
  and the muzzle flash does too (objectmanager.cpp:2104). **Designs must be
  roll-invariant** (radial, no up direction, no text).
- `brightness`/`addbrightness` are stored (object.cpp:3116-3123) but never
  reach the renderer (object.cpp:3229-3230; `RenderBoard` has no brightness
  parameter, d3dgraphics-opengl.cpp:2281). They have no visual effect.
- Depth: GL **disables the depth test** for effects (d3dgraphics-opengl.cpp:2344-2353),
  while D3D only disables Z-write (d3dgraphics-directx.cpp:1834-1845). Effects are
  sorted far-to-near (objectmanager.cpp:2970-3000, 3104-3116). The GL behaviour
  lets effects show through walls, which looks like a backend bug (§10, Q-8).

| Texture | Spawn (source) | Start size (world u) | Growth/frame | Alpha start / per frame | Life (frames @ 30 ms) | Notes |
|---|---|---|---|---|---|---|
| blood | bullet hit on human, objectmanager.cpp:624 | 10 | +0.2 | 0.5 / −0.04 | 12 | |
| blood | flowing droplets, objectmanager.cpp:628 | 0.1–4.0 | +0.1 | 1.0 / 0 | 15 | gravity −0.09 |
| blood | wall decal `AddMapEffect`, objectmanager.cpp:575-600, 2869 | 10 | 0 | 1.0 / −0.002 | 600 | `EFFECT_NOBILLBOARD`, oriented to the block face normal (objectmanager.cpp:588-589) |
| smoke | bullet hits map, objectmanager.cpp:894 | 3 | +0.24 | 0.63 / −0.03 | 20 | |
| smoke | bullet hits article, objectmanager.cpp:1018-1019 | 1 | +0.1 | 0.625 / −0.047 | until alpha ≤ 0 | |
| smoke | gunfire, objectmanager.cpp:2112-2113 | 0.04·attacks and 0.032·attacks | +0.3 / +0.36 | 0.47 / −0.047, −0.055 | 12 / 10 | |
| smoke | grenade, objectmanager.cpp:1217-1224 | 36; 50–70; 40–80 | +0.2 to +0.3 | 0.4–0.6 / −0.004 to −0.01 | 50–120 | |
| smoke | robot death, objectmanager.cpp:1267-1269 | 30–33 | +0.1 | 1.0 / −1/90 | 90 | human type 1 only |
| mflash | gunfire, objectmanager.cpp:2104 | 0.06·attacks (silencer: 1.2) | 0 | 0.63 | 1 | at the weapon's flash point (§6) |
| mflash | grenade explosion, objectmanager.cpp:1214 | 40 | 0 | 0.63 | 2 | |
| yakkyou | shell ejection, objectmanager.cpp:2163 | `yakkyou_size/10`, or 0.012·attacks if negative | 0 | 1.0 | 22 | spins ±30°/frame, gravity −0.15; a **sprite of a spent casing** |

Example: MP5 attacks = 44 (parameter.cpp:539) gives an mflash of 2.64 u and a
casing of 0.53 u. Reference shape for effect textures: 128×128 A8R8G8B8
(FREE-DATA-PACK F-4).

---

## 4. Models (`.x` text) as parsed by the GL backend

The D3D backend uses `D3DXLoadMeshFromX` (d3dgraphics-directx.cpp:592), which
reads the full format. **The GL parser is the binding constraint.** A file that
satisfies it is also ordinary valid `.x` text for D3DX.

### 4.1 Grammar subset (exact behaviour of d3dgraphics-opengl.cpp:468-721)

The parser works on **lines** (`fgets`, 256-byte buffer), not tokens.

```
file      := HEADER-LINE { line }
HEADER    := line whose first 11 chars are exactly "xof 0302txt"        (506-511)
mesh      := MESH-LINE NV-LINE NV*VERTEX-LINE SKIP-LINE NF-LINE NF*FACE-LINE
             ... any lines ... UVHDR-LINE NUV-LINE NV*UV-LINE
MESH-LINE := line whose first 4 chars are "Mesh"  (column 0!)          (514-516)
NV-LINE   := atoi() of the whole line = vertex count NV                  (518-519)
VERTEX    := one vertex per line: x ; y ; z (separators any of " ;,")    (533-538)
SKIP-LINE := exactly ONE line, content ignored                           (540)
NF-LINE   := atoi() = face count NF                                       (542-543)
FACE-LINE := one face per line: n ; i0 , i1 , i2 [, i3]   with n ∈ {3,4} (557-563)
UVHDR     := line whose first 18 chars are " MeshTextureCoords"
             (exactly ONE leading space)                                  (565-568)
NUV-LINE  := atoi() must equal NV, else UVs are skipped                  (570-571)
UV-LINE   := one per line: u ; v                                         (573-577)
```

Consequences, each traceable to the lines above:

1. **The header must be `xof 0302txt`.** `0303txt`, binary and compressed `.x` are rejected and the model returns -1.
2. **`Mesh` must start at column 0.** A `Mesh` indented inside a `Frame { }` is invisible to the parser. `Frame`, `FrameTransformMatrix` and animation data are ignored, and no transforms are applied.
3. Child blocks must be **indented**. A column-0 `MeshNormals`/`MeshMaterialList` would match `"Mesh"` and be read as a new mesh. `MeshTextureCoords` must be indented by **exactly one space**.
4. **One vertex, face or UV per line**, each under 255 chars. Tabs are allowed before numbers (`atof`/`atoi` skip them).
5. **Exactly one line between the last vertex and the face count.** It is skipped unconditionally. Typical DirectX exporters put the face count directly after the vertices, and then the first face is read as the count. Use a blank line.
6. **Faces are triangles or quads only.** `index` stores 5 ints per face (547), so an n-gon with n > 4 overruns the buffer.
7. **UV count must equal the vertex count.** Otherwise the scan stops and u,v stay **uninitialised** memory (`new MODELVDATA[]`, 523, 571). If ` MeshTextureCoords` never appears, the scan consumes the rest of the file (565-581), swallowing any later meshes.
8. **Multiple meshes** are allowed. They are concatenated, each with its own ` MeshTextureCoords` (528-531, 551-555, 583-584). Each must be top-level (column 0).
9. Ignored: normals, materials, `TextureFilename`, vertex colours, `Material` colours. The texture is chosen by game code (§5–§7). D3D draws one subset per material (d3dgraphics-directx.cpp:1764-1766), so **include a `MeshMaterialList` with 1 material** to keep the D3D path safe (§10, Q-5).
10. **Lighting: none.** Every vertex colour is 1.0, or 0.8 when "dark" (d3dgraphics-opengl.cpp:690-703, 2253-2258). D3D uses emissive-only material with the same brightness (d3dgraphics-directx.cpp:1723-1730). Shading must be painted into the texture.

### 4.2 Axes, units, winding

- **Axes:** left-handed, +Y up, as D3D reads the file. GL negates x on load (535).
- **Front of every oriented model is −Z.** Humans and held weapons are rendered with yaw `rx+π` (object.cpp:2158, 2202). Human forward is `(sin rx, 0, cos rx)` (`AddPosOrder(-rx+π/2)`, object.cpp:1469, 1025-1029). `D3DXMatrixRotationY(rx+π)` maps file −Z onto that vector. Bullets (yaw `−rx−π/2`, object.cpp:2897) and the effect transforms are consistent with this.
- **Units:** there are no fixed units. Each model is scaled by a per-object `size` (§5–§7), and one world unit is roughly 1/20 of a person's height (`HUMAN_HEIGHT 20`, object.h:48).
- **Winding / culling:** D3D culls counter-clockwise faces (`D3DCULL_CCW`, d3dgraphics-directx.cpp:474). GL keeps default `glFrontFace(GL_CCW)` / back culling (no override in the GL backend), but emits each face as `(v0, v2, v1)` for triangles, or `(v0, v3, v1)` + `(v1, v3, v2)` for quads (598-686, `GL_TRIANGLE_STRIP` from index 1, 2269). Together with the x-mirror this gives the same result as D3D. **Contract: list each face's vertices clockwise as seen from the side that should be visible.** Equivalently, `(v1−v0)×(v2−v0)` (ordinary cross product, file coordinates) points toward the viewer. Single-sided only: anything that must be seen from both sides needs two faces.
- **Quads must be planar and convex.** GL splits a quad along v1–v3 (d3dgraphics-opengl.cpp:644-684). D3DX triangulation is library behaviour and usually fans from v0, giving the v0–v2 diagonal. A non-planar quad can therefore render differently in the two backends.

### 4.3 MorphingModel

`MorphingModel(A,B)` builds an in-between frame by **averaging positions element by element**
(GL: expanded 6-vertices-per-face arrays, d3dgraphics-opengl.cpp:770-774; D3D:
vertex buffer index by index, d3dgraphics-directx.cpp:671). UVs and colours are
copied from A (d3dgraphics-opengl.cpp:777-788). GL rejects the morph when the face
counts differ (762). D3D rejects it when the vertex counts differ
(d3dgraphics-directx.cpp:642-646). A rejected morph returns −1, and that animation
frame is simply not drawn (2225).

**Contract for morph partners (walk01–04, run01–06):** an identical vertex list
(count and order), an identical face list (count, order, per-face vertex count,
indices) and identical UVs. Only positions may differ.
`check_x.py --morph` checks this.

### 4.4 Minimal valid example

`tools/contract/examples/cube.x` is a 2×2×2 cube standing on y=0, with 24
vertices, 6 quads and full 0..1 UVs per face. It passes
`python3 tools/contract/check_x.py tools/contract/examples/cube.x --closed`.
Excerpt (abridged; see the file for the full text):

```
xof 0302txt 0032

Mesh Cube {
24;
-1.000000;2.000000;-1.000000;,
1.000000;2.000000;-1.000000;,
1.000000;0.000000;-1.000000;,
-1.000000;0.000000;-1.000000;,
...                                   (20 more vertex lines; last ends ";;")

6;
4;0,1,2,3;,
...                                   (last face ends ";;")

 MeshTextureCoords {
24;
0.000000;0.000000;,
1.000000;0.000000;,
1.000000;1.000000;,
0.000000;1.000000;,
...
 }

 MeshMaterialList {
1;
6;
0, ... 0;;
 Material { 1;1;1;1;; 0; 0;0;0;; 0;0;0;; }
 }
}
```

Face 0 is the −Z (front) face. Its order, top-left → top-right → bottom-right
→ bottom-left as seen from −Z, is clockwise, so it is visible from the front.

---

## 5. Human rig

### 5.1 Files and how they are used

| Slot | Files | Array | Source |
|---|---|---|---|
| Upper body (incl. head) | `up0.x`–`up5.x` | `human_upmodel[0..5]` | resource.cpp:143-148 |
| Arms | `arm00.x` (no weapon / WeaponP 2), `arm01.x` (WeaponP 0, two-handed), `arm02.x` (WeaponP 1, one-handed) | `human_armmodel[0..2]` | resource.cpp:149-151; object.cpp:3431-3461 |
| Legs standing | `leg.x` | | resource.cpp:152 |
| Walk cycle | `walk01–04.x` at slots 0,2,4,6; morphs at 1,3,5,7 = avg(01,02), avg(02,03), avg(03,04), avg(04,01) | `human_walkmodel[8]` | resource.cpp:153-156, 166-169 |
| Run cycle | `run01–06.x` at slots 0,2,…,10; morphs at odd slots, avg(06,01) last | `human_runmodel[12]` | resource.cpp:157-162, 170-175 |

Morphing is on (`ENABLE_HUMANMODEL_MORPHING`, resource.h:42). Frame selection
(object.cpp:3519-3539, 30 ms per game frame, main.h:52):

- walk: slot `walkcnt/3 % 8`, a 24-frame (0.72 s) cycle
- run forward: `runcnt/2 % 12`, 24 frames
- run sideways: `runcnt/3 % 12`, 36 frames
- run backward: `runcnt/4 % 12`, 48 frames
- standing or dead: `leg.x`

### 5.2 Transforms at render time (`human::Render`, object.cpp:2125-2205)

All parts use `SetWorldTransform(x,y,z, yaw, pitch, size)`, which is
`v·S·Rx(pitch)·Ry(yaw)·T` (d3dgraphics-directx.cpp:1076-1089; GL
d3dgraphics-opengl.cpp:1855-1866). `pos` is the feet position.

| Part | Translate | Yaw | Pitch | Scale | Source |
|---|---|---|---|---|---|
| Upper body | pos + (0, −1.0, 0) | rx + π | rotation_y (0 alive; death fall) | **9.4** | object.cpp:2154-2159, 178 |
| Legs | pos + (0, −0.2, 0) | legrx + π (blends towards move direction) | rotation_y | **9.0** | object.cpp:2168-2173, 180, 3496-3513 |
| Arms (alive) | pos + (0, **16.0**, 0) | rx + π | armry | **9.0** | object.cpp:2185, 2198, 179 |
| Held weapon | pos + (0, 16.0, 0), then local (−mx/10, my/10, −mz/10) after the rotation | rx + π | armry | weapon `size` | object.cpp:2202; d3dgraphics-directx.cpp:1142-1158 |

Dead or falling bodies use offsets rotated by `rotation_y` (object.cpp:2148-2152,
2162-2166, 2177-2182). `armry` is the aim pitch plus recoil. With no weapon it
is fixed at `ARMRAD_NOWEAPON = −70°` (arms hanging), and while reloading at
`−20°` (object.h:63-64; object.cpp:3421-3460). A positive pitch raises a −Z
pointing arm (`D3DXMatrixRotationX`).

### 5.3 What replacement parts must keep consistent (derived)

The meshes are not constrained by the source. The **hitboxes and pivots** are,
and the visuals must match them. Bullet cylinders measured up from the feet
(objectmanager.cpp:755-787, constants objectmanager.h:53-58): legs 0–10.5 (r
2.6), torso 10.5–17.5 (r 2.4), head 17.5–20.4 (r 1.5). The eye is at 19.0
(gamemain.h:50). Inverting the transforms in §5.2 (`file_y = (world_y − offset)/scale`) gives:

| Feature (world y) | `leg.x`/walk/run file y = (Y+0.2)/9 | `upN.x` file y = (Y+1)/9.4 | `armNN.x` file coords |
|---|---|---|---|
| Soles (0) | **0.022** | — | — |
| Hip / top of legs (10.5) | 1.189 | 1.223 | — |
| Shoulder pivot (16.0) | — | 1.809 | **origin (0,0,0)** |
| Eye (19.0) | — | 2.128 | — |
| Top of head (20.4) | — | 2.277 | — |
| Radius limits | ≤ 0.29 (2.6/9) | torso ≤ 0.255, head ≤ 0.16 | — |

- **Arm models pivot at their origin**, which is the shoulder line, centred on
  the body axis. With pitch 0 they point **forward along file −Z**. A weapon's
  grip sits at file `(−mx, my, −mz)/90` in arm space (derived from §6.1). For
  example, rifle (MP5) `(−0.089, 0.100, −0.389)` and pistol (M92F)
  `(−0.022, 0.078, −0.633)`. Arm hands should be modelled there.
- All parts face −Z and are centred on the Y axis (x = z = 0 is the body axis).
- **Morph sets:** walk01–04 share one topology, and run01–06 share one topology
  (§4.3). `leg.x` and the two sets may differ from each other.
- **One skin covers every part.** Up, arm and leg are all drawn with the same
  `id_texture` (object.cpp:2159, 2173, 2182-2199). Several skins are used with
  more than one upper-body model, e.g. `civ1.bmp` with up0 and up1 (table below).
  **Recommendation (not a source rule):** give up0–up5, arm00–02, leg and the
  walk/run sets **one common UV layout**, so that any skin works on any part.

### 5.4 Skin selection

A human type (0–42, `Human[]` at parameter.cpp:82-460) has `texture` (an index
into `HumanTexturePath[0..29]`, parameter.cpp:469-498) and `model` (an index
into `up0..up5`). `ObjectManager` binds `upmodel[HumanParam.model]` and
`GetHumanTexture(type)` (objectmanager.cpp:203-215, 316-328, 1537-1549). If the
skin is missing, map texture 0 is used instead (objectmanager.cpp:214). Skins are
`./data/model/*.bmp` (forward slashes in source). Type 10 is a robot (smoke on
death), and types 29–32 are zombies (`type`, parameter.cpp; objectmanager.cpp:1246).

| Texture idx | File | Used with up model(s) → human type IDs |
|---|---|---|
| 0 | civ1.bmp | up0.x → [19]; up1.x → [23] |
| 1 | civ2.bmp | up0.x → [20]; up1.x → [24] |
| 2 | civ3.bmp | up0.x → [21]; up1.x → [25] |
| 3 | gates.bmp | up0.x → [41]; up1.x → [42] |
| 4 | gs.bmp | up0.x → [12]; up1.x → [11] |
| 5 | hage.bmp | up0.x → [7] |
| 6 | islam.bmp | up0.x → [17] |
| 7 | islam2.bmp | up0.x → [35] |
| 8 | police.bmp | up2.x → [14] |
| 9 | riiman.bmp | up0.x → [15] |
| 10 | riiman_b.bmp | up0.x → [36]; up1.x → [37] |
| 11 | riiman_g.bmp | up0.x → [13] |
| 12 | riiman_k.bmp | up0.x → [33]; up1.x → [34] |
| 13 | robot.bmp | up0.x → [10] |
| 14 | soldier_black.bmp | up5.x → [0, 1] |
| 15 | soldier_blue.bmp | up5.x → [9] |
| 16 | soldier_green.bmp | up0.x → [2, 3, 4] |
| 17 | soldier_violet.bmp | up0.x → [8] |
| 18 | soldier_white.bmp | up0.x → [5, 6] |
| 19 | soldier0.bmp | up4.x → [26] |
| 20 | soldier1.bmp | up4.x → [27] |
| 21 | soldier2.bmp | up4.x → [28] |
| 22 | soldier3.bmp | up0.x → [39]; up1.x → [40] |
| 23 | syatu.bmp | up0.x → [16]; up1.x → [22] |
| 24 | syatu2.bmp | up1.x → [38] |
| 25 | woman.bmp | up3.x → [18] |
| 26 | zombie1.bmp | up0.x → [29] |
| 27 | zombie2.bmp | up0.x → [30] |
| 28 | zombie3.bmp | up0.x → [31] |
| 29 | zombie4.bmp | up3.x → [32] |

---

## 6. Weapons and articles

### 6.1 Attachment frames (derived from source transforms)

Define the **hand frame** H: origin at pos + (0,16,0) (the shoulder pivot),
+X = the character's right, +Y up, +Z forward, pitched by the aim angle. Then:

- The muzzle flash is spawned at H-point `(flashx, flashy, flashz)/10` and casings at `(yakkyou_px, py, pz)/10`
  (objectmanager.cpp:2099, 2158: yaw `rx`, pitch `−armry`, no scale).
- The weapon model is drawn with yaw `rx+π`, pitch `armry`, local translate `(−mx/10, my/10, −mz/10)`, scale `size`
  (object.cpp:2202). Since `Ry(π)·Rx(a)·Ry(π) = Rx(−a)`, this is the same as placing the **file origin at H-point
  `(mx, my, mz)/10`** with the file axes turned 180° about Y: **file −Z = barrel direction (forward), file −X = character's
  right**, scaled by `size`.
- So a file point `p` sits at H-point `size·(−px, py, −pz) + m/10`. Solving for the flash point gives
  **muzzle_file = (−(fx−mx), fy−my, −(fz−mz)) / (10·size)**. The ejection port is analogous.

The muzzle and ejection values below are **derived**. They are where a
replacement model's muzzle and ejection port must be for flashes and casings to
line up (at zero recoil). Where the model origin sits on the gun is **not
determined by source**. The table assumes the original convention that it sits
at the grip.

### 6.2 Per-weapon parameters (parameter.cpp)

Paths are relative to `./data/model/weapon/`. `size` is the model scale in all
three uses (in hand; on the ground, object.cpp:2531 with objectmanager.cpp:406;
HUD, §6.3).

| ID | Name | Model | Texture | size | grip m=(mx,my,mz) | flash (fx,fy,fz) | eject (px,py,pz) | WeaponP → arm | sound | scope | src |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | NONE | — | — | 9 | (0, 0, 0) | (0, 0, 0) | (0, 0, 0) | 2 → arm00 (none) | bang1 (vol 0) | 0 | parameter.cpp:501-535 |
| 1 | MP5 | mp5.x | mp5.bmp | 7.5 | (8, 9, 35) | (8, 15, 68) | (8, 12, 35) | 0 → arm01 (2-hand) | bang1 (vol 92) | 0 | parameter.cpp:536-570 |
| 2 | PSG1 | psg1.x | psg1.bmp | 8 | (8, 8, 44) | (8, 13, 108) | (8, 13, 38) | 0 → arm01 (2-hand) | bang2 (vol 98) | 2 | parameter.cpp:571-605 |
| 3 | M92F | m92f.x | m92f.bmp | 7 | (2, 7, 57) | (2, 15, 73) | (2, 15, 56) | 1 → arm02 (1-hand) | bang2 (vol 90) | 0 | parameter.cpp:606-640 |
| 4 | GLOCK18 SEMI | glock18.x | glock18.bmp | 5 | (2, 8, 56) | (2, 13, 70) | (2, 14, 54) | 1 → arm02 (1-hand) | bang1 (vol 90) | 0 | parameter.cpp:641-675 |
| 5 | DESERT EAGLE | de.x | de.bmp | 11 | (2, 11, 59) | (2, 14, 75) | (2, 15, 56) | 1 → arm02 (1-hand) | bang3 (vol 96) | 0 | parameter.cpp:676-710 |
| 6 | MAC10 | mac10.x | mac10.bmp | 5.5 | (2, 4, 54) | (2, 15, 73) | (2, 16, 52) | 1 → arm02 (1-hand) | bang4 (vol 90) | 0 | parameter.cpp:711-745 |
| 7 | UMP | ump.x | ump.bmp | 6 | (8, 6, 28) | (8, 13, 64) | (8, 11, 41) | 0 → arm01 (2-hand) | bang5 (vol 94) | 0 | parameter.cpp:746-780 |
| 8 | P90 | p90.x | p90.bmp | 6.5 | (8, 10, 30) | (8, 12, 59) | (8, 4, 18) | 0 → arm01 (2-hand) | bang4 (vol 92) | 0 | parameter.cpp:781-815 |
| 9 | M4 | m4.x | m4.bmp | 6.8 | (9, 6, 37) | (9, 13, 91) | (10, 12, 40) | 0 → arm01 (2-hand) | bang4 (vol 96) | 0 | parameter.cpp:816-850 |
| 10 | AK47 | ak47.x | ak47.bmp | 6 | (9, 11, 39) | (9, 9, 86) | (9, 12, 41) | 0 → arm01 (2-hand) | bang2 (vol 96) | 0 | parameter.cpp:851-885 |
| 11 | AUG | aug.x | aug.bmp | 7.8 | (8, 10, 37) | (8, 12, 77) | (8, 11, 15) | 0 → arm01 (2-hand) | bang5 (vol 98) | 1 | parameter.cpp:886-920 |
| 12 | M249 | m249.x | m249.bmp | 7.5 | (8, 12, 44) | (9, 13, 102) | (9, 11, 45) | 0 → arm01 (2-hand) | bang4 (vol 98) | 0 | parameter.cpp:921-955 |
| 13 | GRENADE | grenade.x | grenade.bmp | 8 | (1, 6, 57) | (1, 6, 57) | (0, 0, 0) | 1 → arm02 (1-hand) | bang1 (vol 0) | 0 | parameter.cpp:956-990 |
| 14 | MP5SD | mp5sd.x | mp5sd.bmp | 7.5 | (7, 8, 35) | (8, 12, 84) | (8, 12, 35) | 0 → arm01 (2-hand) | ka (vol 84) | 0 | parameter.cpp:991-1025 |
| 15 | CASE | case.x | case.bmp | 5 | (15, -2, 35) | (0, 0, 0) | (0, 0, 0) | 2 → arm00 (none) | bang1 (vol 0) | 0 | parameter.cpp:1026-1060 |
| 16 | GLOCK18 FULL | glock18.x | glock18.bmp | 5 | (2, 8, 56) | (2, 13, 70) | (2, 14, 54) | 1 → arm02 (1-hand) | bang1 (vol 90) | 0 | parameter.cpp:1061-1095 |
| 17 | M1911 | cg.x | cg.bmp | 6.8 | (2, 7, 61) | (2, 14, 74) | (2, 16, 57) | 1 → arm02 (1-hand) | bang4 (vol 94) | 0 | parameter.cpp:1096-1130 |
| 18 | GLOCK17 | glock18.x | glock17.bmp | 5 | (2, 8, 56) | (2, 13, 70) | (2, 14, 54) | 1 → arm02 (1-hand) | bang1 (vol 90) | 0 | parameter.cpp:1131-1165 |
| 19 | M1 | m1.x | m1.bmp | 7.7 | (8, 7, 45) | (8, 11, 94) | (8, 11, 42) | 0 → arm01 (2-hand) | bang3 (vol 98) | 0 | parameter.cpp:1166-1200 |
| 20 | FAMAS | famas.x | famas.bmp | 8 | (9, 10, 32) | (9, 12, 70) | (9, 11, 14) | 0 → arm01 (2-hand) | bang5 (vol 96) | 0 | parameter.cpp:1201-1235 |
| 21 | MK23 | mk23.x | mk23.bmp | 5 | (2, 11, 60) | (2, 15, 74) | (1, 16, 60) | 1 → arm02 (1-hand) | bang5 (vol 92) | 0 | parameter.cpp:1236-1270 |
| 22 | MK23 SD | mk23sd.x | mk23.bmp | 5 | (2, 11, 60) | (2, 15, 96) | (1, 16, 60) | 1 → arm02 (1-hand) | ka (vol 86) | 0 | parameter.cpp:1271-1305 |

Derived placement (§6.1):

| ID | Name | origin in hand frame (world u) = m/10 | muzzle in .x file coords | ejection port in .x file coords |
|---|---|---|---|---|
| 1 | MP5 | (0.8, 0.9, 3.5) | (0.000, 0.080, -0.440) | (0.000, 0.040, 0.000) |
| 2 | PSG1 | (0.8, 0.8, 4.4) | (0.000, 0.062, -0.800) | (0.000, 0.062, 0.075) |
| 3 | M92F | (0.2, 0.7, 5.7) | (0.000, 0.114, -0.229) | (0.000, 0.114, 0.014) |
| 4 | GLOCK18 SEMI | (0.2, 0.8, 5.6) | (0.000, 0.100, -0.280) | (0.000, 0.120, 0.040) |
| 5 | DESERT EAGLE | (0.2, 1.1, 5.9) | (0.000, 0.027, -0.145) | (0.000, 0.036, 0.027) |
| 6 | MAC10 | (0.2, 0.4, 5.4) | (0.000, 0.200, -0.345) | (0.000, 0.218, 0.036) |
| 7 | UMP | (0.8, 0.6, 2.8) | (0.000, 0.117, -0.600) | (0.000, 0.083, -0.217) |
| 8 | P90 | (0.8, 1, 3) | (0.000, 0.031, -0.446) | (0.000, -0.092, 0.185) |
| 9 | M4 | (0.9, 0.6, 3.7) | (0.000, 0.103, -0.794) | (-0.015, 0.088, -0.044) |
| 10 | AK47 | (0.9, 1.1, 3.9) | (0.000, -0.033, -0.783) | (0.000, 0.017, -0.033) |
| 11 | AUG | (0.8, 1, 3.7) | (0.000, 0.026, -0.513) | (0.000, 0.013, 0.282) |
| 12 | M249 | (0.8, 1.2, 4.4) | (-0.013, 0.013, -0.773) | (-0.013, -0.013, -0.013) |
| 13 | GRENADE | (0.1, 0.6, 5.7) | (0.000, 0.000, 0.000) | — (no ejection) |
| 14 | MP5SD | (0.7, 0.8, 3.5) | (-0.013, 0.053, -0.653) | (-0.013, 0.053, 0.000) |
| 15 | CASE | (1.5, -0.2, 3.5) | — (no flash) | — (no ejection) |
| 16 | GLOCK18 FULL | (0.2, 0.8, 5.6) | (0.000, 0.100, -0.280) | (0.000, 0.120, 0.040) |
| 17 | M1911 | (0.2, 0.7, 6.1) | (0.000, 0.103, -0.191) | (0.000, 0.132, 0.059) |
| 18 | GLOCK17 | (0.2, 0.8, 5.6) | (0.000, 0.100, -0.280) | (0.000, 0.120, 0.040) |
| 19 | M1 | (0.8, 0.7, 4.5) | (0.000, 0.052, -0.636) | (0.000, 0.052, 0.039) |
| 20 | FAMAS | (0.9, 1, 3.2) | (0.000, 0.025, -0.475) | (0.000, 0.013, 0.225) |
| 21 | MK23 | (0.2, 1.1, 6) | (0.000, 0.080, -0.280) | (0.020, 0.100, 0.000) |
| 22 | MK23 SD | (0.2, 1.1, 6) | (0.000, 0.080, -0.720) | (0.020, 0.100, 0.000) |

Shared assets that force a common UV layout: `glock18.x` is used with both
`glock18.bmp` and `glock17.bmp` (IDs 4, 16, 18), and `mk23.bmp` with both
`mk23.x` and `mk23sd.x` (IDs 21–22). The thrown grenade is drawn with
`grenade.x`/`grenade.bmp` at `size` 8 (`Bullet[1]`, parameter.cpp:1438-1440).
The bullet is `bullet.x`/`bullet.bmp`, size 1.0, nose toward file −Z
(parameter.cpp:1434-1436, object.cpp:2897).

Weapons lying on the ground use yaw `rx` and **roll π/2 about Z**, so they lie
on their side (object.cpp:2531). Grenades in flight: object.cpp:3044.

### 6.3 HUD weapon models

`SetWorldTransformPlayerWeapon` (d3dgraphics-opengl.cpp:1920-1965;
d3dgraphics-directx.cpp:1174ff) unprojects the screen points
`(W−140, H−40, z 0.86)` for the current weapon and `(W−72, H−25, z 0.93)` for
the reserve weapon (d3dgraphics-opengl.cpp:251-258). The model is drawn there
at `size × (0.0004H² − 0.92H + 650)/1000` (×0.30 at H=480,
d3dgraphics-opengl.cpp:1952). The current weapon **spins about the file Y axis
through the file origin** at 2°/frame (gamemain.cpp:3271), and the reserve one
is turned 180° (d3dgraphics-opengl.cpp:1962). A grip-centred origin makes the
HUD model orbit a little instead of spinning in place (§10, Q-6).

### 6.4 Articles (small objects)

All articles use scale `SMALLOBJECT_SCALE = 5.0` (parameter.h:53;
objectmanager.cpp:472, 507, 1582) and are drawn with yaw `rx`, pitch `ry`
(object.cpp:2765). On spawn they drop until the floor is `decide/10` below the
origin (object.cpp:2633-2659). The bullet collision sphere is centred on the
origin with radius `decide × 0.13` (objectmanager.cpp:824-833,
objectmanager.h:60). Paths are relative to `./data/article/`.

| ID | Model | Texture | decide | rest height above floor = decide/10 (world) | origin height in .x units = decide/50 | collision sphere r = decide×0.13 | hp | hit sound | jump | src |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | can.x | can.bmp | 10 | 1 | 0.2 | 1.30 | 6 | can.wav | 10 | parameter.cpp:1348-1353 |
| 1 | pc.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1355-1360 |
| 2 | pc2.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1362-1367 |
| 3 | pc3.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1369-1374 |
| 4 | pc_w.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1376-1381 |
| 5 | pc2_w.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1383-1388 |
| 6 | pc3_w.x | pc.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1390-1395 |
| 7 | isu.x | isu.bmp | 56 | 5.6 | 1.12 | 7.28 | 50 | dan.wav | 6 | parameter.cpp:1397-1402 |
| 8 | dan.x | dan.bmp | 28 | 2.8 | 0.56 | 3.64 | 35 | dan.wav | 8 | parameter.cpp:1404-1409 |
| 9 | pc.x | pc_sw.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1411-1416 |
| 10 | pc.x | pc_d.bmp | 29 | 2.9 | 0.58 | 3.77 | 60 | can.wav | 5 | parameter.cpp:1418-1423 |
| 11 | cone.x | cone.bmp | 35 | 3.5 | 0.7 | 4.55 | 30 | dan.wav | 7 | parameter.cpp:1425-1430 |

Shared textures: `pc.bmp` covers pc, pc2, pc3, pc_w, pc2_w and pc3_w, and `pc.x`
also takes `pc_sw.bmp` and `pc_d.bmp`. Those meshes need a compatible UV layout.
One add-on article per mission comes from a 6-line file named in the `.mif`
(model, texture, decide & 0x7F, hp & 0x7F, sound path, jump & 0xFF;
datafile.cpp:957-1049). It uses the same scale and rules.

---

## 7. Sky

- Files: `data\sky\sky.x` (one model for all skies) and `data\sky\sky<N>.bmp`, N = 1–5 (resource.cpp:748-764). N comes from the mission (`.mif`/`.txt` line, datafile.cpp:840-843, 900-903, gamemain.cpp:1877-1878). The opening and main menu use sky 1 (gamemain.cpp:255, 291). N = 0 means no sky: the clear colour is black (d3dgraphics-opengl.cpp:1769).
- Placement: **centred on the camera, no rotation, scale 2.0** (gamemain.cpp:2769-2772, 378-381). The Z buffer is cleared afterwards (gamemain.cpp:2775), so the sky never occludes anything. It is drawn with the dark flag (0.8) on dark missions.
- Faces must be visible **from inside**: wind them clockwise as seen from the origin (§4.2).
- **Clip and fog (derived):** near plane 1.0, far plane 800 (d3dgraphics.h:52-53). With fog on (the default, gamemain.cpp:1894), linear fog runs from 100 to 800 (d3dgraphics-opengl.cpp:2001-2007; D3D d3dgraphics-directx.cpp:1264-1272) and also applies to the sky. Every sky vertex must be under 400 file units from the origin (×2 < 800) or it is clipped. Sky texels at eye distance d are blended towards the fog colour by `(d−100)/700`. **To show the texture unfogged, keep the sky within about 50 file units** (≤ 100 world). The original radius is not known (§10, Q-4).
- Fog colour per sky number (RGB 0–1): 1 = (0.25, 0.3125, 0.25), 2 = (0.0625, 0.0625, 0.0625), 3 = (0, 0.0625, 0.125), 4 = (0.125, 0.0625, 0.0625), 5 = (0.25, 0.125, 0.125), other = black (d3dgraphics-opengl.cpp:1992-1999; same values in d3dgraphics-directx.cpp:1286-1293). **The horizon of sky N should fade to this colour** so that distant fogged geometry blends with the sky.
- UVs: only the model's own UVs are used, with no scrolling. `sky.x` is shared by all five textures, so all five must fit one UV layout. 256×256 is the reference (FREE-DATA-PACK F-4).

---

## 8. Sounds

### 8.1 Loader constraints

The default build uses **ezds.dll** (`SOUND_ENGINE 0`, sound.h:45). Its WAV
decoding is inside the DLL and is **not determined by source**
(`DSload(filename, id)`, sound-ezds.cpp:195-196).

The DirectSound backend (`SOUND_ENGINE 1/2`) does it in source.
`CheckSoundFile` (sound-directsound.cpp:500-566) requires:

- a RIFF/WAVE file
- an `fmt ` chunk with `wFormatTag` PCM (1) or IEEE_FLOAT (3); **WAVE_FORMAT_EXTENSIBLE (0xFFFE) is rejected**
- 1 or 2 channels
- a `data` chunk

`LoadSound` then requires 8/16/24/32 bits per sample
(sound-directsound.cpp:187-191). **Stereo is reduced to its left channel**
(sound-directsound.cpp:193-202, 255-310). Any sample rate is passed through.
All playback is **one-shot** (`Play(0,0,0)`, never `DSBPLAY_LOOPING`,
sound-directsound.cpp:372, 440). Each sound has up to 100 simultaneous voices
(`MAX_SOUNDLISTS`, sound.h:36). At most 24 sounds can be loaded
(`MAX_LOADSOUND`, sound.h:35). 3D attenuation reaches zero at 335 units
(`MAX_SOUNDDIST`, sound.h:37).

**Contract:** canonical 44-byte-header **PCM, mono, 16-bit** WAV (22,050 Hz is the reference, FREE-DATA-PACK F-4), with no `LIST`/`cue` chunks needed.

### 8.2 Event → sound

| File | Event | Playback | Volume (−100..100+) | Source |
|---|---|---|---|---|
| `bang1.wav` | gunshot, weapons with soundid 0 (MP5, GLOCK18/17) | 3D; the player's own shots are 2D | weapon `soundvolume` | resource.cpp:441, 454-456; soundmanager.cpp:578-592 |
| `bang2.wav` | soundid 1 (PSG1, M92F, AK47) | same | same | resource.cpp:442 |
| `bang3.wav` | soundid 2 (DESERT EAGLE, M1) | same | same | resource.cpp:443 |
| `bang4.wav` | soundid 3 (MAC10, P90, M4, M249, M1911) | same | same | resource.cpp:444 |
| `bang5.wav` | soundid 4 (UMP, AUG, FAMAS, MK23) | same | same | resource.cpp:445 |
| `ka.wav` | soundid 13, silenced (MP5SD, MK23 SD) | same | same | resource.cpp:446, 469-470 |
| `hit1.wav` | bullet hits map (2 of 3 times) | 3D | 95 | resource.cpp:797; soundmanager.cpp:594-603, soundmanager.h:37 |
| `hit3.wav` | bullet hits map (1 of 3 times) | 3D | 95 | resource.cpp:798 |
| `hit2.wav` | bullet hits a human (head/body/leg/zombie) | 3D | 75 | resource.cpp:799; soundmanager.cpp:605-612 |
| `hyu.wav` | enemy bullet passes near the camera | 3D | 80 | resource.cpp:800; soundmanager.cpp:619-631 |
| `bang.wav` | grenade explosion | 3D | 120 | resource.cpp:801; soundmanager.cpp:639-643 |
| `cco.wav` | grenade bounce | 3D | 100 | resource.cpp:802; soundmanager.cpp:633-637 |
| `can.wav` | article hit, articles with sound 0 (can, pc*) | 3D | 110 | resource.cpp:573, 580-582; soundmanager.cpp:614-617 |
| `dan.wav` | article hit, articles with sound 1 (isu, dan, cone) | 3D | 110 | resource.cpp:574, 583-585 |
| add-on article sound | add-on article hit | 3D | 110 | resource.cpp:642 |

No reload, footstep or jump sound exists. The reload sound is commented out
and `reloadid = -1` (resource.cpp:447-448), and footsteps return without
playing (soundmanager.cpp:645-661). Grenade and case have `soundvolume` 0, so
they are silent (soundmanager.cpp:580).

---

## 9. Maps and mission files

### 9.1 `.bd1` block data (datafile.cpp:104-176)

| Offset | Size | Field |
|---|---|---|
| 0 | 10 × 31 bytes | texture file names, each read as 31 raw bytes (datafile.cpp:128-130) and used with `strcpy`, so **each must be NUL-terminated within its 31 bytes (≤ 30 chars)**. They are relative to the `.bd1`'s directory: `directory + name` (d3dgraphics-opengl.cpp:2066-2068; directory from `GetFileDirectory`, datafile.cpp:1682-1694). An empty name means no texture for that slot (d3dgraphics-opengl.cpp:2061-2062). |
| 310 | 2 | block count, uint16 LE; clamped to `MAX_BLOCKS 160` (datafile.cpp:133-135, datafile.h:35) |
| 312 + 316·i | 288 | 72 × float32 LE: x[8], y[8], z[8], then u[6][4], v[6][4] (datafile.cpp:140, 147-163) |
| +288 | 28 | `textureID` of face j = signed byte at offset 4·j, j = 0..5. The other bytes are unused (datafile.cpp:141, 155) |

Face → vertex mapping (`blockdataface`, datafile.cpp:302-381): top (1,0,3,2),
bottom (6,7,4,5), front (5,4,0,1), right (6,5,1,2), back (7,6,2,3),
left (4,7,3,0). Every face uses UV slots (2,3,0,1) for those vertices.

Rendering (d3dgraphics-opengl.cpp:2075-2192): blocks are batched per texture,
and the UVs are the floats from the file (2165-2170). There is no wrap call, so
**GL_REPEAT applies** (§1.1). **Map tiles must tile seamlessly in both
directions.** Each face is tinted by a precomputed light value
`shadow = |n + L|/6 + 0.5` with the fixed, non-unit light vector `L` (`LIGHT_RX/RY`, datafile.h:49-50), giving about 0.55–0.89 (offset 0.3 instead of 0.5 on dark missions)
(datafile.cpp:252-265). Tiles are therefore drawn darker than authored. Map tiles
load with `BlackTransparent=false` and should be opaque (§1.4).

### 9.2 `.pd1` point data (datafile.cpp:416-492; struct `pointdata`, datafile.h:85-96)

The header is a uint16 LE count, clamped to `MAX_POINTS 256` (datafile.h:39).
Each record is **20 bytes**: `float32 x, y, z, r` (r = rotation) followed by
`int8 p1, p2, p3, p4`, which are stored widened to `signed short`
(datafile.cpp:451-462). P1 type constants: 1 HUMAN, 2 WEAPON, 3 AIPATH,
4 HUMANINFO, 5 SMALLOBJ, 6 HUMAN2, 7 RAND_WEAPON, 8 RAND_AIPATH
(datafile.h:98-106).

### 9.3 `.msg` (datafile.cpp:497-541)

The file is the `.pd1` name with its extension replaced by `.msg`
(datafile.cpp:478-489). It holds up to `MAX_POINTMESSAGES 16` lines, each read
with `fgets` into `MAX_POINTMESSAGEBYTE = 73` bytes, i.e. 71 chars + CR/LF
(datafile.h:46-47). CR is converted to LF. The lines are drawn with the
**system font, not `char.dds`** (gamemain.cpp:3145-3146). The encoding is CP932,
because the system font is created with `SHIFTJIS_CHARSET`
(d3dgraphics-opengl.cpp:233).

### 9.4 `.mif` (datafile.cpp:874-952; members datafile.h:177-191)

The file is read line by line with these buffer sizes:

| Line | Field | Buffer |
|---|---|---|
| 1 | `mission_name` | 24 |
| 2 | `mission_fullname` | 64 |
| 3 | `blockfile_path` (.bd1) | `_MAX_PATH` |
| 4 | `pointfile_path` (.pd1) | `_MAX_PATH` |
| 5 | `skynumber` (atoi) | 16 |
| 6 | flags "0".."3": bit0 = extra collision, bit1 = dark screen | 16 |
| 7 | `addsmallobject_path` (`""`/`"!"` = none) | `_MAX_PATH` |
| 8 | `picturefileA_path` | `_MAX_PATH` |
| 9 | `picturefileB_path` (`"!"` = none) | `_MAX_PATH` |
| 10+ | up to 17 `fgets(…,50)` chunks appended to `briefingtext[816]` | 50 each |

A briefing line longer than 48 chars is split across reads, and each read
counts towards the 17. The official `.txt` variant (datafile.cpp:804-869) has
these lines: picture A base name or `!`, picture B base name or `!`, sky number,
then the same 17 × 50 briefing text. Briefing text uses the system font.

---

## 10. Open questions

| ID | Question | Why the source does not settle it | Suggested handling |
|---|---|---|---|
| Q-1 | Which WAV variants does **ezds.dll** (the default sound backend) accept? | The decoder is in the closed DLL (sound-ezds.cpp:195-196). | Ship PCM mono 16-bit, which suits every backend. The web port replaces the backend anyway. |
| Q-2 | What do the HUD frame cells (B0–D2) look like (bevel, icons, how opaque)? | The source fixes only cell positions, tiling roles and the 0.5 alpha. | Design freely, keeping tile edges continuous. |
| Q-3 | How should `title.dds` serve both as a 480×80 logo and a full-screen 4:3 background at 40%? | Both uses draw the full 0..1 UV range (gamemain.cpp:966, scene.cpp:200). | Logo art that also reads when stretched, e.g. centred on a transparent field. Needs a design decision. |
| Q-4 | What size and shape should `sky.x` have (dome or box, radius)? | Not in source. Only the scale 2.0 and the fog/clip limits in §7 apply. | A box or dome with radius ≤ 50, wound inward. |
| Q-5 | Does D3DX give a material-less mesh 0 subsets, so that D3D draws nothing? | Library behaviour (d3dgraphics-directx.cpp:592-593, 1764-1766). | Always include a 1-material `MeshMaterialList`. |
| Q-6 | Where should the weapon model origin go (grip versus centre)? | In-hand placement is relative to the origin (§6.1) and the HUD spins about it (§6.3); neither fixes it on the gun. | Grip, matching the derived muzzle and eject points. Check the HUD look. |
| Q-7 | Are texture sizes or polygon budgets mandatory? | No size checks exist apart from POT practicality (§1.6). | Keep the FREE-DATA-PACK F-4 reference sizes. |
| Q-8 | Should the port keep GL-style effects (depth test off) or D3D-style (Z-test on, Z-write off)? | The backends differ (d3dgraphics-opengl.cpp:2346 vs d3dgraphics-directx.cpp:1837). | Follow D3D. It affects how big or opaque smoke may be without bleeding through walls. |
| Q-9 | Are GL-backend muzzle and casing positions correct? | `GetWorldTransformPos` returns the modelview translation, which includes the camera in GL (d3dgraphics-opengl.cpp:1971-1981), unlike the D3D world matrix. | A porting bug to verify. The derived points in §6 follow the D3D semantics. |
| Q-10 | What should Japanese (CP932) mission names in `char.dds` text show? | Bytes 0xB0–0xDF index HUD cells (§2.1). | Keep the original behaviour, or map them to glyphs in the port. This is a port decision. |
| Q-11 | Which `ScopeGunsight` mode does each scope use, and does the scope texture need a reticle? | `Scope[]` values exist (parameter.cpp:1820ff), but whether the art should include a reticle is a design question. | Scope art without a reticle. The engine draws lines for modes 1 and 2. |
