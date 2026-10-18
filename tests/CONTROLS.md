# Browser controls regression checks

Run `python3 -m unittest discover -s tools/tests -v` and `python3 -m unittest discover -s tools -p 'test_*.py' -v` for the existing archive, packaging, build, and asset checks. Run `tools/build-playable-web.sh` with Emscripten installed to exercise the playable WASM build and fingerprinted HTML packaging.

Controls acceptance checks:

- Before starting, keyboard-tab to the labelled Classic/Modern selector and Controls button. Changing preset is saved locally; a blocked `localStorage` leaves Classic selected and does not block play.
- Open Controls by keyboard, verify modal focus moves to Close, read the selected preset and its bindings, close with Escape, and verify focus returns to Controls. Pointer lock is released before opening.
- Classic does not modify config.dat bindings. Modern applies the browser-only key map after game and extension config load, on the next game start only; the original ZIP is never rewritten.
- Check Modern's original XOPS codes: WASD 54/32/50/35, Shift 20, Space 17, R 49, G 38, right mouse 19, X 55, Q 48, slots 23/24, and left mouse 18. Camera arrow-key bindings remain untouched.
- Browser launch with a validated user-selected ZIP should remain local (no upload). Start play, open/close Controls, and verify the main-loop frame counter resumes and pointer lock remains released until the user clicks the canvas.

No arbitrary key editor or in-game hot rebinding is provided. Presets apply on next start/mission initialization.