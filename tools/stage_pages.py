#!/usr/bin/env python3
"""Stage only the exact fingerprinted engine/page files for public hosting.
Never copy the repository, xopsdata, user archives, or raw build directory.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]

def stage(build, destination):
    html = (build / 'index.html').read_text()
    scripts = re.findall(r'<script src="(openxops\.[0-9a-f]{16}\.js)"', html)
    wasm = re.findall(r"'?(openxops\.[0-9a-f]{16}\.wasm)'?", html)
    if len(scripts) != 1 or len(wasm) != 1:
        raise ValueError('Expected exactly one fingerprinted engine script and WASM reference')
    if scripts[0].split('.')[1] != wasm[0].split('.')[1]:
        raise ValueError('Engine fingerprints differ')
    names = ['index.html', scripts[0], wasm[0], 'LICENSE']
    if '_qa_reticle_pixels' in (build / scripts[0]).read_text():
        raise ValueError('Refusing to publish a diagnostic/test-hook engine build')
    # Detect accidental original assets even if renamed; metadata only is committed.
    reference = json.loads((ROOT/'tools/manifest/xops096_reference.json').read_text())
    if isinstance(reference, dict):
        records = reference.get('files', [])
    else:
        records = reference
    originals = {r['md5'] for r in records if r.get('md5')}
    for name in names:
        data = (build/name).read_bytes()
        if hashlib.md5(data).hexdigest() in originals:
            raise ValueError('Original game asset detected: '+name)
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in names:
        shutil.copyfile(build/name, destination/name)
    shutil.copyfile(ROOT/'NOTICE.md', destination/'NOTICE.md')
    (destination/'.nojekyll').touch()
    return names

if __name__ == '__main__':
    print('Public files:', stage(ROOT/'build-web', ROOT/'build-pages'))
