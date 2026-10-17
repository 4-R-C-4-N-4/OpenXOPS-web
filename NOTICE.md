# NOTICE

OpenXOPS-web is an **unofficial** fork of OpenXOPS 1.080. It is not affiliated
with or endorsed by the OpenXOPS Project, [-_-;](mikan), or Team MITEI.

## What is in this repository

- `src/`: OpenXOPS 1.080 source, (c) 2014-2023 OpenXOPS Project / [-_-;](mikan),
  New BSD License (see `LICENSE`). Imported verbatim at tag `upstream-1.080`.
  Every later change is visible with `git diff upstream-1.080 -- src`.
- `src/lib/`: headers and prebuilt Windows libraries for zlib (zlib License),
  libpng (libpng License) and libjpeg (IJG License), as shipped upstream. The
  web build uses Emscripten's own ports instead.

## What is NOT in this repository, and must never be added

- **X operations (本家XOPS) data**: graphics, models, maps, missions and sounds.
  The graphics are copyrighted by TENKUU and have not been licensed for
  OpenXOPS or derived projects (openxops.net, developer-original.php). Players
  supply their own copy of `xops096.zip` from
  https://www.vector.co.jp/soft/win95/game/se284467.html
- `ezds.dll` and `config4xops05.exe`, (c) 2003-2006 nine-two. Neither is used.
