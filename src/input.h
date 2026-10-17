//! @file input.h
//! @brief InputControlクラスの宣言

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

#ifndef INPUT_H
#define INPUT_H

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 1		//!< Select include file.
#endif
#include "main.h"

#include <windows.h>

#define INPUT_INTERFACE 0	//!< @brief 入力を取得するインターフェースの選択 @details WindowsInput：0　RawInput：1　DirectInput：2

#if INPUT_INTERFACE == 0
 #define INPUT_CORE "WindowsInput"					//!< バージョン表示用情報
#elif INPUT_INTERFACE == 1
 #include <commctrl.h>
 #pragma comment (lib, "comctl32.lib")

 #define INPUT_CORE "RawInput"						//!< バージョン表示用情報
#elif INPUT_INTERFACE == 2
 #define DIRECTINPUT_VERSION 0x0800	//!< 警告「DIRECTINPUT_VERSION undefined. Defaulting to version 0x0800」を防止
 #include <dinput.h>

 #pragma comment(lib, "dinput8.lib")
 #pragma comment(lib, "dxguid.lib")

 #define INPUT_CORE "DirectInput"					//!< バージョン表示用情報
#endif

//! @brief 入力デバイスを管理するクラス
//! @details マウスやキーボードなどの入力デバイスを管理しています。
class InputControl
{
	HWND hWnd;				//!< ウィンドウハンドル
#if INPUT_INTERFACE == 2
	LPDIRECTINPUT8 pDI;					//!< DIRECTINPUT8のポインタ
	LPDIRECTINPUTDEVICE8 pDIDevice;		//!< DIRECTINPUTDEVICE8のポインタ・キーボード用
	LPDIRECTINPUTDEVICE8 pMouse;		//!< DIRECTINPUTDEVICE8のポインタ・マウス用
#else
	bool InitFlag;			//!< 初期化フラグ
#endif
	bool MouseLimitFlag;	//!< マウス移動量を制限するフラグ
	char keys[256];			//!< キー情報を格納する配列
	char keys_lt[256];		//!< （前回の）キー情報を格納する配列
	int mx;					//!< マウスのX座標
	int my;					//!< マウスのY座標
#if INPUT_INTERFACE == 0
	POINT point_lt;			//!< （前回の）マウス座標
#endif
	bool mbl;				//!< マウスの左ボタン
	bool mbr;				//!< マウスの右ボタン
	bool mbl_lt;			//!< （前回の）マウスの左ボタン
	bool mbr_lt;			//!< （前回の）マウスの右ボタン
#if INPUT_INTERFACE == 1
	char keys_proc[256];	//!< キー情報を格納する配列（プロシージャ）
	int mx_proc;			//!< マウスのX座標（プロシージャ）
	int my_proc;			//!< マウスのY座標（プロシージャ）
	bool mbl_proc;			//!< マウスの左ボタン（プロシージャ）
	bool mbr_proc;			//!< マウスの右ボタン（プロシージャ）

	static LRESULT CALLBACK SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	void GetRowInput(WPARAM wParam, LPARAM lParam);
#endif

public:
	InputControl();
	~InputControl();
	int InitInput(WindowControl *WindowCtrl);
	void DestroyInput();
	void SetMouseLimitFlag(bool flag);
	void GetInputState(bool mousemode);
	void MoveMouseCenter();
	bool CheckKeyNow(int id);
	bool CheckKeyDown(int id);
	bool CheckKeyUp(int id);
	void GetMouseMovement(int *x, int *y);
	bool CheckMouseButtonNowL();
	bool CheckMouseButtonDownL();
	bool CheckMouseButtonUpL();
	bool CheckMouseButtonNowR();
	bool CheckMouseButtonDownR();
	bool CheckMouseButtonUpR();
	int CheckInputAll();
};

int OriginalkeycodeToDinputdef(int code);
bool GetDoubleKeyCode(int id, int *CodeL, int *CodeR);
int GetEscKeycode();
int GetHomeKeycode();
int GetFunctionKeycode(int key);

#endif