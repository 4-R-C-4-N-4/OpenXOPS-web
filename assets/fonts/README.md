# Bitmap text assets

`src/platform/text/font_atlas.inc` is a glyph bitmap atlas built from the
Shinonome public-domain bitmap font family (12/16 dot). The Electronic Font
Open Laboratory (efont) states the original Shinonome font data,
documentation and scripts are Public Domain; the Japanese license text is
included verbatim as `SHINONOME-LICENSE.txt` (matches `LICENSE` in the
upstream source archive below).

## Coverage

The atlas now covers:

- Latin-1 Supplement (U+0000-U+00FF: ASCII punctuation, Latin accented
  letters, common symbols) — hand-authored, unchanged by the generator.
- Hiragana, katakana, full-width symbols and JIS X 0208 level-1 + level-2
  kanji (~6,900 codepoints), parsed from the real Shinonome 16x16 `shnmk16.bdf`
  glyph bitmaps.
- Half-width katakana (U+FF61-U+FF9F), parsed from the real Shinonome 8x16
  `shnm8x16r.bdf` glyph bitmaps.

CP932 decoding (`cp932_table.inc`, `tools/fonts/gen_cp932.py`) is complete
and correct for the full Windows-31J repertoire; any codepoint still without
a matching atlas entry renders as a visible outline box (see `text_bitmap()`
in `src/platform/text/text.cpp`) instead of being dropped or rendered as a
generic `?`, so any remaining gap stays visible rather than misleading.

## Regenerating `font_atlas.inc`

`tools/fonts/gen_font_atlas.py` downloads the pinned, checksummed upstream
Shinonome source tarball, parses the BDF glyph data for the two fonts above,
and emits a sorted `kFontGlyphs[]` table (binary-searched by codepoint in
`text.cpp`), merging it with the existing hand-authored Latin-1 entries. Run
it with no arguments from the repo root to regenerate `font_atlas.inc`.

**Source**: the Debian `xfonts-shinonome` source package, which repackages
the upstream efont/Shinonome release (`shinonome-0.9.11`,
http://openlab.ring.gr.jp/efont/shinonome/) unmodified.

- URL: `http://deb.debian.org/debian/pool/main/x/xfonts-shinonome/xfonts-shinonome_0.9.11.orig.tar.gz`
  (mirror: `https://archive.debian.org/debian/pool/main/x/xfonts-shinonome/xfonts-shinonome_0.9.11.orig.tar.gz`)
- SHA-256: `b5527bbd77a4d8df66c938015c3c85f75da488cfd4d3d9366f6563b090b32895`
- License: Public Domain (`LICENSE` in the tarball == `SHINONOME-LICENSE.txt`
  here, verbatim).
- BDF files used: `bdf/shnmk16.bdf` (16x16, JIS X 0208) and
  `bdf/shnm8x16r.bdf` (8x16, JIS X 0201 — half-width katakana range only;
  its ASCII glyphs are intentionally not used, to leave the existing
  hand-authored Latin-1 entries untouched).

The tarball itself is **not** committed to this repository (it's ~36MB
uncompressed and the generator re-downloads/verifies it by checksum); only
the generator script and its generated output (`font_atlas.inc`) are
tracked.

The CP932 mapping table is generated separately via Python's `cp932` codec
by `tools/fonts/gen_cp932.py` (re-run it with no arguments to regenerate
`cp932_table.inc` from the same repo root).
