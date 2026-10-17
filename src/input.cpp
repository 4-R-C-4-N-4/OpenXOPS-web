//! @file input.cpp
//! @brief InputControlクラスの定義

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

#include "input.h"

#if INPUT_INTERFACE <= 2	//[OpenXOPS-web] Windows input backends only

//! @brief コンストラクタ
InputControl::InputControl()
{
	hWnd = NULL;
#if INPUT_INTERFACE == 2
	pDI = NULL;
	pDIDevice = NULL;
	pMouse = NULL;
#else
	InitFlag = false;
#endif
	MouseLimitFlag = true;

	//キーボード設定値初期化
	for(int i=0; i<256; i++){
		keys[i] = 0;
	}
	memcpy(keys_lt, keys, sizeof(char)*256);

	//マウスの設定値初期化
	mx = 0;
	my = 0;
#if INPUT_INTERFACE == 0
	point_lt.x = 0;
	point_lt.y = 0;
#endif
	mbl = false;
	mbr = false;
	mbl_lt = mbl;
	mbr_lt = mbr;

#if INPUT_INTERFACE == 1
	memcpy(keys_proc, keys, sizeof(char)*256);
	mx_proc = 0;
	my_proc = 0;
	mbl_proc = mbl;
	mbr_proc = mbr;
#endif
}

//! @brief ディストラクタ
InputControl::~InputControl()
{
	DestroyInput();
}

//! @brief 初期化
//! @param WindowCtrl WindowControlクラスのポインタ
//! @return 成功：0　失敗：1
int InputControl::InitInput(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
 #if INPUT_INTERFACE == 0
	OutputLog.WriteLog(LOG_INIT, "Input", "WindowsInput");
 #elif INPUT_INTERFACE == 1
	OutputLog.WriteLog(LOG_INIT, "Input", "RawInput");
 #elif INPUT_INTERFACE == 2
	OutputLog.WriteLog(LOG_INIT, "Input", "DirectInput");
 #endif
#endif

	//ウィンドウハンドルを設定
	hWnd = WindowCtrl->GethWnd();

#if INPUT_INTERFACE == 1
	RAWINPUTDEVICE Rid[2];

	//マウス
	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = 0x02; 
	Rid[0].dwFlags = NULL;//RIDEV_NOLEGACY;
	Rid[0].hwndTarget = 0;

	//キーボード
	Rid[1].usUsagePage = 0x01; 
	Rid[1].usUsage = 0x06; 
	Rid[1].dwFlags = NULL;//RIDEV_NOLEGACY;
	Rid[1].hwndTarget = 0;

	//WM_INPUTで取得するデバイスを登録
	if( RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE ){
		return 1;
	}

	//プロシージャ登録
	SetWindowSubclass(hWnd, SubClassProc, 1, (DWORD_PTR)this);
#elif INPUT_INTERFACE == 2
	//DirectInput初期化
	if( FAILED( DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&pDI, NULL) ) ){
		return 1;
	}

	//キーボード初期化
	if( FAILED( pDI->CreateDevice(GUID_SysKeyboard, &pDIDevice, NULL) ) ){
		return 1;
	}
	pDIDevice->SetDataFormat(&c_dfDIKeyboard);
	pDIDevice->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

	//マウス初期化
	if( FAILED( pDI->CreateDevice(GUID_SysMouse, &pMouse, NULL) ) ){
		return 1;
	}
	pMouse->SetDataFormat(&c_dfDIMouse2);
	pMouse->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	pMouse->Acquire();
#endif

	//カーソルを非表示
	ShowCursor(false);

#if INPUT_INTERFACE != 2
	InitFlag = true;
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	return 0;
}

//! @brief 解放
//! @attention 本関数は自動的に呼び出されますが、明示的に呼び出すことも可能です。
void InputControl::DestroyInput()
{
#if INPUT_INTERFACE != 2
	if( InitFlag == false ){ return; }
#else
	if( (pDI == NULL)&&(pDIDevice == NULL)&&(pMouse == NULL) ){ return; }
#endif

#if INPUT_INTERFACE == 0
	InitFlag = false;
#elif INPUT_INTERFACE == 1
	InitFlag = false;

	//プロシージャ解放
	RemoveWindowSubclass(hWnd, SubClassProc, 1);
#elif INPUT_INTERFACE == 2
	//キーボードデバイスを解放
	if( pDIDevice != NULL ){
		pDIDevice->Unacquire();
		pDIDevice->Release();
		pDIDevice = NULL;
	}

	//マウスデバイスを解放
	if( pMouse != NULL ){
		pMouse->Unacquire();
		pMouse->Release();
		pMouse = NULL;
	}

	//DirectInputを解放
	if( pDI != NULL){
		pDI->Release();
		pDI = NULL;
	}
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
 #if INPUT_INTERFACE == 0
	OutputLog.WriteLog(LOG_CLEANUP, "Input", "WindowsInput");
 #elif INPUT_INTERFACE == 1
	OutputLog.WriteLog(LOG_CLEANUP, "Input", "RawInput");
 #elif INPUT_INTERFACE == 2
	OutputLog.WriteLog(LOG_CLEANUP, "Input", "DirectInput");
 #endif
#endif
}

#if INPUT_INTERFACE == 1
//! @brief RowInputを取得するプロシージャ
LRESULT CALLBACK InputControl::SubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	//未使用引数対策
	UNREFERENCED_PARAMETER(uIdSubclass);

	if( uMsg == WM_INPUT ){
		InputControl* input = (InputControl*)dwRefData;
		input->GetRowInput(wParam, lParam);
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//! @brief プロシージャからRowInput取得
void InputControl::GetRowInput(WPARAM wParam, LPARAM lParam)
{
	//未使用引数対策
	UNREFERENCED_PARAMETER(wParam);

	UINT dwSize;

	//RowInputサイズ取得
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
	LPBYTE lpb = new BYTE[dwSize];
	if( lpb == NULL ){ return; }

	//RowInput実データ取得
	if( GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize ){
		return;
	}

	RAWINPUT* raw = (RAWINPUT*)lpb;

	//キーボード
	if( raw->header.dwType == RIM_TYPEKEYBOARD ){
		if( (raw->data.keyboard.Flags&0x01) == RI_KEY_MAKE ){		//キーが押された
			int Vkey = raw->data.keyboard.VKey;
			keys_proc[Vkey] = (char)0x80;
		}
		if( (raw->data.keyboard.Flags&0x01) == RI_KEY_BREAK ){		//キーが離された
			int Vkey = raw->data.keyboard.VKey;
			keys_proc[Vkey] = (char)0x00;
		}
	}

	//マウス
	if( raw->header.dwType == RIM_TYPEMOUSE ){
		if( (raw->data.mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_DOWN) != 0 ){	//左ボタンが押された
			mbl_proc = true;
		}
		if( (raw->data.mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_UP) != 0 ){		//左ボタンが離された
			mbl_proc = false;
		}
		if( (raw->data.mouse.usButtonFlags&RI_MOUSE_RIGHT_BUTTON_DOWN) != 0 ){	//右ボタンが押された
			mbr_proc = true;
		}
		if( (raw->data.mouse.usButtonFlags&RI_MOUSE_RIGHT_BUTTON_UP) != 0 ){	//右ボタンが離された
			mbr_proc = false;
		}

		//マウス座標加算
		mx_proc += (int)raw->data.mouse.lLastX;
		my_proc += (int)raw->data.mouse.lLastY;
	}

	delete[] lpb;
}
#endif

//! @brief マウス移動量を制限するフラグ設定
//! @param flag 有効：true　無効：false
//! @attention マウス入力量がchar型 8bit相当になります。
void InputControl::SetMouseLimitFlag(bool flag)
{
	MouseLimitFlag = flag;
}

//! @brief 入力デバイスの状態を更新
//! @param mousemode マウスの座標取得　絶対値（座標）：false　相対値（移動量）：true
void InputControl::GetInputState(bool mousemode)
{
#if INPUT_INTERFACE == 0
	//取得直前のキー情報を、前フレーム情報として記録
	memcpy(keys_lt, keys, sizeof(char)*256);

	//現在のキーボード入力を取得
	if( GetKeyboardState((PBYTE)&keys) == 0 ){ return; }
#elif INPUT_INTERFACE == 1
	//取得直前のキー情報を、前フレーム情報として記録
	memcpy(keys_lt, keys, sizeof(char)*256);

	//現在のキーボード入力を取得
	memcpy(keys, keys_proc, sizeof(char)*256);
#elif INPUT_INTERFACE == 2
	//キーボードデバイスが正しく使用できれば
	if( pDIDevice != NULL ){
		HRESULT hr = pDIDevice->Acquire();
		if( (hr==DI_OK) || (hr==S_FALSE) ){
			//取得直前のキー情報を、前フレーム情報として記録
			memcpy(keys_lt, keys, sizeof(char)*256);

			//現在のキーボード入力を取得
			pDIDevice->GetDeviceState(sizeof(keys), &keys);
		}
	}
#endif

	POINT point;

	//位置をスクリーン座標で取得（WinAPI）
	GetCursorPos(&point);
	ScreenToClient(hWnd, &point);

#if INPUT_INTERFACE == 0
	//マウス座標を出力
	if( mousemode == false ){
		mx = point.x;
		my = point.y;
	}
	else{
		mx = point.x - point_lt.x;
		my = point.y - point_lt.y;
	}

	//マウス座標を前フレーム情報として記録
	point_lt = point;


	//取得直前のボタン情報を、前フレーム情報として記録
	mbl_lt = mbl;
	mbr_lt = mbr;

	//マウスのボタンを取得
	if( GetKeyState(VK_LBUTTON) < 0 ){ mbl = true; }
	else{ mbl = false; }
	if( GetKeyState(VK_RBUTTON) < 0 ){ mbr = true; }
	else{ mbr = false; }
#elif INPUT_INTERFACE == 1
	//マウス座標を出力
	if( mousemode == false ){
		mx = point.x;
		my = point.y;
	}
	else{
		mx = mx_proc;
		my = my_proc;
	}

	mx_proc = 0;
	my_proc = 0;


	//取得直前のボタン情報を、前フレーム情報として記録
	mbl_lt = mbl;
	mbr_lt = mbr;

	//マウスのボタンを取得
	mbl = mbl_proc;
	mbr = mbr_proc;
#elif INPUT_INTERFACE == 2
	//マウスデバイスが正しく初期化されていれば
	if( pMouse != NULL ){
		//マウス情報を取得（DirectInput）
		DIMOUSESTATE2 dIMouseState={0};
		if( FAILED(pMouse->GetDeviceState( sizeof(DIMOUSESTATE2), &dIMouseState ) ) ){
			pMouse->Acquire();
		}

		//マウス座標を出力
		if( mousemode == false ){
			mx = point.x;
			my = point.y;
		}
		else{
			mx = dIMouseState.lX;
			my = dIMouseState.lY;
		}

		//取得直前のボタン情報を、前フレーム情報として記録
		mbl_lt = mbl;
		mbr_lt = mbr;

		//マウスのボタンを取得
		if( dIMouseState.rgbButtons[0]&0x80 ){ mbl = true; }
		else{ mbl = false; }
		if( dIMouseState.rgbButtons[1]&0x80 ){ mbr = true; }
		else{ mbr = false; }
	}
#endif

	if( MouseLimitFlag == true ){
		//マウスが相対値（移動量）取得モードならば、char型 8bit相当にする。
		if( mousemode == true ){
			if( mx > 127 ) { mx = 127; }
			if( mx < -128 ){ mx = -128; }
			if( my > 127 ) { my = 127; }
			if( my < -128 ){ my = -128; }
		}
	}
}

//! @brief マウスを中心に移動
void InputControl::MoveMouseCenter()
{
	POINT point;

	//ウィンドウ座標の中央を求める
	point.x = GameConfig.GetScreenWidth()/2;
	point.y = GameConfig.GetScreenHeight()/2;

#if INPUT_INTERFACE == 0
	//前回の座標を書き換え
	point_lt = point;
#endif

	//スクリーン座標に変換し、座標変更
	ClientToScreen(hWnd, &point);
	SetCursorPos(point.x, point.y);
}

//[OpenXOPS-web] CheckKeyNow() ... GetFunctionKeycode() are platform-neutral and were
//moved verbatim to platform/input-common.cpp, which every target compiles.

#endif
