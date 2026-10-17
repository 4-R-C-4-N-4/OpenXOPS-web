#!/usr/bin/env python3
"""Inspect rendered SFX: header check, level stats, spectrogram sheet (stdlib + Pillow).

Usage:
    python3 tools/sfxgen/analyze.py check  <wav ...>     # CheckSoundFile/LoadSound-style parse
    python3 tools/sfxgen/analyze.py stats  <wav ...>     # duration, peak dBFS, RMS dBFS
    python3 tools/sfxgen/analyze.py sheet  OUT.png <wav ...>   # combined log-frequency spectrograms

`check` mirrors SoundControl::CheckSoundFile (upstream sound-directsound.cpp):
the first chunk must be RIFF/WAVE; the `fmt ` and `data` sub-chunks are found
by scanning (any order, unknown chunks like LIST/fact skipped, odd sizes
padded); wFormatTag must be PCM(1)/IEEE_FLOAT(3) and channels 1 or 2; then
LoadSound needs 8/16/24/32 bits. It also asserts the pack contract: PCM, mono,
16-bit, 22050 Hz and the canonical 44-byte header.
"""
import cmath
import math
import os
import struct
import sys


def parse_wav(path):
    with open(path, "rb") as f:
        d = f.read()
    if len(d) < 12 or d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise ValueError("not RIFF/WAVE")
    riff_end = min(len(d), 8 + struct.unpack_from("<I", d, 4)[0])
    i, fmt, data = 12, None, None
    while i + 8 <= riff_end:
        cid = d[i:i + 4]
        size = struct.unpack_from("<I", d, i + 4)[0]
        body = d[i + 8:i + 8 + size]
        if cid == b"fmt " and fmt is None:
            if len(body) < 16:          # sizeof(PCMWAVEFORMAT)
                raise ValueError("short fmt chunk")
            tag, ch, rate, bps, align, bits = struct.unpack_from("<HHIIHH", body, 0)
            fmt = dict(tag=tag, channels=ch, rate=rate, byterate=bps, align=align, bits=bits, offset=i)
        elif cid == b"data" and data is None:
            data = dict(offset=i + 8, size=size)
        i += 8 + size + (size & 1)
    if fmt is None:
        raise ValueError("no fmt chunk")
    if fmt["tag"] not in (1, 3):
        raise ValueError(f"wFormatTag {fmt['tag']:#x} rejected (PCM/IEEE_FLOAT only)")
    if fmt["channels"] not in (1, 2):
        raise ValueError(f"{fmt['channels']} channels rejected")
    if data is None:
        raise ValueError("no data chunk")
    if fmt["bits"] not in (8, 16, 24, 32):
        raise ValueError(f"{fmt['bits']} bits rejected by LoadSound")
    return d, fmt, data


def read_samples(path):
    d, fmt, data = parse_wav(path)
    assert fmt["tag"] == 1 and fmt["bits"] == 16 and fmt["channels"] == 1
    n = data["size"] // 2
    s = struct.unpack_from("<%dh" % n, d, data["offset"])
    return [v / 32768.0 for v in s], fmt["rate"]


def cmd_check(paths):
    bad = 0
    for p in paths:
        try:
            d, fmt, data = parse_wav(p)
            problems = []
            if fmt["tag"] != 1:
                problems.append("not PCM")
            if fmt["channels"] != 1:
                problems.append("not mono")
            if fmt["rate"] != 22050:
                problems.append("rate %d" % fmt["rate"])
            if fmt["bits"] != 16:
                problems.append("%d-bit" % fmt["bits"])
            if fmt["byterate"] != fmt["rate"] * fmt["align"] or fmt["align"] != fmt["channels"] * fmt["bits"] // 8:
                problems.append("inconsistent byterate/align")
            if fmt["offset"] != 12 or data["offset"] != 44:
                problems.append("not canonical 44-byte header")
            if data["offset"] + data["size"] > len(d):
                problems.append("data chunk truncated")
            if struct.unpack_from("<I", d, 4)[0] != len(d) - 8:
                problems.append("RIFF size mismatch")
            status = "OK" if not problems else "BAD (" + ", ".join(problems) + ")"
        except ValueError as e:
            status = "REJECTED (%s)" % e
        bad += status != "OK"
        print(f"{os.path.basename(p):10s} {status}")
    return 1 if bad else 0


def cmd_stats(paths):
    print(f"{'file':10s} {'dur_s':>6s} {'peak_dBFS':>9s} {'rms_dBFS':>8s}")
    for p in paths:
        x, rate = read_samples(p)
        pk = max(abs(v) for v in x)
        rms = math.sqrt(sum(v * v for v in x) / len(x))
        print(f"{os.path.basename(p):10s} {len(x) / rate:6.3f} {20 * math.log10(pk):9.2f} {20 * math.log10(rms):8.2f}")
    return 0


# --------------------------------------------------------------------------
# Spectrogram
# --------------------------------------------------------------------------

def fft(a):
    n = len(a)
    if n == 1:
        return list(a)
    ev, od = fft(a[0::2]), fft(a[1::2])
    out = [0j] * n
    for k in range(n // 2):
        t = cmath.exp(-2j * math.pi * k / n) * od[k]
        out[k] = ev[k] + t
        out[k + n // 2] = ev[k] - t
    return out


def spectrogram(x, rate, nfft=1024, hop=96):
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / nfft) for i in range(nfft)]
    pad = [0.0] * (nfft // 2)
    y = pad + x + pad
    frames = []
    for s in range(0, len(y) - nfft + 1, hop):
        seg = [y[s + i] * win[i] for i in range(nfft)]
        sp = fft(seg)[:nfft // 2 + 1]
        frames.append([20 * math.log10(abs(c) / (nfft / 4) + 1e-9) for c in sp])
    return frames


def colormap(v):
    """0..1 -> (r,g,b), 'inferno'-like."""
    stops = [(0.0, (0, 0, 4)), (0.25, (87, 16, 110)), (0.5, (188, 55, 84)),
             (0.75, (249, 142, 9)), (1.0, (252, 255, 164))]
    v = min(1.0, max(0.0, v))
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        if v <= b:
            t = (v - a) / (b - a)
            return tuple(int(ca[k] + (cb[k] - ca[k]) * t) for k in range(3))
    return stops[-1][1]


def cmd_sheet(out, paths, fmin=30.0, fmax=11025.0, height=220, px_per_s=600, floor_db=-90.0):
    from PIL import Image, ImageDraw

    panels = []
    for p in paths:
        x, rate = read_samples(p)
        hop = 96
        frames = spectrogram(x, rate, hop=hop)
        nb = len(frames[0])
        w = max(1, int(len(x) / rate * px_per_s))
        img = Image.new("RGB", (w, height))
        px = img.load()
        for col in range(w):
            fr = frames[min(len(frames) - 1, int(col / w * len(frames)))]
            for row in range(height):
                f = fmin * (fmax / fmin) ** (1 - row / (height - 1))       # log axis, top = fmax
                b = f / (rate / 2) * (nb - 1)
                b0 = int(b)
                b1 = min(nb - 1, b0 + 1)
                v = fr[b0] + (fr[b1] - fr[b0]) * (b - b0)
                px[col, row] = colormap((v - floor_db) / (-floor_db))
        # waveform strip
        wf = Image.new("RGB", (w, 60), (20, 20, 20))
        dr = ImageDraw.Draw(wf)
        spp = len(x) / w
        for col in range(w):
            seg = x[int(col * spp):max(int(col * spp) + 1, int((col + 1) * spp))]
            lo, hi = min(seg), max(seg)
            dr.line([(col, 30 - hi * 29), (col, 30 - lo * 29)], fill=(120, 200, 255))
        panels.append((os.path.basename(p), img, wf, len(x) / rate))

    label_w, gap = 70, 8
    W = label_w + max(pi.width for _, pi, _, _ in panels) + 20
    H = sum(height + 60 + gap + 4 for _ in panels) + 30
    sheet = Image.new("RGB", (W, H), (0, 0, 0))
    dr = ImageDraw.Draw(sheet)
    dr.text((4, 4), f"log-freq {fmin:.0f} Hz - {fmax / 1000:.1f} kHz, {floor_db:.0f}..0 dB, "
                    f"{px_per_s} px/s, ticks every 100 ms", fill=(255, 255, 255))
    y = 24
    for name, img, wf, dur in panels:
        sheet.paste(img, (label_w, y))
        sheet.paste(wf, (label_w, y + height))
        dr.text((4, y + 4), name, fill=(255, 255, 255))
        dr.text((4, y + 20), f"{dur:.2f} s", fill=(180, 180, 180))
        for f in (100, 200, 1000, 5000):
            row = int((1 - math.log(f / fmin) / math.log(fmax / fmin)) * (height - 1))
            dr.line([(label_w - 8, y + row), (label_w - 1, y + row)], fill=(200, 200, 200))
            dr.text((label_w - 40, y + row - 6), f"{f // 1000}k" if f >= 1000 else str(f), fill=(200, 200, 200))
        t = 0.0
        while t <= dur:
            xx = label_w + int(t * px_per_s)
            dr.line([(xx, y + height + 60), (xx, y + height + 64)], fill=(200, 200, 200))
            t += 0.1
        y += height + 60 + gap + 4
    sheet.save(out)
    print(out)
    return 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "check":
        return cmd_check(argv[1:])
    if cmd == "stats":
        return cmd_stats(argv[1:])
    if cmd == "sheet":
        return cmd_sheet(argv[1], argv[2:])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
