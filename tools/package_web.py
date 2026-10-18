#!/usr/bin/env python3
"""Publish matching fingerprinted JS/WASM names and atomically replace the page.
Run after em++ or after HTML-only edits. No game assets are packaged.
"""
from pathlib import Path
import hashlib
import os
import shutil

ROOT = Path(__file__).resolve().parents[1]

def package(build, template):
    js = (build / 'openxops.js').read_bytes()
    wasm = (build / 'openxops.wasm').read_bytes()
    version = hashlib.sha256(js + wasm).hexdigest()[:16]
    js_name, wasm_name = f'openxops.{version}.js', f'openxops.{version}.wasm'
    shutil.copyfile(build / 'openxops.js', build / js_name)
    shutil.copyfile(build / 'openxops.wasm', build / wasm_name)
    html = template.read_text()
    for token in ('__ENGINE_SCRIPT__', '__ENGINE_WASM__'):
        if token not in html:
            raise ValueError(f'Missing build token {token}')
    html = html.replace('__ENGINE_SCRIPT__', js_name).replace('__ENGINE_WASM__', wasm_name)
    temporary = build / 'index.html.next'
    temporary.write_text(html)
    os.replace(temporary, build / 'index.html')
    return version

if __name__ == '__main__':
    print('Packaged engine build:', package(ROOT / 'build-web', ROOT / 'web/index.html'))
