# Scoped reticle and held-weapon reproduction

## Parent runtime verification

Tested the shipped legacy-GL renderer, unchanged, with the exact private
`/home/ivy/Downloads/xops096.zip` in Chromium. No original assets or screenshots
are committed. The speculative alpha-state patch was discarded.

- Production Draw2DLine/Draw2DBox black cross over white: 360/360 sampled row
  pixels dark with fog disabled; 360/360 with fog enabled. A proposed alpha-test
  explanation is not supported by this reproduction: opaque black has alpha 1.
- Training Yard: assign PSG1 (weapon ID 2), Classic Shift to scope, look upward
  with the up-arrow. Both thin crosshair axes and thicker scope-edge bars are
  visibly rendered against the sky. Unscoped PSG1 has no sight by original rules.
- All 22 normal weapon model/texture pairs have nonnegative resource handles
  after mission initialization. This alone does not prove visual correctness.
- Actual first-person geometry AND rotating HUD models were inspected for MP5,
  PSG1, M4, AK47 and M249. They render with Home-enabled arms. Default arms-off
  is inherited from original settings. Home toggles it during gameplay.

These cases do not reproduce missing PSG1 crosshairs or invisible held guns.
They do not close every reported missing-asset case, other browser/GPU failures,
all maps, weapon animations, or the Japanese text fallback (still question marks).
Do not call a renderer bug fixed when the tested baseline already passes.

## Repeat locally

Build explicitly with `OPENXOPS_TEST_HOOKS=1 bash tools/build-playable-web.sh`,
serve `build-web`, select the original ZIP and start the actual game first.
Evaluate `tests/browser/reticle-probe.js` in that page: it asserts both 360-pixel
results. It invokes production line/box drawing, not a substitute JS renderer.
The next normal frame replaces this synthetic test image.

For mission reproduction, Enter dismisses the opening; click Training Yard then
Start. `Module._qa_weapon_resources()` must return 22 after mission loading;
`Module._qa_give_weapon(2)` assigns PSG1 for repeatable inspection. Use normal
scope/Home inputs rather than changing their gameplay behavior. Test hooks are
not safe to invoke before main has initialized GL/resources.

Normal builds do not include any QA source or exports. Public staging explicitly
rejects a selected engine script containing `_qa_reticle_pixels`, even if someone
accidentally tries to publish a diagnostic build. Regression tests cover that
rejection before an existing destination is touched. Rebuild WITHOUT the flag
before staging/public deployment. Older fingerprinted local QA files are never
selected by the production page or the artifact allowlist.
