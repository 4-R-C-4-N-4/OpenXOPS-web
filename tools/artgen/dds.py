"""Uncompressed DDS writer + an engine-mimicking reader (stdlib only).

Contract: docs/ASSET-CONTRACT.md section 1.3 (LoadDDSTexture,
d3dgraphics-opengl.cpp:1228-1306 at upstream-1.080):

- 128-byte header ("DDS " + 124). Height @0x0C, width @0x10, flags @0x08,
  dwRGBBitCount @0x58, "Caps" @0x70 (really dwCaps2).
- Only bit count 16 or 32 is accepted; masks and FourCC are ignored.
- 16 bpp is always A4R4G4B4: byte0 = G<<4|B, byte1 = A<<4|R; each nibble is
  expanded as nibble<<4 (0..240, never 255).
- 32 bpp is always A8R8G8B8, stored B,G,R,A.
- Pixels are read top-down from byte 128 with no pitch padding.
- If DDSD_MIPMAPCOUNT (flags 0x20000) and 0x400000 in dwCaps2 are both set,
  128 bytes of pixel data are skipped (a loader bug). We never set either.

The writer emits a fully standard header (DDPF_RGB|DDPF_ALPHAPIXELS with the
real masks, DDSCAPS_TEXTURE, no mipmaps), so other tools read it the same way.
"""
import struct

DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PITCH = 0x1, 0x2, 0x4, 0x8
DDSD_PIXELFORMAT, DDSD_MIPMAPCOUNT = 0x1000, 0x20000
DDPF_ALPHAPIXELS, DDPF_FOURCC, DDPF_RGB = 0x1, 0x4, 0x40
DDSCAPS_TEXTURE = 0x1000

FORMATS = {
    # name: (bits, rmask, gmask, bmask, amask)
    "dds_argb8": (32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000),
    "dds_argb4": (16, 0x0F00, 0x00F0, 0x000F, 0xF000),
}


def header(w, h, fmt):
    bits, rm, gm, bm, am = FORMATS[fmt]
    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT
    hd = struct.pack("<4sIIIIIII", b"DDS ", 124, flags, h, w, w * bits // 8, 0, 0)
    hd += b"\0" * 44                                   # dwReserved1[11]
    hd += struct.pack("<II4sIIIII", 32, DDPF_RGB | DDPF_ALPHAPIXELS, b"\0\0\0\0",
                      bits, rm, gm, bm, am)            # DDS_PIXELFORMAT @0x4C
    hd += struct.pack("<IIIII", DDSCAPS_TEXTURE, 0, 0, 0, 0)  # caps, caps2..4, reserved2
    assert len(hd) == 128
    return hd


def _n4(v):
    """8-bit -> 4-bit nibble that decodes (n<<4) closest to v."""
    return min(15, (v + 8) >> 4)


def encode(rgba_bytes, w, h, fmt):
    """rgba_bytes: top-down R,G,B,A (Pillow 'RGBA'.tobytes())."""
    assert len(rgba_bytes) == w * h * 4
    if fmt == "dds_argb8":
        out = bytearray(len(rgba_bytes))
        out[0::4] = rgba_bytes[2::4]  # B
        out[1::4] = rgba_bytes[1::4]  # G
        out[2::4] = rgba_bytes[0::4]  # R
        out[3::4] = rgba_bytes[3::4]  # A
        return bytes(out)
    if fmt == "dds_argb4":
        out = bytearray(w * h * 2)
        for k in range(w * h):
            r, g, b, a = rgba_bytes[4 * k:4 * k + 4]
            out[2 * k] = (_n4(g) << 4) | _n4(b)
            out[2 * k + 1] = (_n4(a) << 4) | _n4(r)
        return bytes(out)
    raise ValueError("unknown DDS format %r" % fmt)


def write_dds(img, path, fmt="dds_argb8"):
    """Write a Pillow image (converted to RGBA) as uncompressed DDS."""
    im = img.convert("RGBA")
    w, h = im.size
    data = header(w, h, fmt) + encode(im.tobytes(), w, h, fmt)
    with open(path, "wb") as f:
        f.write(data)
    return len(data)


def engine_decode(data):
    """Decode DDS bytes exactly as LoadDDSTexture does.

    Returns (width, height, rgba_bytes) or raises ValueError where the engine
    would return false. BlackTransparent is never passed (contract 1.4).
    """
    if len(data) < 128:
        raise ValueError("short header")
    hd = data[:128]
    if hd[:4] != b"DDS ":
        raise ValueError("not DDS")
    width = struct.unpack_from("<i", hd, 0x10)[0]
    height = struct.unpack_from("<i", hd, 0x0C)[0]
    index = struct.unpack_from("<i", hd, 0x58)[0]
    flag = struct.unpack_from("<i", hd, 0x08)[0]
    caps = struct.unpack_from("<i", hd, 0x70)[0]
    if width <= 0 or height <= 0:
        raise ValueError("non-positive dimensions")
    if index not in (16, 32):
        raise ValueError("bit count %d rejected by loader" % index)
    pos = 128
    if (flag & 0x00020000) and (caps & 0x00400000):
        pos += 128  # loader bug: skips 128 bytes of pixel data
    bpp = index // 8
    need = pos + width * height * bpp
    if len(data) < need:
        raise ValueError("truncated pixel data (%d < %d)" % (len(data), need))
    out = bytearray(width * height * 4)
    for k in range(width * height):
        p = data[pos + k * bpp:pos + (k + 1) * bpp]
        if index == 16:
            out[4 * k + 0] = (p[1] << 4) & 0xF0
            out[4 * k + 1] = p[0] & 0xF0
            out[4 * k + 2] = (p[0] << 4) & 0xF0
            out[4 * k + 3] = p[1] & 0xF0
        else:
            out[4 * k + 0] = p[2]
            out[4 * k + 1] = p[1]
            out[4 * k + 2] = p[0]
            out[4 * k + 3] = p[3]
    return width, height, bytes(out)


def engine_load(path):
    with open(path, "rb") as f:
        return engine_decode(f.read())


def info(data):
    """Header fields as a dict (for verification / reports)."""
    h, w = struct.unpack_from("<II", data, 0x0C)
    flags = struct.unpack_from("<I", data, 0x08)[0]
    pf_size, pf_flags, fourcc, bits, rm, gm, bm, am = struct.unpack_from("<II4sIIIII", data, 0x4C)
    caps, caps2 = struct.unpack_from("<II", data, 0x6C)
    mips = struct.unpack_from("<I", data, 0x1C)[0]
    return {"magic": data[:4], "size": struct.unpack_from("<I", data, 4)[0], "flags": flags,
            "width": w, "height": h, "pf_size": pf_size, "pf_flags": pf_flags,
            "fourcc": fourcc, "bits": bits, "masks": (rm, gm, bm, am),
            "caps": caps, "caps2": caps2, "mipmaps": mips, "bytes": len(data)}


def verify_dds(path, size=None, fmt=None):
    """Return a list of problems (empty == OK) against the section 1.3 contract."""
    with open(path, "rb") as f:
        data = f.read()
    probs = []
    if data[:4] != b"DDS ":
        return ["not DDS"]
    if len(data) < 128:
        return ["short header"]
    i = info(data)
    if not i["width"] or not i["height"]:
        return ["empty dimensions"]
    if i["size"] != 124 or i["pf_size"] != 32:
        probs.append("header sizes %d/%d" % (i["size"], i["pf_size"]))
    if i["pf_flags"] & DDPF_FOURCC:
        probs.append("FourCC set")
    if not (i["pf_flags"] & DDPF_ALPHAPIXELS):
        probs.append("DDPF_ALPHAPIXELS not set")
    want = fmt or {32: "dds_argb8", 16: "dds_argb4"}.get(i["bits"])
    if want not in FORMATS:
        probs.append("bit count %d" % i["bits"])
        return probs
    bits, rm, gm, bm, am = FORMATS[want]
    if i["bits"] != bits:
        probs.append("bits %d != %d" % (i["bits"], bits))
    if i["masks"] != (rm, gm, bm, am):
        probs.append("masks %r" % (i["masks"],))
    if (i["flags"] & DDSD_MIPMAPCOUNT) or i["mipmaps"] > 1 or (i["caps2"] & 0x400000):
        probs.append("mipmaps present")
    if i["bytes"] != 128 + i["width"] * i["height"] * bits // 8:
        probs.append("file %d bytes, expected %d" % (i["bytes"], 128 + i["width"] * i["height"] * bits // 8))
    if size and (i["width"], i["height"]) != tuple(size):
        probs.append("dims %dx%d != %dx%d" % (i["width"], i["height"], size[0], size[1]))
    try:
        w, h, px = engine_decode(data)
        alphas = px[3::4]
        if max(alphas) == 0:
            probs.append("alpha channel empty: engine draws nothing")
    except ValueError as e:
        probs.append("engine decode: %s" % e)
    return probs
