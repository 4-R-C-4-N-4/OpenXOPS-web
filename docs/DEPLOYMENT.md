# Deployment

## Live preview

https://4-r-c-4-n-4.github.io/OpenXOPS-web/

The portfolio's Games section links here in a new tab:
https://4-r-c-4-n-4.github.io/#adversary

This is a separate GitHub Pages **project site**, hosted under the same domain
as the portfolio. It does not overwrite the root site or `/tiny-games/`.

## Existing architecture and integration

- `4-R-C-4-N-4.github.io`: root portfolio, deployed from main/root by Pages.
  Its cabinet UI fetches `/tiny-games/games.json` and supports `embed:false`
  for new-tab games.
- `tiny-games`: independently builds its games and manifest through its Pages
  workflow to `/tiny-games/`. No changes to that build are required.
- `OpenXOPS-web`: independently compiles and deploys the WASM preview to
  `/OpenXOPS-web/` through `.github/workflows/deploy-pages.yml`.
- The root portfolio keeps a small `SITE_GAMES` list for independent projects;
  OpenXOPS has `url:'/OpenXOPS-web/'` and `embed:false`. Existing manifest
  games retain their original paths and embedding behaviour.

## Automatic updates

Push game code, web shell, tools or workflow changes to OpenXOPS-web/main.
The workflow pins Emscripten6.0.12, runs tooling tests, builds WASM, stages the
allowlisted public files, uploads a Pages artifact and deploys it. Manual
`workflow_dispatch` is also enabled. No cross-repository PAT/deploy key is
needed; Pages uses the workflow's short-lived GitHub token and OIDC.

Only the first addition of the portfolio cabinet changes the root-site repo.
Subsequent game updates deploy independently, so its link remains stable.

## Public artifact boundary

`tools/stage_pages.py` copies only:
- `index.html`
- the JS and WASM files referenced by that page (matching content fingerprint)
- engine `LICENSE` and `NOTICE.md`
- `.nojekyll` (hidden files may be omitted by artifact upload; not needed for Actions Pages)

No original ZIP, extracted data, source files, local saves or screenshots are
published. A metadata MD5 check rejects accidentally matching original assets.
The downloaded first production artifact was inspected: page, matching JS/WASM,
LICENSE and NOTICE only. Player game-data selection remains browser-local.

## Local preflight

```sh
bash tools/build-playable-web.sh
python3 tools/stage_pages.py
python3 -m http.server 8766 --bind 127.0.0.1 --directory build-pages
```

Never serve or publish the repository root containing private `xopsdata/`.

## Browser storage origin

The live site's remembered archive is separate from localhost: browser storage
is origin-scoped. Select the ZIP once on the public URL; it will then be
remembered on later visits to that origin. No cookies or server uploads.

## Verified first deployment

GitHub Actions run39134157979 built and deployed successfully. Chromium at the
public project URL loaded the original Downloads/xops096.zip, remembered it,
and reached Training Yard first-person gameplay. Relative fingerprinted JS/WASM
URLs work under the project subpath. Known preview limitations remain documented
in RUNNING.md.
