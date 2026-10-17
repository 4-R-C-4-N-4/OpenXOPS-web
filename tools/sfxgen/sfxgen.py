#!/usr/bin/env python3
"""sfxgen: a tiny deterministic procedural sound-effect synthesiser (stdlib only).

Every sound is built from scratch with seeded noise, oscillators, filters and
envelopes. Nothing is sampled from any existing recording. Output is the
canonical 44-byte-header RIFF/WAVE, PCM, mono, 16-bit, 22050 Hz that
docs/ASSET-CONTRACT.md §8 asks for.

Usage:
    python3 tools/sfxgen/sfxgen.py [--out assets/pack/data/sound] [--seed 2003] [--only NAME ...]
    python3 tools/sfxgen/sfxgen.py --list

Signals are plain Python lists of floats in [-1, 1] at SR. All randomness goes
through random.Random seeded per recipe, so the same seed renders bit-identical
files on the same platform.
"""
import argparse
import math
import os
import random
import struct
import sys
import zlib

SR = 22050
TWO_PI = 2.0 * math.pi
DEFAULT_SEED = 2003


# --------------------------------------------------------------------------
# Basics
# --------------------------------------------------------------------------

def nsamp(sec):
    return max(1, int(round(sec * SR)))


def silence(dur):
    return [0.0] * nsamp(dur)


def rng_for(seed, name):
    """Independent, stable RNG stream per recipe name."""
    return random.Random((seed * 1000003) ^ zlib.crc32(name.encode("ascii")))


def white(dur, rng):
    return [rng.random() * 2.0 - 1.0 for _ in range(nsamp(dur))]


def pink(dur, rng):
    """Paul Kellet's economy pink filter on white noise (approx -3 dB/oct)."""
    b0 = b1 = b2 = 0.0
    out = []
    for _ in range(nsamp(dur)):
        w = rng.random() * 2.0 - 1.0
        b0 = 0.99765 * b0 + w * 0.0990460
        b1 = 0.96300 * b1 + w * 0.2965164
        b2 = 0.57000 * b2 + w * 1.0526913
        out.append((b0 + b1 + b2 + w * 0.1848) * 0.2)
    return out


def brown(dur, rng):
    """Leaky-integrated white noise (approx -6 dB/oct)."""
    y = 0.0
    out = []
    for _ in range(nsamp(dur)):
        y = 0.995 * y + (rng.random() * 2.0 - 1.0) * 0.1
        out.append(y)
    return normalize(out, 0.0)


def sine(dur, freq, phase=0.0):
    n = nsamp(dur)
    return [math.sin(phase + TWO_PI * freq * i / SR) for i in range(n)]


def sweep(dur, f0, f1, curve="exp", phase=0.0):
    """Sine whose frequency glides f0 -> f1 (exponential or linear)."""
    n = nsamp(dur)
    out = []
    ph = phase
    for i in range(n):
        t = i / max(1, n - 1)
        f = f0 * (f1 / f0) ** t if curve == "exp" else f0 + (f1 - f0) * t
        out.append(math.sin(ph))
        ph += TWO_PI * f / SR
    return out


def modal(dur, partials, rng=None, jitter=0.0):
    """Sum of exponentially decaying sinusoids: partials = [(freq, amp, tau_s), ...].

    Models struck metal / resonant bodies (inharmonic partial sets).
    """
    n = nsamp(dur)
    out = [0.0] * n
    for k, (f, a, tau) in enumerate(partials):
        if rng is not None and jitter:
            f *= 1.0 + jitter * (rng.random() * 2.0 - 1.0)
        ph = rng.random() * TWO_PI if rng is not None else 0.0
        w = TWO_PI * f / SR
        d = math.exp(-1.0 / (tau * SR))
        g = a
        for i in range(n):
            out[i] += g * math.sin(ph + w * i)
            g *= d
    return out


def click(width_ms=1.0, rng=None):
    """Very short broadband transient (a raised-cosine-windowed noise or impulse)."""
    n = max(2, nsamp(width_ms / 1000.0))
    out = []
    for i in range(n):
        w = 0.5 - 0.5 * math.cos(TWO_PI * (i + 0.5) / n)
        s = (rng.random() * 2.0 - 1.0) if rng is not None else (1.0 if i < n // 2 else -1.0)
        out.append(w * s)
    return out


# --------------------------------------------------------------------------
# Envelopes
# --------------------------------------------------------------------------

def env_exp(dur, tau, attack=0.0):
    """Linear attack (s) then exponential decay with time constant tau (s)."""
    n = nsamp(dur)
    na = int(attack * SR)
    out = []
    for i in range(n):
        if i < na:
            out.append(i / na)
        else:
            out.append(math.exp(-(i - na) / (tau * SR)))
    return out


def env_adsr(dur, a, d, s, r):
    n = nsamp(dur)
    na, nd, nr = int(a * SR), int(d * SR), int(r * SR)
    out = []
    for i in range(n):
        if i < na:
            v = i / max(1, na)
        elif i < na + nd:
            v = 1.0 - (1.0 - s) * (i - na) / max(1, nd)
        else:
            v = s
        if i >= n - nr:
            v *= (n - i) / max(1, nr)
        out.append(v)
    return out


def env_bell(dur, peak_at, rise_pow=2.0, fall_pow=2.0):
    """Smooth swell: 0 -> 1 at peak_at (fraction of dur) -> 0."""
    n = nsamp(dur)
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        if t <= peak_at:
            v = (t / peak_at) ** rise_pow
        else:
            v = ((1.0 - t) / (1.0 - peak_at)) ** fall_pow
        out.append(v)
    return out


# --------------------------------------------------------------------------
# Filters (RBJ cookbook biquads)
# --------------------------------------------------------------------------

def biquad_coeffs(kind, f, q=0.7071, gain_db=0.0):
    f = min(max(f, 10.0), SR * 0.49)
    w0 = TWO_PI * f / SR
    cw, sw = math.cos(w0), math.sin(w0)
    alpha = sw / (2.0 * q)
    if kind == "lp":
        b = ((1 - cw) / 2, 1 - cw, (1 - cw) / 2)
        a = (1 + alpha, -2 * cw, 1 - alpha)
    elif kind == "hp":
        b = ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2)
        a = (1 + alpha, -2 * cw, 1 - alpha)
    elif kind == "bp":      # constant 0 dB peak gain
        b = (alpha, 0.0, -alpha)
        a = (1 + alpha, -2 * cw, 1 - alpha)
    elif kind == "peak":
        A = 10 ** (gain_db / 40.0)
        b = (1 + alpha * A, -2 * cw, 1 - alpha * A)
        a = (1 + alpha / A, -2 * cw, 1 - alpha / A)
    else:
        raise ValueError(kind)
    a0 = a[0]
    return (b[0] / a0, b[1] / a0, b[2] / a0, a[1] / a0, a[2] / a0)


def biquad(x, kind, f, q=0.7071, gain_db=0.0):
    b0, b1, b2, a1, a2 = biquad_coeffs(kind, f, q, gain_db)
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for s in x:
        y = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, s, y1, y
        out.append(y)
    return out


def biquad_sweep(x, kind, f0, f1, q=0.7071, curve="exp", block=16):
    """Time-varying biquad: cutoff glides f0 -> f1 over the signal length."""
    n = len(x)
    x1 = x2 = y1 = y2 = 0.0
    out = []
    coeffs = None
    for i, s in enumerate(x):
        if i % block == 0:
            t = i / max(1, n - 1)
            f = f0 * (f1 / f0) ** t if curve == "exp" else f0 + (f1 - f0) * t
            coeffs = biquad_coeffs(kind, f, q)
        b0, b1, b2, a1, a2 = coeffs
        y = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, s, y1, y
        out.append(y)
    return out


def lp(x, f, q=0.7071, order=1):
    for _ in range(order):
        x = biquad(x, "lp", f, q)
    return x


def hp(x, f, q=0.7071, order=1):
    for _ in range(order):
        x = biquad(x, "hp", f, q)
    return x


def bp(x, f, q=1.0):
    return biquad(x, "bp", f, q)


# --------------------------------------------------------------------------
# Combinators / dynamics
# --------------------------------------------------------------------------

def mul(x, env):
    n = min(len(x), len(env))
    return [x[i] * env[i] for i in range(n)]


def gain(x, g):
    return [s * g for s in x]


def db(g_db):
    return 10 ** (g_db / 20.0)


def place(dst, src, at_sec=0.0, g=1.0):
    """Mix src into dst starting at at_sec (extends dst as needed). Returns dst."""
    off = int(round(at_sec * SR))
    need = off + len(src)
    if need > len(dst):
        dst.extend([0.0] * (need - len(dst)))
    for i, s in enumerate(src):
        dst[off + i] += g * s
    return dst


def mix(*layers):
    """mix((sig, gain, at_sec), ...) -> new list."""
    out = []
    for layer in layers:
        sig, g = layer[0], layer[1]
        at = layer[2] if len(layer) > 2 else 0.0
        place(out, sig, at, g)
    return out


def soft_clip(x, drive=1.0):
    """tanh saturation, gain-compensated so small signals keep unit gain."""
    k = math.tanh(drive)
    return [math.tanh(drive * s) / k for s in x]


def fit(x, dur):
    n = nsamp(dur)
    return (x + [0.0] * n)[:n]


def peak(x):
    return max(abs(s) for s in x) if x else 0.0


def normalize(x, peak_db=-1.0):
    p = peak(x)
    if p <= 0:
        return list(x)
    g = db(peak_db) / p
    return [s * g for s in x]


def fades(x, in_ms=0.5, out_ms=20.0):
    n = len(x)
    ni = min(n, int(in_ms * SR / 1000.0))
    no = min(n, int(out_ms * SR / 1000.0))
    y = list(x)
    for i in range(ni):
        y[i] *= (i + 1) / (ni + 1)
    for i in range(no):
        y[n - 1 - i] *= 0.5 - 0.5 * math.cos(math.pi * i / max(1, no))
    return y


def dc_block(x, f=25.0):
    return hp(x, f, 0.7071)


# --------------------------------------------------------------------------
# Room: sparse synthetic impulse response + direct convolution
# --------------------------------------------------------------------------

def room_ir(rng, length=0.35, rt=0.25, early=(0.004, 0.030), n_early=8, density=900.0):
    """Sparse IR as a list of (delay_samples, gain).

    Early reflections: n_early taps spread in the `early` window.
    Late tail: Poisson-ish taps at `density` taps/s with an exponential
    envelope whose -60 dB time is rt.
    """
    taps = []
    for _ in range(n_early):
        d = early[0] + rng.random() * (early[1] - early[0])
        g = (0.35 + 0.4 * rng.random()) * (1 if rng.random() < 0.5 else -1)
        taps.append((int(d * SR), g * math.exp(-6.91 * d / rt)))
    t = early[1]
    while t < length:
        t += rng.expovariate(density)
        g = (rng.random() * 2.0 - 1.0) * 0.5 * math.exp(-6.91 * t / rt)
        taps.append((int(t * SR), g))
    taps.sort()
    return taps


def convolve_sparse(x, taps):
    n = len(x) + (taps[-1][0] if taps else 0)
    out = [0.0] * n
    for d, g in taps:
        if g == 0.0:
            continue
        for i, s in enumerate(x):
            out[d + i] += g * s
    return out


def room(x, rng, wet=0.25, length=0.35, rt=0.25, damp=3500.0, **kw):
    """Dry + damped wet convolution tail."""
    taps = room_ir(rng, length=length, rt=rt, **kw)
    w = lp(convolve_sparse(x, taps), damp)
    out = list(x) + [0.0] * (len(w) - len(x))
    for i, s in enumerate(w):
        out[i] += wet * s
    return out


# --------------------------------------------------------------------------
# WAV output (canonical 44-byte header)
# --------------------------------------------------------------------------

def to_pcm16(x):
    out = bytearray()
    for s in x:
        v = int(round(s * 32767.0))
        v = 32767 if v > 32767 else (-32768 if v < -32768 else v)
        out += struct.pack("<h", v)
    return bytes(out)


def wav_bytes(x, rate=SR):
    data = to_pcm16(x)
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    hdr += b"data" + struct.pack("<I", len(data))
    assert len(hdr) == 44
    return hdr + data + (b"\0" if len(data) & 1 else b"")


def finish(x, dur=None, peak_db=-1.0, fade_in_ms=0.5, fade_out_ms=20.0):
    """Standard mastering: DC block, trim/pad, peak-normalise, edge fades."""
    x = dc_block(x)
    if dur is not None:
        x = fit(x, dur)
    x = normalize(x, peak_db)
    return fades(x, fade_in_ms, fade_out_ms)


def write_wav(path, x):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(wav_bytes(x))


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def main(argv=None):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import recipes

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="assets/pack/data/sound")
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args(argv)

    if a.list:
        for name, (fn, summary) in recipes.RECIPES.items():
            print(f"{name:10s} {summary}")
        return 0
    for name, (fn, summary) in recipes.RECIPES.items():
        if a.only and name not in a.only:
            continue
        x = fn(rng_for(a.seed, name))
        path = os.path.join(a.out, name + ".wav")
        write_wav(path, x)
        print(f"{path}: {len(x) / SR:.3f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
