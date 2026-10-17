# Procedural sound effects

Fourteen original effects for the events in `docs/ASSET-CONTRACT.md` §8.2.
All audio is synthesized from seeded noise, oscillators, envelopes, filters,
inharmonic resonances and a synthetic sparse room response. No recordings or
original XOPS assets are used. Weapon labels describe event slots, not sampled
or physically accurate weapons.

## Regenerate and verify

From the repository root (Python 3, standard library only):

```sh
python3 tools/sfxgen/sfxgen.py --seed 2003
python3 -m unittest discover -s tools/sfxgen -v
python3 tools/sfxgen/analyze.py check assets/pack/data/sound/*.wav
python3 tools/sfxgen/analyze.py stats assets/pack/data/sound/*.wav
python3 tools/validate_pack.py assets/pack
```

The pack validator may still fail on unrelated unfilled pack slots. All sound
slots must pass. WAVs are canonical 44-byte RIFF headers, PCM16 little-endian,
mono, 22050 Hz, one-shot, peak-normalized with DC blocking and edge fades.
Per-recipe RNG streams are independent: `--only bang1` produces the same file
as a full render. Use `--list` for summaries or `--out DIR` for a scratch render.
Tests regenerate every file in a temporary directory and compare SHA-256 with
the shipped bytes, check headers/levels, and verify provenance and slot status.
Bit-identical regeneration is verified on this Python/platform; floating-point
math libraries on other platforms can differ at quantization boundaries.

`recipes.py` contains the complete numeric recipes. `bang1`–`bang5` are distinct
gunshots; `ka` a suppressed shot; `bang` an explosion; `hit1`/`hit3` hard-surface
impacts; `hit2` a muted body impact; `hyu` a passing bullet; `cco` a grenade
bounce; `can` resonant metal and `dan` a dull papery article impact.

## Preview

With Pillow installed (only the preview requires it):

```sh
python3 tools/sfxgen/analyze.py sheet docs/img/sfx_spectrograms.png assets/pack/data/sound/*.wav
```

The sheet shows log-frequency spectrograms (30 Hz–11.025 kHz, −90 to 0 dB)
and waveform strips on a common 600 px/s scale. It is a technical preview,
not a substitute for human listening or in-engine mix approval.

## License and provenance

The generated WAVs and the synthesis/analysis code in this directory are
dedicated to the public domain under **CC0-1.0**:
<https://creativecommons.org/publicdomain/zero/1.0/>.
Author: OpenXOPS-web contributors (AI-assisted procedural synthesis).
Sound provenance is appended in `assets/provenance.jsonl`, including SHA-256,
generator, recipe name/source/description, seed, date and license.
`curated_by: null` intentionally means no human listening approval is claimed.
If recipes or seed change, regenerate and append new provenance with the new
hashes; the validator uses the latest record for each path.
