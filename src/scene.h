//! @file scene.h
//! @brief 各画面を管理するクラスの宣言

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

#ifndef SCENE_H
#define SCENE_H

//#include "main.h"
//	ヘッダーファイル中の下で宣言する。

//! @brief 画面管理クラス（基底クラス）
//! @details ゲームの画面を管理する最も基底のクラスです。
class scene
{
protected:
	class StateMachine *GameState;		//!< ゲーム全体の状態遷移クラス
	class WindowControl *WindowCtrl;	//!< ウィンドウ制御クラス
	class D3DGraphics *d3dg;			//!< 描画クラス
	class InputControl *inputCtrl;		//!< 入力取得クラス
	unsigned int framecnt;				//!< フレーム数のカウント

public:
	scene();
	~scene();
	virtual void SetClass(StateMachine *in_GameState, WindowControl *in_WindowCtrl, D3DGraphics *in_d3dg, InputControl *in_inputCtrl);
	virtual int Create();
	virtual int Recovery();
	virtual void Input();
	virtual void Process();
	virtual bool RenderMain();
	virtual void Destroy();
	float GetEffectAlpha(unsigned int tframecnt, float MaxAlpha, float timingsec, float offsetsec, bool reversal);
	float GetEffectAlphaLoop(unsigned int tframecnt, float MaxAlpha, float timingsec, bool reversal);
};

//! @brief 2D画面管理クラス（基底クラス）
//! @details ゲーム画面の中で、2D描画のみを行うシーンを管理する基底のクラスです。
class D2Dscene : public scene
{
protected:
	int gametitle;				//!< ゲームタイトル画像
	virtual void Render2D();

public:
	D2Dscene();
	~D2Dscene();
	virtual int Create();
	virtual int Recovery();
	virtual bool RenderMain();
	virtual void Destroy();
};

//! @brief 3D画面管理クラス（基底クラス）
//! @details ゲーム画面の中で、3D描画を行うシーンを管理する基底のクラスです。
class D3Dscene : public scene
{
protected:
	class SoundManager *GameSound;		//!< ゲーム効果音管理クラス
	float camera_x;			//!< カメラ座標
	float camera_y;			//!< カメラ座標
	float camera_z;			//!< カメラ座標
	float camera_rx;		//!< カメラ回転角度
	float camera_ry;		//!< カメラ回転角度
	virtual void Render3D();
	virtual void Render2D();

public:
	D3Dscene();
	~D3Dscene();
	virtual void SetClass(StateMachine *in_GameState, WindowControl *in_WindowCtrl, D3DGraphics *in_d3dg, InputControl *in_inputCtrl, SoundManager *in_GameSound);
	virtual void Process();
	virtual void Sound();
	virtual bool RenderMain();
};

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 3		//!< Select include file.
#endif
#include "main.h"

#endif