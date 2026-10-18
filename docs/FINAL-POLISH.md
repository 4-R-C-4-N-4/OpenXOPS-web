# Final engine polish before the free starter pack

Baseline: 4e35c43, playable deployed preview. Keep original gameplay and the
legacy-GL renderer; no broad rewrite or replacement-art generation in this pass.

## Bounded workstreams

1. Text: replace Draw2DMSFontText's CP932-to-question-mark placeholder with
   readable Japanese. Reuse reviewed recovery work only after inspection; do not
   cherry-pick entire historical platform branches. Ship only verified freely
   redistributable font/code with notices. Test CP932 decoding, malformed input,
   multiline layout and actual menu/briefing pixels. Preserve ASCII HUD rendering.
2. Browser lifecycle: pause on hidden tab/window blur, reset held/queued input,
   release pointer lock, and require explicit user resume. Reuse controls pause
   semantics without letting dialog close override another pause reason. Prevent
   unintended movement/fire on resume. Test real mission + help + fullscreen.
3. Release/pack handoff audit: reproduce remaining issues, inspect diagnostic
   coverage and current free-pack validation. Produce a prioritized issue ledger
   and smallest honest upload-free starter-mission gate. Read-only audit, not new
   graphics generation or declaration of full parity.

## Acceptance and exclusions

Parent reruns combined tests and production WASM build, exercises exact private
Downloads/xops096.zip through real missions, reviews screenshots/console, and
verifies deployed page after publication. Test-only hooks remain excluded from
public artifacts. No original data or derived screenshots enter the repository.

Do not reopen scoped PSG1 or Home-disabled arms as bugs without a failing repro.
Known baseline tests: scoped PSG1 visible; MP5/PSG1/M4/AK47/M249 held geometry and
HUD models visible with Home; 22 normal weapon resource pairs loaded.

Free starter pack follows this pass. Target CC0/public-domain assets, not an
assumption that unlicensed material is usable. Models, sounds, HUD/font, authored
map/point/mission data and textures all need provenance. Preserve local original
ZIP and loose-file override paths as optional modding features. No complete
free-pack readiness claim until an original-free mission actually boots and plays.
