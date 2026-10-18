# Bitmap text assets

`src/platform/text/font_atlas.inc` is a glyph bitmap atlas in the Shinonome
public-domain bitmap font style (12/16 dot). The Electronic Font Open
Laboratory (efont) states the original Shinonome font data, documentation and
scripts are Public Domain; the Japanese license text is included verbatim as
`SHINONOME-LICENSE.txt` (source: https://github.com/code4fukui/shinonome-font).

**Current coverage**: this atlas presently covers Latin-1 Supplement
(U+0000-U+00FF: ASCII, Latin accented letters, common symbols) only — it does
not yet include hiragana, katakana or kanji glyphs. CP932 decoding
(`cp932_table.inc`, `tools/fonts/gen_cp932.py`) is complete and correct for
the full Windows-31J repertoire; any codepoint without a matching atlas entry
renders as a visible outline box (see `text_bitmap()` in
`src/platform/text/text.cpp`) instead of being dropped or rendered as a
generic `?`, so missing coverage is visible and the output is never
misleading. Extending the atlas with the Shinonome kana/kanji glyph bitmaps
(from the upstream repo's `16/kanjic/font_src.bit` and `16/hankaku`
directories) is tracked as follow-up work; see `docs/TEXT-RENDERING.md`.

The CP932 mapping table is generated via Python's `cp932` codec by
`tools/fonts/gen_cp932.py` (re-run it with no arguments to regenerate
`cp932_table.inc` from the same repo root).
