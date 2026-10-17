# OpenXOPS-web (unofficial fork)

A port of OpenXOPS 1.080 to the browser (WebAssembly) and Linux. The game logic
is upstream's; the Win32/DirectX layer is replaced with SDL2, OpenAL and
OpenGL ES 3 / WebGL 2.

- Scope and plan: [docs/SCOPE-OF-WORK.md](docs/SCOPE-OF-WORK.md)
- Status: [PROGRESS.md](PROGRESS.md)
- Licensing and data: [NOTICE.md](NOTICE.md). **No X operations data is included.**
  You need your own `xops096.zip`.

Layout: `src/` holds the upstream source (tag `upstream-1.080` is the pristine
import). Upstream's original README follows.

Tip: the upstream source is CP932 until Phase 0 converts it. For readable diffs:

    git config diff.cp932.textconv "iconv -f CP932 -t UTF-8"

---

<img width="800" height="320" alt="Image" src="https://github.com/user-attachments/assets/a076a462-b7c9-4da3-8900-70d5d4a86b6a" />

# OpenXOPS

OpenXOPS is free, open-source, offline FPS(First-person shooter) game. 
It is based on [X operations](https://rucsgames.net/se/xops/), a Japanese freeware FPS released by Team MITEI in 2003.

OpenXOPS is primarily written in C/C++ and is developed for Windows.

## Official Website

https://openxops.net/en/

## Development environment

* Programming language : C/C++
* IDE : Microsoft Visual Studio 2019
* Add SDK : Microsoft DirectX SDK (June 2010)

Used technology...
* 3D/2D Graphics : DirectX or OpenGL
* Input : WindowsAPI or DirectInput
* Sound : ezds.dll or DirectSound
* File : C standard library (stdio.h)
* Physics engine : Original
* AI engine : Original

**Game engine is not used.**

## License

OpenXOPS is New BSD License.

## Source Code

OpenXOPS uses Subversion (SVN) as its official version control system.

**The source code is not hosted on GitHub.**
Please visit the official OpenXOPS website for the source code
and Subversion repository.

https://openxops.net/en/developer.php

---

OpenXOPSは、2003年にチームMITEIが発表した日本製フリーFPS「[X operations](https://rucsgames.net/se/xops/)」（本家XOPS）をもとに開発されている、無料のオープンソース・オフラインFPSです。

主にC/C++で開発され、Windowsで動作します。

## 公式Webサイト

https://openxops.net/

## 開発言語や環境など

* 開発言語 ： C/C++
* 開発環境 ： Microsoft Visual Studio 2019
* 使用SDK ： Microsoft DirectX SDK (June 2010)

一般的に「ゲームエンジン」と呼ばれるものは一切使用せず、全てフルスクラッチで書き上げています。
* 3D/2D描画関係 ： DirectX または OpenGL
* 入力関係 ： DirectInput または WindowsAPI
* サウンド関係 ： 本家XOPS付属の ezds.dll または DirectSound　（※）
* ファイル読み込み関係 ： C言語標準関数
* 物理エンジン ： 独自
* AI関係 ： 独自

## ライセンス

ライセンスは修正BSDライセンスを適用しています。

## ソースコード

OpenXOPSの公式ソースコード管理にはSubversion（SVN）を使用しています。

**GitHub上ではソースコードを公開していません。**
ソースコードおよびSubversionリポジトリについては、
OpenXOPS公式Webサイトをご覧ください。

https://openxops.net/developer.php
