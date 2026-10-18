#!/usr/bin/env python3
"""Generate the four P0 effect billboards; never reads original game data.

Contract: docs/ASSET-CONTRACT.md section 3.1 (RenderBoard billboards).
All four are drawn as a 1x1 quad with normal alpha blending (no additive, no
black colour key) and spin in random 10-degree steps on every spawn, so the
art must be roll-invariant: radial, no up direction, no text. Reference shape
is 128x128 A8R8G8B8 (assets/manifest.json: ref_size [128,128],
ref_format dds_argb8, alpha "channel").

- mflash: bright yellow-white flare, soft radial falloff, alpha 0 at border.
- smoke: soft grey puff, radial falloff with mild noise.
- blood: dark red splatter blob, soft alpha falloff.
- yakkyou: small brass cartridge-case sprite on a transparent field (the
  effect is spun by the engine itself, so the sprite need not be symmetric).

Deterministic: fixed RNG seeds, no network. Re-running reproduces the same
bytes (tools/tests/test_effects.py::test_determinism).
"""
import datetime
import json
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

import artgen
from dds import engine_load, write_dds

SIZE = 128
CENTER = (SIZE - 1) / 2.0
MAXR = SIZE / 2.0  # alpha reaches 0 at/after this radius -> border is 0


def _radial_alpha(x, y, r_falloff, power=1.0):
    """0..255 alpha, 255 at centre, 0 at radius MAXR (border), smooth falloff."""
    dx, dy = x - CENTER, y - CENTER
    d = math.hypot(dx, dy) / r_falloff
    if d >= 1.0:
        return 0
    return int(round(255 * (1.0 - d) ** power))


def mflash_image():
    """Bright yellow-white star/flare, soft radial falloff, roll-invariant."""
    im = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    px = im.load()
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - CENTER, y - CENTER
            d = math.hypot(dx, dy)
            ang = math.atan2(dy, dx)
            # 12-fold symmetric spikes so no single roll angle stands out.
            spike = 0.22 * (abs(math.cos(6 * ang)) ** 6)
            r_eff = (MAXR - 2) * (1.0 - spike)
            if d >= r_eff:
                a = 0
            else:
                t = d / r_eff
                a = int(round(255 * (1.0 - t) ** 1.6))
            if a <= 0:
                continue
            # Core: white-hot; mid: bright yellow; edge: orange, fading.
            core = max(0.0, 1.0 - d / (MAXR * 0.35))
            r = 255
            g = int(round(255 * (0.75 + 0.25 * core)))
            b = int(round(80 + 175 * core))
            px[x, y] = (r, g, b, a)
    return im


def smoke_image(seed=20261018):
    """Soft grey puff, radial falloff with mild noise, alpha 0 at border."""
    rng = random.Random(seed)
    # Low-res noise field, upsampled, for smooth mild grain (deterministic).
    nlo = 16
    noise = [[rng.uniform(-1.0, 1.0) for _ in range(nlo)] for _ in range(nlo)]
    im = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    px = im.load()
    for y in range(SIZE):
        for x in range(SIZE):
            # Fuller core than a linear cone: bullet-impact puffs are drawn tiny
            # (objectmanager.cpp:1032) at 0.625 alpha and must still read.
            d = math.hypot(x - CENTER, y - CENTER) / (MAXR - 1)
            if d >= 1.0:
                continue
            a = 255 * (1.0 - d * d) ** 1.8
            nx, ny = x * (nlo - 1) / (SIZE - 1), y * (nlo - 1) / (SIZE - 1)
            ix, iy = int(nx), int(ny)
            fx, fy = nx - ix, ny - iy
            ix1, iy1 = min(ix + 1, nlo - 1), min(iy + 1, nlo - 1)
            top = noise[iy][ix] * (1 - fx) + noise[iy][ix1] * fx
            bot = noise[iy1][ix] * (1 - fx) + noise[iy1][ix1] * fx
            n = top * (1 - fy) + bot * fy
            # Dusty grey-brown, darker than pale walls so impacts stay visible.
            k = 1.0 + 0.18 * n
            col = tuple(max(0, min(255, int(round(c * k)))) for c in (150, 140, 124))
            a = max(0, min(255, int(round(a * (1.0 + 0.25 * n)))))
            px[x, y] = col + (a,)
    return im


def blood_image(seed=20261018):
    """Dark red splatter blob, soft alpha falloff, roll-invariant overall."""
    rng = random.Random(seed)
    scale = 4
    big = Image.new('L', (SIZE * scale, SIZE * scale), 0)
    draw = ImageDraw.Draw(big)
    c = CENTER * scale
    # Central blob plus scattered satellite droplets, symmetric in expectation
    # (random angles cover the full circle -> no preferred roll direction).
    # Irregular core: overlapping lumps of different sizes, not one circle.
    main_r = SIZE * scale * 0.16
    for _ in range(7):
        ang = rng.uniform(0, 2 * math.pi)
        off = rng.uniform(0, main_r * 0.7)
        rr = main_r * rng.uniform(0.55, 1.05)
        x0, y0 = c + off * math.cos(ang), c + off * math.sin(ang)
        draw.ellipse((x0 - rr, y0 - rr * rng.uniform(0.7, 1.0), x0 + rr, y0 + rr), fill=255)
    # Uneven spray: random angles (clumped, not evenly spaced), mostly tiny
    # droplets, a few elongated streaks. Avoids a symmetric "germ" look.
    for i in range(26):
        ang = rng.uniform(0, 2 * math.pi)
        dist = main_r * rng.uniform(0.9, 2.5)
        rad = SIZE * scale * rng.choice((0.008, 0.012, 0.018, 0.03))
        cx = c + dist * math.cos(ang)
        cy = c + dist * math.sin(ang)
        if rng.random() < 0.3:
            ex, ey = c + dist * 0.75 * math.cos(ang), c + dist * 0.75 * math.sin(ang)
            draw.line((ex, ey, cx, cy), fill=255, width=max(1, int(rad * 1.2)))
        draw.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=255)
    big = big.filter(ImageFilter.GaussianBlur(radius=scale * 1.5))
    mask = big.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    # Force alpha to 0 at the border regardless of blur bleed.
    mpx = mask.load()
    for y in range(SIZE):
        for x in range(SIZE):
            edge = 1.0 - min(1.0, math.hypot(x - CENTER, y - CENTER) / (MAXR - 1))
            edge = max(0.0, edge) ** 0.6
            mpx[x, y] = int(round(mpx[x, y] * edge))
    im = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    rpx = im.load()
    for y in range(SIZE):
        for x in range(SIZE):
            a = mpx[x, y]
            if a <= 0:
                continue
            rpx[x, y] = (110, 8, 8, a)
    return im


def yakkyou_image():
    """Small brass cartridge-case sprite centred on a transparent field."""
    im = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    scale = 4
    big = Image.new('RGBA', (SIZE * scale, SIZE * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    cx = SIZE * scale / 2.0
    cy = SIZE * scale / 2.0
    length = SIZE * scale * 0.62
    width = SIZE * scale * 0.16
    x0, x1 = cx - length / 2.0, cx + length / 2.0
    y0, y1 = cy - width / 2.0, cy + width / 2.0
    brass = (176, 140, 60, 255)
    brass_dark = (120, 92, 34, 255)
    brass_light = (224, 190, 108, 255)
    # Case body (rounded rectangle via ellipse-capped shape).
    draw.rounded_rectangle((x0, y0, x1, y1), radius=width * 0.45, fill=brass)
    # Neck taper at one end.
    neck_len = length * 0.18
    nx0 = x1 - neck_len
    draw.polygon([(nx0, y0 + width * 0.08), (x1, cy - width * 0.22),
                  (x1, cy + width * 0.22), (nx0, y1 - width * 0.08)], fill=brass_dark)
    # Rim/primer at the other end.
    rim_r = width * 0.55
    draw.ellipse((x0 - rim_r * 0.3, cy - rim_r, x0 + rim_r * 0.9, cy + rim_r), fill=brass_dark)
    draw.ellipse((x0 - rim_r * 0.1, cy - rim_r * 0.55, x0 + rim_r * 0.55, cy + rim_r * 0.55),
                 fill=(70, 55, 20, 255))
    # Highlight stripe for a metallic read.
    draw.line((x0 + width * 0.3, cy - width * 0.18, x1 - neck_len, cy - width * 0.18),
              fill=brass_light, width=max(1, int(width * 0.12)))
    big = big.filter(ImageFilter.GaussianBlur(radius=scale * 0.6))
    im = big.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    # Guarantee a fully transparent border ring (no hard billboard edge).
    px = im.load()
    for y in range(SIZE):
        for x in range(SIZE):
            if x < 2 or y < 2 or x >= SIZE - 2 or y >= SIZE - 2:
                r, g, b, a = px[x, y]
                px[x, y] = (r, g, b, 0)
    return im


GENERATORS = {
    'data/blood.dds': blood_image,
    'data/mflash.dds': mflash_image,
    'data/smoke.dds': smoke_image,
    'data/yakkyou.dds': yakkyou_image,
}

PROMPTS = {
    'data/blood.dds': 'Original dark-red splatter blob with soft falloff alpha, '
                       'roll-invariant (no up direction), alpha 0 at the border.',
    'data/mflash.dds': 'Original bright yellow-white star flare, radial soft '
                        'falloff alpha, 12-fold symmetric spikes, alpha 0 at the border.',
    'data/smoke.dds': 'Original soft grey smoke puff, radial falloff alpha with '
                       'mild procedural noise, alpha 0 at the border.',
    'data/yakkyou.dds': 'Original small brass spent-cartridge-case sprite on a '
                         'transparent field, matching effecttexture_yakkyou billboard use.',
}


def main():
    root = Path(artgen.REPO)
    manifest = json.loads((root / 'assets/manifest.json').read_text())
    preview_dir = root / 'docs/img'
    preview_dir.mkdir(parents=True, exist_ok=True)
    records = []
    for path, gen in GENERATORS.items():
        im = gen()
        assert im.size == (SIZE, SIZE)
        dst = root / 'assets/pack' / path
        dst.parent.mkdir(parents=True, exist_ok=True)
        write_dds(im, dst, fmt='dds_argb8')
        probs = artgen.verify_dds(dst, (SIZE, SIZE), 'dds_argb8')
        assert not probs, (path, probs)
        w, h, pixels = engine_load(dst)
        overlay = Image.frombytes('RGBA', (w, h), pixels)
        checker = Image.new('RGBA', (w, h))
        checker.putdata([(150, 150, 150, 255) if (x // 8 + y // 8) % 2 else (210, 210, 210, 255)
                          for y in range(h) for x in range(w)])
        name = Path(path).stem
        Image.alpha_composite(checker, overlay).convert('RGB').save(
            preview_dir / f'{name}_preview.png')
        sha = artgen.sha256(dst)
        records.append(dict(
            path=path, sha256=sha,
            generator='Pillow procedural tools/artgen/effects.py',
            prompt=PROMPTS[path],
            license='CC0-1.0', author='OpenXOPS-web contributors',
            date=datetime.date.today().isoformat(), curated_by=None,
            source='Original mathematical/procedural generation; no original assets used.'))
        print(dst, sha)
    add_path = root / 'PROVENANCE-ADD.jsonl'
    with add_path.open('a') as f:
        for rec in records:
            f.write(json.dumps(rec) + '\n')
    return records


if __name__ == '__main__':
    main()
