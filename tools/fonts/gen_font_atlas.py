#!/usr/bin/env python3
"""Generate src/platform/text/font_atlas.inc from the public-domain Shinonome
bitmap font (shinonome-0.9.11), adding real JIS X 0208 glyph coverage
(hiragana, katakana, full-width symbols/kanji) and half-width katakana on top
of the existing hand-authored Latin-1 Supplement entries.

Source: the Debian "xfonts-shinonome" source package, which repackages the
upstream efont/Shinonome release (http://openlab.ring.gr.jp/efont/shinonome/)
unmodified. The Electronic Font Open Laboratory states all Shinonome font
data, documentation and scripts are Public Domain; see
assets/fonts/SHINONOME-LICENSE.txt for the verbatim license text and
assets/fonts/README.md for provenance notes.

This script downloads the pinned, checksummed upstream tarball (not
committed to the repo -- see assets/fonts/README.md), extracts two BDF files
in-memory:

  bdf/shnmk16.bdf   -- 16x16 JIS X 0208 (hiragana/katakana/symbols/kanji),
                       Public Domain, ENCODING = JIS kuten code (hex digits
                       of STARTCHAR interpreted as decimal), converted to
                       Unicode via EUC-JP (ENCODING + 0x8080, decode 'euc_jp').
  bdf/shnm8x16r.bdf -- 8x16 JIS X 0201 (ASCII + half-width katakana), Public
                       Domain. Only the half-width katakana range
                       (ENCODING 0xA1-0xDF) is used here; ASCII glyphs are
                       already hand-authored in the existing atlas and are
                       left untouched to avoid any behavior change there.

Glyph storage format (unchanged, matches the existing hand-authored entries):
  Each glyph is 16 rows x 2 bytes (32 bytes), MSB-first, one bit per pixel in
  a 16-wide cell. Half-width (8px) glyphs use only the first byte of each
  row (bits 0-7 from the left); the second byte is zero. Full-width (16px)
  glyphs use both bytes of each row. This matches src/platform/text/text.cpp
  text_bitmap()'s bit-addressing (gx = sx for full-width px=16, gx = sx for
  half-width cw=8), so no renderer change is needed.

Output is a sorted-by-codepoint `static const FontGlyph kFontGlyphs[]` C++
array (binary-search lookup in text.cpp), merging the new kana/kanji entries
with the existing Latin-1 entries already in font_atlas.inc.

Usage: python3 tools/fonts/gen_font_atlas.py [out.inc]
"""
import hashlib
import io
import os
import re
import sys
import tarfile
import urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DEFAULT = os.path.join(ROOT, "src", "platform", "text", "font_atlas.inc")
CACHE = os.path.join(ROOT, "tools", "fonts", ".cache", "xfonts-shinonome_0.9.11.orig.tar.gz")

# Mirrors of the exact same upstream-repackaged Debian source tarball.
URLS = [
    "http://deb.debian.org/debian/pool/main/x/xfonts-shinonome/xfonts-shinonome_0.9.11.orig.tar.gz",
    "https://archive.debian.org/debian/pool/main/x/xfonts-shinonome/xfonts-shinonome_0.9.11.orig.tar.gz",
]
SHA256 = "b5527bbd77a4d8df66c938015c3c85f75da488cfd4d3d9366f6563b090b32895"

KANJI_BDF = "xfonts-shinonome-0.9.11/bdf/shnmk16.bdf"
HALF_BDF = "xfonts-shinonome-0.9.11/bdf/shnm8x16r.bdf"


def fetch_tarball() -> bytes:
    if os.path.exists(CACHE):
        with open(CACHE, "rb") as f:
            data = f.read()
        if hashlib.sha256(data).hexdigest() == SHA256:
            return data
    last_err = None
    for url in URLS:
        try:
            with urllib.request.urlopen(url, timeout=60) as resp:
                data = resp.read()
        except Exception as e:  # noqa: BLE001
            last_err = e
            continue
        digest = hashlib.sha256(data).hexdigest()
        if digest != SHA256:
            last_err = RuntimeError(f"sha256 mismatch for {url}: got {digest}, want {SHA256}")
            continue
        os.makedirs(os.path.dirname(CACHE), exist_ok=True)
        with open(CACHE, "wb") as f:
            f.write(data)
        return data
    raise RuntimeError(f"could not fetch/verify shinonome tarball: {last_err}")


def parse_bdf(text: str):
    """Yield (encoding:int, width:int, height:int, rows:list[int]) per glyph.

    rows[i] is the raw BITMAP hex row (as an int, MSB-aligned to the font's
    declared glyph width) for scanline i, top to bottom.
    """
    lines = text.splitlines()
    i = 0
    n = len(lines)
    fbb_w = fbb_h = None
    while i < n:
        line = lines[i]
        if line.startswith("FONTBOUNDINGBOX"):
            parts = line.split()
            fbb_w, fbb_h = int(parts[1]), int(parts[2])
        if line.startswith("STARTCHAR"):
            encoding = None
            bbw, bbh = fbb_w, fbb_h
            i += 1
            while i < n and not lines[i].startswith("BITMAP"):
                if lines[i].startswith("ENCODING"):
                    encoding = int(lines[i].split()[1])
                elif lines[i].startswith("BBX"):
                    p = lines[i].split()
                    bbw, bbh = int(p[1]), int(p[2])
                i += 1
            # i at BITMAP
            i += 1
            rows = []
            while i < n and not lines[i].startswith("ENDCHAR"):
                rows.append(int(lines[i].strip(), 16))
                i += 1
            if encoding is not None and encoding >= 0:
                yield encoding, bbw, bbh, rows
        i += 1


def pack_rows(rows, width, height, nibble_hex_width):
    """Pack up to 16 rows of up to 16 columns into a 32-byte (16x2) cell,
    left-aligned, MSB-first, matching the existing hand-authored format."""
    out = bytearray(32)
    for r in range(min(16, height)):
        val = rows[r] if r < len(rows) else 0
        # BDF BITMAP hex rows are padded to a multiple of 8 bits (hex digit
        # pairs); shift so the glyph's actual leftmost pixel sits at bit 15.
        padded_bits = nibble_hex_width * 4
        val <<= (16 - padded_bits) if padded_bits < 16 else 0
        if padded_bits > 16:
            val >>= (padded_bits - 16)
        val &= 0xFFFF
        out[r * 2] = (val >> 8) & 0xFF
        out[r * 2 + 1] = val & 0xFF
    return bytes(out)


def jis_kuten_to_unicode(encoding: int):
    hi = (encoding >> 8) & 0xFF
    lo = encoding & 0xFF
    if not (0x21 <= hi <= 0x7E and 0x21 <= lo <= 0x7E):
        return None
    euc = bytes([hi | 0x80, lo | 0x80])
    try:
        s = euc.decode("euc_jp")
    except UnicodeDecodeError:
        return None
    if len(s) != 1:
        return None
    return ord(s)


def half_katakana_to_unicode(encoding: int):
    if 0xA1 <= encoding <= 0xDF:
        return 0xFF61 + (encoding - 0xA1)
    return None


def load_existing_latin1_entries(out_path: str):
    """Pull the existing hand-authored {codepoint:{bytes32}} entries out of
    a previously generated font_atlas.inc, so regeneration is additive and
    never drops the Latin-1 coverage."""
    entries = {}
    if not os.path.exists(out_path):
        return entries
    pat = re.compile(r"\{0x([0-9a-fA-F]{4}),\{([^}]*)\}\}")
    with open(out_path, "r", encoding="utf-8") as f:
        text = f.read()
    for m in pat.finditer(text):
        cp = int(m.group(1), 16)
        byte_strs = [b.strip() for b in m.group(2).split(",") if b.strip()]
        data = bytes(int(b, 16) for b in byte_strs)
        if len(data) == 32:
            entries[cp] = data
    return entries


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else OUT_DEFAULT
    existing = load_existing_latin1_entries(out_path)
    # Keep only the pre-existing Latin-1 range (U+0000-U+00FF); anything else
    # already in the file is prior generator output we are about to replace.
    latin1 = {cp: bits for cp, bits in existing.items() if cp <= 0xFF}

    tar_bytes = fetch_tarball()
    tf = tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r:gz")
    kanji_text = tf.extractfile(KANJI_BDF).read().decode("ascii", "replace")
    half_text = tf.extractfile(HALF_BDF).read().decode("ascii", "replace")

    glyphs = dict(latin1)  # codepoint -> 32 bytes

    # Full-width: hiragana, katakana, symbols, level-1+2 kanji (JIS X 0208).
    count_full = 0
    for encoding, bbw, bbh, rows in parse_bdf(kanji_text):
        cp = jis_kuten_to_unicode(encoding)
        if cp is None:
            continue
        hexw = max(1, (bbw + 3) // 4) if bbw else 4
        glyphs[cp] = pack_rows(rows, bbw or 16, bbh or 16, hexw)
        count_full += 1

    # Half-width katakana (JIS X 0201), mapped to the Unicode half-width
    # katakana block U+FF61-U+FF9F. ASCII range intentionally skipped: the
    # existing Latin-1 atlas already owns 0x00-0xFF and must stay unchanged.
    count_half = 0
    for encoding, bbw, bbh, rows in parse_bdf(half_text):
        cp = half_katakana_to_unicode(encoding)
        if cp is None:
            continue
        hexw = max(1, (bbw + 3) // 4) if bbw else 2
        glyphs[cp] = pack_rows(rows, bbw or 8, bbh or 16, hexw)
        count_half += 1

    ordered = sorted(glyphs.items())
    lines = [
        "// GENERATED by tools/fonts/gen_font_atlas.py. Do not hand-edit.",
        "// Shinonome public-domain bitmap glyphs (12/16 dot): Latin-1",
        "// Supplement (hand-authored) plus real JIS X 0208 hiragana/",
        "// katakana/full-width-symbol/kanji coverage and JIS X 0201",
        "// half-width katakana, both parsed from the upstream shinonome-",
        "// 0.9.11 BDF fonts. See assets/fonts/README.md and NOTICE.md for",
        "// provenance and license (Public Domain).",
        "// Codepoints absent from this table fall back to a visible",
        "// outline box in text_bitmap() rather than being silently dropped.",
        f"// {len(ordered)} glyphs total ({count_full} full-width kana/kanji,",
        f"// {count_half} half-width katakana, {len(latin1)} Latin-1).",
        "static const FontGlyph kFontGlyphs[] = {",
    ]
    for cp, data in ordered:
        hexed = ",".join(f"0x{b:02x}" for b in data)
        lines.append(f"  {{0x{cp:04x},{{{hexed}}}}},")
    lines.append("};")
    with open(out_path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {out_path}: {len(ordered)} glyphs "
          f"({count_full} full-width, {count_half} half-width katakana, {len(latin1)} Latin-1), "
          f"{os.path.getsize(out_path)} bytes")


if __name__ == "__main__":
    main()
