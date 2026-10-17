"""Per-sound recipes for the OpenXOPS-web Free Data Pack (data/sound/*.wav).

One function per file. Each takes a seeded random.Random and returns the
finished float signal. Event mapping: docs/ASSET-CONTRACT.md §8.2.
"""
from collections import OrderedDict

from sfxgen import (SR, biquad_sweep, bp, brown, click, db, env_bell, env_exp, finish, hp, lp, mix,
                    modal, mul, peak, pink, place, room, soft_clip, sweep, white)


# --------------------------------------------------------------------------
# Shared gunshot model
# --------------------------------------------------------------------------

def gunshot(rng, p):
    """Layered gunshot.

    click : 1-2 ms broadband muzzle transient
    crack : high-passed white noise, very fast decay (supersonic crack)
    body  : band-limited noise around body_f (the 'bark' of the weapon)
    boom  : low sine glide (chest thump), heavier weapons only
    mech  : optional resonant metallic ping (action / receiver ring)
    tail  : low-passed pink noise with a falling cutoff, then a room IR
    """
    dur = p["dur"]
    crack = mul(hp(white(dur, rng), p["crack_hp"], order=2), env_exp(dur, p["crack_tau"]))
    body = mul(bp(white(dur, rng), p["body_f"], p.get("body_q", 0.9)), env_exp(dur, p["body_tau"], attack=0.0008))
    body = soft_clip(gain_to(body, 1.0), 1.5)
    boom = mul(sweep(dur, p["boom_f0"], p["boom_f1"]), env_exp(dur, p["boom_tau"], attack=0.001))
    tail = biquad_sweep(pink(dur, rng), "lp", p["tail_f0"], p["tail_f1"])
    tail = mul(gain_to(tail, 1.0), env_exp(dur, p["tail_tau"], attack=0.004))
    layers = [
        (click(p.get("click_ms", 1.2), rng), p.get("click_g", 0.9)),
        (gain_to(crack, 1.0), p["crack_g"]),
        (body, p["body_g"]),
        (boom, p["boom_g"]),
        (tail, p["tail_g"]),
    ]
    if p.get("mech"):
        f, g, tau, at = p["mech"]
        layers.append((modal(0.12, [(f, 1.0, tau), (f * 2.76, 0.5, tau * 0.6)], rng), g, at))
    x = mix(*layers)
    x = soft_clip(gain_to(x, 1.0), p.get("drive", 2.0))
    x = room(x, rng, wet=p.get("wet", 0.25), length=p.get("room_len", 0.3), rt=p.get("rt", 0.25),
             damp=p.get("damp", 3500.0))
    return finish(x, dur)


def gain_to(x, target):
    pk = peak(x)
    return [s * target / pk for s in x] if pk else x


GUNSHOTS = OrderedDict()

# bang1: 9 mm pistol / SMG (GLOCK17, GLOCK18, MP5). Tight, mid-bright, short.
GUNSHOTS["bang1"] = dict(
    dur=0.32, crack_hp=2500, crack_tau=0.006, crack_g=0.8,
    body_f=900, body_q=1.0, body_tau=0.018, body_g=0.9,
    boom_f0=180, boom_f1=90, boom_tau=0.020, boom_g=0.45,
    tail_f0=2500, tail_f1=400, tail_tau=0.060, tail_g=0.35,
    drive=2.2, wet=0.22, room_len=0.25, rt=0.22,
    mech=(3100, 0.10, 0.02, 0.004))

# bang2: 7.62 mm rifle (AK47, PSG1; also M92F). Loud crack, strong mid-low body, longer tail.
GUNSHOTS["bang2"] = dict(
    dur=0.55, crack_hp=1800, crack_tau=0.009, crack_g=1.0,
    body_f=550, body_q=0.8, body_tau=0.035, body_g=1.0,
    boom_f0=140, boom_f1=55, boom_tau=0.060, boom_g=0.8,
    tail_f0=1800, tail_f1=180, tail_tau=0.140, tail_g=0.55,
    drive=2.8, wet=0.30, room_len=0.40, rt=0.38)

# bang3: magnum / battle rifle (DESERT EAGLE, M1). Deepest, boomiest, longest.
GUNSHOTS["bang3"] = dict(
    dur=0.62, crack_hp=1200, crack_tau=0.010, crack_g=0.8,
    body_f=330, body_q=0.7, body_tau=0.050, body_g=1.0,
    boom_f0=110, boom_f1=40, boom_tau=0.110, boom_g=1.2,
    tail_f0=1200, tail_f1=120, tail_tau=0.190, tail_g=0.65,
    drive=3.2, wet=0.32, room_len=0.45, rt=0.45, damp=2500.0)

# bang4: 5.56 mm / fast automatics (M4, M249, P90, MAC10; also M1911). Snappy, bright, very short.
GUNSHOTS["bang4"] = dict(
    dur=0.26, crack_hp=3500, crack_tau=0.005, crack_g=1.0,
    body_f=1400, body_q=1.1, body_tau=0.012, body_g=0.7,
    boom_f0=220, boom_f1=110, boom_tau=0.014, boom_g=0.35,
    tail_f0=4000, tail_f1=700, tail_tau=0.045, tail_g=0.30,
    drive=2.0, wet=0.18, room_len=0.20, rt=0.18, damp=5000.0, click_ms=0.8)

# bang5: .45 / bullpup (UMP, AUG, FAMAS, MK23). Boxy mid 'pop' with a metallic receiver ring.
GUNSHOTS["bang5"] = dict(
    dur=0.38, crack_hp=2200, crack_tau=0.007, crack_g=0.6,
    body_f=700, body_q=1.6, body_tau=0.028, body_g=1.0,
    boom_f0=160, boom_f1=80, boom_tau=0.030, boom_g=0.55,
    tail_f0=2200, tail_f1=300, tail_tau=0.080, tail_g=0.40,
    drive=2.4, wet=0.25, room_len=0.30, rt=0.28,
    mech=(1850, 0.35, 0.035, 0.002))


def make_gunshot(name):
    def fn(rng):
        return gunshot(rng, GUNSHOTS[name])
    fn.__name__ = name
    return fn


# --------------------------------------------------------------------------
# Other sounds
# --------------------------------------------------------------------------

def r_bang(rng):
    """Grenade explosion: crack + sub boom + falling-cutoff roar + long rumble + debris."""
    dur = 1.6
    crack = mul(hp(white(0.08, rng), 1500, order=2), env_exp(0.08, 0.010))
    boom = mul(sweep(dur, 85, 28), env_exp(dur, 0.22, attack=0.004))
    roar = biquad_sweep(brown(dur, rng), "lp", 1400, 90, q=0.9)
    roar = mul(gain_to(roar, 1.0), env_exp(dur, 0.30, attack=0.006))
    rumble = lp(brown(dur, rng), 110, order=2)
    rumble = mul(gain_to(rumble, 1.0), env_exp(dur, 0.55, attack=0.05))
    x = mix((click(2.0, rng), 0.8), (gain_to(crack, 1.0), 0.9), (boom, 1.3), (roar, 1.0), (rumble, 0.7))
    # sparse debris patter, 0.15-0.9 s
    for _ in range(14):
        t = 0.15 + rng.random() * 0.75
        g = 0.12 * (1.0 - (t - 0.15) / 0.8) * (0.5 + rng.random())
        d = bp(click(1.5, rng) + [0.0] * 200, 1500 + 3000 * rng.random(), 3.0)
        place(x, gain_to(d, 1.0), t, g)
    x = soft_clip(gain_to(x, 1.0), 2.5)
    x = room(x, rng, wet=0.30, length=0.6, rt=0.7, damp=1800.0, early=(0.01, 0.06))
    return finish(x, dur, fade_out_ms=120.0)


def r_ka(rng):
    """Suppressed shot: soft low-passed 'thup' + mechanical slide clicks."""
    dur = 0.24
    thup = mul(lp(white(dur, rng), 700, order=2), env_exp(dur, 0.018, attack=0.0015))
    low = mul(sweep(dur, 160, 80), env_exp(dur, 0.025, attack=0.001))
    puff = mul(bp(white(dur, rng), 1800, 0.8), env_exp(dur, 0.008))
    x = mix((gain_to(thup, 1.0), 1.0), (low, 0.7), (gain_to(puff, 1.0), 0.25))
    for at, f, g in ((0.030, 3400, 0.35), (0.062, 2600, 0.30)):
        clk = mix((bp(click(0.6, rng) + [0.0] * 60, f, 4.0), 1.0),
                  (modal(0.05, [(f * 1.31, 0.5, 0.006), (f * 2.1, 0.3, 0.004)], rng), 0.4))
        place(x, gain_to(clk, 1.0), at, g)
    x = room(x, rng, wet=0.12, length=0.15, rt=0.12, damp=3000.0)
    return finish(x, dur)


def r_can(rng):
    """Bullet hits a thin metal can: sharp tick + inharmonic ringing partials."""
    dur = 0.45
    ring = modal(dur, [(1240, 1.0, 0.16), (2930, 0.7, 0.12), (4170, 0.55, 0.09),
                       (5620, 0.35, 0.06), (7340, 0.25, 0.04), (845, 0.3, 0.05)], rng, jitter=0.01)
    tick = mul(hp(white(0.03, rng), 3000, order=2), env_exp(0.03, 0.003))
    knock = mul(bp(white(0.06, rng), 600, 1.2), env_exp(0.06, 0.010))
    x = mix((gain_to(ring, 1.0), 0.8), (gain_to(tick, 1.0), 0.7), (gain_to(knock, 1.0), 0.3))
    x = room(x, rng, wet=0.12, length=0.2, rt=0.18, damp=6000.0)
    return finish(x, dur, fade_out_ms=40.0)


def r_dan(rng):
    """Bullet hits a cardboard box: dull hollow thud + papery rustle, no ring."""
    dur = 0.22
    thud = mul(lp(white(dur, rng), 500, order=2), env_exp(dur, 0.030, attack=0.001))
    body = mul(bp(white(dur, rng), 180, 2.5), env_exp(dur, 0.035, attack=0.001))
    paper = mul(bp(white(dur, rng), 2600, 0.6), env_exp(dur, 0.012))
    flap = mul(bp(white(dur, rng), 3800, 0.8), env_exp(dur, 0.020))
    x = mix((gain_to(thud, 1.0), 1.0), (gain_to(body, 1.0), 0.8),
            (gain_to(paper, 1.0), 0.35), (gain_to(flap, 1.0), 0.12, 0.018))
    x = room(x, rng, wet=0.08, length=0.12, rt=0.1, damp=2500.0)
    return finish(x, dur)


def _chip(rng, dur, crack_hp, n_grains, grain_span):
    crack = mul(hp(white(dur, rng), crack_hp, order=2), env_exp(dur, 0.006))
    thump = mul(lp(white(dur, rng), 350, order=2), env_exp(dur, 0.012, attack=0.0005))
    x = mix((click(0.8, rng), 0.7), (gain_to(crack, 1.0), 1.0), (gain_to(thump, 1.0), 0.45))
    for _ in range(n_grains):
        t = 0.008 + rng.random() * grain_span
        g = 0.25 * (1.0 - t / (grain_span + 0.02)) * (0.4 + rng.random())
        d = bp(click(0.5, rng) + [0.0] * 80, 2000 + 5000 * rng.random(), 2.0)
        place(x, gain_to(d, 1.0), t, g)
    return x


def r_hit1(rng):
    """Bullet hits concrete (A): hard crack + thump + gritty debris grains."""
    dur = 0.22
    x = _chip(rng, dur, 2200, 18, 0.12)
    x = room(x, rng, wet=0.15, length=0.15, rt=0.14, damp=4000.0)
    return finish(x, dur)


def r_hit3(rng):
    """Bullet hits concrete (B): shorter chip + a brief descending ricochet whine."""
    dur = 0.32
    x = _chip(rng, 0.12, 3000, 8, 0.05)
    wl = 0.24
    whine = sweep(wl, 3600, 2100)
    whine = [s * (1.0 + 0.25 * w) for s, w in zip(whine, sweep(wl, 38, 30))]   # slight wobble
    hiss = bp(white(wl, rng), 3000, 2.0)
    env = env_bell(wl, 0.12, 1.0, 1.6)
    whine = mul(mix((whine, 1.0), (gain_to(hiss, 1.0), 0.35)), env)
    place(x, gain_to(whine, 1.0), 0.018, 0.4)
    x = room(x, rng, wet=0.18, length=0.18, rt=0.16, damp=5000.0)
    return finish(x, dur)


def r_hit2(rng):
    """Bullet hits a person: muted low thump + soft mid slap (non-gory)."""
    dur = 0.18
    thump = mul(sweep(dur, 120, 65), env_exp(dur, 0.030, attack=0.0015))
    flesh = mul(lp(white(dur, rng), 450, order=2), env_exp(dur, 0.020, attack=0.001))
    slap = mul(bp(white(dur, rng), 1100, 1.0), env_exp(dur, 0.006))
    x = mix((thump, 1.0), (gain_to(flesh, 1.0), 0.8), (gain_to(slap, 1.0), 0.25))
    x = soft_clip(gain_to(x, 1.0), 1.5)
    return finish(x, dur)


def r_hyu(rng):
    """Bullet passing close by: swelling band-passed noise whose centre falls (doppler)."""
    dur = 0.30
    n = white(dur, rng)
    whiz = biquad_sweep(n, "bp", 5200, 1300, q=4.0)
    whiz2 = biquad_sweep(white(dur, rng), "bp", 2600, 700, q=6.0)
    tone = sweep(dur, 3000, 1200)
    env = env_bell(dur, 0.38, 2.2, 1.8)
    x = mix((gain_to(whiz, 1.0), 1.0), (gain_to(whiz2, 1.0), 0.45), (tone, 0.18))
    x = mul(x, env)
    return finish(x, dur, fade_in_ms=2.0)


def r_cco(rng):
    """Grenade bounce on hard ground: low metallic clank + knock + tiny secondary rattle."""
    dur = 0.30
    clank = modal(dur, [(610, 1.0, 0.07), (1490, 0.8, 0.05), (2370, 0.6, 0.04),
                        (3820, 0.4, 0.025), (5210, 0.25, 0.015)], rng, jitter=0.01)
    knock = mul(lp(white(dur, rng), 600, order=2), env_exp(dur, 0.010, attack=0.0005))
    tick = mul(hp(white(dur, rng), 2500, order=1), env_exp(dur, 0.003))
    x = mix((gain_to(clank, 1.0), 0.7), (gain_to(knock, 1.0), 0.8), (gain_to(tick, 1.0), 0.5))
    rattle = modal(0.12, [(720, 1.0, 0.03), (1750, 0.7, 0.02), (2900, 0.4, 0.012)], rng, jitter=0.02)
    place(x, gain_to(rattle, 1.0), 0.075, 0.22)
    x = room(x, rng, wet=0.15, length=0.2, rt=0.18, damp=4500.0)
    return finish(x, dur)


RECIPES = OrderedDict([
    ("bang", (r_bang, "grenade explosion: crack + 85->28 Hz boom + roar LP 1400->90 Hz + rumble + debris, room rt 0.7 s, 1.6 s")),
    ("bang1", (make_gunshot("bang1"), "gunshot 9mm pistol/SMG: crack HP2.5k, body BP900, boom 180->90, 0.32 s")),
    ("bang2", (make_gunshot("bang2"), "gunshot 7.62 rifle: crack HP1.8k, body BP550, boom 140->55, room rt 0.38, 0.55 s")),
    ("bang3", (make_gunshot("bang3"), "gunshot magnum/battle rifle: body BP330, boom 110->40, room rt 0.45, 0.62 s")),
    ("bang4", (make_gunshot("bang4"), "gunshot 5.56/fast auto: crack HP3.5k, body BP1400, short tail, 0.26 s")),
    ("bang5", (make_gunshot("bang5"), "gunshot .45/bullpup: boxy body BP700 Q1.6 + 1850 Hz receiver ring, 0.38 s")),
    ("ka", (r_ka, "suppressed shot: LP700 thup + 160->80 Hz + two slide clicks (3.4k, 2.6k), 0.24 s")),
    ("can", (r_can, "can hit: modal partials 845/1240/2930/4170/5620/7340 Hz + HP tick, 0.45 s")),
    ("dan", (r_dan, "cardboard hit: LP500 thud + BP180 body + paper rustle BP2.6k/3.8k, 0.22 s")),
    ("hit1", (r_hit1, "concrete hit A: HP2.2k crack + LP350 thump + 18 debris grains, 0.22 s")),
    ("hit2", (r_hit2, "body hit: 120->65 Hz thump + LP450 noise + soft BP1.1k slap, 0.18 s")),
    ("hit3", (r_hit3, "concrete hit B: chip + 3.6k->2.1k ricochet whine, 0.32 s")),
    ("hyu", (r_hyu, "bullet whiz: BP noise 5.2k->1.3k (Q4) + 2.6k->700 + faint 3k->1.2k tone, bell env, 0.30 s")),
    ("cco", (r_cco, "grenade bounce: modal clank 610/1490/2370/3820/5210 Hz + knock + rattle at 75 ms, 0.30 s")),
])
