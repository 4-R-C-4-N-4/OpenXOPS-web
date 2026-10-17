//! @file d3dgraphics-stub.cpp
//! @brief [OpenXOPS-web] No-op stub of D3DGraphics for the Phase 0 native build.
//!        Nothing is drawn. LoadModel()/LoadTexture() only check that the file
//!        can be opened and hand out a slot number, so resource bookkeeping in
//!        game logic follows the same paths as with a real renderer.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#include "../../d3dgraphics.h"

#include <stdio.h>
#include <string.h>

//! @brief ファイルが開けるか確認
static bool StubFileExists(const char *filename)
{
	if( filename == NULL ){ return false; }
#ifdef ENABLE_PATH_DELIMITER_SLASH
	filename = ChangePathDelimiter(filename);
#endif
	FILE *fp = fopen(filename, "rb");
	if( fp == NULL ){ return false; }
	fclose(fp);
	return true;
}

D3DGraphics::D3DGraphics()
{
	width = 0;
	height = 0;
	fullscreenflag = false;
	LanguageMode = 0;
	for(int i=0; i<MAX_MODEL; i++){ modeluse[i] = false; }
	for(int i=0; i<MAX_TEXTURE; i++){
		textureuse[i] = false;
		texturewidth[i] = 0;
		textureheight[i] = 0;
	}
	world_x = 0.0f;
	world_y = 0.0f;
	world_z = 0.0f;

	BlockDataIF = NULL;
	bs = 0;
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		mapTextureID[i] = -1;
	}

	TextureFontFname[0] = '\0';
	TextureFont = -1;
	StartRenderFlag = false;

#ifdef ENABLE_DEBUGCONSOLE
	memset(DebugFontData, 0, sizeof(DebugFontData));
	TextureDebugFont = -1;
#endif
}

D3DGraphics::~D3DGraphics()
{
	DestroyD3D();
}

void D3DGraphics::SetFullScreenFlag(bool fullscreen){ fullscreenflag = fullscreen; }
bool D3DGraphics::GetFullScreenFlag(){ return fullscreenflag; }
void D3DGraphics::SetlanguageMode(int mode){ LanguageMode = mode; }
int D3DGraphics::GetlanguageMode(){ return LanguageMode; }

//! @return 成功：0　失敗：1
int D3DGraphics::InitD3D(WindowControl *WindowCtrl, const char *TextureFontFilename, bool fullscreen, int language)
{
	if( WindowCtrl == NULL ){ return 1; }
	if( TextureFontFilename == NULL ){ return 1; }
	if( strlen(TextureFontFilename) >= sizeof(TextureFontFname) ){ return 1; }

	width = GameConfig.GetScreenWidth();
	height = GameConfig.GetScreenHeight();
	fullscreenflag = fullscreen;
	LanguageMode = language;

	strcpy(TextureFontFname, TextureFontFilename);
	TextureFont = LoadTexture(TextureFontFname, true, false);

#ifdef ENABLE_DEBUGCONSOLE
	InitDebugFontData();
	if( LoadDebugFontTexture() == false ){
		return 1;
	}
#endif

	return 0;
}

//! @return 成功：0　待ち：1　失敗：2
int D3DGraphics::ResetD3D(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 2; }
	return 0;
}

void D3DGraphics::DestroyD3D()
{
	CleanupD3Dresource();
}

void D3DGraphics::CleanupD3Dresource()
{
	for(int i=0; i<MAX_MODEL; i++){ modeluse[i] = false; }
	for(int i=0; i<MAX_TEXTURE; i++){ textureuse[i] = false; }
	TextureFont = -1;
#ifdef ENABLE_DEBUGCONSOLE
	TextureDebugFont = -1;
#endif
}

#ifdef ENABLE_DEBUGCONSOLE
//! @return 成功：true　失敗：false
bool D3DGraphics::LoadDebugFontTexture()
{
	for(int i=0; i<MAX_TEXTURE; i++){
		if( textureuse[i] == false ){
			textureuse[i] = true;
			texturewidth[i] = 8 * 16;
			textureheight[i] = 16 * 8;
			TextureDebugFont = i;
			return true;
		}
	}
	return false;
}
#endif

//! @return 成功：モデルのデータ番号（0以上）　失敗：-1
int D3DGraphics::LoadModel(const char* filename)
{
	if( StubFileExists(filename) == false ){ return -1; }
	for(int i=0; i<MAX_MODEL; i++){
		if( modeluse[i] == false ){
			modeluse[i] = true;
			return i;
		}
	}
	return -1;
}

//! @return 成功：新しいモデルのデータ番号（0以上）　失敗：-1
int D3DGraphics::MorphingModel(int idA, int idB)
{
	if( (idA < 0)||(MAX_MODEL <= idA) ){ return -1; }
	if( modeluse[idA] == false ){ return -1; }
	if( (idB < 0)||(MAX_MODEL <= idB) ){ return -1; }
	if( modeluse[idB] == false ){ return -1; }

	for(int i=0; i<MAX_MODEL; i++){
		if( modeluse[i] == false ){
			modeluse[i] = true;
			return i;
		}
	}
	return -1;
}

int D3DGraphics::GetTotalModels()
{
	int cnt = 0;
	for(int i=0; i<MAX_MODEL; i++){
		if( modeluse[i] == true ){ cnt += 1; }
	}
	return cnt;
}

void D3DGraphics::CleanupModel(int id)
{
	if( (id < 0)||(MAX_MODEL <= id) ){ return; }
	modeluse[id] = false;
}

//! @return 成功：テクスチャのデータ番号（0以上）　失敗：-1
int D3DGraphics::LoadTexture(const char* filename, bool texturefont, bool BlackTransparent)
{
	UNREFERENCED_PARAMETER(texturefont);
	UNREFERENCED_PARAMETER(BlackTransparent);

	if( StubFileExists(filename) == false ){ return -1; }
	for(int i=0; i<MAX_TEXTURE; i++){
		if( textureuse[i] == false ){
			textureuse[i] = true;
			texturewidth[i] = 0;	//画像は読まない（サイズ不明）
			textureheight[i] = 0;
			return i;
		}
	}
	return -1;
}

//! @return 成功：0　失敗：1
int D3DGraphics::GetTextureSize(int id, int *out_width, int *out_height)
{
	if( (id < 0)||(MAX_TEXTURE <= id) ){ return 1; }
	if( textureuse[id] == false ){ return 1; }
	if( (out_width == NULL)||(out_height == NULL) ){ return 1; }

	*out_width = texturewidth[id];
	*out_height = textureheight[id];
	return 0;
}

int D3DGraphics::GetTotalTextures()
{
	int cnt = 0;
	for(int i=0; i<MAX_TEXTURE; i++){
		if( textureuse[i] == true ){ cnt += 1; }
	}
	return cnt;
}

void D3DGraphics::CleanupTexture(int id)
{
	if( (id < 0)||(MAX_TEXTURE <= id) ){ return; }
	textureuse[id] = false;
}

//! @return 成功：0　失敗：1
int D3DGraphics::StartRender()
{
	StartRenderFlag = true;
	return 0;
}

//! @return 成功：false　失敗：true
bool D3DGraphics::EndRender()
{
	StartRenderFlag = false;
	return false;
}

void D3DGraphics::ResetZbuffer(){}

void D3DGraphics::ResetWorldTransform()
{
	world_x = 0.0f;
	world_y = 0.0f;
	world_z = 0.0f;
}

void D3DGraphics::SetWorldTransform(float x, float y, float z, float rx, float ry, float size)
{
	UNREFERENCED_PARAMETER(rx); UNREFERENCED_PARAMETER(ry); UNREFERENCED_PARAMETER(size);
	world_x = x; world_y = y; world_z = z;
}

void D3DGraphics::SetWorldTransform(float x, float y, float z, float rx, float ry1, float ry2, float size)
{
	UNREFERENCED_PARAMETER(rx); UNREFERENCED_PARAMETER(ry1); UNREFERENCED_PARAMETER(ry2); UNREFERENCED_PARAMETER(size);
	world_x = x; world_y = y; world_z = z;
}

void D3DGraphics::SetWorldTransformEffect(float x, float y, float z, float rx, float ry, float rt, float size)
{
	UNREFERENCED_PARAMETER(rx); UNREFERENCED_PARAMETER(ry); UNREFERENCED_PARAMETER(rt); UNREFERENCED_PARAMETER(size);
	world_x = x; world_y = y; world_z = z;
}

//! @note 上流では手の位置（mx,my,mz）を回転してから加算する。スタブは平行移動のみ近似。
void D3DGraphics::SetWorldTransformHumanWeapon(float x, float y, float z, float mx, float my, float mz, float rx, float ry, float size)
{
	UNREFERENCED_PARAMETER(mx); UNREFERENCED_PARAMETER(my); UNREFERENCED_PARAMETER(mz);
	UNREFERENCED_PARAMETER(rx); UNREFERENCED_PARAMETER(ry); UNREFERENCED_PARAMETER(size);
	world_x = x; world_y = y; world_z = z;
}

void D3DGraphics::SetWorldTransformPlayerWeapon(bool rotation, float camera_rx, float camera_ry, float rx, float size)
{
	UNREFERENCED_PARAMETER(rotation); UNREFERENCED_PARAMETER(camera_rx); UNREFERENCED_PARAMETER(camera_ry);
	UNREFERENCED_PARAMETER(rx); UNREFERENCED_PARAMETER(size);
}

void D3DGraphics::GetWorldTransformPos(float *x, float *y, float *z)
{
	if( (x == NULL)||(y == NULL)||(z == NULL) ){ return; }
	*x = world_x;
	*y = world_y;
	*z = world_z;
}

void D3DGraphics::SetFog(bool enablefog, int skynumber)
{
	UNREFERENCED_PARAMETER(enablefog); UNREFERENCED_PARAMETER(skynumber);
}

void D3DGraphics::SetCamera(float camera_x, float camera_y, float camera_z, float camera_rx, float camera_ry, float viewangle)
{
	UNREFERENCED_PARAMETER(camera_x); UNREFERENCED_PARAMETER(camera_y); UNREFERENCED_PARAMETER(camera_z);
	UNREFERENCED_PARAMETER(camera_rx); UNREFERENCED_PARAMETER(camera_ry); UNREFERENCED_PARAMETER(viewangle);
}

//! @brief マップデータを取り込む（上流と同じくブロック用テクスチャを読み込む）
void D3DGraphics::LoadMapdata(BlockDataInterface* in_BlockDataIF, const char *directory)
{
	if( in_BlockDataIF == NULL ){ return; }
	if( directory == NULL ){ return; }

	char fname[MAX_PATH];
	char fnamefull[MAX_PATH];

	BlockDataIF = in_BlockDataIF;
	bs = BlockDataIF->GetTotaldatas();

	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		BlockDataIF->GetTexture(fname, i);

		if( strcmp(fname, "") == 0 ){
			mapTextureID[i] = -1;
		}
		else{
			snprintf(fnamefull, sizeof(fnamefull), "%s%s", directory, fname);
			mapTextureID[i] = LoadTexture(fnamefull, false, false);
		}
	}
}

void D3DGraphics::RenderMapdata(bool wireframe){ UNREFERENCED_PARAMETER(wireframe); }

//! @return テクスチャのデータ番号（失敗：-1）
int D3DGraphics::GetMapTextureID(int id)
{
	if( (id < 0)||(TOTAL_BLOCKTEXTURE <= id ) ){ return -1; }
	return mapTextureID[id];
}

void D3DGraphics::CleanupMapdata()
{
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		CleanupTexture(mapTextureID[i]);
	}
	bs = 0;
	BlockDataIF = NULL;
}

void D3DGraphics::RenderModel(int id_model, int id_texture, bool darkflag, bool HUDmode, bool nomodel)
{
	UNREFERENCED_PARAMETER(id_model); UNREFERENCED_PARAMETER(id_texture); UNREFERENCED_PARAMETER(darkflag);
	UNREFERENCED_PARAMETER(HUDmode); UNREFERENCED_PARAMETER(nomodel);
}

void D3DGraphics::RenderBoard(int id_texture, float alpha, bool nomodel)
{
	UNREFERENCED_PARAMETER(id_texture); UNREFERENCED_PARAMETER(alpha); UNREFERENCED_PARAMETER(nomodel);
}

void D3DGraphics::StartEffectRender(){}
void D3DGraphics::EndEffectRender(){}

void D3DGraphics::ScreenBrightness(int Width, int Height, int Brightness)
{
	UNREFERENCED_PARAMETER(Width); UNREFERENCED_PARAMETER(Height); UNREFERENCED_PARAMETER(Brightness);
}

void D3DGraphics::RenderCenterline(){}

void D3DGraphics::Renderline(float x1, float y1, float z1, float x2, float y2, float z2, int color)
{
	UNREFERENCED_PARAMETER(x1); UNREFERENCED_PARAMETER(y1); UNREFERENCED_PARAMETER(z1);
	UNREFERENCED_PARAMETER(x2); UNREFERENCED_PARAMETER(y2); UNREFERENCED_PARAMETER(z2); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DMSFontText(int x, int y, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DMSFontTextScaling(int x, int y, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DMSFontTextCenter(int x, int y, int w, int h, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(w); UNREFERENCED_PARAMETER(h);
	UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DMSFontTextCenterScaling(int x, int y, int w, int h, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(w); UNREFERENCED_PARAMETER(h);
	UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DMSSmallFontText(int x, int y, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}

//! @brief 文字列の表示サイズを計算（上流と同一の計算。描画に依存しない）
void D3DGraphics::GetFontTextSize(const char *str, int base_w, int base_h, int limitlen, int *w, int *h)
{
	*w = base_w;
	*h = base_h;
	if( str == NULL ){ return; }
	if( limitlen <= 0 ){ return; }

	if( (int)strlen(str) <= limitlen ){ return; }

	float percent = (float)limitlen / strlen(str);

	*w = (int)((float)base_w * percent);
	*h = (int)((float)base_h * percent);
	return;
}

void D3DGraphics::Draw2DTextureFontText(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
	UNREFERENCED_PARAMETER(fontwidth); UNREFERENCED_PARAMETER(fontheight);
}

void D3DGraphics::Draw2DTextureFontTextScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
	UNREFERENCED_PARAMETER(fontwidth); UNREFERENCED_PARAMETER(fontheight);
}

void D3DGraphics::Draw2DTextureFontTextCenter(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
	UNREFERENCED_PARAMETER(fontwidth); UNREFERENCED_PARAMETER(fontheight);
}

void D3DGraphics::Draw2DTextureFontTextCenterScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
	UNREFERENCED_PARAMETER(fontwidth); UNREFERENCED_PARAMETER(fontheight);
}

#ifdef ENABLE_DEBUGCONSOLE
void D3DGraphics::Draw2DTextureDebugFontText(int x, int y, const char *str, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(str); UNREFERENCED_PARAMETER(color);
}
#endif

void D3DGraphics::Draw2DLine(int x1, int y1, int x2, int y2, int color)
{
	UNREFERENCED_PARAMETER(x1); UNREFERENCED_PARAMETER(y1); UNREFERENCED_PARAMETER(x2); UNREFERENCED_PARAMETER(y2);
	UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DCycle(int x, int y, int r, int color)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(r); UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DBox(int x1, int y1, int x2, int y2, int color)
{
	UNREFERENCED_PARAMETER(x1); UNREFERENCED_PARAMETER(y1); UNREFERENCED_PARAMETER(x2); UNREFERENCED_PARAMETER(y2);
	UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DBoxScaling(int x1, int y1, int x2, int y2, int color)
{
	UNREFERENCED_PARAMETER(x1); UNREFERENCED_PARAMETER(y1); UNREFERENCED_PARAMETER(x2); UNREFERENCED_PARAMETER(y2);
	UNREFERENCED_PARAMETER(color);
}

void D3DGraphics::Draw2DTexture(int x, int y, int id, int in_width, int in_height, float alpha)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(id);
	UNREFERENCED_PARAMETER(in_width); UNREFERENCED_PARAMETER(in_height); UNREFERENCED_PARAMETER(alpha);
}

void D3DGraphics::Draw2DTextureScaling(int x, int y, int id, int in_width, int in_height, float alpha)
{
	UNREFERENCED_PARAMETER(x); UNREFERENCED_PARAMETER(y); UNREFERENCED_PARAMETER(id);
	UNREFERENCED_PARAMETER(in_width); UNREFERENCED_PARAMETER(in_height); UNREFERENCED_PARAMETER(alpha);
}

//! @return 成功：true　失敗：false
bool D3DGraphics::SaveScreenShot(const char *fname)
{
	UNREFERENCED_PARAMETER(fname);
	return false;
}

//! @return カラーコード（上流 OpenGL 版と同じ RGBA 配置）
int D3DGraphics::GetColorCode(float red, float green, float blue, float alpha)
{
	unsigned char red2, green2, blue2, alpha2;
	red2 = (unsigned char)(red*255);
	green2 = (unsigned char)(green*255);
	blue2 = (unsigned char)(blue*255);
	alpha2 = (unsigned char)(alpha*255);

	return (int)(((unsigned int)red2 << 24) | ((unsigned int)green2 << 16) | ((unsigned int)blue2 << 8) | (unsigned int)alpha2);
}
