"""Sound effects for the UFO FIELD mission (docs/MISSION-UFO-FIELD.md
section 3/E3 and section 4), reusing tools/sfxgen's stdlib synthesiser
exactly as tools/sfxgen/recipes.py does for the classic data pack.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SFXGEN_DIR = ROOT / "tools" / "sfxgen"
if str(SFXGEN_DIR) not in sys.path:
    sys.path.insert(0, str(SFXGEN_DIR))

import sfxgen as sx  # noqa: E402

OUT_DIR = ROOT / "assets" / "pack" / "data" / "sound"
SEED = 2003


def laser1(rng):
    """ZAP PISTOL: a bright descending pew with a short ring."""
    dur = 0.28
    core = sx.mul(sx.sweep(dur, 2600, 420), sx.env_exp(dur, 0.045, attack=0.0005))
    crack = sx.mul(sx.hp(sx.white(dur, rng), 4200, order=2), sx.env_exp(dur, 0.01))
    ring = sx.mul(sx.modal(0.18, [(1800, 1.0, 0.05), (2650, 0.5, 0.035)], rng), sx.env_exp(0.18, 0.05))
    x = sx.mix((core, 1.0), (crack, 0.5), (ring, 0.4))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.6)
    x = sx.room(x, rng, wet=0.18, length=0.2, rt=0.15, damp=4000.0)
    return sx.finish(x, dur + 0.1)


def laser2(rng):
    """PHOTON SMG: a rapid high-pitched chirp (one burst-representative shot)."""
    dur = 0.14
    chirp = sx.mul(sx.sweep(dur, 4200, 1800, curve="exp"), sx.env_exp(dur, 0.02, attack=0.0003))
    buzz = sx.mul(sx.bp(sx.white(dur, rng), 3000, 2.0), sx.env_exp(dur, 0.015))
    x = sx.mix((chirp, 1.0), (buzz, 0.35))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.4)
    x = sx.room(x, rng, wet=0.1, length=0.12, rt=0.08, damp=5000.0)
    return sx.finish(x, dur + 0.06)


def laser3(rng):
    """PLASMA RIFLE: a heavy low thump followed by a zap."""
    dur = 0.4
    thump = sx.mul(sx.sweep(dur, 160, 55), sx.env_exp(dur, 0.06, attack=0.001))
    zap = sx.mul(sx.sweep(dur, 1900, 300), sx.env_exp(dur, 0.1, attack=0.004))
    crack = sx.mul(sx.hp(sx.white(dur, rng), 2200, order=2), sx.env_exp(dur, 0.02))
    x = sx.mix((thump, 1.0), (zap, 0.6), (crack, 0.35))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.9)
    x = sx.room(x, rng, wet=0.3, length=0.35, rt=0.3, damp=2600.0)
    return sx.finish(x, dur + 0.15)


def moo(rng):
    """Cow moo: two overlapping low formant sweeps with a nasal buzz."""
    dur = 1.1
    base = sx.mul(sx.sweep(dur, 95, 140, curve="linear"), sx.env_bell(dur, 0.35, 1.5, 2.2))
    formant1 = sx.lp(sx.gain(base, 1.0), 320, order=1)
    formant2 = sx.bp(sx.gain(base, 0.8), 650, 2.0)
    buzz = sx.mul(sx.white(dur, rng), sx.env_bell(dur, 0.35, 1.5, 2.2))
    buzz = sx.gain(sx.bp(buzz, 200, 3.0), 0.25)
    x = sx.mix((formant1, 0.9), (formant2, 0.55), (buzz, 0.4))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.3)
    x = sx.room(x, rng, wet=0.2, length=0.4, rt=0.35, damp=1800.0)
    return sx.finish(x, dur + 0.2, fade_out_ms=120)


def teleport(rng):
    """Wave deploy shimmer: rising sweep layered with glittering noise taps."""
    dur = 0.9
    rise = sx.mul(sx.sweep(dur, 220, 2600, curve="exp"), sx.env_bell(dur, 0.55, 1.2, 2.5))
    shimmer = sx.gain(sx.bp(sx.white(dur, rng), 5200, 4.0), 0.5)
    shimmer = sx.mul(shimmer, sx.env_bell(dur, 0.65, 1.0, 2.0))
    sparkle = [0.0] * sx.nsamp(dur)
    for _ in range(14):
        at = rng.uniform(0.05, dur - 0.08)
        tap = sx.mul(sx.sine(0.06, rng.uniform(1800, 4200)), sx.env_exp(0.06, 0.015))
        sx.place(sparkle, tap, at, 0.35)
    x = sx.mix((rise, 0.9), (shimmer, 0.5), (sparkle, 1.0))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.2)
    x = sx.room(x, rng, wet=0.35, length=0.5, rt=0.4, damp=6000.0)
    return sx.finish(x, dur + 0.25, fade_out_ms=150)


RECIPES = {
    "laser1": (laser1, "zap pistol pew: bright descending sweep + ring, 0.28 s"),
    "laser2": (laser2, "photon smg rapid chirp: high exp sweep + buzz, 0.14 s"),
    "laser3": (laser3, "plasma rifle thump-zap: low sweep + zap + crack, 0.4 s"),
    "moo": (moo, "cow moo: dual formant sweep + nasal buzz, 1.1 s"),
    "teleport": (teleport, "wave deploy shimmer: rising sweep + sparkle taps, 0.9 s"),
}


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, (fn, _summary) in RECIPES.items():
        x = fn(sx.rng_for(SEED, name))
        sx.write_wav(str(OUT_DIR / f"{name}.wav"), x)


if __name__ == "__main__":
    main()
