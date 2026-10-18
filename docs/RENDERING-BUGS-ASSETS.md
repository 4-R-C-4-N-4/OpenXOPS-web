# Rendering and asset bug ledger

## Coverage findings

- `TOTAL_PARAMETERINFO_WEAPON` is 23 (including ID 0, the intentionally empty “none” entry). The source/ZIP auditor examined 22 normal weapon definitions in `/home/ivy/Downloads/xops096.zip`; all 44 nonempty weapon model/texture paths were present. This establishes archive presence only, not successful runtime loading or rendering. Do not count absence of weapon 0 as a missing asset.
- The reviewed model set has 19 `xof 0302txt 0064` headers and 3 `xof 0302txt 0032` headers. Header class alone does not establish parser compatibility; geometry must be exercised.
- The committed `assets/pack` is not the original game-data archive. Auditing all tracked C++ headers/sources in `/home/ivy/Work/OpenXOPS-web-wt/cleanup-assets/src` against `/home/ivy/Downloads/xops096.zip` found 46 non-commented literal load calls: 44 paths present and 2 unverified dynamic paths (not reported missing); zero missing load-call assets. The full reference report also includes 46 weapon-field references, of which two intentionally empty weapon-0 fields are unverified. This is archive presence only, not proof of runtime loading or rendering. No proprietary source archive or copyright asset data is included here.

## Visibility is not a load/render failure

- `Config` initializes `ShowArmFlag=false` (config.cpp:53; also defaults/reset paths around 645–657). `gamemain.cpp:1764` copies that setting to `Camera_HOMEmode`; Home toggles it near 2458. Thus a missing held gun with this option off is intentional visibility behavior, not evidence of a broken weapon model.
- `gamemain.cpp:2787` computes `DrawPlayer` and passes it to `ObjMgr.Render` at 2797, affecting player/arm visibility separately from HUD weapon rendering. HUD weapon models are drawn via `RenderModel(..., HUDmode=true)` around 3272–3278. World weapons/objects use their own non-HUD rendering paths. Test these as separate cases.

## Open investigation ledger (source review; no runtime defect asserted)

1. **Held arm/gun absent:** first reproduce with Home mode on and confirm `ShowArmFlag`, selected weapon ID, and weapon model/texture handles. If handles are negative, classify as load failure; if valid but no draw, inspect object/player draw gating and transforms. Baseline Home mode off is expected to hide arms.
2. **HUD weapon absent/malformed:** independently verify selected weapon model and texture load results, then compare HUD `RenderModel` path with world path. Capture GL state before/after HUD draws; do not infer from world visibility.
3. **World weapon absent:** verify object manager object existence and its model/texture identifiers before investigating renderer. Distinguish absent object from failed asset load and failed draw.
4. **`.x` parse/placement hypothesis:** portable renderer performs `gluUnProject` then `glTranslated` in the examined path (d3dgraphics-portable.cpp:1696–1702); `RenderModel` begins near 1970. Concrete follow-up: log parse success, mesh/vertex counts, model/texture IDs, and matrix mode/stack at entry/exit for representative 0064 and 0032 models; compare one model in HUD and world modes. This is a test proposal, not a confirmed bug.

## Classification rule

Report four stages independently: archive path missing/empty; load/parser rejected; resource handle invalid; valid resource not drawn/visible. Preserve intentional settings as a separate visibility category. A source-only audit cannot prove runtime rendering correctness.
