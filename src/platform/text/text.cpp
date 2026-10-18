#include "text.h"
#include <algorithm>
#include <cstring>
#include <limits>

// CP932->Unicode decode table and Shinonome 12/16 public-domain bitmap glyph
// atlas. See assets/fonts/README.md and NOTICE.md for provenance/license.
namespace {
#include "cp932_table.inc"
struct FontGlyph { uint16_t code; uint8_t bits[32]; };
#include "font_atlas.inc"
const FontGlyph* find_glyph(uint16_t cp) {
  auto it = std::lower_bound(std::begin(kFontGlyphs), std::end(kFontGlyphs), cp,
      [](const FontGlyph& g, uint16_t c) { return g.code < c; });
  return it != std::end(kFontGlyphs) && it->code == cp ? it : nullptr;
}
}  // namespace

bool cp932_decode(const unsigned char* in, size_t n, uint16_t* cp, size_t* used) {
  if (!cp || !used || (!in && n)) return false;
  *cp = 0xfffd;
  *used = n ? 1 : 0;
  if (!n) return false;
  const uint16_t one = kCp932Single[in[0]];
  if (one != 0xffff) {
    if (one != 0xfffd) *cp = one;
    return one != 0xfffd;
  }
  // in[0] is a double-byte lead. If the string is truncated right after the
  // lead byte, consume just that byte (already set above) and report failure.
  if (n < 2) return false;
  const unsigned char t = in[1];
  if (t < 0x40 || t > 0xfc || t == 0x7f) {
    // Malformed trail byte: consume only the lead byte so the caller can
    // keep resynchronising on the next byte instead of skipping valid data.
    return false;
  }
  const int li = in[0] <= 0x9f ? in[0] - 0x81 : in[0] - 0xe0 + 31;
  const uint16_t val = kCp932Double[li * 189 + (t - 0x40)];
  if (!val) return false;
  *cp = val;
  *used = 2;
  return true;
}

namespace {
// Shared scan: walks the CP932 byte string, invoking cb(cp, is_wide) for each
// decoded codepoint (or U+FFFD for invalid bytes) and tracking line layout.
template <typename Cb>
void scan_text(const char* bytes, int px, int* out_cols, int* out_lines, Cb cb) {
  const size_t len = std::strlen(bytes);
  const int cw = px / 2;
  size_t i = 0;
  int cols = 0, lines = 1, x = 0;
  while (i < len) {
    uint16_t cp;
    size_t used;
    cp932_decode(reinterpret_cast<const unsigned char*>(bytes) + i, len - i, &cp, &used);
    i += used ? used : 1;
    if (cp == '\n') {
      cols = std::max(cols, x);
      x = 0;
      ++lines;
      continue;
    }
    const bool wide = cp >= 0x100;
    cb(cp, wide, x, lines - 1);
    x += wide ? px : cw;
  }
  cols = std::max(cols, x);
  if (out_cols) *out_cols = cols;
  if (out_lines) *out_lines = lines;
}
}  // namespace

bool text_measure(const char* bytes, int px, int* width, int* height) {
  if (!bytes || !width || !height || (px != 12 && px != 16)) return false;
  int cols = 0, lines = 1;
  scan_text(bytes, px, &cols, &lines, [](uint16_t, bool, int, int) {});
  *width = cols;
  *height = lines * px;
  return true;
}

bool text_bitmap(const char* bytes, int px, std::vector<unsigned char>& rgba, int* width, int* height) {
  if (!bytes || !width || !height || (px != 12 && px != 16)) return false;
  *width = 0;
  *height = 0;
  rgba.clear();
  int cols = 0, lines = 1;
  scan_text(bytes, px, &cols, &lines, [](uint16_t, bool, int, int) {});
  const long long ww = cols, hh = (long long)lines * px;
  if (ww <= 0 || hh <= 0 || ww > std::numeric_limits<int>::max() ||
      hh > std::numeric_limits<int>::max() || ww * hh > 100000000) {
    return false;
  }
  *width = (int)ww;
  *height = (int)hh;
  rgba.assign((size_t)*width * (*height) * 4, 0);
  const int cw = px / 2;
  scan_text(bytes, px, nullptr, nullptr, [&](uint16_t cp, bool wide, int x, int row) {
    const int gw = wide ? px : cw;
    const int y = row * px;
    const FontGlyph* g = find_glyph(cp);
    for (int sy = 0; sy < px; sy++) {
      for (int sx = 0; sx < gw; sx++) {
        bool on = false;
        if (g) {
          const int gx = wide ? sx * 16 / px : sx * 8 / cw;
          const int gy = sy * 16 / px;
          on = (g->bits[gy * 2 + gx / 8] & (0x80 >> (gx & 7))) != 0;
        } else if (cp != ' ' && cp != '\t') {
          // Unknown codepoint: draw a visible outline box instead of dropping
          // the character silently.
          on = (sx == 0 || sy == 0 || sx == gw - 1 || sy == px - 1);
        }
        if (on) {
          const size_t k = ((size_t)(y + sy) * (*width) + x + sx) * 4;
          rgba[k] = rgba[k + 1] = rgba[k + 2] = 255;
          rgba[k + 3] = 255;
        }
      }
    }
  });
  return true;
}
