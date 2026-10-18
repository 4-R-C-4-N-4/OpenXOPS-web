//! @file d3dgraphics.h
//! @brief D3DGraphicsクラスの宣言

//--------------------------------------------------------------------------------
// 
// OpenXOPS
// Copyright (c) 2014-2023, OpenXOPS Project / [-_-;](mikan) All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice, 
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice, 
//   this list of conditions and the following disclaimer in the documentation 
//   and/or other materials provided with the distribution.
// * Neither the name of the OpenXOPS Project nor the names of its contributors 
//   may be used to endorse or promote products derived from this software 
//   without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL OpenXOPS Project BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//--------------------------------------------------------------------------------

// ***** OpenGL core only *****
//
// libjpeg
//    Copyright (C) 1991-2014, Thomas G. Lane, Guido Vollbeding.
//    this software is based in part on the work of the Independent JPEG Group
//
// zlib
//    Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
//
// libpng
//    Copyright (c) 1998-2014 Glenn Randers-Pehrson
//
// ****************************

#ifndef D3DGRAPHICS_H
#define D3DGRAPHICS_H

#define MAX_MODEL 72		//!< 最大モデル数
#define MAX_TEXTURE 72		//!< 最大テクスチャ数

#define CLIPPINGPLANE_NEAR 1.0f		//!< 近クリップ面（≒描画する最小距離）
#define CLIPPINGPLANE_FAR 800.0f	//!< 遠クリップ面（≒描画する最大距離）

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 1		//!< Select include file.
#endif
#include "main.h"

//! @brief 使用するグラフィックスコアの選択
//! @details DirectX 9.0c（固定パイプライン）：0　DirectX 9.0c（シェーダー）：1　OpenGL 1.1：8　OpenGL ES 3.0（OpenXOPS-web）：9
//! @attention DirectX 9.0cのシェーダーを使う場合、"resource.rc"も編集してください。
//! @note [OpenXOPS-web] 0, 1 and 8 are Windows-only. Non-Windows targets default to 9.
#ifndef GRAPHIC_ENGINE
 #ifdef _WIN32
  #define GRAPHIC_ENGINE 0
 #else
  #define GRAPHIC_ENGINE 9
 #endif
#endif

#if (GRAPHIC_ENGINE != 9) && !defined(_WIN32) && !defined(OPENXOPS_PORTABLE_GL)
 #error "GRAPHIC_ENGINE 0, 1 and 8 require Windows; use 9 (GLES) on this platform"
#endif

#if GRAPHIC_ENGINE == 9

#ifdef OPENXOPS_PLATFORM_STUB
 #define GRAPHICS_CORE "none (stub)"		//!< バージョン表示用情報
#else
 #define GRAPHICS_CORE "OpenGL ES 3.0"		//!< バージョン表示用情報
#endif

#elif (GRAPHIC_ENGINE == 0)||(GRAPHIC_ENGINE == 1)

#define BLOCKDATA_GPUMEMORY	//!< @brief ブロックデータを格納するメモリーを選択 @details 定数宣言有効：GPUメモリー　定数宣言無効（コメント化）：メインメモリー

#pragma warning(disable:4819)		//VC++警告防止
#include <d3dx9.h>
#pragma warning(default:4819)

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#if GRAPHIC_ENGINE == 0
 #define GRAPHICS_CORE "DirectX 9.0"				//!< バージョン表示用情報
#else
 #define GRAPHICS_CORE "DirectX 9.0 (Shader)"		//!< バージョン表示用情報
 #define HLSLFILE 201								//!< リソース内のHLSLファイル
#endif

#elif GRAPHIC_ENGINE == 8

//#define ENABLE_OPENGL_LIBJPEGPNG		//!< OpenGLにおいてlibjpeg・libpngを使用する

#include <ctype.h>
#ifdef OPENXOPS_PORTABLE_GL
 using HWND=void*; using HGLRC=void*; using HFONT=void*; using WCHAR=wchar_t;
 struct PAINTSTRUCT { int unused; };
 #define ZeroMemory(p,n) memset((p),0,(n))
#else
 #include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#ifdef ENABLE_OPENGL_LIBJPEGPNG
 #include "lib/libjpeg/jpeglib.h"
 #include "lib/zlib/zlib.h"
 #include "lib/libpng/png.h"

 //※libjpeg.lib, zlib.lib, libpng.lib は自前で用意する必要があります。
 #pragma comment(lib, "lib/libjpeg/libjpeg.lib")
 #pragma comment(lib, "lib/zlib/zlib.lib")
 #pragma comment(lib, "lib/libpng/libpng.lib")

 //#pragma comment(lib, "legacy_stdio_definitions.lib")

 #pragma comment(linker, "/NODEFAULTLIB:libcmt.lib")
 #pragma comment(linker, "/NODEFAULTLIB:libcpmt.lib")
#endif

#define GRAPHICS_CORE "OpenGL 1.1"		//!< バージョン表示用情報

#endif	//GRAPHIC_ENGINE

#if (GRAPHIC_ENGINE == 0)||(GRAPHIC_ENGINE == 1)

//! 3Dポリゴン描画用構造体
typedef struct
{
	D3DXVECTOR3 position;	//!< The position
	DWORD       color;		//!< The color 
	FLOAT       tu;			//!< The texture coordinates
	FLOAT       tv;			//!< The texture coordinates
} VERTEXTXTA;

//! 2Dポリゴン描画用構造体
typedef struct
{
	FLOAT    x;		//!< position
	FLOAT    y;		//!< position
	FLOAT    z;		//!< position
	FLOAT    rhw;	//!< vector
	D3DCOLOR color;	//!< color
	FLOAT    tu;	//!< texture coordinates
	FLOAT    tv;	//!< texture coordinates
} TLVERTX;

#elif GRAPHIC_ENGINE == 8

//! モデルデータの頂点格納構造体
typedef struct
{
	float x;	//!< position
	float y;	//!< position
	float z;	//!< position
	float u;	//!< texture coordinates
	float v;	//!< texture coordinates
} MODELVDATA;

//! モデルデータ構造体
typedef struct
{
	bool useflag;			//!< 有効化フラグ
	int polygons;			//!< ポリゴン数
	float *VertexAry;		//!< 頂点格納配列
	float *ColorAry;		//!< 色格納配列
	float *ColorGrayAry;	//!< 色格納配列（暗い）
	float *TexCoordAry;		//!< テクスチャ座標格納配列
} MODELDATA;

//! テクスチャ構造体
typedef struct
{
	bool useflag;			//!< 有効化フラグ
	int width;				//!< 幅
	int height;				//!< 高さ
	unsigned char *data;	//!< 実データ
} TEXTUREDATA;

#endif	//GRAPHIC_ENGINE


//! @brief 画面描画を行うクラス
//! @details 画面の描画機能やそれに直接関連する処理を行います。
//! @details 具体的に、3D描画・2D描画・モデルファイルやテクスチャ管理　を行う機能があります。
class D3DGraphics
{

#if (GRAPHIC_ENGINE == 0)||(GRAPHIC_ENGINE == 1)

	LPDIRECT3D9 pD3D;					//!< DIRECT3D9のポインタ
	LPDIRECT3DDEVICE9 pd3dDevice;		//!< DIRECT3DDEVICE9のポインタ
	float aspect;						//!< 画面のアスペクト比
	bool fullscreenflag;				//!< フルスクリーン表示
	int LanguageMode;					//!< 言語選択
	LPD3DXMESH pmesh[MAX_MODEL];				//!< （Xファイル用）D3DXMESHのポインタ
	DWORD nummaterials[MAX_MODEL];				//!< （Xファイル用）マテリアル数
	LPDIRECT3DTEXTURE9 ptextures[MAX_TEXTURE];	//!< テクスチャを格納

#if GRAPHIC_ENGINE == 1
	LPDIRECT3DVERTEXSHADER9	hVertexShader;			//!< 頂点シェーダー
	LPD3DXCONSTANTTABLE hVSConstantTable;			//!< 頂点シェーダーの定数テーブル
	D3DXHANDLE hVSWorldVertexPos;					//!< 頂点シェーダーの変換行列
	D3DXHANDLE hVSWorldViewPos;						//!< 頂点シェーダーの変換行列
	D3DXHANDLE hVSWorldViewProj;					//!< 頂点シェーダーの変換行列
	D3DXHANDLE hVSDiffuse;							//!< 頂点シェーダーのディフューズ色
	LPDIRECT3DPIXELSHADER9 pPixelShader;			//!< ピクセルシェーダー
	LPD3DXCONSTANTTABLE pPSConstantTable;			//!< ピクセルシェーダーの定数テーブル
	D3DXHANDLE hPSViewPos;							//!< ピクセルシェーダーのカメラ座標
	D3DXHANDLE hPSEnable2DRendering;				//!< ピクセルシェーダーの2D描画フラグ
	D3DXHANDLE hPSEnableTexture;					//!< ピクセルシェーダーのテクスチャフラグ
	D3DXHANDLE hPSEnableFog;						//!< ピクセルシェーダーのフォグ有効フラグ
	D3DXHANDLE hPSFogColor;							//!< ピクセルシェーダーのフォグ色
	D3DXHANDLE hPSFogStart;							//!< ピクセルシェーダーのフォグの開始深度
	D3DXHANDLE hPSFogEnd;							//!< ピクセルシェーダーのフォグの終了深度
	D3DXMATRIX VSWorldPosMatrix;					//!< 頂点シェーダーに与えた変換行列
	D3DXMATRIX VSViewPosMatrix;						//!< 頂点シェーダーに与えた変換行列
	D3DXMATRIX VSViewProjMatrix;					//!< 頂点シェーダーに与えた変換行列
#endif

	class BlockDataInterface* BlockDataIF;		//!< 読み込んだブロックデータを格納するクラスへのポインタ
	int bs;										//!< ブロック数
	int mapTextureID[TOTAL_BLOCKTEXTURE];		//!< テクスチャ番号
#ifdef BLOCKDATA_GPUMEMORY
	LPDIRECT3DVERTEXBUFFER9 g_pVB;				//!< 頂点情報（GPU管理）
#else
	VERTEXTXTA g_pVertices[MAX_BLOCKS][6][4];	//!< 頂点情報（CPU管理）
#endif

	float HUD_myweapon_x[TOTAL_HAVEWEAPON];		//!< HUDの武器描画 X座標
	float HUD_myweapon_y[TOTAL_HAVEWEAPON];		//!< HUDの武器描画 Y座標
	float HUD_myweapon_z[TOTAL_HAVEWEAPON];		//!< HUDの武器描画 Z座標

	bool StartRenderFlag;			//!< StartRender()関数 実行中を表すフラグ
	bool EnableFogFlag;				//!< フォグ使用中を表すフラグ

	LPD3DXSPRITE ptextsprite;			//!< 2Dテクスチャスプライト
	LPD3DXFONT pxmsfont;				//!< システムフォント
	LPD3DXFONT pxmssmallfont;			//!< システムフォント（小さい）
	char TextureFontFname[_MAX_PATH];	//!< テクスチャフォントのファイル名
	int TextureFont;					//!< テクスチャフォントのテクスチャID

	int InitSubset();
	void CleanupD3Dresource();
	void Start2DMSFontTextRender();
	void End2DMSFontTextRender();
	void Start2DRender();
	void End2DRender();

#elif GRAPHIC_ENGINE == 8

	HWND hWnd;				//!< ウィンドウハンドル
	HGLRC hGLRC;			//!< OpenGLのコンテキスト
	int width;				//!< 幅
	int height;				//!< 高さ
	bool fullscreenflag;	//!< フルスクリーン表示
	int LanguageMode;		//!< 言語選択
	PAINTSTRUCT Paint_ps;					//!< BeginPaint()関数とEndPaint()関数用
	MODELDATA pmodel[MAX_MODEL];			//!< モデルデータを格納
	TEXTUREDATA ptextures[MAX_TEXTURE];		//!< テクスチャを格納
	GLuint textureobjname[MAX_TEXTURE];		//!< テクスチャオブジェクト
	HFONT SystemFont;				//!< システムフォント用の論理フォント
	HFONT SystemSmallFont;			//!< システムフォント用の論理フォント（小さい）
	WCHAR *now_SystemFontUStr;		//!< 現在表示中のシステムフォントによる文字列（Unicode）
	GLuint SystemFontListIdx;		//!< システムフォントのディスプレイリスト
	int SystemFontListIdxSize;		//!< システムフォントのディスプレイリストのサイズ
	float SystemFont_posz;			//!< システムフォントのZ座標
	int now_textureid;		//!< 現在設定中のテクスチャ番号

	float camera_x;			//!< カメラ座標
	float camera_y;			//!< カメラ座標
	float camera_z;			//!< カメラ座標
	float camera_rx;		//!< カメラ回転角度
	float camera_ry;		//!< カメラ回転角度
	float viewangle;		//!< カメラの視野角

	class BlockDataInterface* BlockDataIF;		//!< 読み込んだブロックデータを格納するクラスへのポインタ
	int bs;										//!< ブロック数
	int mapTextureID[TOTAL_BLOCKTEXTURE];		//!< テクスチャ番号

	char TextureFontFname[_MAX_PATH];	//!< テクスチャフォントのファイル名
	int TextureFont;				//!< テクスチャフォントのテクスチャID

	float HUD_myweapon_x[TOTAL_HAVEWEAPON];		//!< HUD_Aの武器描画 X座標
	float HUD_myweapon_y[TOTAL_HAVEWEAPON];		//!< HUD_Aの武器描画 Y座標
	float HUD_myweapon_z[TOTAL_HAVEWEAPON];		//!< HUD_Aの武器描画 Z座標

#ifdef ENABLE_OPENGL_LIBJPEGPNG
	jpeg_decompress_struct cinfo;	//!< libjpeg
	jpeg_error_mgr jerr;			//!< libjpeg
#endif

	void CleanupD3Dresource();
	int CheckTextureFileExtension(const char* filename, int nowformat);
	int CheckTextureFileTypeFlag(const char* filename, int nowformat);
	bool LoadBMPTexture(const char* filename, bool BlackTransparent, TEXTUREDATA *ptexture);
	bool LoadDDSTexture(const char* filename, bool BlackTransparent, TEXTUREDATA *ptexture);
	bool LoadJPEGTexture(const char* filename, bool BlackTransparent, TEXTUREDATA *ptexture);
	bool LoadPNGTexture(const char* filename, bool BlackTransparent, TEXTUREDATA *ptexture);
	void SetTexture(int TextureID);
	int StrMaxLineLen(const char *str);
	void Start2DRender();
	void End2DRender();

#elif GRAPHIC_ENGINE == 9

	//[OpenXOPS-web] platform-neutral state (src/platform/*/d3dgraphics-*.cpp)
	int width;				//!< 幅
	int height;				//!< 高さ
	bool fullscreenflag;	//!< フルスクリーン表示
	int LanguageMode;		//!< 言語選択
	bool modeluse[MAX_MODEL];		//!< モデルの使用フラグ
	bool textureuse[MAX_TEXTURE];	//!< テクスチャの使用フラグ
	int texturewidth[MAX_TEXTURE];	//!< テクスチャの幅
	int textureheight[MAX_TEXTURE];	//!< テクスチャの高さ
	float world_x;			//!< ワールド変換の平行移動（GetWorldTransformPos用）
	float world_y;			//!< ワールド変換の平行移動（GetWorldTransformPos用）
	float world_z;			//!< ワールド変換の平行移動（GetWorldTransformPos用）

	class BlockDataInterface* BlockDataIF;		//!< 読み込んだブロックデータを格納するクラスへのポインタ
	int bs;										//!< ブロック数
	int mapTextureID[TOTAL_BLOCKTEXTURE];		//!< テクスチャ番号

	char TextureFontFname[_MAX_PATH];	//!< テクスチャフォントのファイル名
	int TextureFont;				//!< テクスチャフォントのテクスチャID

	bool StartRenderFlag;			//!< StartRender()関数 実行中を表すフラグ

	void CleanupD3Dresource();

#endif	//GRAPHIC_ENGINE

#ifdef ENABLE_DEBUGCONSOLE
	unsigned char DebugFontData[96][16];	//!< デバック用フォントデータ格納配列
	int TextureDebugFont;					//!< デバック用フォントのテクスチャID
	void InitDebugFontData();
	bool LoadDebugFontTexture();
#endif

public:
	D3DGraphics();
	~D3DGraphics();
	void SetFullScreenFlag(bool fullscreen);
	bool GetFullScreenFlag();
	void SetlanguageMode(int mode);
	int GetlanguageMode();
	int InitD3D(WindowControl *WindowCtrl, const char *TextureFontFilename, bool fullscreen, int language);
	int ResetD3D(WindowControl *WindowCtrl);
	void DestroyD3D();
	int LoadModel(const char* filename);
	int MorphingModel(int idA, int idB);
	int GetTotalModels();
	void CleanupModel(int id);
	int LoadTexture(const char* filename, bool texturefont, bool BlackTransparent);
	int GetTextureSize(int id, int *width, int *height);
	int GetTotalTextures();
	void CleanupTexture(int id);
	int StartRender();
	bool EndRender();
	void ResetZbuffer();
	void ResetWorldTransform();
	void SetWorldTransform(float x, float y, float z, float rx, float ry, float size);
	void SetWorldTransform(float x, float y, float z, float rx, float ry1, float ry2, float size);
	void SetWorldTransformEffect(float x, float y, float z, float rx, float ry, float rt, float size);
	void SetWorldTransformHumanWeapon(float x, float y, float z, float mx, float my, float mz, float rx, float ry, float size);
	void SetWorldTransformPlayerWeapon(bool rotation, float camera_rx, float camera_ry, float rx, float size);
	void GetWorldTransformPos(float *x, float *y, float *z);
	void SetFog(bool enablefog, int skynumber);
	void SetCamera(float camera_x, float camera_y, float camera_z, float camera_rx, float camera_ry, float viewangle);
	void LoadMapdata(class BlockDataInterface* in_BlockDataIF, const char *directory);
	void RenderMapdata(bool wireframe);
	int GetMapTextureID(int id);
	void CleanupMapdata();
	void RenderModel(int id_model, int id_texture, bool darkflag, bool HUDmode, bool nomodel);
	void RenderBoard(int id_texture, float alpha, bool nomodel);
	void StartEffectRender();
	void EndEffectRender();
	void ScreenBrightness(int Width, int Height, int Brightness);
	void RenderCenterline();
	void Renderline(float x1, float y1, float z1, float x2, float y2, float z2, int color);
	void Draw2DMSFontText(int x, int y, const char *str, int color);
	void Draw2DMSFontTextScaling(int x, int y, const char *str, int color);
	void Draw2DMSFontTextCenter(int x, int y, int w, int h, const char *str, int color);
	void Draw2DMSFontTextCenterScaling(int x, int y, int w, int h, const char *str, int color);
	void Draw2DMSSmallFontText(int x, int y, const char *str, int color);
	void GetFontTextSize(const char *str, int base_w, int base_h, int limitlen, int *w, int *h);
	void Draw2DTextureFontText(int x, int y, const char *str, int color, int fontwidth, int fontheight);
	void Draw2DTextureFontTextScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight);
	void Draw2DTextureFontTextCenter(int x, int y, const char *str, int color, int fontwidth, int fontheight);
	void Draw2DTextureFontTextCenterScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight);
#ifdef ENABLE_DEBUGCONSOLE
	void Draw2DTextureDebugFontText(int x, int y, const char *str, int color);
#endif
	void Draw2DLine(int x1, int y1, int x2, int y2, int color);
	void Draw2DCycle(int x, int y, int r, int color);
	void Draw2DBox(int x1, int y1, int x2, int y2, int color);
	void Draw2DBoxScaling(int x1, int y1, int x2, int y2, int color);
	void Draw2DTexture(int x, int y, int id, int width, int height, float alpha);
	void Draw2DTextureScaling(int x, int y, int id, int width, int height, float alpha);
	bool SaveScreenShot(const char *fname);
	int GetColorCode(float red, float green, float blue, float alpha);
};

#endif