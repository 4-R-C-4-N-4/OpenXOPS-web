// Standalone harness for tools/tests/test_text_rendering.py. Exercises
// src/platform/text/{cp932_decode,text_measure,text_bitmap} directly with
// plain g++ (no Emscripten/SDL/GL needed) and prints one "KEY=VALUE" line
// per fact so the Python test can assert on them without parsing C++.
#include "../../src/platform/text/text.h"
#include <cstdio>
#include <cstring>

int main() {
  // ASCII round trip: single-byte decode must equal the input byte.
  {
    const unsigned char a = 'A';
    uint16_t cp = 0;
    size_t used = 0;
    bool ok = cp932_decode(&a, 1, &cp, &used);
    printf("ascii_ok=%d ascii_cp=%u ascii_used=%zu\n", ok ? 1 : 0, cp, used);
  }

  // Known CP932 double-byte sequences -> expected Unicode codepoints.
  // 0x82 0xA0 = full-width hiragana "あ" (U+3042)
  // 0x82 0xA2 = full-width hiragana "い" (U+3044)
  // 0x93 0xFA = kanji "日" (U+65E5)
  {
    const unsigned char a[] = {0x82, 0xA0};
    const unsigned char i[] = {0x82, 0xA2};
    const unsigned char hi[] = {0x93, 0xFA};
    uint16_t cp;
    size_t used;
    cp932_decode(a, 2, &cp, &used);
    printf("hiragana_a_cp=0x%04x hiragana_a_used=%zu\n", cp, used);
    cp932_decode(i, 2, &cp, &used);
    printf("hiragana_i_cp=0x%04x hiragana_i_used=%zu\n", cp, used);
    cp932_decode(hi, 2, &cp, &used);
    printf("kanji_hi_cp=0x%04x kanji_hi_used=%zu\n", cp, used);
  }

  // Truncated lead byte (buffer ends right after a valid CP932 lead byte):
  // must fail, and must consume exactly the 1 remaining byte, never read OOB.
  {
    const unsigned char trunc[] = {0x82};
    uint16_t cp;
    size_t used;
    bool ok = cp932_decode(trunc, 1, &cp, &used);
    printf("truncated_ok=%d truncated_used=%zu truncated_cp=0x%04x\n", ok ? 1 : 0, used, cp);
  }

  // Malformed trail byte (lead byte followed by a byte outside 0x40-0xFC):
  // must fail and consume only the lead byte so the caller resyncs cleanly.
  {
    const unsigned char bad[] = {0x82, 0x01, 'X'};
    uint16_t cp;
    size_t used;
    bool ok = cp932_decode(bad, 3, &cp, &used);
    printf("malformed_ok=%d malformed_used=%zu\n", ok ? 1 : 0, used);
  }

  // Empty input: must fail and consume 0 bytes.
  {
    uint16_t cp;
    size_t used;
    bool ok = cp932_decode(nullptr, 0, &cp, &used);
    printf("empty_ok=%d empty_used=%zu\n", ok ? 1 : 0, used);
  }

  // text_measure: half-width-only, full-width-only, and mixed widths.
  {
    int w, h;
    text_measure("ABC", 16, &w, &h);
    printf("measure_half_w=%d measure_half_h=%d\n", w, h);
    // "あい" = two full-width hiragana, 0x82 0xA0 0x82 0xA2
    const char full[] = {(char)0x82, (char)0xA0, (char)0x82, (char)0xA2, 0};
    text_measure(full, 16, &w, &h);
    printf("measure_full_w=%d measure_full_h=%d\n", w, h);
    // "A" + full-width "あ" + newline + "BC"
    const char mixed[] = {'A', (char)0x82, (char)0xA0, '\n', 'B', 'C', 0};
    text_measure(mixed, 16, &w, &h);
    printf("measure_mixed_w=%d measure_mixed_h=%d\n", w, h);
  }

  // text_bitmap: sanity on size and at least one opaque pixel for a glyph
  // known to be in the compact atlas (ASCII 'A' always falls back to the
  // outline-box path if missing, so it is guaranteed non-blank either way).
  {
    std::vector<unsigned char> rgba;
    int w, h;
    bool ok = text_bitmap("A", 16, rgba, &w, &h);
    size_t opaque = 0;
    for (size_t k = 3; k < rgba.size(); k += 4) {
      if (rgba[k] != 0) opaque++;
    }
    printf("bitmap_ok=%d bitmap_w=%d bitmap_h=%d bitmap_size=%zu bitmap_opaque=%zu\n",
           ok ? 1 : 0, w, h, rgba.size(), opaque);
  }

  // Real kana/kanji glyph coverage (not outline-box placeholders): "あ",
  // "ア", "日本" (two kanji), a level-2 kanji "鬱" (U+9B31, CP932 0xEA,0x9D),
  // and full-width "！" (U+FF01, CP932 0x81,0x49) must each render at least
  // one opaque pixel AND not match the generic outline-box pattern (which
  // only ever lights up the border row/column of the glyph cell).
  {
    auto opaque_count = [](const char* bytes) {
      std::vector<unsigned char> rgba;
      int w, h;
      text_bitmap(bytes, 16, rgba, &w, &h);
      size_t n = 0;
      for (size_t k = 3; k < rgba.size(); k += 4) if (rgba[k] != 0) n++;
      return n;
    };
    auto interior_opaque = [](const char* bytes) {
      // An outline-box placeholder only paints the 1px border; a real glyph
      // paints interior pixels too. Count opaque pixels strictly inside the
      // 16x16 cell border.
      std::vector<unsigned char> rgba;
      int w, h;
      text_bitmap(bytes, 16, rgba, &w, &h);
      size_t n = 0;
      for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
          size_t k = ((size_t)y * w + x) * 4 + 3;
          if (rgba[k] != 0) n++;
        }
      }
      return n;
    };
    const char hiragana_a[] = {(char)0x82, (char)0xa0, 0};        // あ
    const char katakana_a[] = {(char)0x83, (char)0x41, 0};        // ア
    const char nihon[] = {(char)0x93, (char)0xfa, (char)0x96, (char)0x7b, 0};  // 日本
    const char level2_kanji[] = {(char)0xea, (char)0x9d, 0};      // 鬱 (U+9B31)
    const char fullwidth_bang[] = {(char)0x81, (char)0x49, 0};    // ！(U+FF01)
    printf("glyph_hiragana_a_opaque=%zu glyph_hiragana_a_interior=%zu\n",
           opaque_count(hiragana_a), interior_opaque(hiragana_a));
    printf("glyph_katakana_a_opaque=%zu glyph_katakana_a_interior=%zu\n",
           opaque_count(katakana_a), interior_opaque(katakana_a));
    printf("glyph_nihon_opaque=%zu glyph_nihon_interior=%zu\n",
           opaque_count(nihon), interior_opaque(nihon));
    printf("glyph_level2_kanji_opaque=%zu glyph_level2_kanji_interior=%zu\n",
           opaque_count(level2_kanji), interior_opaque(level2_kanji));
    printf("glyph_fullwidth_bang_opaque=%zu glyph_fullwidth_bang_interior=%zu\n",
           opaque_count(fullwidth_bang), interior_opaque(fullwidth_bang));
  }

  // text_bitmap/text_measure invalid-argument handling.
  {
    int w, h;
    bool ok1 = text_measure(nullptr, 16, &w, &h);
    bool ok2 = text_measure("x", 13, &w, &h);
    printf("invalid_null=%d invalid_px=%d\n", ok1 ? 1 : 0, ok2 ? 1 : 0);
  }

  return 0;
}
