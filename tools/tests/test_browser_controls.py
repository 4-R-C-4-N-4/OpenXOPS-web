"""Regression checks for browser input presets and controls UI wiring."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BrowserControlsTests(unittest.TestCase):
    def test_modern_bindings_use_original_xops_codes(self):
        source = (ROOT / "src/platform/portable/browser-controls.cpp").read_text()
        expected = {
            "KEY_MOVEFORWARD": 54, "KEY_MOVEBACKWARD": 50,
            "KEY_MOVELEFT": 32, "KEY_MOVERIGHT": 35, "KEY_WALK": 20,
            "KEY_JUMP": 17, "KEY_RELOAD": 49, "KEY_DROPWEAPON": 38,
            "KEY_ZOOMSCOPE": 19, "KEY_SHOTMODE": 55,
            "KEY_SWITCHWEAPON": 48, "KEY_WEAPON1": 23,
            "KEY_WEAPON2": 24, "KEY_SHOT": 18,
        }
        for key, code in expected.items():
            with self.subTest(key=key):
                self.assertRegex(source, rf"SetKeycode\({key},\s*{code}\)")
        self.assertIn("if( preset != 1 ){ return; }", source)

    def test_browser_applies_current_choice_after_config_load(self):
        main = (ROOT / "src/main.cpp").read_text()
        load = main.index("GameConfig.LoadExtFile(NULL);")
        apply = main.index("openxops_apply_controls(EM_ASM_INT({ return Module.browserInputPreset")
        initialize = main.index("InitGame(&MainWindow")
        self.assertLess(load, apply)
        self.assertLess(apply, initialize)
        self.assertIn("Module.browserInputPreset", main)
        self.assertNotIn("localStorage.getItem", main)

    def test_browser_pause_requires_explicit_resume_and_tracks_reasons(self):
        page = (ROOT / "web/index.html").read_text()
        main = (ROOT / "src/main.cpp").read_text()
        self.assertIn("browserPaused", page)
        self.assertIn("visibilitychange", page)
        self.assertIn("window.addEventListener('blur'", page)
        self.assertNotIn("window.addEventListener('focus'", page)  # focus must not auto-resume
        self.assertIn("resume-game", page)
        self.assertIn("Module.browserPaused", main)
        self.assertIn("SDL_FlushEvents(SDL_KEYDOWN, SDL_MOUSEWHEEL)", (ROOT / "src/platform/sdl/input-sdl.cpp").read_text())

    def test_accessible_setup_dialog_and_storage_failure(self):
        page = (ROOT / "web/index.html").read_text()
        self.assertIn('id="controls-setup"', page)
        self.assertIn('id="controls"', page)
        self.assertIn('aria-labelledby="controls-title"', page)
        self.assertIn("localStorage.getItem('openxops-input-preset')", page)
        self.assertIn("localStorage.setItem('openxops-input-preset',value)", page)
        self.assertIn("const saved=savePreset(selectedPreset)", page)
        self.assertIn("Module.browserInputPreset=selectedPreset", page)
        self.assertIn("browser storage is unavailable", page)
        self.assertIn("controlsOpener.focus()", page)
        self.assertIn("document.exitPointerLock?.()", page)
        self.assertRegex(page, re.compile(r"controls-dialog.*?addEventListener\('cancel'", re.S))
        self.assertIn("next game start", page)
        self.assertIn("event.stopPropagation()", page)
        self.assertIn("Module._openxops_reset_input", page)
        self.assertIn("SDL_FlushEvents(SDL_KEYDOWN, SDL_MOUSEWHEEL)", (ROOT / "src/platform/sdl/input-sdl.cpp").read_text())
        self.assertNotIn("mouse button 3", page)
        self.assertIn("right mouse aims", page)
        self.assertIn("Home toggles arms/held-weapon visibility", page)
        self.assertNotIn("Tab walks and toggles", page)


if __name__ == "__main__":
    unittest.main()
