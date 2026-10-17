#!/usr/bin/env python3
"""Generate our CC0 scope mask; never reads original game data."""
import datetime
import json
from pathlib import Path
from PIL import Image, ImageDraw
import artgen
from dds import engine_load, write_dds


def scope_image(size=128):
    scale = 8
    mask = Image.new('L', (size * scale, size * scale), 255)
    # Inset clear circle, centred symmetrically; supersampling anti-aliases edge.
    ImageDraw.Draw(mask).ellipse((4*scale, 4*scale, (size-4)*scale-1,
                                 (size-4)*scale-1), fill=0)
    mask = mask.resize((size,size), Image.Resampling.LANCZOS)
    im = Image.new('RGBA', (size,size), (0,0,0,255))
    im.putalpha(mask)
    return im


def main():
    root = Path(artgen.REPO)
    dst = root/'assets/pack/data/scope.dds'
    dst.parent.mkdir(parents=True, exist_ok=True)
    write_dds(scope_image(), dst)
    assert not artgen.verify_dds(dst, (128,128), 'dds_argb8')
    w,h,px = engine_load(dst)
    overlay = Image.frombytes('RGBA', (w,h), px)
    checker = Image.new('RGBA', (w,h))
    checker.putdata([(150,150,150,255) if (x//8+y//8)%2 else (210,210,210,255)
                     for y in range(h) for x in range(w)])
    preview = root/'docs/img/scope_preview.png'
    preview.parent.mkdir(parents=True, exist_ok=True)
    Image.alpha_composite(checker, overlay).convert('RGB').save(preview)
    manifest = root/'assets/manifest.json'
    data = json.loads(manifest.read_text())
    for slot in data['slots']:
        if slot['path'] == 'data/scope.dds':
            slot['status'] = 'generated'
    # Retain the manifest's established one-space JSON indentation.
    manifest.write_text(json.dumps(data, indent=1)+'\n')
    record = dict(path='data/scope.dds', sha256=artgen.sha256(dst),
                  generator='Pillow procedural tools/artgen/scope.py',
                  prompt='Original black surround, clear circular aperture, antialiased edge; no reticle (engine draws it).',
                  license='CC0-1.0', author='OpenXOPS-web contributors',
                  date=datetime.date.today().isoformat(), curated_by=None,
                  source='Original mathematical circle mask; no original assets used.')
    with (root/'assets/provenance.jsonl').open('a') as f:
        f.write(json.dumps(record)+'\n')
    print(dst, record['sha256'])


if __name__ == '__main__':
    main()
