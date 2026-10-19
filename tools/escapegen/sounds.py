"""Sound effects for the UFO ESCAPE mission (docs/MISSION-UFO-ESCAPE.md
section 4), reusing tools/sfxgen's stdlib synthesiser exactly as
tools/ufogen/sounds.py does.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SFXGEN_DIR = ROOT / "tools" / "sfxgen"
if str(SFXGEN_DIR) not in sys.path:
    sys.path.insert(0, str(SFXGEN_DIR))

import sfxgen as sx  # noqa: E402

OUT_DIR = ROOT / "assets" / "pack" / "data" / "sound"
SEED = 2608


def throw(rng):
    """Whoosh of a thrown rock: a short band-passed noise swell with a
    rising-then-falling pitch sweep for the air cutting past it."""
    dur = 0.45
    sweep = sx.mul(sx.sweep(dur, 300, 900, curve="exp"), sx.env_bell(dur, 0.4, 1.0, 1.6))
    wind = sx.mul(sx.bp(sx.white(dur, rng), 1400, 1.2), sx.env_bell(dur, 0.45, 1.1, 1.8))
    tail = sx.mul(sx.hp(sx.white(dur, rng), 2200, order=1), sx.env_exp(dur, 0.05))
    x = sx.mix((sweep, 0.5), (wind, 0.9), (tail, 0.3))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.3)
    x = sx.room(x, rng, wet=0.12, length=0.15, rt=0.12, damp=5000.0)
    return sx.finish(x, dur + 0.1)


def glass(rng):
    """Glass tube shatter + liquid splash: a bright noise crack followed by
    a wetter, lower splash swell."""
    dur = 0.6
    crack = sx.mul(sx.hp(sx.white(0.12, rng), 3500, order=2), sx.env_exp(0.12, 0.012))
    shards = sx.modal(0.35, [(2600, 1.0, 0.07), (3400, 0.6, 0.05), (4300, 0.4, 0.04)], rng)
    shards = sx.mul(shards, sx.env_exp(0.35, 0.09))
    splash = sx.mul(sx.bp(sx.white(dur, rng), 900, 1.5), sx.env_bell(dur, 0.45, 1.3, 2.0))
    splash = sx.lp(splash, 2000, order=1)
    x = sx.mix((crack, 1.0), (shards, 0.6), (splash, 0.7))
    x = sx.soft_clip(sx.gain(x, 1.0 / max(1e-6, sx.peak(x))), 1.5)
    x = sx.room(x, rng, wet=0.22, length=0.3, rt=0.25, damp=3500.0)
    return sx.finish(x, dur + 0.15, fade_out_ms=120)


RECIPES = {
    "throw": (throw, "thrown-rock whoosh: band-passed wind swell + rising sweep, 0.45 s"),
    "glass": (glass, "glass shatter + liquid splash: noise crack + modal shards + wet swell, 0.6 s"),
}


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, (fn, _summary) in RECIPES.items():
        x = fn(sx.rng_for(SEED, name))
        sx.write_wav(str(OUT_DIR / f"{name}.wav"), x)


if __name__ == "__main__":
    main()
