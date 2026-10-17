//! @file scene.cpp
//! @brief 各画面を管理するクラスの定義

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

#include "scene.h"

//! @brief コンストラクタ
scene::scene()
{
	GameState = NULL;
	WindowCtrl = NULL; 
	d3dg = NULL;
	inputCtrl = NULL;
	framecnt = 0;
}

//! @brief ディストラクタ
scene::~scene()
{}

//! @brief クラスを設定
void scene::SetClass(StateMachine *in_GameState, WindowControl *in_WindowCtrl, D3DGraphics *in_d3dg, InputControl *in_inputCtrl)
{
	GameState = in_GameState;
	WindowCtrl = in_WindowCtrl; 
	d3dg = in_d3dg;
	inputCtrl = in_inputCtrl;
}

//! @brief シーン作成
int scene::Create()
{
	inputCtrl->MoveMouseCenter();
	framecnt = 0;

	GameState->NextState();
	return 0;
}

//! @brief シーン描画回復
int scene::Recovery()
{
	return 0;
}

//! @brief シーン入力処理
void scene::Input()
{
	inputCtrl->GetInputState(false);

	if( inputCtrl->CheckKeyDown(GetEscKeycode()) ){
		GameState->PushBackSpaceKey();
	}
	else if( inputCtrl->CheckMouseButtonUpL() ){
		GameState->PushMouseButton();
	}
}

//! @brief シーン計算処理
void scene::Process()
{
	framecnt += 1;
}

//! @brief シーン描画処理
//! @return 成功：false　失敗：true
//! @attention 失敗した場合、D3DGraphicsクラスのResetD3D()関数を呼び出してください。
bool scene::RenderMain()
{
	if( d3dg->StartRender() ){ return true; }

	d3dg->Draw2DBox(0, 0, GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), d3dg->GetColorCode(0.0f,0.0f,0.0f,1.0f));
	d3dg->Draw2DTextureFontText(10, 10, "hello world !", d3dg->GetColorCode(1.0f,1.0f,1.0f,1.0f), 20, 32);

	d3dg->ScreenBrightness(GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), GameConfig.GetBrightness());

	return d3dg->EndRender();
}

//! @brief シーン解放処理
void scene::Destroy()
{
	GameState->NextState();
}

//! @brief フレームカウントを元にアルファ値・透明度を設定（1回）
//! @param tframecnt 対象とするカウント値
//! @param MaxAlpha 最大アルファ値
//! @param timingsec 間隔・秒数
//! @param offsetsec オフセット・秒数
//! @param reversal 反転フラグ（false：フィールドイン　true：フィールドアウト）
//! @return アルファ値
float scene::GetEffectAlpha(unsigned int tframecnt, float MaxAlpha, float timingsec, float offsetsec, bool reversal)
{
	float alpha;
	unsigned int frametiming;
	unsigned int frameoffset;

	frametiming = (unsigned int)(timingsec*GAMEFPS);
	frameoffset = (unsigned int)(offsetsec*GAMEFPS);
	alpha = MaxAlpha/frametiming * (tframecnt - frameoffset);

	if( alpha < 0.0f ){ alpha = 0.0f; }
	if( alpha > 1.0f ){ alpha = 1.0f; }

	if( reversal == true ){
		return 1.0f - alpha;
	}
	return alpha;
}

//! @brief フレームカウントを元にアルファ値・透明度を設定（ループ）
//! @param tframecnt 対象とするカウント値
//! @param MaxAlpha 最大アルファ値
//! @param timingsec 間隔・秒数
//! @param reversal 反転フラグ（false：フィールドイン　true：フィールドアウト）
//! @return アルファ値
float scene::GetEffectAlphaLoop(unsigned int tframecnt, float MaxAlpha, float timingsec, bool reversal)
{
	float alpha;
	unsigned int frametiming;

	frametiming = (unsigned int)(timingsec*GAMEFPS);
	alpha = MaxAlpha/frametiming * (tframecnt%frametiming);

	if( reversal == true ){
		return 1.0f - alpha;
	}
	return alpha;
}

//! @brief コンストラクタ
D2Dscene::D2Dscene()
{
	gametitle = -1;
}

//! @brief ディストラクタ
D2Dscene::~D2Dscene()
{}

//! @brief 2Dシーン初期化
int D2Dscene::Create()
{
	gametitle = d3dg->LoadTexture("data\\title.dds", false, false);
	GameState->NextState();
	if( gametitle == -1 ){ return 1; }
	return 0;
}

//! @brief シーン描画回復
int D2Dscene::Recovery()
{
	//初期化
	gametitle = d3dg->LoadTexture("data\\title.dds", false, false);
	if( gametitle == -1 ){ return 1; }
	return 0;
}

//! @brief 2Dシーン描画処理（2D）
void D2Dscene::Render2D()
{
	d3dg->Draw2DTextureFontText(10, 10, "hello world !", d3dg->GetColorCode(1.0f,1.0f,1.0f,1.0f), 20, 32);
}

//! @brief 2Dシーン描画処理（メイン）
//! @return 成功：false　失敗：true
//! @attention 失敗した場合、D3DGraphicsクラスのResetD3D()関数を呼び出してください。
bool D2Dscene::RenderMain()
{
	if( d3dg->StartRender() ){ return true; }

	d3dg->Draw2DTexture(0, 0, gametitle, GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), 0.4f);
	d3dg->Draw2DBox(11, 11, GameConfig.GetScreenWidth() - 10, GameConfig.GetScreenHeight() - 10, d3dg->GetColorCode(0.0f,0.0f,0.0f,0.75f));

	Render2D();

	d3dg->ScreenBrightness(GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), GameConfig.GetBrightness());

	return d3dg->EndRender();
}

//! @brief 2Dシーン解放
void D2Dscene::Destroy()
{
	d3dg->CleanupTexture(gametitle);
	GameState->NextState();
}


//! @brief コンストラクタ
D3Dscene::D3Dscene()
{
	GameSound = NULL;
	camera_x = 0.0f;
	camera_y = 0.0f;
	camera_z = 0.0f;
	camera_rx = 0.0f;
	camera_ry = 0.0f;
}

//! @brief ディストラクタ
D3Dscene::~D3Dscene()
{}

//! @brief 3Dシーンクラスを設定
void D3Dscene::SetClass(StateMachine *in_GameState, WindowControl *in_WindowCtrl, D3DGraphics *in_d3dg, InputControl *in_inputCtrl, SoundManager *in_GameSound)
{
	GameState = in_GameState;
	WindowCtrl = in_WindowCtrl; 
	d3dg = in_d3dg;
	inputCtrl = in_inputCtrl;
	GameSound = in_GameSound;
}

//! @brief 3Dシーン主計算処理
void D3Dscene::Process()
{}

//! @brief 3Dシーンサウンド再生処理
void D3Dscene::Sound()
{
	//サウンドを再生
	GameSound->PlayWorldSound(camera_x, camera_y, camera_z, camera_rx, 1);
}

//! @brief 3Dシーン描画処理（3D）
void D3Dscene::Render3D()
{}

//! @brief 3Dシーン描画処理（2D）
void D3Dscene::Render2D()
{}

//! @brief 3Dシーン描画処理（メイン）
//! @return 成功：false　失敗：true
//! @attention 失敗した場合、D3DGraphicsクラスのResetD3D()関数を呼び出してください。
bool D3Dscene::RenderMain()
{
	if( d3dg->StartRender() ){ return true; }

	Render3D();
	Render2D();

	d3dg->ScreenBrightness(GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), GameConfig.GetBrightness());

	return d3dg->EndRender();
}