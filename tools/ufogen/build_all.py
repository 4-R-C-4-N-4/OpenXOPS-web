#!/usr/bin/env python3
"""Build every UFO FIELD asset and append PROVENANCE-ADD.jsonl (never
assets/provenance.jsonl -- that file is the parent's merge target).

Usage: python3 -m tools.ufogen.build_all
"""
from __future__ import annotations

import datetime
import hashlib
import json
from pathlib import Path

from tools.ufogen import weapons, laser, cow, human_skins_ufo, block_textures, sounds

ROOT = Path(__file__).resolve().parents[2]
PROV_PATH = ROOT / "PROVENANCE-ADD.jsonl"
PREVIEW_DIR = ROOT / "docs" / "img" / "ufo"
DATE = "2026-10-18"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _entry(path: Path, generator: str, recipe: dict, prompt: str) -> dict:
    rel = str(path.relative_to(ROOT / "assets" / "pack")).replace("\\", "/")
    return {
        "path": rel,
        "sha256": _sha256(path),
        "generator": generator,
        "recipe": recipe,
        "license": "CC0-1.0",
        "author": "OpenXOPS-web contributors (AI-assisted procedural generation)",
        "date": DATE,
        "curated_by": None,
        "prompt": prompt,
    }


def build_weapons(entries: list) -> dict:
    polys = {}
    for key, spec in weapons.UFO_WEAPON_SPECS.items():
        path, poly_count = weapons.write_weapon_model(key)
        polys[key] = poly_count
        weapons.save_bmp8(weapons.paint_weapon_atlas(key), weapons.OUT_WEAPON / f"{key}.bmp")
        entries.append(_entry(
            Path(path), "tools/ufogen/weapons.py (Python stdlib)",
            {"slot": f"model/weapon/{key}.x", "poly_count": poly_count,
             "template": spec["template"], "grip": spec["grip"], "muzzle": spec["muzzle"],
             "source_refs": [spec["source"], "docs/MISSION-UFO-FIELD.md#3", "docs/ASSET-CONTRACT.md#6"]},
            f"Procedural UFO FIELD weapon model: {spec['name']}, anchored to the {spec['template']} template hold offsets."))
        entries.append(_entry(
            weapons.OUT_WEAPON / f"{key}.bmp", "tools/ufogen/weapons.py (Python stdlib/Pillow)",
            {"slot": f"model/weapon/{key}.bmp"},
            f"Procedural weapon skin for {spec['name']}."))
    return polys


def build_laser(entries: list) -> int:
    path, poly_count = laser.write_laser_model()
    entries.append(_entry(
        Path(path), "tools/ufogen/laser.py (Python stdlib)",
        {"slot": "model/laser.x", "poly_count": poly_count,
         "source_refs": ["src/parameter.cpp:1434-1436", "src/object.cpp:2897",
                          "docs/MISSION-UFO-FIELD.md#3"]},
        "Procedural elongated laser bolt model, ~3.5x bullet.x along the travel axis."))
    for name in laser.COLORS:
        out = laser.OUT_MODEL / f"{name}.bmp"
        laser.save_bmp8(laser.paint_laser_texture(name), out)
        entries.append(_entry(
            out, "tools/ufogen/laser.py (Python stdlib/Pillow)", {"slot": f"model/{name}.bmp"},
            f"Procedural laser bolt texture {name} with a white-hot core gradient."))
    return poly_count


def build_cow(entries: list) -> int:
    path, poly_count = cow.write_cow_model()
    entries.append(_entry(
        Path(path), "tools/ufogen/cow.py (Python stdlib)",
        {"slot": "model/cow.x", "poly_count": poly_count,
         "source_refs": ["docs/ASSET-CONTRACT.md#6.4", "docs/MISSION-UFO-FIELD.md#4"]},
        "Procedural low-poly cow prop model sized for SMALLOBJECT_SCALE 5.0."))
    out = cow.OUT_MODEL / "cow.bmp"
    cow.save_bmp8(cow.paint_cow_atlas(), out)
    entries.append(_entry(out, "tools/ufogen/cow.py (Python stdlib/Pillow)", {"slot": "model/cow.bmp"},
                           "Procedural Holstein-patterned cow skin with a pink nose."))
    return poly_count


def build_human_skins(entries: list) -> None:
    for name, filename in human_skins_ufo.FILES.items():
        out = human_skins_ufo.OUT_MODEL / filename
        human_skins_ufo.save_bmp8(human_skins_ufo.make_atlas(name), out)
        entries.append(_entry(
            out, "tools/ufogen/human_skins_ufo.py (Python stdlib/Pillow)",
            {"slot": f"model/{filename}"},
            f"Procedural UFO FIELD human skin: {name}, UV-aligned to HUMAN_ATLAS."))


def build_block_textures(entries: list) -> None:
    for filename, fn in block_textures.GENERATORS.items():
        out = block_textures.OUT_DIR / filename
        block_textures.save_bmp8(fn(), out)
        entries.append(_entry(
            out, "tools/ufogen/block_textures.py (Python stdlib/Pillow)",
            {"slot": f"ufofield/{filename}"},
            f"Procedural seamless map block texture for ufofield: {filename}."))


def build_sounds(entries: list) -> None:
    sounds.OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, (fn, summary) in sounds.RECIPES.items():
        x = fn(sounds.sx.rng_for(sounds.SEED, name))
        out = sounds.OUT_DIR / f"{name}.wav"
        sounds.sx.write_wav(str(out), x)
        entries.append(_entry(
            out, "tools/ufogen/sounds.py (tools/sfxgen, Python stdlib)",
            {"name": name, "seed": sounds.SEED, "source": "tools/ufogen/sounds.py",
             "description": summary},
            f"Synthesize an original procedural effect without samples: {summary}"))


def build_previews() -> None:
    from PIL import Image
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    weapon_strip = Image.new("RGB", (128 * 4, 128), (20, 20, 24))
    for i, key in enumerate(weapons.UFO_WEAPON_SPECS):
        img = weapons.paint_weapon_atlas(key)
        weapon_strip.paste(img, (i * 128, 0))
    weapon_strip.save(PREVIEW_DIR / "weapons_preview.png")

    cow_img = cow.paint_cow_atlas().resize((256, 256))
    cow_img.save(PREVIEW_DIR / "cow_preview.png")

    alien_strip = Image.new("RGB", (256 * 4, 256), (20, 20, 24))
    for i, name in enumerate(human_skins_ufo.FILES):
        alien_strip.paste(human_skins_ufo.make_atlas(name), (i * 256, 0))
    alien_strip.save(PREVIEW_DIR / "human_skins_preview.png")

    laser_strip = Image.new("RGB", (64 * 3, 64), (20, 20, 24))
    for i, name in enumerate(laser.COLORS):
        laser_strip.paste(laser.paint_laser_texture(name), (i * 64, 0))
    laser_strip.save(PREVIEW_DIR / "laser_preview.png")

    tex_strip = Image.new("RGB", (256 * 10, 256), (20, 20, 24))
    for i, (filename, fn) in enumerate(block_textures.GENERATORS.items()):
        tex_strip.paste(fn(), (i * 256, 0))
    tex_strip.save(PREVIEW_DIR / "block_textures_preview.png")


def main() -> None:
    entries: list = []
    polys = build_weapons(entries)
    laser_polys = build_laser(entries)
    cow_polys = build_cow(entries)
    build_human_skins(entries)
    build_block_textures(entries)
    build_sounds(entries)
    build_previews()

    with open(PROV_PATH, "w", encoding="utf-8") as fh:
        for e in entries:
            fh.write(json.dumps(e, sort_keys=False) + "\n")

    print(f"Wrote {len(entries)} provenance records to {PROV_PATH}")
    print("Weapon poly counts:", polys)
    print("Laser poly count:", laser_polys)
    print("Cow poly count:", cow_polys)


if __name__ == "__main__":
    main()
