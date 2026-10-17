#!/usr/bin/env python3
"""Run 100 ticks per SDL driver on disposable COPY of legally obtained game data.
Usage: python tests/sdl-game-smoke.py build-sdl/openxops xopsdata/extracted
No original assets or generated configs are written to the repository.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve(strict=True)
source = Path(sys.argv[2]).resolve(strict=True)
assert (source / "data").is_dir(), "Expected extracted game root containing data/"
for driver in ("offscreen", "dummy"):
    with tempfile.TemporaryDirectory(prefix="openxops-sdl-", dir=os.environ.get("TMPDIR")) as root:
        target = Path(root) / "game"
        shutil.copytree(source, target)
        env = dict(os.environ, SDL_VIDEODRIVER=driver, OPENXOPS_MAX_TICKS="100",
                   LIBGL_ALWAYS_SOFTWARE="1", ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1")
        mesa = Path("/usr/share/glvnd/egl_vendor.d/50_mesa.json")
        if mesa.exists():
            env["__EGL_VENDOR_LIBRARY_FILENAMES"] = str(mesa)
        result = subprocess.run([str(exe), "-Dir", str(target)], env=env,
                                capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        print(output, end="")
        assert result.returncode == 0, f"{driver}: exit {result.returncode}"
        assert "[sdl] 100 ticks done, closing" in output, f"{driver}: tick bound not reached"
        assert "Sanitizer" not in output and "runtime error:" not in output
        expected = "GLES3 context created" if driver == "offscreen" else "dummy video: no GL context"
        assert expected in output
        print(f"{driver}: 100 ticks, clean exit, no sanitizer reports")
