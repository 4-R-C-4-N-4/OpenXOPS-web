#!/usr/bin/env python3
"""Report source-referenced asset presence in an original OpenXOPS ZIP (not render validity)."""
import argparse
import json
import re
import zipfile
from pathlib import Path

CALL = re.compile(r'\bLoad(?:Model|Texture|Sound)\s*\(\s*("(?:\\.|[^"\\])*")')
WEAPON = re.compile(r'Weapon\s*\[\s*(\d+)\s*\]\s*\.\s*(name|model|texture)\s*=\s*("(?:\\.|[^"\\])*")\s*;')


def c_string(token):
    """Decode a C++ ordinary string literal using its source UTF-8 representation."""
    try:
        return bytes(token[1:-1], 'utf-8').decode('unicode_escape')
    except (UnicodeDecodeError, ValueError):
        return None


def normalize(path):
    path = path.replace('\\', '/')
    while path.startswith('./'):
        path = path[2:]
    path = re.sub(r'/+', '/', path)
    parts = path.split('/')
    if not path or any(part in ('.', '..') for part in parts):
        return None
    return '/'.join(parts).casefold()


def strip_cpp_comments(text):
    """Replace C++ comments with spaces, preserving quoted literals and newlines."""
    out = list(text)
    i = 0
    state = 'code'
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ''
        if state == 'code':
            if ch == '"':
                state = 'string'
            elif ch == "'":
                state = 'char'
            elif ch == '/' and nxt == '/':
                out[i] = out[i + 1] = ' '
                i += 1
                state = 'line-comment'
            elif ch == '/' and nxt == '*':
                out[i] = out[i + 1] = ' '
                i += 1
                state = 'block-comment'
        elif state in ('string', 'char'):
            if ch == '\\' and i + 1 < len(text):
                i += 1
            elif (state == 'string' and ch == '"') or (state == 'char' and ch == "'"):
                state = 'code'
        elif state == 'line-comment':
            if ch == '\n':
                state = 'code'
            else:
                out[i] = ' '
        elif state == 'block-comment':
            if ch == '*' and nxt == '/':
                out[i] = out[i + 1] = ' '
                i += 1
                state = 'code'
            elif ch != '\n':
                out[i] = ' '
        i += 1
    return ''.join(out)


def source_literals(text):
    out = []
    code = strip_cpp_comments(text)
    for match in CALL.finditer(code):
        decoded = c_string(match.group(1))
        if decoded is not None:
            out.append({'path': decoded, 'source': 'load-call'})
    weapons = {}
    for match in WEAPON.finditer(code):
        idx, field = int(match.group(1)), match.group(2)
        value = c_string(match.group(3))
        weapons.setdefault(idx, {})[field] = value
    for idx, data in sorted(weapons.items()):
        if idx == 0 and data.get('name') == 'NONE' and data.get('model') == '' and data.get('texture') == '':
            continue
        for field in ('model', 'texture'):
            if field in data and data[field] is not None:
                out.append({'path': data[field], 'source': f'weapon:{idx}:{data.get("name", "")}'})
    return out


def audit(archive, sources):
    with zipfile.ZipFile(archive) as z:
        entries = {}
        for name in z.namelist():
            if name.endswith('/'):
                continue
            key = normalize(name)
            if key is not None:
                entries[key] = name
        refs, weapons = [], {}
        for source in sources:
            text = source.read_text(encoding='utf-8', errors='replace')
            refs.extend(source_literals(text))
            for m in WEAPON.finditer(strip_cpp_comments(text)):
                idx, field = int(m.group(1)), m.group(2)
                val = c_string(m.group(3))
                weapons.setdefault(idx, {'id': idx, 'name': None, 'model': None, 'texture': None})[field] = val
        result = []
        for ref in refs:
            path = ref['path']; key = normalize(path)
            if key is None:
                status = 'unverified'
            elif path == '':
                status = 'empty'
            elif key not in entries:
                status = 'missing'
            else:
                status = 'present'
            ref.update({'normalized': key, 'status': status, 'format': Path(path).suffix.lower().lstrip('.') or None})
            result.append(ref)
        weapon_rows = []
        for idx, row in sorted(weapons.items()):
            if idx == 0 and row['name'] == 'NONE' and row['model'] == '' and row['texture'] == '':
                continue
            assets = {}
            for field in ('model', 'texture'):
                path = row[field]
                key = normalize(path) if path else None
                assets[field] = {'path': path, 'format': Path(path).suffix.lower().lstrip('.') or None,
                                 'status': 'empty' if path == '' else 'unverified' if path is None or key is None else 'present' if key in entries else 'missing'}
            weapon_rows.append({'id': idx, 'name': row['name'], **assets})
        return {'references': result, 'weapons': weapon_rows,
                'missing': [r for r in result if r['status'] == 'missing'],
                'empty': [r for r in result if r['status'] == 'empty']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('zip'); parser.add_argument('sources', nargs='+')
    parser.add_argument('--json', action='store_true', help='emit machine-readable JSON')
    args = parser.parse_args()
    report = audit(args.zip, [Path(p) for p in args.sources])
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(json.dumps(report, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
