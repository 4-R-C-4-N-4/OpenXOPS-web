//! @file d3dgraphics-directx.cpp
//! @brief D3DGraphicsクラスの定義（DirectX版）

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

#include "d3dgraphics.h"

#if (GRAPHIC_ENGINE == 0)||(GRAPHIC_ENGINE == 1)

//! @brief コンストラクタ
D3DGraphics::D3DGraphics()
{
	pD3D = NULL;
	pd3dDevice = NULL;
	aspect = 1.0f;
	fullscreenflag = false;
	LanguageMode = 0;
	for(int i=0; i<MAX_MODEL; i++){
		pmesh[i] = NULL;
		nummaterials[i] = 0;
	}
	for(int i=0; i<MAX_TEXTURE; i++){
		ptextures[i] = NULL;
	}

#if GRAPHIC_ENGINE == 1
	hVertexShader = NULL;
	hVSConstantTable = NULL;
	hVSWorldVertexPos = NULL;
	hVSWorldViewPos = NULL;
	hVSWorldViewProj = NULL;
	hVSDiffuse = NULL;
	pPixelShader = NULL;
	pPSConstantTable = NULL;
	hPSViewPos = NULL;
	hPSEnable2DRendering = NULL;
	hPSEnableTexture = NULL;
	hPSEnableFog = NULL;
	hPSFogColor = NULL;
	hPSFogStart = NULL;
	hPSFogEnd = NULL;
#endif

	BlockDataIF = NULL;
	bs = 0;
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		mapTextureID[i] = -1;
	}
#ifdef BLOCKDATA_GPUMEMORY
	g_pVB = NULL;
#else
	ZeroMemory(g_pVertices, sizeof(VERTEXTXTA)*MAX_BLOCKS*6*4);
#endif

	for( int i=0; i<TOTAL_HAVEWEAPON; i++ ){
		HUD_myweapon_x[i] = 0.0f;
		HUD_myweapon_y[i] = 0.0f;
		HUD_myweapon_z[i] = 0.0f;
	}

	StartRenderFlag = false;
	EnableFogFlag = false;

	ptextsprite = NULL;
	pxmsfont = NULL;
	pxmssmallfont = NULL;
	TextureFontFname[0] = '\0';
	TextureFont = -1;

#ifdef ENABLE_DEBUGCONSOLE
	ZeroMemory(DebugFontData, sizeof(unsigned char)*96*16);
	TextureDebugFont = -1;
#endif
}

//! @brief ディストラクタ
D3DGraphics::~D3DGraphics()
{
	DestroyD3D();
}

//! @brief フルスクリーンフラグ設定
//! @param fullscreen フルスクリーンフラグ
//! @attention 同フラグは、初期化時の InitD3D() 関数でも設定できます。
void D3DGraphics::SetFullScreenFlag(bool fullscreen)
{
	fullscreenflag = fullscreen;
}

//! @brief フルスクリーンフラグ取得
//! @return フルスクリーンフラグ
bool D3DGraphics::GetFullScreenFlag()
{
	return fullscreenflag;
}

//! @brief 言語選択の設定
//! @param mode 言語選択
//! @attention 同設定値は、初期化時の InitD3D() 関数でも設定できます。
void D3DGraphics::SetlanguageMode(int mode)
{
	LanguageMode = mode;
}

//! @brief 言語選択の設定取得
//! @return 言語選択
int D3DGraphics::GetlanguageMode()
{
	return LanguageMode;
}

//! @brief 初期化@n
//! （DirectX 9）
//! @param WindowCtrl WindowControlクラスのポインタ
//! @param TextureFontFilename 使用するテクスチャフォントのファイル名
//! @param fullscreen false：ウィンドウ表示　true：フルスクリーン用表示
//! @param language 言語選択（English/Japanese：0、English/Korean：1）
//! @return 成功：0　失敗：1
int D3DGraphics::InitD3D(WindowControl *WindowCtrl, const char *TextureFontFilename, bool fullscreen, int language)
{
	if( WindowCtrl == NULL ){ return 1; }
	if( TextureFontFilename == NULL ){ return 1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_INIT, "Graphics", "DirectX");
#endif

	D3DPRESENT_PARAMETERS d3dpp;
	RECT rec;

	GetClientRect(WindowCtrl->GethWnd(), &rec);

	fullscreenflag = fullscreen;
	LanguageMode = language;

	//D3D9の作成
	pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if( pD3D == NULL ){
		return 1;
	}

	//D3Dデバイスの作成
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	if( fullscreenflag == false ){
		d3dpp.Windowed = TRUE;
		d3dpp.BackBufferWidth = rec.right;
		d3dpp.BackBufferHeight = rec.bottom;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
		d3dpp.FullScreen_RefreshRateInHz = 0;
	}
	else{
		D3DDISPLAYMODE dispmode;
		pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dispmode);

		d3dpp.Windowed = FALSE;
		d3dpp.BackBufferWidth = rec.right;
		d3dpp.BackBufferHeight = rec.bottom;
		d3dpp.BackBufferFormat = dispmode.Format;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
		d3dpp.FullScreen_RefreshRateInHz = dispmode.RefreshRate;
	}

	if( FAILED( pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, WindowCtrl->GethWnd(), D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &pd3dDevice) ) ){
		if( FAILED( pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, WindowCtrl->GethWnd(), D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pd3dDevice) ) ){
			return 1;
		}
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	//テクスチャフォント用画像のファイル名を設定
	strcpy(TextureFontFname, TextureFontFilename);

	//描画関係の詳細な設定
	if( InitSubset() != 0){
		return 1;
	}
	

#if GRAPHIC_ENGINE == 1
	HRESULT hr;
	LPD3DXBUFFER pCode;
	LPD3DXBUFFER pErrorMes;

	//頂点シェーダー初期化
	//hr = D3DXCompileShaderFromFile("DirectX_Shader01.fx", NULL, NULL, "VertexShader_Main", "vs_2_0", 0, &pCode, &pErrorMes, &hVSConstantTable);
	hr = D3DXCompileShaderFromResource(NULL, MAKEINTRESOURCE(HLSLFILE), NULL, NULL, "VertexShader_Main", "vs_2_0", 0, &pCode, &pErrorMes, &hVSConstantTable);
	if( FAILED(hr) ){
#ifdef _DEBUG
		//シェーダーコンパイル時のエラー表示
		OutputDebugStringA( (const char*)pErrorMes->GetBufferPointer() );
		//WindowCtrl->ErrorInfo( (const char*)pErrorMes->GetBufferPointer() );
#endif
		return 1;
	}
	hr = pd3dDevice->CreateVertexShader((DWORD*)pCode->GetBufferPointer(), &hVertexShader);
	pCode->Release();
	if( FAILED(hr) ){ return 1; }

	//頂点シェーダーの関連変数
	hVSWorldVertexPos = hVSConstantTable->GetConstantByName(NULL, "WorldVertexPos");
	if( hVSWorldVertexPos == NULL ){ return 1; }
	hVSWorldViewPos = hVSConstantTable->GetConstantByName(NULL, "WorldViewPos");
	if( hVSWorldViewPos == NULL ){ return 1; }
	hVSWorldViewProj = hVSConstantTable->GetConstantByName(NULL, "WorldViewProj");
	if( hVSWorldViewProj == NULL ){ return 1; }
	hVSDiffuse = hVSConstantTable->GetConstantByName(NULL, "Diffuse");
	if( hVSDiffuse == NULL ){ return 1; }

	//ピクセルシェーダー初期化
	//hr = D3DXCompileShaderFromFile("DirectX_Shader01.fx", NULL, NULL, "PixelShader_Main", "ps_2_0", 0, &pCode, &pErrorMes, &pPSConstantTable);
	hr = D3DXCompileShaderFromResource(NULL, MAKEINTRESOURCE(HLSLFILE), NULL, NULL, "PixelShader_Main", "ps_2_0", 0, &pCode, &pErrorMes, &pPSConstantTable);
	if( FAILED(hr) ){
#ifdef _DEBUG
		//シェーダーコンパイル時のエラー表示
		OutputDebugStringA( (const char*)pErrorMes->GetBufferPointer() );
		//WindowCtrl->ErrorInfo( (const char*)pErrorMes->GetBufferPointer() );
#endif
		return 1;
	}
	hr = pd3dDevice->CreatePixelShader((DWORD*)pCode->GetBufferPointer(), &pPixelShader);
	pCode->Release();
	if( FAILED(hr) ){ return 1; }

	//ピクセルシェーダーの関連変数
	hPSViewPos = pPSConstantTable->GetConstantByName(NULL, "ViewPos");
	//if( hPSViewPos == NULL ){ return 1; }
	hPSEnable2DRendering = pPSConstantTable->GetConstantByName(NULL, "Enable2DRendering");
	//if( hPSEnable2DRendering == NULL ){ return 1; }
	hPSEnableTexture = pPSConstantTable->GetConstantByName(NULL, "EnableTexture");
	if( hPSEnableTexture == NULL ){ return 1; }
	hPSEnableFog = pPSConstantTable->GetConstantByName(NULL, "EnableFog");
	if( hPSEnableFog == NULL ){ return 1; }
	hPSFogColor = pPSConstantTable->GetConstantByName(NULL, "FogColor");
	if( hPSFogColor == NULL ){ return 1; }
	hPSFogStart = pPSConstantTable->GetConstantByName(NULL, "FogStart");
	if( hPSFogStart == NULL ){ return 1; }
	hPSFogEnd = pPSConstantTable->GetConstantByName(NULL, "FogEnd");
	if( hPSFogEnd == NULL ){ return 1; }

	D3DXMatrixIdentity(&VSWorldPosMatrix);
	D3DXMatrixIdentity(&VSViewPosMatrix);
	D3DXMatrixIdentity(&VSViewProjMatrix);
#endif


	//アスペクト比を設定
	aspect = (float)rec.right / (float)rec.bottom;

	//マウスカーソルを消す
	//ShowCursor(FALSE);


	//HUD　現在持っている武器を描画する座標
	HUD_myweapon_x[0] = GameConfig.GetScreenWidth() - 140.0f;
	HUD_myweapon_y[0] = GameConfig.GetScreenHeight() - 40.0f;
	HUD_myweapon_z[0] = 0.86f;

	//HUD　予備の武器を描画する座標
	HUD_myweapon_x[1] = GameConfig.GetScreenWidth() - 72.0f;
	HUD_myweapon_y[1] = GameConfig.GetScreenHeight() - 25.0f;
	HUD_myweapon_z[1] = 0.93f;


#ifdef ENABLE_DEBUGCONSOLE
	InitDebugFontData();
	if( LoadDebugFontTexture() == false ){
		return 1;
	}
#endif

	return 0;
}

//! @brief リセット@n
//! （ウィンドウ最小化からの復帰　など）
//! @param WindowCtrl WindowControlクラスのポインタ
//! @return 成功：0　待ち：1　失敗：2
int D3DGraphics::ResetD3D(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 2; }

	//フォーカスを失っているなら待たせる
	if( pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICELOST ){
		return 1;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Graphics", "DirectX lost");
#endif

	//リソース解放
	CleanupD3Dresource();

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_INIT, "Graphics", "DirectX (Reset)");
#endif

	D3DPRESENT_PARAMETERS d3dpp;
	RECT rec;

	GetClientRect(WindowCtrl->GethWnd(), &rec);

	//D3Dデバイスの作成
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	if( fullscreenflag == false ){
		d3dpp.Windowed = TRUE;
		d3dpp.BackBufferWidth = rec.right;
		d3dpp.BackBufferHeight = rec.bottom;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
		d3dpp.FullScreen_RefreshRateInHz = 0;
	}
	else{
		D3DDISPLAYMODE dispmode;
		pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dispmode);

		d3dpp.Windowed = FALSE;
		d3dpp.BackBufferWidth = rec.right;
		d3dpp.BackBufferHeight = rec.bottom;
		d3dpp.BackBufferFormat = dispmode.Format;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
		d3dpp.FullScreen_RefreshRateInHz = dispmode.RefreshRate;
	}

	if( FAILED( pd3dDevice->Reset(&d3dpp) ) ){
		return 2;
	}

	//描画関係の詳細な設定
	if( InitSubset() != 0){
		return 2;
	}

#ifdef ENABLE_DEBUGCONSOLE
	if( LoadDebugFontTexture() == false ){
		return 2;
	}
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	return 0;
}

//! @brief 解放
//! @attention 本関数は自動的に呼び出されますが、明示的に呼び出すことも可能です。
void D3DGraphics::DestroyD3D()
{
	if( (pd3dDevice == NULL)&&(pD3D == NULL) ){ return; }

	//リソース解放
	CleanupD3Dresource();

#if GRAPHIC_ENGINE == 1
	if( pPSConstantTable != NULL ){
		pPSConstantTable->Release();
		pPSConstantTable = NULL;
	}
	if( pPixelShader != NULL ){
		pPixelShader->Release();
		pPixelShader = NULL;
	}
	if( hVSConstantTable != NULL ){
		hVSConstantTable->Release();
		hVSConstantTable = NULL;
	}
	if( hVertexShader != NULL ){
		hVertexShader->Release();
		hVertexShader = NULL;
	}
#endif

	if( pd3dDevice != NULL ){
		pd3dDevice->Release();
		pd3dDevice = NULL;
	}
	if( pD3D != NULL ){
		pD3D->Release();
		pD3D = NULL;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CLEANUP, "Graphics", "DirectX");
#endif
}

//! @brief 描画関係の細部設定
//! @attention 初期化時に1度だけ実行してください。
int D3DGraphics::InitSubset()
{
	//ライト
	//pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(0,255,255,255) );
	pd3dDevice->LightEnable(0, FALSE);
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	// テクスチャフィルタを使う
	pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	//ミップマップの詳細レベル (LOD) バイアスを指定する。
	float LODBias = -0.2f;
	pd3dDevice->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD)(&LODBias)) );

	//アルファ・ブレンディングを行う
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	//透過処理を行う
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	//アルファテストに対応しているかチェック
	D3DCAPS9 Caps;
	pd3dDevice->GetDeviceCaps(&Caps);
	if( Caps.AlphaCmpCaps & D3DPCMPCAPS_GREATEREQUAL ){
		//アルファテスト設定
		//　完全に透明なピクセルは描画しない
		pd3dDevice->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x00000001);
		pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE); 
		pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
	}

	//深度バッファ比較関数
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);

	//ポリゴンの裏・表
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);


	//テキストスプライト初期化
	if( FAILED( D3DXCreateSprite( pd3dDevice, &ptextsprite ) ) ){
		return 1;
	}

	if( LanguageMode == 1 ){
		// ---English/Korean---

		//フォント名：Dotum　サイズ：18
		if( FAILED( D3DXCreateFont( pd3dDevice, (int)(((float)GameConfig.GetScreenHeight() / 480) * -18), 0, FW_NORMAL, 1, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS,
										DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Dotum", &pxmsfont) ) 
		){
			return 1;
		}
		//フォント名：Dotum　サイズ：12
		if( FAILED( D3DXCreateFont( pd3dDevice, -12, 0, FW_NORMAL, 1, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS,
										DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Dotum", &pxmssmallfont) ) 
		){
			return 1;
		}
	}
	else{
		// ---English/Japanese---

		//フォント名：ＭＳ ゴシック　サイズ：18
		//if( FAILED( D3DXCreateFont( pd3dDevice, -18, 0, FW_NORMAL, 1, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
		if( FAILED( D3DXCreateFont( pd3dDevice, (int)(((float)GameConfig.GetScreenHeight() / 480) * -18), 0, FW_NORMAL, 1, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
										DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "ＭＳ ゴシック", &pxmsfont) ) 
		){
			return 1;
		}
		//フォント名：ＭＳ ゴシック　サイズ：12
		if( FAILED( D3DXCreateFont( pd3dDevice, -12, 0, FW_NORMAL, 1, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
										DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "ＭＳ ゴシック", &pxmssmallfont) ) 
		){
			return 1;
		}
	}

	//テクスチャフォント用画像を取得
	TextureFont = LoadTexture(TextureFontFname, true, false);

	//フォグは再度初期化
	EnableFogFlag = false;

	return 0;
}

//! @brief デバイスのリソースを解放
void D3DGraphics::CleanupD3Dresource()
{
	if( TextureFont != -1 ){
		CleanupTexture(TextureFont);
		TextureFont = -1;
	}

#ifdef ENABLE_DEBUGCONSOLE
	if( TextureDebugFont != -1 ){
		CleanupTexture(TextureDebugFont);
		TextureDebugFont = -1;
	}
#endif

	if( pxmsfont != NULL ){
		pxmsfont->Release();
		pxmsfont = NULL;
	}
	if( pxmssmallfont != NULL ){
		pxmssmallfont->Release();
		pxmssmallfont = NULL;
	}
	if( ptextsprite != NULL ){
		ptextsprite->Release();
		ptextsprite = NULL;
	}

	CleanupMapdata();

	for(int i=0; i<MAX_MODEL; i++){
		CleanupModel(i);
	}
	for(int i=0; i<MAX_TEXTURE; i++){
		CleanupTexture(i);
	}
}

//! @brief モデルファイルを読み込む（.x）
//! @param filename ファイル名
//! @return 成功：モデルのデータ番号（0以上）　失敗：-1
int D3DGraphics::LoadModel(const char* filename)
{
	if( filename == NULL ){ return -1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Model", filename);
#endif

	int id = -1;

	//空いている要素を探す
	for(int i=0; i<MAX_MODEL; i++){
		if( pmesh[i] == NULL ){
			id = i;
			break;
		}
	}
	if( id == -1 ){ return -1; }

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	filename = ChangePathDelimiter(filename);
#endif

	//.xファイルをバッファーに読み込む
	if( FAILED( D3DXLoadMeshFromX( filename, D3DXMESH_SYSTEMMEM, pd3dDevice, NULL, 
				NULL, NULL, &nummaterials[id], &pmesh[id] ) ) ) {
		return -1;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", id);
#endif
	return id;
}

//! @brief モデルファイルの中間データを作成（モーフィング）
//! @param idA モデルAのデータ番号
//! @param idB モデルBのデータ番号
//! @return 成功：新しいモデルのデータ番号（0以上）　失敗：-1
//! @attention モデルAとモデルBは、頂点数・ポリゴン数・インデックスが同じである必要があります。
//! @attention それぞれのモデルデータが正しくないか 頂点数が異なる場合、実行に失敗します。
int D3DGraphics::MorphingModel(int idA, int idB)
{
#ifdef ENABLE_DEBUGLOG
	char str[128];
	sprintf(str, "Create morphing model  ID : %d and %d", idA, idB);

	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Model", str);
#endif

	//データが正しいか調べる
	if( (idA < 0)||(MAX_MODEL <= idA) ){ return -1; }
	if( pmesh[idA] == NULL ){ return -1; }
	if( (idB < 0)||(MAX_MODEL <= idB) ){ return -1; }
	if( pmesh[idB] == NULL ){ return -1; }

	int idN = -1;
	int numvA, numvB;
	LPDIRECT3DVERTEXBUFFER9 pvbA, pvbB, pvbN;
	D3DXVECTOR3 *pVerticesA, *pVerticesB, *pVerticesN;
	int FVFsize;

	//空いている要素を探す
	for(int i=0; i<MAX_MODEL; i++){
		if( pmesh[i] == NULL ){
			idN = i;
			break;
		}
	}
	if( idN == -1 ){ return -1; }

	//頂点数を取得
	numvA = pmesh[idA]->GetNumVertices();
	numvB = pmesh[idB]->GetNumVertices();

	//頂点数が同じかどうか調べる
	if( numvA != numvB ){ return -1; }

	//頂点データをコピー（実質的に領域確保用のダミー）
	if( pmesh[idA]->CloneMeshFVF(pmesh[idA]->GetOptions(), pmesh[idA]->GetFVF(), pd3dDevice, &pmesh[idN]) != D3D_OK ){
		return -1;
	}

	//マテリアル情報をコピー
	nummaterials[idN] = nummaterials[idA];

	//バッファーを取得
	pmesh[idA]->GetVertexBuffer(&pvbA);
	pmesh[idB]->GetVertexBuffer(&pvbB);
	pmesh[idN]->GetVertexBuffer(&pvbN);

	//1頂点あたりのバイト数取得
	FVFsize = D3DXGetFVFVertexSize(pmesh[idN]->GetFVF());

	//各頂点を読み出し計算
	for(int i=0; i<numvA; i++){
		pvbA->Lock(i*FVFsize, sizeof(D3DXVECTOR3), (void**)&pVerticesA, D3DLOCK_READONLY);
		pvbB->Lock(i*FVFsize, sizeof(D3DXVECTOR3), (void**)&pVerticesB, D3DLOCK_READONLY);
		pvbN->Lock(i*FVFsize, sizeof(D3DXVECTOR3), (void**)&pVerticesN, 0);

		//平均化
		pVerticesN->x = (pVerticesA->x + pVerticesB->x)/2;
		pVerticesN->y = (pVerticesA->y + pVerticesB->y)/2;
		pVerticesN->z = (pVerticesA->z + pVerticesB->z)/2;

		pvbA->Unlock();
		pvbB->Unlock();
		pvbN->Unlock();
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", idN);
#endif
	return idN;
}

//! @brief 読み込み済みのモデル数を取得
//! @return モデル数
int D3DGraphics::GetTotalModels()
{
	int cnt = 0;

	//使用中の要素を数える
	for(int i=0; i<MAX_MODEL; i++){
		if( pmesh[i] != NULL ){ cnt += 1; }
	}

	return cnt;
}

//! @brief モデルファイルを解放
//! @param id モデルのデータ番号
void D3DGraphics::CleanupModel(int id)
{
	if( (id < 0)||(MAX_MODEL <= id) ){ return; }
	if( pmesh[id] != NULL ){
		pmesh[id]->Release();
		pmesh[id] = NULL;

#ifdef ENABLE_DEBUGLOG
		//ログに出力
		OutputLog.WriteLog(LOG_CLEANUP, "Model", id);
#endif
	}
}

//! @brief テクスチャを読み込む
//! @param filename ファイル名
//! @param texturefont テクスチャフォントフラグ
//! @param BlackTransparent 黒を透過する
//! @return 成功：テクスチャのデータ番号（0以上）　失敗：-1
int D3DGraphics::LoadTexture(const char* filename, bool texturefont, bool BlackTransparent)
{
	if( filename == NULL ){ return -1; }

	int id = -1;
	D3DXIMAGE_INFO info;
	int MipLevels;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Texture", filename);
#endif

	//空いているデータ番号を探す
	for(int i=0; i<MAX_TEXTURE; i++){
		if( ptextures[i] == NULL ){
			id = i;
			break;
		}
	}
	if( id == -1 ){ return -1; }

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	filename = ChangePathDelimiter(filename);
#endif

	//ファイル情報を取得
	if( D3DXGetImageInfoFromFile(filename, &info) != D3D_OK ){ return -1; }

	//ミップマップレベルを設定
	if( texturefont == true ){
		MipLevels = 1;
	}
	else{
		MipLevels = 4;//D3DX_DEFAULT;
	}

	//テクスチャを読み込む
	if( BlackTransparent == false ){
		if( FAILED( D3DXCreateTextureFromFileEx(pd3dDevice, filename, info.Width, info.Height, MipLevels, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0x00000000, NULL, NULL, &ptextures[id]) ) ) {
			return -1;
		}
	}
	else{
		if( FAILED( D3DXCreateTextureFromFileEx(pd3dDevice, filename, info.Width, info.Height, MipLevels, 0, D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DCOLOR_ARGB(255, 0, 0, 0), NULL, NULL, &ptextures[id]) ) ) {
			return -1;
		}
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	//OutputLog.WriteLog(LOG_COMPLETE, "", id);

	char str[32];
	int width, height;
	GetTextureSize(id, &width, &height);
	sprintf(str, "ID：%d (size:%dx%d)", id, width, height);
	OutputLog.WriteLog(LOG_COMPLETE, "", str);
#endif
	return id;
}

#ifdef ENABLE_DEBUGCONSOLE
//! @brief デバック用フォントを読み込む
//! @return 成功：true　失敗：false
//! @attention この関数を呼び出す前に、InitDebugFontData()関数を実行してください。
bool D3DGraphics::LoadDebugFontTexture()
{
	int charwidth = 8;
	int charheight = 16;
	int width = charwidth * 16;
	int height = charheight * 8;
	int headersize = 54;
	int bufsize = headersize  + width*height*3;
	int datacnt = 0;
	int id = -1;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Texture", "DebugFontTexture");
#endif

	//既に読み込まれているなら失敗
	if( TextureDebugFont != -1 ){ return false; }

	//空いているデータ番号を探す
	for(int i=0; i<MAX_TEXTURE; i++){
		if( ptextures[i] == NULL ){
			id = i;
			break;
		}
	}
	if( id == -1 ){ return false; }

	//.bmp展開用領域作成
	unsigned char *bmpdata = new unsigned char [bufsize];

	//.bmpヘッダー作成
	for(int i=0; i<headersize; i++){ bmpdata[i] = 0x00; }
	bmpdata[0x00] = 'B';
	bmpdata[0x01] = 'M';
	Set4ByteLittleEndian(&(bmpdata[0x02]), bufsize);
	bmpdata[0x0A] = (unsigned char)headersize;
	bmpdata[0x0E] = (unsigned char)(headersize - 14);
	Set4ByteLittleEndian(&(bmpdata[0x12]), width);
	Set4ByteLittleEndian(&(bmpdata[0x16]), height);
	bmpdata[0x1A] = 1;
	bmpdata[0x1C] = 24;
	bmpdata[0x1E] = 0;
	Set4ByteLittleEndian(&(bmpdata[0x22]), bufsize - headersize);

	datacnt = headersize;

	//6行分のデータを作成
	for(int cnt_y=5; cnt_y>=0; cnt_y--){
		for(int line_y=(charheight-1); line_y>=0; line_y--){
			for(int cnt_x=0; cnt_x<16; cnt_x++){
				for(int line_x=(charwidth-1); line_x>=0; line_x--){
					unsigned char mask;

					//ビット判定用マスク作成
					switch(line_x){
						case 0: mask = 0x01; break;
						case 1: mask = 0x02; break;
						case 2: mask = 0x04; break;
						case 3: mask = 0x08; break;
						case 4: mask = 0x10; break;
						case 5: mask = 0x20; break;
						case 6: mask = 0x40; break;
						case 7: mask = 0x80; break;
						default: mask = 0x00;		//事実上エラー
					}

					//該当ビットが1なら白、0なら黒。
					if( (DebugFontData[cnt_y*16 + cnt_x][line_y] & mask) != 0 ){
						bmpdata[datacnt + 0] = 255;
						bmpdata[datacnt + 1] = 255;
						bmpdata[datacnt + 2] = 255;
					}
					else{
						bmpdata[datacnt + 0] = 0;
						bmpdata[datacnt + 1] = 0;
						bmpdata[datacnt + 2] = 0;
					}

					datacnt += 3;
				}
			}
		}
	}

	//制御コードが入っている上の2行 32文字分は、空欄にする。
	for(int cnt_y=0; cnt_y<2; cnt_y++){
		for(int line_y=(charheight-1); line_y>=0; line_y--){
			for(int cnt_x=0; cnt_x<16; cnt_x++){
				for(int line_x=0; line_x<charwidth; line_x++){
					bmpdata[datacnt + 0] = 0;
					bmpdata[datacnt + 1] = 0;
					bmpdata[datacnt + 2] = 0;

					datacnt += 3;
				}
			}
		}
	}

	//.bmpテクスチャとして読み込む
	if( FAILED( D3DXCreateTextureFromFileInMemoryEx(pd3dDevice, bmpdata, bufsize, width, height, 1, 0, D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DCOLOR_ARGB(255, 0, 0, 0), NULL, NULL, &ptextures[id]) ) ) {
		TextureDebugFont = -1;
		return false;
	}

	/*
	//.bmpとして保存してみる
	FILE *fp;
	fp = fopen("debugfont-test.bmp", "wb");
	fwrite(bmpdata, sizeof(unsigned char), bufsize, fp);
	fclose(fp);
	*/

	//.bmp展開用領域解放
	delete [] bmpdata;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	//OutputLog.WriteLog(LOG_COMPLETE, "", id);

	char str[32];
	sprintf(str, "ID：%d (size:%dx%d)", id, width, height);
	OutputLog.WriteLog(LOG_COMPLETE, "", str);
#endif

	//テクスチャID設定
	TextureDebugFont = id;
	return true;
}
#endif

//! @brief テクスチャのサイズを取得
//! @param id テクスチャのデータ番号
//! @param width 幅を受け取るポインタ
//! @param height 高さを受け取るポインタ
//! @return 成功：0　失敗：1
//! @attention サーフェイスのサイズを取得します。GPUにロードされたサイズであり、テクスチャ（現物）と異なる場合があります。
int D3DGraphics::GetTextureSize(int id, int *width, int *height)
{
	//無効なデータ番号が指定されていたら、処理せず返す。
	if( id == -1 ){ return 1; }
	if( ptextures[id] == NULL ){ return 1; }
	if( width == NULL ){ return 1; }
	if( height == NULL ){ return 1; }

	IDirect3DSurface9 *surface;
	D3DSURFACE_DESC desc;

	//サーフェイスを取得
	ptextures[id]->GetSurfaceLevel(0, &surface);

	//幅と高さを取得
	surface->GetDesc(&desc);
	*width = desc.Width;
	*height = desc.Height;

	//サーフェイスを解放
	surface->Release();

	return 0;
}

//! @brief 読み込み済みのテクスチャ数を取得
//! @return テクスチャ数
int D3DGraphics::GetTotalTextures()
{
	int cnt = 0;

	//使用中の要素を数える
	for(int i=0; i<MAX_TEXTURE; i++){
		if( ptextures[i] != NULL ){ cnt += 1; }
	}

	return cnt;
}

//! @brief テクスチャを解放
//! @param id テクスチャのデータ番号
void D3DGraphics::CleanupTexture(int id)
{
	if( (id < 0)||(MAX_TEXTURE <= id) ){ return; }
	if( ptextures[id] != NULL ){
		ptextures[id]->Release();
		ptextures[id] = NULL;

#ifdef ENABLE_DEBUGLOG
		//ログに出力
		OutputLog.WriteLog(LOG_CLEANUP, "Texture", id);
#endif
	}
}

//! @brief 全ての描画処理を開始
//! @return 成功：0　失敗：1
//! @attention 描画処理の最初に呼び出す必要があります。
int D3DGraphics::StartRender()
{
	if( StartRenderFlag == true ){ return 1; }

	//領域を初期化
	pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);

	if( SUCCEEDED( pd3dDevice->BeginScene() ) ){
		//Zバッファ初期化
		pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

#if GRAPHIC_ENGINE == 1
		//頂点シェーダーとピクセルシェーダー設定
		pd3dDevice->SetVertexShader(hVertexShader);
		pd3dDevice->SetPixelShader(pPixelShader);
#endif

		//座標ゼロ地点にワールド変換行列
		ResetWorldTransform();

		//描画中のフラグを立てる
		StartRenderFlag = true;
		return 0;
	}

	return 1;
}

//! @brief 全ての描画処理を終了
//! @return 成功：false　失敗：true
//! @attention 描画処理の最後に呼び出す必要があります。
bool D3DGraphics::EndRender()
{
	//描画中なら終了
	if( StartRenderFlag == true ){
		pd3dDevice->EndScene();
	}

	HRESULT hr = pd3dDevice->Present(NULL, NULL, NULL, NULL);

	//フラグを false に
	StartRenderFlag = false;

	if( hr == D3DERR_DEVICELOST ){
		return true;
	}
	return false;
}

//! @brief Zバッファをリセット
void D3DGraphics::ResetZbuffer()
{
	//Zバッファを一度無効にし、初期化後、再度有効に
	pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	pd3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
	pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
}

//! @brief ワールド空間を原点（0,0,0）に戻す　など
void D3DGraphics::ResetWorldTransform()
{
	D3DXMATRIX matWorld;
	D3DXMatrixIdentity(&matWorld);
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif
}

//! @brief ワールド空間の座標・角度・拡大率を設定
//! @param x X座標
//! @param y Y座標
//! @param z Z座標
//! @param rx 横軸角度
//! @param ry 縦軸角度
//! @param size 拡大率
void D3DGraphics::SetWorldTransform(float x, float y, float z, float rx, float ry, float size)
{
	SetWorldTransform(x, y, z, rx, ry, 0.0f, size);
}

//! @brief ワールド空間の座標・角度・拡大率を設定
//! @param x X座標
//! @param y Y座標
//! @param z Z座標
//! @param rx 横軸角度
//! @param ry1 縦軸角度
//! @param ry2 縦軸角度
//! @param size 拡大率
void D3DGraphics::SetWorldTransform(float x, float y, float z, float rx, float ry1, float ry2, float size)
{
	D3DXMATRIX matWorld;
	D3DXMATRIX matWorld1, matWorld2, matWorld3, matWorld4, matWorld5;

	//行列を作成
	D3DXMatrixTranslation(&matWorld1, x, y, z);
	D3DXMatrixRotationY(&matWorld2, rx);
	D3DXMatrixRotationX(&matWorld3, ry1);
	D3DXMatrixRotationZ(&matWorld4, ry2);
	D3DXMatrixScaling(&matWorld5, size, size, size);

	//計算
	matWorld = matWorld5 * matWorld4 * matWorld3 * matWorld2 * matWorld1;

	//適用
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif
}

//! @brief ワールド空間の座標・角度・拡大率を設定（エフェクト用）
//! @param x X座標
//! @param y Y座標
//! @param z Z座標
//! @param rx 横軸角度
//! @param ry 縦軸角度
//! @param rt 回転角度
//! @param size 拡大率
void D3DGraphics::SetWorldTransformEffect(float x, float y, float z, float rx, float ry, float rt, float size)
{
	D3DXMATRIX matWorld;
	D3DXMATRIX matWorld1, matWorld2, matWorld3, matWorld4, matWorld5;

	//行列を作成
	D3DXMatrixTranslation(&matWorld1, x, y, z);
	D3DXMatrixRotationY(&matWorld2, rx);
	D3DXMatrixRotationZ(&matWorld3, ry);
	D3DXMatrixRotationX(&matWorld4, rt);
	D3DXMatrixScaling(&matWorld5, size, size, size);

	//計算
	matWorld = matWorld5 * matWorld4 * matWorld3 * matWorld2 * matWorld1;

	//適用
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif
}

//! @brief ワールド空間を人が武器を持つ場所に設定
//! @param x X座標
//! @param y Y座標
//! @param z Z座標
//! @param mx 手元を原点にした モデルのX座標
//! @param my 手元を原点にした モデルのY座標
//! @param mz 手元を原点にした モデルのZ座標
//! @param rx 横軸角度
//! @param ry 縦軸角度
//! @param size 拡大率
void D3DGraphics::SetWorldTransformHumanWeapon(float x, float y, float z, float mx, float my, float mz, float rx, float ry, float size)
{
	D3DXMATRIX matWorld;
	D3DXMATRIX matWorld1, matWorld2, matWorld3, matWorld4, matWorld5;

	//行列を作成
	D3DXMatrixTranslation(&matWorld1, x, y, z);
	D3DXMatrixRotationY(&matWorld2, rx);
	D3DXMatrixRotationX(&matWorld3, ry);
	D3DXMatrixTranslation(&matWorld4, mx, my, mz);
	D3DXMatrixScaling(&matWorld5, size, size, size);

	//計算
	matWorld = matWorld5 * matWorld4 * matWorld3 * matWorld2 * matWorld1;

	//適用
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif
}

//! @brief ワールド空間を所持している武器を描画する場所に設定
//! @param rotation 武器を回転させる
//! @param camera_rx カメラの横軸角度
//! @param camera_ry カメラの縦軸角度
//! @param rx 武器のの縦軸角度
//! @param size 描画サイズ
//! @note rotation・・　true：現在持っている武器です。　false：予備の武器です。（rx は無視されます）
//! @todo 位置やサイズの微調整
void D3DGraphics::SetWorldTransformPlayerWeapon(bool rotation, float camera_rx, float camera_ry, float rx, float size)
{
	D3DXMATRIX matWorld;
	D3DXMATRIX matWorldV;
	D3DXMATRIXA16 matProj;
	D3DVIEWPORT9 pViewport;
	D3DXVECTOR3 p1;
	D3DXMATRIX matWorld1, matWorld2, matWorld3, matWorld4, matWorld5, matWorld6;

	if( rotation == true ){
		p1 = D3DXVECTOR3(HUD_myweapon_x[0], HUD_myweapon_y[0], HUD_myweapon_z[0]);
	}
	else{
		p1 = D3DXVECTOR3(HUD_myweapon_x[1], HUD_myweapon_y[1], HUD_myweapon_z[1]);
	}

	pd3dDevice->GetViewport(&pViewport);

#if GRAPHIC_ENGINE == 0
	//カメラ座標
	pd3dDevice->GetTransform(D3DTS_VIEW, &matWorldV);

	//カメラ設定（射影変換行列）viewangle
	pd3dDevice->GetTransform(D3DTS_PROJECTION, &matProj);
#else
	//カメラ座標
	matWorldV = VSViewPosMatrix;

	//カメラ設定（射影変換行列）viewangle
	matProj = VSViewProjMatrix;
#endif

	D3DXMatrixIdentity(&matWorld);

	//スクリーン空間からオブジェクト空間にベクトルを射影
	D3DXVec3Unproject(&p1, &p1, &pViewport, &matProj, &matWorldV, &matWorld);

	//size = size * 0.3f;
	size = size * (0.0004f*GameConfig.GetScreenHeight()*GameConfig.GetScreenHeight() - 0.92f*GameConfig.GetScreenHeight() + 650.0f) / 1000.f;

	//行列計算
	D3DXMatrixTranslation(&matWorld1, p1.x, p1.y, p1.z);
	D3DXMatrixRotationY(&matWorld2, camera_rx *-1);
	D3DXMatrixRotationZ(&matWorld3, camera_ry);
	if( rotation == true ){
		D3DXMatrixRotationY(&matWorld5, rx);
	}
	else{
		D3DXMatrixRotationY(&matWorld5, D3DX_PI);
	}
	D3DXMatrixScaling(&matWorld6, size, size, size);

	matWorld = matWorld6 * matWorld5 * matWorld3 * matWorld2 * matWorld1;

	//適用
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif
}

//! @brief ワールド空間の座標を取得
//! @param *x x軸を受け取るポインタ
//! @param *y y軸を受け取るポインタ
//! @param *z z軸を受け取るポインタ
void D3DGraphics::GetWorldTransformPos(float *x, float *y, float *z)
{
	if( (x == NULL)||(y == NULL)||(z == NULL) ){ return; }

	D3DXMATRIX matWorld;
#if GRAPHIC_ENGINE == 0
	pd3dDevice->GetTransform(D3DTS_WORLD, &matWorld);
#else
	matWorld = VSWorldPosMatrix;
#endif
	*x = matWorld._41;
	*y = matWorld._42;
	*z = matWorld._43;
}

//! @brief フォグを設定
//! @param enablefog フォグ有効化
//! @param skynumber 空の番号
void D3DGraphics::SetFog(bool enablefog, int skynumber)
{
	if( enablefog == true ){
		if( EnableFogFlag == false ){
			//フォグ
			float fog_st = 100;
			float fog_end = 800;
#if GRAPHIC_ENGINE == 0
			pd3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
			pd3dDevice->SetRenderState(D3DRS_FOGCOLOR, D3DCOLOR_RGBA(0, 0, 0, 0));
			pd3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
			pd3dDevice->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
			pd3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD*)(&fog_st));
			pd3dDevice->SetRenderState(D3DRS_FOGEND,   *(DWORD*)(&fog_end));
#else
			float FogColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableFog, true);
			pPSConstantTable->SetFloatArray(pd3dDevice, hPSFogColor, FogColor, 4);
			pPSConstantTable->SetFloat(pd3dDevice, hPSFogStart, fog_st);
			pPSConstantTable->SetFloat(pd3dDevice, hPSFogEnd, fog_end);
#endif
		}

		D3DCOLOR skycolor;

		//空の番号により色を決定
		switch(skynumber){
			case 1: skycolor = D3DCOLOR_RGBA(64, 64+16, 64, 0); break;
			case 2: skycolor = D3DCOLOR_RGBA(16, 16, 16, 0); break;
			case 3: skycolor = D3DCOLOR_RGBA(0, 16, 32, 0); break;
			case 4: skycolor = D3DCOLOR_RGBA(32, 16, 16, 0); break;
			case 5: skycolor = D3DCOLOR_RGBA(64, 32, 32, 0); break;
			default: skycolor = D3DCOLOR_RGBA(0, 0, 0, 0); break;
		}

		//フォグを設定
#if GRAPHIC_ENGINE == 0
		pd3dDevice->SetRenderState(D3DRS_FOGCOLOR, skycolor);
#else
		float FogColor[4] = {(float)((skycolor>>16)&0xFF)/255.0f, (float)((skycolor>>8)&0xFF)/255.0f, (float)(skycolor&0xFF)/255.0f, 1.0f};
		pPSConstantTable->SetFloatArray(pd3dDevice, hPSFogColor, FogColor, 4);
#endif

		EnableFogFlag = true;
	}
	else{
#if GRAPHIC_ENGINE == 0
		pd3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
#else
		pPSConstantTable->SetBool(pd3dDevice, hPSEnableFog, false);
#endif
		EnableFogFlag = false;
	}
}

//! @brief カメラ（視点）を設定
//! @param camera_x カメラのX座標
//! @param camera_y カメラのY座標
//! @param camera_z カメラのZ座標
//! @param camera_rx カメラの横軸角度
//! @param camera_ry カメラの縦軸角度
//! @param viewangle 視野角
//! @todo 真上/真下を向くと、所持武器のモデル描画の向きがおかしくなる。
void D3DGraphics::SetCamera(float camera_x, float camera_y, float camera_z, float camera_rx, float camera_ry, float viewangle)
{
	float vUpVecF_x, vUpVecF_y, vUpVecF_z;
	D3DXMATRIX matWorld;
	D3DXMATRIXA16 matView;

	//camera_ryを -PI～PI の間に正規化
	camera_ry = AngleNormalization(camera_ry);

	//カメラの向きを決定
	if( fabsf(camera_ry) == D3DX_PI/2 ){
		//真上or真下を向いているならば、カメラの上部指定を横向きにする。
		vUpVecF_x = cosf(camera_rx); vUpVecF_y = 0.0f; vUpVecF_z = sinf(camera_rx);
	}
	else{
		if( fabsf(camera_ry) < D3DX_PI/2 ){
			vUpVecF_x = 0.0f; vUpVecF_y = 1.0f; vUpVecF_z = 0.0f;
		}
		else{
			vUpVecF_x = 0.0f; vUpVecF_y = -1.0f; vUpVecF_z = 0.0f;
		}
	}

	D3DXMatrixIdentity(&matWorld);
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldVertexPos, &matWorld);
	VSWorldPosMatrix = matWorld;
#endif

	//カメラ座標
	D3DXVECTOR3 vEyePt( camera_x, camera_y, camera_z );
	D3DXVECTOR3 vLookatPt( cosf(camera_rx)*cosf(camera_ry) + camera_x, sinf(camera_ry) + camera_y, sinf(camera_rx)*cosf(camera_ry) + camera_z );
	D3DXVECTOR3 vUpVec( vUpVecF_x, vUpVecF_y, vUpVecF_z );
	D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_VIEW, &matView);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldViewPos, &matView);
	VSViewPosMatrix = matView;

	if( hPSViewPos != NULL ){
		float Camera[4] = {camera_x, camera_y, camera_z, 1.0f};
		pPSConstantTable->SetFloatArray(pd3dDevice, hPSViewPos, Camera, 4);
	}
#endif

	//カメラ設定（射影変換行列）viewangle
	D3DXMATRIXA16 matProj;
	D3DXMatrixPerspectiveFovLH( &matProj, viewangle, aspect, CLIPPINGPLANE_NEAR, CLIPPINGPLANE_FAR);
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
#else
	hVSConstantTable->SetMatrix(pd3dDevice, hVSWorldViewProj, &matProj);
	VSViewProjMatrix = matProj;
#endif
}

//! @brief マップデータを取り込む
//! @param in_BlockDataIF ブロックデータ
//! @param directory ブロックデータが存在するディレクトリ
void D3DGraphics::LoadMapdata(BlockDataInterface* in_BlockDataIF, const char *directory)
{
	//ブロックデータが指定されていなければ、処理しない。
	if( in_BlockDataIF == NULL ){ return; }
	if( directory == NULL ){ return; }

	char fname[MAX_PATH];
	char fnamefull[MAX_PATH];
	//int bs;
	blockdata data;
	int vID[4];
	int uvID[4];

	//クラスを設定
	BlockDataIF = in_BlockDataIF;

	//テクスチャ読み込み
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		//テクスチャ名を取得
		BlockDataIF->GetTexture(fname, i);

		if( strcmp(fname, "") == 0 ){	//指定されていなければ、処理しない
			mapTextureID[i] = -1;
		}
		else{
			//「ディレクトリ＋ファイル名」を生成し、読み込む
			strcpy(fnamefull, directory);
			strcat(fnamefull, fname);
			mapTextureID[i] = LoadTexture(fnamefull, false, false);
		}
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Mapdata", "(Vertex data)");
#endif

#ifdef BLOCKDATA_GPUMEMORY
	VERTEXTXTA* pVertices;

	//ブロック数を取得
	bs = BlockDataIF->GetTotaldatas();

	//ブロック数分のバッファーを作成
	pd3dDevice->CreateVertexBuffer(bs*6*4*sizeof(VERTEXTXTA), 0, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, D3DPOOL_DEFAULT, &g_pVB, NULL);

	for(int i=0; i<bs; i++){
		//データを取得
		BlockDataIF->Getdata(&data, i);

		for(int j=0; j<6; j++){
			//面の頂点データの関連付けを取得
			blockdataface(j, &vID[0], &uvID[0]);

			//GPUをロック（1面分）
			g_pVB->Lock((i*6+j)*4*sizeof(VERTEXTXTA), 4*sizeof(VERTEXTXTA), (void**)&pVertices, 0);

			//頂点座標・UV座標・色を設定
			pVertices[0].position = D3DXVECTOR3( data.x[ vID[1] ], data.y[ vID[1] ], data.z[ vID[1] ] );
			pVertices[0].tu       = data.material[j].u[ uvID[1] ];
			pVertices[0].tv       = data.material[j].v[ uvID[1] ];
			pVertices[1].position = D3DXVECTOR3( data.x[ vID[2] ], data.y[ vID[2] ], data.z[ vID[2] ] );
			pVertices[1].tu       = data.material[j].u[ uvID[2] ];
			pVertices[1].tv       = data.material[j].v[ uvID[2] ];
			pVertices[2].position = D3DXVECTOR3( data.x[ vID[0] ], data.y[ vID[0] ], data.z[ vID[0] ] );
			pVertices[2].tu       = data.material[j].u[ uvID[0] ];
			pVertices[2].tv       = data.material[j].v[ uvID[0] ];
			pVertices[3].position = D3DXVECTOR3( data.x[ vID[3] ], data.y[ vID[3] ], data.z[ vID[3] ] );
			pVertices[3].tu       = data.material[j].u[ uvID[3] ];
			pVertices[3].tv       = data.material[j].v[ uvID[3] ];
			for(int k=0; k<4; k++){
				pVertices[k].color = D3DCOLOR_COLORVALUE(data.material[j].shadow, data.material[j].shadow, data.material[j].shadow, 1.0f);
			}

			//GPUのロックを解除
			g_pVB->Unlock();
		}
	}
#else
	//ブロック数を取得
	bs = BlockDataIF->GetTotaldatas();

	for(int i=0; i<bs; i++){
		//データを取得
		BlockDataIF->Getdata(&data, i);

		for(int j=0; j<6; j++){
			//面の頂点データの関連付けを取得
			blockdataface(j, vID, uvID);

			//頂点座標・UV座標・色を設定
			g_pVertices[i][j][0].position = D3DXVECTOR3( data.x[ vID[1] ], data.y[ vID[1] ], data.z[ vID[1] ] );
			g_pVertices[i][j][0].tu       = data.material[j].u[ uvID[1] ];
			g_pVertices[i][j][0].tv       = data.material[j].v[ uvID[1] ];
			g_pVertices[i][j][1].position = D3DXVECTOR3( data.x[ vID[2] ], data.y[ vID[2] ], data.z[ vID[2] ] );
			g_pVertices[i][j][1].tu       = data.material[j].u[ uvID[2] ];
			g_pVertices[i][j][1].tv       = data.material[j].v[ uvID[2] ];
			g_pVertices[i][j][2].position = D3DXVECTOR3( data.x[ vID[0] ], data.y[ vID[0] ], data.z[ vID[0] ] );
			g_pVertices[i][j][2].tu       = data.material[j].u[ uvID[0] ];
			g_pVertices[i][j][2].tv       = data.material[j].v[ uvID[0] ];
			g_pVertices[i][j][3].position = D3DXVECTOR3( data.x[ vID[3] ], data.y[ vID[3] ], data.z[ vID[3] ] );
			g_pVertices[i][j][3].tu       = data.material[j].u[ uvID[3] ];
			g_pVertices[i][j][3].tv       = data.material[j].v[ uvID[3] ];
			for(int k=0; k<4; k++){
				g_pVertices[i][j][k].color = D3DCOLOR_COLORVALUE(data.material[j].shadow, data.material[j].shadow, data.material[j].shadow, 1.0f);
			}
		}
	}
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	//OutputLog.WriteLog(LOG_COMPLETE, "", id);

	char str[32];
	sprintf(str, "Blocks:%d, Vertices:%d", bs, bs*6*4);
	OutputLog.WriteLog(LOG_COMPLETE, "", str);
#endif
}

//! @brief マップデータを描画
//! @param wireframe ワイヤーフレーム表示
void D3DGraphics::RenderMapdata(bool wireframe)
{
	//ブロックデータが読み込まれていなければ、処理しない。
	if( BlockDataIF == NULL ){ return; }

	blockdata data;
	int textureID;

	if( wireframe == true ){
		int linecolor = GetColorCode(0.0f,1.0f,0.0f,1.0f);

		//ワイヤーフレーム表示
		for(int i=0; i<bs; i++){
			BlockDataIF->Getdata(&data, i);
			Renderline(data.x[0], data.y[0], data.z[0], data.x[1], data.y[1], data.z[1], linecolor);
			Renderline(data.x[1], data.y[1], data.z[1], data.x[2], data.y[2], data.z[2], linecolor);
			Renderline(data.x[2], data.y[2], data.z[2], data.x[3], data.y[3], data.z[3], linecolor);
			Renderline(data.x[3], data.y[3], data.z[3], data.x[0], data.y[0], data.z[0], linecolor);
			Renderline(data.x[4], data.y[4], data.z[4], data.x[5], data.y[5], data.z[5], linecolor);
			Renderline(data.x[5], data.y[5], data.z[5], data.x[6], data.y[6], data.z[6], linecolor);
			Renderline(data.x[6], data.y[6], data.z[6], data.x[7], data.y[7], data.z[7], linecolor);
			Renderline(data.x[7], data.y[7], data.z[7], data.x[4], data.y[4], data.z[4], linecolor);
			Renderline(data.x[0], data.y[0], data.z[0], data.x[4], data.y[4], data.z[4], linecolor);
			Renderline(data.x[1], data.y[1], data.z[1], data.x[5], data.y[5], data.z[5], linecolor);
			Renderline(data.x[2], data.y[2], data.z[2], data.x[6], data.y[6], data.z[6], linecolor);
			Renderline(data.x[3], data.y[3], data.z[3], data.x[7], data.y[7], data.z[7], linecolor);
		}
		return;
	}

	//深度バッファ比較関数を設定
	//pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);


#ifdef BLOCKDATA_GPUMEMORY
	//データ設定
	pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(VERTEXTXTA));

	for(textureID=0; textureID<TOTAL_BLOCKTEXTURE; textureID++){
		//テクスチャが正常に読み込めていなければ設定
#if GRAPHIC_ENGINE == 0
		if( mapTextureID[textureID] == -1 ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pd3dDevice->SetTexture(0, NULL);
		}
		else if( ptextures[ mapTextureID[textureID] ] == NULL ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pd3dDevice->SetTexture(0, NULL);
		}
		else{
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			pd3dDevice->SetTexture(0, ptextures[mapTextureID[textureID]] );
		}
#else
		if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, false); }
		if( mapTextureID[textureID] == -1 ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
			pd3dDevice->SetTexture(0, NULL);
		}
		else if( ptextures[ mapTextureID[textureID] ] == NULL ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
			pd3dDevice->SetTexture(0, NULL);
		}
		else{
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
			pd3dDevice->SetTexture(0, ptextures[mapTextureID[textureID]] );
		}
#endif

		for(int i=0; i<bs; i++){
			//データ取得
			BlockDataIF->Getdata(&data, i);

			for(int j=0; j<6; j++){
				//テクスチャのデータ番号を取得
				int ID = data.material[j].textureID;

				if( textureID == ID ){
					//面を描画
					pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, (i*6+j)*4, 2);
				}
			}
		}
	}
#else
	//データを設定
	pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);

	for(textureID=0; textureID<TOTAL_BLOCKTEXTURE; textureID++){
		//テクスチャが正常に読み込めていなければ設定
#if GRAPHIC_ENGINE == 0
		if( mapTextureID[textureID] == -1 ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pd3dDevice->SetTexture(0, NULL);
		}
		else if( ptextures[ mapTextureID[textureID] ] == NULL ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pd3dDevice->SetTexture(0, NULL);
		}
		else{
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			pd3dDevice->SetTexture(0, ptextures[mapTextureID[textureID]] );
		}
#else
		if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, false); }
		if( mapTextureID[textureID] == -1 ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
			pd3dDevice->SetTexture(0, NULL);
		}
		else if( ptextures[ mapTextureID[textureID] ] == NULL ){
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
			pd3dDevice->SetTexture(0, NULL);
		}
		else{
			pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
			pd3dDevice->SetTexture(0, ptextures[mapTextureID[textureID]] );
		}
#endif

		for(int i=0; i<bs; i++){
			//データ取得
			BlockDataIF->Getdata(&data, i);

			for(int j=0; j<6; j++){
				//テクスチャのデータ番号を取得
				int ID = data.material[j].textureID;

				if( textureID == ID ){
					//面を描画
					pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_pVertices[i][j], sizeof(VERTEXTXTA));
				}
			}
		}
	}
#endif

	//深度バッファ比較関数を元に戻す
	//pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
}

//! @brief マップテクスチャを取得
//! @param id テクスチャ番号
//! @return テクスチャのデータ番号（失敗：-1）
int D3DGraphics::GetMapTextureID(int id)
{
	if( (id < 0)||(TOTAL_BLOCKTEXTURE <= id ) ){ return -1; }
	return mapTextureID[id];
}

//! @brief マップデータを解放
void D3DGraphics::CleanupMapdata()
{
#ifdef BLOCKDATA_GPUMEMORY
	if( g_pVB == NULL ){ return; }
#endif
	if( (bs == 0)&&(BlockDataIF == NULL) ){ return; }

	//テクスチャを解放
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		CleanupTexture(mapTextureID[i]);
	}

#ifdef BLOCKDATA_GPUMEMORY
	//頂点データ解放
	if( g_pVB != NULL ){
		g_pVB->Release();
		g_pVB = NULL;
	}
#endif
	bs = 0;

	BlockDataIF = NULL;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CLEANUP, "Mapdata", "（Vertex data）");
#endif
}

//! @brief モデルファイルを描画
//! @param id_model モデルのデータ番号
//! @param id_texture テクスチャのデータ番号
//! @param darkflag モデルを暗くする
//! @param HUDmode HUD表示用モデルに設定
//! @param nomodel モデル描画無効化
void D3DGraphics::RenderModel(int id_model, int id_texture, bool darkflag, bool HUDmode, bool nomodel)
{
	//無効な引数が設定されていれば失敗
	if( id_model == -1 ){ return; }
	//if( id_texture == -1 ){ return; }

	//指定したモデルが初期化されていなければ失敗
	if( pmesh[id_model] == NULL ){ return; }

	if( nomodel == true ){
		Renderline(1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, GetColorCode(1.0f,0.5f,0.0f,1.0f));
		Renderline(0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, GetColorCode(1.0f,0.5f,0.0f,1.0f));
		Renderline(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, GetColorCode(1.0f,0.5f,0.0f,1.0f));
		return;
	}

	float Brightness;

	if( darkflag == false ){
		Brightness = 1.0f;
	}
	else{
		Brightness = 0.8f;
	}

	//ライティング有効化
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	//色合い設定
#if GRAPHIC_ENGINE == 0
	D3DMATERIAL9 mtrl;
	ZeroMemory(&mtrl, sizeof(mtrl));
	mtrl.Emissive = D3DXCOLOR(Brightness, Brightness, Brightness, 1.0f);
	pd3dDevice->SetMaterial(&mtrl);
#else
	float DiffuseColor[4] = {Brightness, Brightness, Brightness, 1.0f};
	hVSConstantTable->SetFloatArray(pd3dDevice, hVSDiffuse, DiffuseColor, 4);
#endif

	//テクスチャ設定
#if GRAPHIC_ENGINE == 0
	if( id_texture == -1 ){
		pd3dDevice->SetTexture(0, NULL);
	}
	else if( ptextures[id_texture] == NULL ){
		pd3dDevice->SetTexture(0, NULL);
	}
	else{
		pd3dDevice->SetTexture( 0, ptextures[id_texture] );
	}
#else
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, false); }
	if( id_texture == -1 ){
		pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
		pd3dDevice->SetTexture(0, NULL);
	}
	else if( ptextures[id_texture] == NULL ){
		pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
		pd3dDevice->SetTexture(0, NULL);
	}
	else{
		pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
		pd3dDevice->SetTexture( 0, ptextures[id_texture] );
	}
#endif

	//描画
	for(int i=0; i<(signed)nummaterials[id_model]; i=i+1){
		pmesh[id_model]->DrawSubset(i);
	}

#if GRAPHIC_ENGINE == 1
	//色合い再設定
	float DiffuseColor2[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	hVSConstantTable->SetFloatArray(pd3dDevice, hVSDiffuse, DiffuseColor2, 4);
#endif

	//ライティング無効化
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
}

//! @brief 板を描画
//! @param id_texture テクスチャのデータ番号
//! @param alpha 透明度　（0.0～1.0　0.0：完全透明）
//! @param nomodel 板の描画無効化
void D3DGraphics::RenderBoard(int id_texture, float alpha, bool nomodel)
{
	//テクスチャが設定されていなければ、処理しない。
	if( id_texture == -1 ){ return; }

	if( nomodel == true ){
		Renderline(0.0f, 0.5f, -0.5f, 0.0f, -0.5f, -0.5f, GetColorCode(0.75f,0.75f,0.75f,1.0f));
		Renderline(0.0f, -0.5f, -0.5f, 0.0f, -0.5f, 0.5f, GetColorCode(0.75f,0.75f,0.75f,1.0f));
		Renderline(0.0f, -0.5f, 0.5f, 0.0f, 0.5f, 0.5f, GetColorCode(0.75f,0.75f,0.75f,1.0f));
		Renderline(0.0f, 0.5f, 0.5f, 0.0f, 0.5f, -0.5f, GetColorCode(0.75f,0.75f,0.75f,1.0f));
		return;
	}

	VERTEXTXTA BoardVertices[4];

	//頂点座標・UV座標・色/透明度を設定
	BoardVertices[0].position = D3DXVECTOR3(0.0f, 0.5f, -0.5f);
	BoardVertices[0].tu       = 1.0f;
	BoardVertices[0].tv       = 0.0f;
	BoardVertices[1].position = D3DXVECTOR3(0.0f, -0.5f, -0.5f);
	BoardVertices[1].tu       = 1.0f;
	BoardVertices[1].tv       = 1.0f;
	BoardVertices[2].position = D3DXVECTOR3(0.0f, 0.5f, 0.5f);
	BoardVertices[2].tu       = 0.0f;
	BoardVertices[2].tv       = 0.0f;
	BoardVertices[3].position = D3DXVECTOR3(0.0f, -0.5f, 0.5f);
	BoardVertices[3].tu       = 0.0f;
	BoardVertices[3].tv       = 1.0f;
	for(int i=0; i<4; i++){
		BoardVertices[i].color = D3DCOLOR_COLORVALUE(1.0f, 1.0f, 1.0f, alpha);
	}

	//アルファブレンドを設定
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	//テクスチャとデータ形式を設定し描画
#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, false); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
#endif
	pd3dDevice->SetTexture(0, ptextures[id_texture]);
	pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, BoardVertices, sizeof(VERTEXTXTA));

	//アルファブレンドを元に戻す
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
}

//! @brief エフェクト描画用設定
void D3DGraphics::StartEffectRender()
{
	//深度バッファ比較関数 無効化
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
}

//! @brief エフェクト描画用設定を解除
void D3DGraphics::EndEffectRender()
{
	//深度バッファ比較関数 有効化
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}

//! @brief 画面の明るさを設定
//! @param Width 幅
//! @param Height 高さ
//! @param Brightness 画面の明るさ　（0 で不変、1 以上で明るさの度合い）
void D3DGraphics::ScreenBrightness(int Width, int Height, int Brightness)
{
	//明るさ不変なら処理しない（軽量化）
	if( Brightness == 0 ){ return; }

	//透明度を設定し、描画
	float alpha = 0.02f * Brightness;
	Draw2DBox(0, 0, Width, Height, D3DCOLOR_COLORVALUE(1.0f,1.0f,1.0f,alpha));
}

//! @brief 【デバック用】中心線表示
void D3DGraphics::RenderCenterline()
{
	ResetWorldTransform();
	Renderline(100.0f, 0.0f, 0.0f, -100.0f, 0.0f, 0.0f, GetColorCode(1.0f,0.0f,0.0f,1.0f));
	Renderline(0.0f, 100.0f, 0.0f, 0.0f, -100.0f, 0.0f, GetColorCode(0.0f,1.0f,0.0f,1.0f));
	Renderline(0.0f, 0.0f, 100.0f, 0.0f, 0.0f, -100.0f, GetColorCode(0.0f,0.0f,1.0f,1.0f));
}

//! @brief 【デバック用】線表示
void D3DGraphics::Renderline(float x1, float y1, float z1, float x2, float y2, float z2, int color)
{
	VERTEXTXTA mv[2];

	mv[0].position = D3DXVECTOR3(x1, y1, z1);
	mv[1].position = D3DXVECTOR3(x2, y2, z2);
	for(int i=0; i<2; i++){
		mv[i].color = color;
		mv[i].tu = 0.0f;
		mv[i].tv = 0.0f;
	}

#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, false); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
#endif
	pd3dDevice->SetTexture(0, NULL);
	pd3dDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, mv, sizeof(VERTEXTXTA));
}

//! @brief 2D システムフォントによるテキスト表示を開始
//! @attention DirectXの ID3DXSprite を初期化しています。
void D3DGraphics::Start2DMSFontTextRender()
{
	ptextsprite->Begin(D3DXSPRITE_ALPHABLEND);
}

//! @brief 文字を表示（システムフォント使用）
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：可）
//! @param color 色
//! @warning <b>表示は非常に低速です。</b>画面内で何度も呼び出すとパフォーマンスに影響します。
//! @warning 「改行コードを活用し一度に表示する」「日本語が必要ない文字はテクスチャフォントを活用する」などの対応を講じてください。
//! @attention DirectXの ID3DXSprite を使用し、システムフォントで表示しています。
//! @attention フォントの種類やサイズは固定です。　文字を二重に重ねて立体感を出さないと見にくくなります。
void D3DGraphics::Draw2DMSFontText(int x, int y, const char *str, int color)
{
	//if( ptextsprite == NULL ){ return; }
	if( str == NULL ){ return; }

	//テキストスプライト初期化
	Start2DMSFontTextRender();

	//基準座標を設定
	D3DXMATRIX matWorld;
	D3DXMatrixIdentity(&matWorld);
	ptextsprite->SetTransform(&matWorld);

	//文字を表示
	RECT rc = {x, y, 0, 0};
	pxmsfont->DrawText(ptextsprite, str, -1, &rc, DT_NOCLIP, color);

	//テキストスプライト解放
	End2DMSFontTextRender();
}

//! @brief 文字を表示（システムフォント使用）【スケーリング機能付き】
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：可）
//! @param color 色
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DMSFontText()関数と同等です。
void D3DGraphics::Draw2DMSFontTextScaling(int x, int y, const char *str, int color)
{
	if( str == NULL ){ return; }

	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;

	Draw2DMSFontText((int)(scaling_x * x), (int)(scaling_y * y), str, color);
}

//! @brief 文字を中央揃えで表示（システムフォント使用）
//! @param x x座標
//! @param y y座標
//! @param w 横の大きさ
//! @param h 縦の大きさ
//! @param str 文字列　（改行コード：可）
//! @param color 色
//! @warning <b>表示は非常に低速です。</b>画面内で何度も呼び出すとパフォーマンスに影響します。
//! @warning 「改行コードを活用し一度に表示する」「日本語が必要ない文字はテクスチャフォントを活用する」などの対応を講じてください。
//! @attention DirectXの ID3DXSprite を使用し、システムフォントで表示しています。
//! @attention フォントの種類やサイズは固定です。　文字を二重に重ねて立体感を出さないと見にくくなります。
void D3DGraphics::Draw2DMSFontTextCenter(int x, int y, int w, int h, const char *str, int color)
{
	//if( ptextsprite == NULL ){ return; }
	if( str == NULL ){ return; }

	//テキストスプライト初期化
	Start2DMSFontTextRender();

	//基準座標を設定
	D3DXMATRIX matWorld;
	D3DXMatrixIdentity(&matWorld);
	ptextsprite->SetTransform(&matWorld);

	//文字を表示
	RECT rc = {x, y, x+w, y+h};
	pxmsfont->DrawText(ptextsprite, str, -1, &rc, DT_CENTER, color);

	//テキストスプライト解放
	End2DMSFontTextRender();
}

//! @brief 文字を中央揃えで表示（システムフォント使用）【スケーリング機能付き】
//! @param x x座標
//! @param y y座標
//! @param w 横の大きさ
//! @param h 縦の大きさ
//! @param str 文字列　（改行コード：可）
//! @param color 色
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DMSFontTextCenter()関数と同等です。
void D3DGraphics::Draw2DMSFontTextCenterScaling(int x, int y, int w, int h, const char *str, int color)
{
	if( str == NULL ){ return; }

	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;

	Draw2DMSFontTextCenter((int)(scaling_x * x), (int)(scaling_y * y), w, h, str, color);
}

//! @brief 文字を表示（システムフォント使用、小さい）
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：可）
//! @param color 色
//! @note 基本仕様はDraw2DMSFontText()と同等ですが、文字が小さくなります。
void D3DGraphics::Draw2DMSSmallFontText(int x, int y, const char *str, int color)
{
	//if( ptextsprite == NULL ){ return; }
	if( str == NULL ){ return; }

	//テキストスプライト初期化
	Start2DMSFontTextRender();

	//基準座標を設定
	D3DXMATRIX matWorld;
	D3DXMatrixIdentity(&matWorld);
	ptextsprite->SetTransform(&matWorld);

	//文字を表示
	RECT rc = {x, y, 0, 0};
	pxmssmallfont->DrawText(ptextsprite, str, -1, &rc, DT_NOCLIP, color);

	//テキストスプライト解放
	End2DMSFontTextRender();
}

//! @brief 2D システムフォントによるテキスト表示を終了
//! @attention DirectXの ID3DXSprite を解放しています。
void D3DGraphics::End2DMSFontTextRender()
{
	ptextsprite->End();
}

//! @brief 2D描画用設定
void D3DGraphics::Start2DRender()
{
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	//深度バッファ比較関数を設定
	pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

	//フォグを一時的に無効にする
#if GRAPHIC_ENGINE == 0
	pd3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
#else
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableFog, false);
#endif
}

//! @brief テクスチャフォントサイズ計算
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param base_w デフォルトの一文字の幅
//! @param base_h デフォルトの一文字の高さ
//! @param limitlen デフォルトの文字数
//! @param w 一文字の幅を取得するポインタ
//! @param h 一文字の高さを取得するポインタ
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

//! @brief 文字を表示（テクスチャフォント使用）
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param color 色
//! @param fontwidth 一文字の幅
//! @param fontheight 一文字の高さ
//! @attention 文字を二重に重ねて立体感を出さないと見にくくなります。
void D3DGraphics::Draw2DTextureFontText(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	//テクスチャフォントの取得に失敗していれば、処理しない
	if( TextureFont == -1 ){ return; }
	if( str == NULL ){ return; }

	//2D描画用設定を適用
	Start2DRender();

	int w;
	float font_u, font_v;
	float t_u, t_v;
	TLVERTX pBoxVertices[4];

	//1文字のUV座標を計算
	font_u = 1.0f / 16;
	font_v = 1.0f / 16;

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//テクスチャをフォントテクスチャに設定
#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
#endif
	pd3dDevice->SetTexture( 0, ptextures[TextureFont] );

	//データ形式を設定
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	// 与えられた文字数分ループ
	for(int i=0; i<(int)strlen(str); i++){
		//UV座標を計算
		w = str[i];
		if( w < 0 ){ w += 256; }
		t_u = (w % 16) * font_u;
		t_v = (w / 16) * font_v;

		//頂点座標・UV座標・色を設定
		pBoxVertices[0].x = (float)x + i*fontwidth;
		pBoxVertices[0].y = (float)y;
		pBoxVertices[0].tu = t_u;
		pBoxVertices[0].tv = t_v;
		pBoxVertices[1].x = (float)x + fontwidth + i*fontwidth;
		pBoxVertices[1].y = (float)y;
		pBoxVertices[1].tu = t_u + font_u;
		pBoxVertices[1].tv = t_v;
		pBoxVertices[2].x = (float)x + i*fontwidth;
		pBoxVertices[2].y = (float)y + fontheight;
		pBoxVertices[2].tu = t_u;
		pBoxVertices[2].tv = t_v + font_v;
		pBoxVertices[3].x = (float)x + fontwidth + i*fontwidth;
		pBoxVertices[3].y = (float)y + fontheight;
		pBoxVertices[3].tu = t_u + font_u;
		pBoxVertices[3].tv = t_v + font_v;
		for(int j=0; j<4; j++){
			pBoxVertices[j].z = 0.0f;
			pBoxVertices[j].rhw = 1.0f;
			pBoxVertices[j].color = color;
		}

		//表示
		pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pBoxVertices, sizeof(TLVERTX));
	}

	//2D描画用設定を解除
	End2DRender();
}

//! @brief 文字を表示（テクスチャフォント使用）【スケーリング機能付き】
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param color 色
//! @param fontwidth 一文字の幅
//! @param fontheight 一文字の高さ
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DTextureFontText()関数と同等です。
void D3DGraphics::Draw2DTextureFontTextScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	if( str == NULL ){ return; }

	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;
	
	Draw2DTextureFontText((int)(scaling_x * x), (int)(scaling_y * y), str, color, (int)(scaling_x * fontwidth), (int)(scaling_x * fontheight));
}

//! @brief 中央寄せに文字を表示（テクスチャフォント使用）
//! @param x x座標の加算分
//! @param y y座標
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param color 色
//! @param fontwidth 一文字の幅
//! @param fontheight 一文字の高さ
//! @attention 文字を二重に重ねて立体感を出さないと見にくくなります。
void D3DGraphics::Draw2DTextureFontTextCenter(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	if( str == NULL ){ return; }

	Draw2DTextureFontText((GameConfig.GetScreenWidth() - strlen(str)*fontwidth)/2 + x, y, str, color, fontwidth, fontheight);
}

//! @brief 中央寄せに文字を表示（テクスチャフォント使用）【スケーリング機能付き】
//! @param x x座標の加算分
//! @param y y座標
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param color 色
//! @param fontwidth 一文字の幅
//! @param fontheight 一文字の高さ
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DTextureFontTextCenter()関数と同等です。
void D3DGraphics::Draw2DTextureFontTextCenterScaling(int x, int y, const char *str, int color, int fontwidth, int fontheight)
{
	if( str == NULL ){ return; }

	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;
	
	Draw2DTextureFontTextCenter((int)(scaling_x * x), (int)(scaling_y * y), str, color, (int)(scaling_x * fontwidth), (int)(scaling_x * fontheight));
}

#ifdef ENABLE_DEBUGCONSOLE
//! @brief 文字を表示（デバック用フォント使用）
//! @param x x座標
//! @param y y座標
//! @param str 文字列　（改行コード：<b>不可</b>）
//! @param color 色
//! @attention 一文字の幅および高さは 8x16 固定です。
//! @attention 文字を二重に重ねて立体感を出さないと見にくくなります。
void D3DGraphics::Draw2DTextureDebugFontText(int x, int y, const char *str, int color)
{
	if( str == NULL ){ return; }

	int fontwidth = 8;
	int fontheight = 16;

	//テクスチャフォントの取得に失敗していれば、処理しない
	if( TextureDebugFont == -1 ){ return; }

	//2D描画用設定を適用
	Start2DRender();

	// テクスチャフィルタ無効化
	pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);

	int w;
	float font_u, font_v;
	float t_u, t_v;
	TLVERTX pBoxVertices[4];

	//1文字のUV座標を計算
	font_u = 1.0f / 16;
	font_v = 1.0f / 8;

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//テクスチャをフォントテクスチャに設定
#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
#endif
	pd3dDevice->SetTexture( 0, ptextures[TextureDebugFont] );

	//データ形式を設定
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	// 与えられた文字数分ループ
	for(int i=0; i<(int)strlen(str); i++){
		//UV座標を計算
		w = str[i];
		if( w < 0 ){ w = ' '; }
		t_u = (w % 16) * font_u;
		t_v = (w / 16) * font_v;

		//頂点座標・UV座標・色を設定
		pBoxVertices[0].x = (float)x + i*fontwidth;
		pBoxVertices[0].y = (float)y;
		pBoxVertices[0].tu = t_u;
		pBoxVertices[0].tv = t_v;
		pBoxVertices[1].x = (float)x + fontwidth + i*fontwidth;
		pBoxVertices[1].y = (float)y;
		pBoxVertices[1].tu = t_u + font_u;
		pBoxVertices[1].tv = t_v;
		pBoxVertices[2].x = (float)x + i*fontwidth;
		pBoxVertices[2].y = (float)y + fontheight;
		pBoxVertices[2].tu = t_u;
		pBoxVertices[2].tv = t_v + font_v;
		pBoxVertices[3].x = (float)x + fontwidth + i*fontwidth;
		pBoxVertices[3].y = (float)y + fontheight;
		pBoxVertices[3].tu = t_u + font_u;
		pBoxVertices[3].tv = t_v + font_v;
		for(int j=0; j<4; j++){
			pBoxVertices[j].z = 0.0f;
			pBoxVertices[j].rhw = 1.0f;
			pBoxVertices[j].color = color;
		}

		//表示
		pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pBoxVertices, sizeof(TLVERTX));
	}

	//テクスチャフィルタ有効化
	pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	//2D描画用設定を解除
	End2DRender();
}
#endif

//! @brief 線を描画
//! @param x1 始点の x座標
//! @param y1 始点の y座標
//! @param x2 終点の x座標
//! @param y2 終点の y座標
//! @param color 色
void D3DGraphics::Draw2DLine(int x1, int y1, int x2, int y2, int color)
{
	TLVERTX pLineVertices[2];

	//2D描画用設定を適用
	Start2DRender();

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//頂点座標と色などを設定
	pLineVertices[0].x = (float)x1;
	pLineVertices[0].y = (float)y1;
	pLineVertices[1].x = (float)x2;
	pLineVertices[1].y = (float)y2;
	for(int i=0; i<2; i++){
		pLineVertices[i].z = 0.0f;
		pLineVertices[i].rhw = 1.0f;
		pLineVertices[i].color = color;
		pLineVertices[i].tu = 0.0f;
		pLineVertices[i].tv = 0.0f;
	}

#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
#endif
	pd3dDevice->SetTexture(0, NULL);

	//データ形式を設定し、描画。
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, pLineVertices, sizeof(TLVERTX));

	//2D描画用設定を解除
	End2DRender();
}

//! @brief 円（16角形）を描画
//! @param x 中心の x座標
//! @param y 中心の y座標
//! @param r 半径
//! @param color 色
void D3DGraphics::Draw2DCycle(int x, int y, int r, int color)
{
	TLVERTX pLineVertices[16+1];

	//2D描画用設定を適用
	Start2DRender();

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//頂点座標と色などを設定
	for(int i=0; i<16+1; i++){
		pLineVertices[i].x = (float)x + cosf(DegreeToRadian((360.0f/16.0f)) * i) * r;
		pLineVertices[i].y = (float)y + sinf(DegreeToRadian((360.0f/16.0f)) * i) * r;

		pLineVertices[i].z = 0.0f;
		pLineVertices[i].rhw = 1.0f;
		pLineVertices[i].color = color;
		pLineVertices[i].tu = 0.0f;
		pLineVertices[i].tv = 0.0f;
	}

#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
#endif
	pd3dDevice->SetTexture(0, NULL);

	//データ形式を設定し、描画。
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 16, pLineVertices, sizeof(TLVERTX));

	//2D描画用設定を解除
	End2DRender();
}

//! @brief 四角形を描画
//! @param x1 左上の x座標
//! @param y1 左上の y座標
//! @param x2 右下の x座標
//! @param y2 右下の y座標
//! @param color 色
void D3DGraphics::Draw2DBox(int x1, int y1, int x2, int y2, int color)
{
	TLVERTX pBoxVertices[4];

	//2D描画用設定を適用
	Start2DRender();

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//頂点座標と色などを設定
	pBoxVertices[0].x = (float)x1;
	pBoxVertices[0].y = (float)y1;
	pBoxVertices[1].x = (float)x2;
	pBoxVertices[1].y = (float)y1;
	pBoxVertices[2].x = (float)x1;
	pBoxVertices[2].y = (float)y2;
	pBoxVertices[3].x = (float)x2;
	pBoxVertices[3].y = (float)y2;
	for(int i=0; i<4; i++){
		pBoxVertices[i].z = 0.0f;
		pBoxVertices[i].rhw = 1.0f;
		pBoxVertices[i].color = color;
		pBoxVertices[i].tu = 0.0f;
		pBoxVertices[i].tv = 0.0f;
	}

#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, false);
#endif
	pd3dDevice->SetTexture(0, NULL);

	//データ形式を設定し、描画。
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pBoxVertices, sizeof(TLVERTX));

	//2D描画用設定を解除
	End2DRender();
}

//! @brief 四角形を描画【スケーリング機能付き】
//! @param x1 左上の x座標
//! @param y1 左上の y座標
//! @param x2 右下の x座標
//! @param y2 右下の y座標
//! @param color 色
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DBox()関数と同等です。
void D3DGraphics::Draw2DBoxScaling(int x1, int y1, int x2, int y2, int color)
{
	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;

	Draw2DBox((int)(scaling_x * x1), (int)(scaling_y * y1), (int)(scaling_x * x2), (int)(scaling_y * y2), color);
}

//! @brief 画像を描画
//! @param x x座標
//! @param y y座標
//! @param id テクスチャのデータ番号
//! @param width 幅
//! @param height 高さ
//! @param alpha 透明度（0.0～1.0）
void D3DGraphics::Draw2DTexture(int x, int y, int id, int width, int height, float alpha)
{
	//無効なテクスチャ番号を指定されていれば処理しない
	if( id == -1 ){ return; }

	TLVERTX pBoxVertices[4];

	//2D描画用設定を適用
	Start2DRender();

	//ワールド座標を原点に戻す
	ResetWorldTransform();

	//頂点座標・UV座標・色を設定
	pBoxVertices[0].x = (float)x;
	pBoxVertices[0].y = (float)y;
	pBoxVertices[0].tu = 0.0f;
	pBoxVertices[0].tv = 0.0f;
	pBoxVertices[1].x = (float)x + width;
	pBoxVertices[1].y = (float)y;
	pBoxVertices[1].tu = 1.0f;
	pBoxVertices[1].tv = 0.0f;
	pBoxVertices[2].x = (float)x;
	pBoxVertices[2].y = (float)y + height;
	pBoxVertices[2].tu = 0.0f;
	pBoxVertices[2].tv = 1.0f;
	pBoxVertices[3].x = (float)x + width;
	pBoxVertices[3].y = (float)y + height;
	pBoxVertices[3].tu = 1.0f;
	pBoxVertices[3].tv = 1.0f;
	for(int i=0; i<4; i++){
		pBoxVertices[i].z = 0.0f;
		pBoxVertices[i].rhw = 1.0f;
		pBoxVertices[i].color = D3DCOLOR_COLORVALUE(1.0f,1.0f,1.0f,alpha);
	}

	//テクスチャとデータ形式を設定し、描画
#if GRAPHIC_ENGINE == 1
	if( hPSEnable2DRendering != NULL ){ pPSConstantTable->SetBool(pd3dDevice, hPSEnable2DRendering, true); }
	pPSConstantTable->SetBool(pd3dDevice, hPSEnableTexture, true);
#endif
	pd3dDevice->SetTexture( 0, ptextures[id] );
	pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pBoxVertices, sizeof(TLVERTX));

	//2D描画用設定を解除
	End2DRender();
}

//! @brief 画像を描画【スケーリング機能付き】
//! @param x x座標
//! @param y y座標
//! @param id テクスチャのデータ番号
//! @param width 幅
//! @param height 高さ
//! @param alpha 透明度（0.0～1.0）
//! @attention 640x480相当の解像度から現解像度へスケーリングします。それ以外はDraw2DTexture()関数と同等です。
void D3DGraphics::Draw2DTextureScaling(int x, int y, int id, int width, int height, float alpha)
{
	float scaling_x = (float)GameConfig.GetScreenHeight() / 480;//(float)GameConfig.GetScreenWidth() / 640;
	float scaling_y = (float)GameConfig.GetScreenHeight() / 480;

	Draw2DTexture((int)(scaling_x * x), (int)(scaling_y * y), id, (int)(scaling_x * width), (int)(scaling_y * height), alpha);
}

//! @brief 2D描画用設定を解除
void D3DGraphics::End2DRender()
{
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

	//深度バッファ比較関数を元に戻す
	pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	//フォグを元に戻す
	if( EnableFogFlag == true ){
#if GRAPHIC_ENGINE == 0
		pd3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
#else
		pPSConstantTable->SetBool(pd3dDevice, hPSEnableFog, true);
#endif
	}
}

//! @brief 画面のスクリーンショットを保存
//! @param filename ファイル名
//! @return 成功：true　失敗：false
bool D3DGraphics::SaveScreenShot(const char* filename)
{
	if( filename == NULL ){ return false; }

	LPDIRECT3DSURFACE9 pSurface = NULL;
	HRESULT hr;

	//サーフェースを作成し、画面を取得
	pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pSurface);

	//サーフェイスを画像に出力
	hr = D3DXSaveSurfaceToFile(filename, D3DXIFF_BMP, pSurface, NULL, NULL);

	//解放
	pSurface->Release();

	if( hr == D3D_OK ){
		return true;
	}
	return false;
}

//! @brief カラーコードを取得
//! @param red 赤（0.0f～1.0f）
//! @param green 緑（0.0f～1.0f）
//! @param blue 青（0.0f～1.0f）
//! @param alpha 透明度（0.0f～1.0f）
//! @return カラーコード
int D3DGraphics::GetColorCode(float red, float green, float blue, float alpha)
{
	return D3DCOLOR_COLORVALUE(red, green, blue, alpha);
}

#endif	//GRAPHIC_ENGINE