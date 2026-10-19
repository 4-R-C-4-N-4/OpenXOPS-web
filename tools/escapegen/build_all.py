#!/usr/bin/env python3
"""Build every UFO ESCAPE asset and append PROVENANCE-ADD.jsonl (never
assets/provenance.jsonl -- that file is the parent's merge target).

Usage: python3 -m tools.escapegen.build_all
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from tools.escapegen import block_textures, rock, tube, sounds

ROOT = Path(__file__).resolve().parents[2]
PROV_PATH = ROOT / "PROVENANCE-ADD.jsonl"
PREVIEW_DIR = ROOT / "docs" / "img" / "escape"
DATE = "2026-10-18"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _entry(path: Path, generator: str, prompt: str) -> dict:
    rel = str(path.relative_to(ROOT / "assets" / "pack")).replace("\\", "/")
    name = Path(generator.split(" ")[0]).name
    return {
        "path": rel,
        "sha256": _sha256(path),
        "license": "CC0-1.0",
        "author": "OpenXOPS-web contributors (clean-room procedural)",
        "generator": generator,
        "date": DATE,
        "curated_by": None,
        "prompt": prompt,
        "source_url": f"https://github.com/4-R-C-4-N-4/OpenXOPS-web/blob/main/{generator.split(' ')[0]}",
    }


def build_block_textures(entries: list) -> None:
    for filename, fn in block_textures.GENERATORS.items():
        out = block_textures.OUT_DIR / filename
        block_textures.save_bmp8(fn(), out)
        entries.append(_entry(
            out, "tools/escapegen/block_textures.py (Python stdlib/Pillow)",
            f"Procedural seamless map block texture for ufoescape: {filename} "
            "(alien-ship hull/floor/goo/vat/lab art, see docs/MISSION-UFO-ESCAPE.md section 4)."))


def build_rock(entries: list) -> int:
    path, poly_count = rock.write_rock_model()
    entries.append(_entry(
        Path(path), "tools/escapegen/rock.py (Python stdlib)",
        f"Procedural MOON ROCK throwable weapon model: lumpy jittered-sphere hull, "
        f"{poly_count} polys, anchored to the M92F template grip/muzzle offsets."))
    rock.save_bmp8(rock.paint_rock_atlas(), rock.OUT_WEAPON / "rock.bmp")
    entries.append(_entry(
        rock.OUT_WEAPON / "rock.bmp", "tools/escapegen/rock.py (Python stdlib/Pillow)",
        "Procedural grey-blue moon rock skin with craters and highlight pocks."))
    return poly_count


def build_tube(entries: list) -> int:
    path, poly_count = tube.write_tube_model()
    entries.append(_entry(
        Path(path), "tools/escapegen/tube.py (Python stdlib)",
        f"Procedural specimen-tube prop model: {tube.SEGMENTS}-sided glass cylinder + "
        f"metal base/cap, {poly_count} polys, file height ~5.2 (world ~26 at SMALLOBJECT_SCALE 5)."))
    tube.save_bmp8(tube.paint_tube_atlas(), tube.OUT_MODEL / "tube.bmp")
    entries.append(_entry(
        tube.OUT_MODEL / "tube.bmp", "tools/escapegen/tube.py (Python stdlib/Pillow)",
        "Procedural specimen-tube skin: glowing green liquid with bubbles, a floating "
        "cow silhouette on one side and an alien embryo silhouette on the other."))
    return poly_count


def build_sounds(entries: list) -> None:
    sounds.OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, (fn, summary) in sounds.RECIPES.items():
        x = fn(sounds.sx.rng_for(sounds.SEED, name))
        out = sounds.OUT_DIR / f"{name}.wav"
        sounds.sx.write_wav(str(out), x)
        entries.append(_entry(
            out, "tools/escapegen/sounds.py (tools/sfxgen, Python stdlib)",
            f"Synthesize an original procedural effect without samples: {summary}"))


def build_previews() -> None:
    from PIL import Image
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    tex_strip = Image.new("RGB", (256 * 10, 256), (20, 20, 24))
    for i, (filename, fn) in enumerate(block_textures.GENERATORS.items()):
        tex_strip.paste(fn(), (i * 256, 0))
    tex_strip.save(PREVIEW_DIR / "block_textures_preview.png")

    rock_img = rock.paint_rock_atlas().resize((256, 256))
    rock_img.save(PREVIEW_DIR / "rock_preview.png")

    tube_img = tube.paint_tube_atlas().resize((256, 256))
    tube_img.save(PREVIEW_DIR / "tube_preview.png")


def main() -> None:
    entries: list = []
    build_block_textures(entries)
    rock_polys = build_rock(entries)
    tube_polys = build_tube(entries)
    build_sounds(entries)
    build_previews()

    with open(PROV_PATH, "w", encoding="utf-8") as fh:
        for e in entries:
            fh.write(json.dumps(e, sort_keys=False) + "\n")

    print(f"Wrote {len(entries)} provenance records to {PROV_PATH}")
    print("Rock poly count:", rock_polys)
    print("Tube poly count:", tube_polys)


if __name__ == "__main__":
    main()
