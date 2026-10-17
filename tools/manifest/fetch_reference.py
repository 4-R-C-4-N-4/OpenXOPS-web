#!/usr/bin/env python3
"""Fetch the XOPS 0.96 file list from openxops.net and keep only factual metadata.

Writes tools/manifest/xops096_reference.json with, per original file:
path, size, md5, and width/height/format parsed from the "format details"
column. Prose summaries and the site's map/mission analysis are NOT copied
(the site asks that its analysis not be reproduced).

Usage:
    python3 tools/manifest/fetch_reference.py            # fetch from the web
    python3 tools/manifest/fetch_reference.py --html F   # parse a saved copy

The output is sorted and stable, so re-running against the same page gives
byte-identical JSON. Stdlib only.
"""
import argparse
import html.parser
import json
import os
import re
import sys
import urllib.request

URL = "https://openxops.net/filesystem-xops096filelist.php"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "xops096_reference.json")
MD5_RE = re.compile(r"^[0-9a-f]{32}$")


class _TableParser(html.parser.HTMLParser):
    """Collect every <tr> as a list of cell texts."""

    def __init__(self):
        super().__init__()
        self.rows, self._row, self._cell = [], None, None

    def handle_starttag(self, tag, attrs):
        if tag == "tr":
            self._row = []
        elif tag in ("td", "th") and self._row is not None:
            self._cell = []

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self._row is not None and self._cell is not None:
            self._row.append("".join(self._cell).strip())
            self._cell = None
        elif tag == "tr" and self._row is not None:
            self.rows.append(self._row)
            self._row = None

    def handle_data(self, data):
        if self._cell is not None:
            self._cell.append(data)


def norm_path(p):
    p = p.replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    return p


def parse_format(path, details):
    """Return (width, height, format) from the 'File format details' cell."""
    low = path.lower()
    w = h = None
    m = re.search(r"(\d+)x(\d+)", details)
    if m:
        w, h = int(m.group(1)), int(m.group(2))
    fmt = None
    if "A8R8G8B8" in details:
        fmt = "dds_argb8"
    elif "A4R4G4B4" in details:
        fmt = "dds_argb4"
    elif "256color" in details:
        fmt = "bmp8"
    elif "24bit" in details and low.endswith(".bmp"):
        fmt = "bmp24"
    elif details.startswith("xof 0302txt"):
        fmt = "x_text"
    elif low.endswith(".wav"):
        fmt = "wav_pcm16_mono_22k" if "1ch/22.050kHz/16bit" in details else "wav"
    elif low.endswith(".bd1"):
        fmt = "bd1"
    elif low.endswith(".pd1"):
        fmt = "pd1"
    elif low.endswith((".txt", ".msg")):
        fmt = "text_cp932"
    elif low.endswith((".jpg", ".jpeg")):
        fmt = "jpeg"
    if fmt is None or not (fmt.startswith(("bmp", "dds", "png", "jpeg"))):
        w = h = None
    return w, h, fmt


def parse(html_text):
    p = _TableParser()
    p.feed(html_text)
    out = {}
    for row in p.rows:
        # File path | File type | Format details | Summary | Size | Last modified | Bin/Text | MD5
        if len(row) != 8 or not MD5_RE.match(row[7].lower()):
            continue
        path = norm_path(row[0])
        try:
            size = int(row[4].replace(",", ""))
        except ValueError:
            size = None
        w, h, fmt = parse_format(path, row[2])
        out[path.lower()] = {
            "path": path,
            "size": size,
            "md5": row[7].lower(),
            "width": w,
            "height": h,
            "format": fmt,
        }
    return [out[k] for k in sorted(out)]


def main():
    ap = argparse.ArgumentParser(description="Fetch XOPS 0.96 file metadata (path/size/md5/shape).")
    ap.add_argument("--html", help="parse a saved HTML file instead of fetching")
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    if a.html:
        with open(a.html, encoding="utf-8") as f:
            text = f.read()
    else:
        req = urllib.request.Request(URL, headers={"User-Agent": "Mozilla/5.0 (OpenXOPS-web manifest tool)"})
        with urllib.request.urlopen(req, timeout=60) as r:
            text = r.read().decode("utf-8", "replace")
    files = parse(text)
    if not files:
        sys.exit("no file rows parsed; page layout changed?")
    doc = {
        "source": URL,
        "note": "Factual metadata only (path, size, md5, parsed width/height/format). "
                "Used for shape targets and the clean-room MD5 guard.",
        "files": files,
    }
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False, sort_keys=True)
        f.write("\n")
    print("wrote %s (%d files)" % (os.path.relpath(a.out), len(files)))


if __name__ == "__main__":
    main()
