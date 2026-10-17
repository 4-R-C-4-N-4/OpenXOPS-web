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

//! @brief キーボードの入力をチェック（リアルタイム）
//! @return 押されてない：false　押されている：true
bool InputControl::CheckKeyNow(int id)
{
	if( (id < 0)||(256 <= id) ){ return false; }

	//現在押されていれば
	if( keys[id]&0x80 ){ return true; }
	return false;
}

//! @brief キーボードの入力をチェック（押された瞬間）
//! @return 押された瞬間でない：false　押された瞬間である：true
bool InputControl::CheckKeyDown(int id)
{
	if( (id < 0)||(256 <= id) ){ return false; }

	//前回は押されておらず、現在押されていれば
	if( ((keys_lt[id]&0x80) == 0)&&(keys[id]&0x80) ){ return true; }
	return false;
}

//! @brief キーボードの入力をチェック（離された瞬間）
//! @return 離された瞬間でない：false　離された瞬間である：true
bool InputControl::CheckKeyUp(int id)
{
	if( (id < 0)||(256 <= id) ){ return false; }

	//前回を押されており、現在押されていなければ
	if( (keys_lt[id]&0x80)&&((keys[id]&0x80) == 0) ){ return true; }
	return false;
}

//! @brief マウスの入力をチェック
//! @param x x軸を受け取る整数値型ポインタ
//! @param y y軸を受け取る整数値型ポインタ
//! @attention 値は直前に実行した GetInputState() への引数に影響される。
void InputControl::GetMouseMovement(int *x, int *y)
{
	if( (x == NULL)||(y == NULL) ){ return; }

	//マウス座標を代入
	*x = mx;
	*y = my;
}

//! @brief マウス・左ボタンの入力をチェック（リアルタイム）
//! @return 押されてない：false　押されている：true
bool InputControl::CheckMouseButtonNowL()
{
	//現在の情報を返す
	return mbl;
}

//! @brief マウス・左ボタンの入力をチェック（押された瞬間）
//! @return 押された瞬間でない：false　押された瞬間である：true
bool InputControl::CheckMouseButtonDownL()
{
	//前回は押されておらず、現在押されていれば
	if( (mbl_lt == false)&&(mbl == true) ){ return true; }
	return false;
}

//! @brief マウス・左ボタンの入力をチェック（離された瞬間）
//! @return 離された瞬間でない：false　離された瞬間である：true
bool InputControl::CheckMouseButtonUpL()
{
	//前回を押されており、現在押されていなければ
	if( (mbl_lt == true)&&(mbl == false) ){ return true; }
	return false;
}

//! @brief マウス・右ボタンの入力をチェック（リアルタイム）
//! @return 押されてない：false　押されている：true
bool InputControl::CheckMouseButtonNowR()
{
	//現在の情報を返す
	return mbr;
}

//! @brief マウス・右ボタンの入力をチェック（押された瞬間）
//! @return 押された瞬間でない：false　押された瞬間である：true
bool InputControl::CheckMouseButtonDownR()
{
	//前回は押されておらず、現在押されていれば
	if( (mbr_lt == false)&&(mbr == true) ){ return true; }
	return false;
}

//! @brief マウス・右ボタンの入力をチェック（離された瞬間）
//! @return 離された瞬間でない：false　離された瞬間である：true
bool InputControl::CheckMouseButtonUpR()
{
	//前回を押されており、現在押されていなければ
	if( (mbr_lt == true)&&(mbr == false) ){ return true; }
	return false;
}

//! @brief オリジナルキーコードの入力チェック
//! @return 押されたオリジナルキーコード
//! @attention オリジナルキーコードに存在しないキーは判定しない。
int InputControl::CheckInputAll()
{
	for(int i=0; i<=0x4A; i++){
		int KeyCode = OriginalkeycodeToDinputdef(i);

		if( KeyCode == -1 ){
			if( CheckMouseButtonUpL() == true ){
				return i;
			}
		}
		if( KeyCode == -2 ){
			if( CheckMouseButtonUpR() == true ){
				return i;
			}
		}
		if( KeyCode == -3 ){
			int CodeL, CodeR;
			GetDoubleKeyCode(0, &CodeL, &CodeR);
			if( CheckKeyUp(CodeL) == true ){
				return i;
			}
			if( CheckKeyUp(CodeR) == true ){
				return i;
			}
		}
		if( KeyCode == -4 ){
			int CodeL, CodeR;
			GetDoubleKeyCode(1, &CodeL, &CodeR);
			if( CheckKeyUp(CodeL) == true ){
				return i;
			}
			if( CheckKeyUp(CodeR) == true ){
				return i;
			}
		}

		if( CheckKeyUp(KeyCode) == true ){
			return i;
		}
	}

	return -1;
}

//! @brief オリジナルキーコードから取得用キーコードへ変換
//! @param code オリジナルキーコード
//! @return 1以上：取得用キーコード　-1以下：特殊　0：失敗
//! @attention 以下、特殊な戻り値の場合―<br>-1：MOUSE L　　-2：MOUSE R　　-3：DIK_LSHIFT / DIK_RSHIFT　　-4：DIK_LCONTROL / DIK_RCONTROL
int OriginalkeycodeToDinputdef(int code)
{
	int out = 0;

#if INPUT_INTERFACE != 2
	switch(code){
		case 0x00: out = VK_UP; break;
		case 0x01: out = VK_DOWN; break;
		case 0x02: out = VK_LEFT; break;
		case 0x03: out = VK_RIGHT; break;
		case 0x04: out = VK_NUMPAD0; break;
		case 0x05: out = VK_NUMPAD1; break;
		case 0x06: out = VK_NUMPAD2; break;
		case 0x07: out = VK_NUMPAD3; break;
		case 0x08: out = VK_NUMPAD4; break;
		case 0x09: out = VK_NUMPAD5; break;
		case 0x0A: out = VK_NUMPAD6; break;
		case 0x0B: out = VK_NUMPAD7; break;
		case 0x0C: out = VK_NUMPAD8; break;
		case 0x0D: out = VK_NUMPAD9; break;
		case 0x0E: out = VK_BACK; break;
		case 0x0F: out = VK_RETURN; break;

		case 0x10: out = VK_TAB; break;
		case 0x11: out = VK_SPACE; break;
		case 0x12: out = -1; break;				//MOUSE L
		case 0x13: out = -2; break;				//MOUSE R
		case 0x14: out = VK_SHIFT; break;
		case 0x15: out = VK_CONTROL; break;
		case 0x16: out = '0'; break;
		case 0x17: out = '1'; break;
		case 0x18: out = '2'; break;
		case 0x19: out = '3'; break;
		case 0x1A: out = '4'; break;
		case 0x1B: out = '5'; break;
		case 0x1C: out = '6'; break;
		case 0x1D: out = '7'; break;
		case 0x1E: out = '8'; break;
		case 0x1F: out = '9'; break;

		case 0x20: out = 'A'; break;
		case 0x21: out = 'B'; break;
		case 0x22: out = 'C'; break;
		case 0x23: out = 'D'; break;
		case 0x24: out = 'E'; break;
		case 0x25: out = 'F'; break;
		case 0x26: out = 'G'; break;
		case 0x27: out = 'H'; break;
		case 0x28: out = 'I'; break;
		case 0x29: out = 'J'; break;
		case 0x2A: out = 'K'; break;
		case 0x2B: out = 'L'; break;
		case 0x2C: out = 'M'; break;
		case 0x2D: out = 'N'; break;
		case 0x2E: out = 'O'; break;
		case 0x2F: out = 'P'; break;

		case 0x30: out = 'Q'; break;
		case 0x31: out = 'R'; break;
		case 0x32: out = 'S'; break;
		case 0x33: out = 'T'; break;
		case 0x34: out = 'U'; break;
		case 0x35: out = 'V'; break;
		case 0x36: out = 'W'; break;
		case 0x37: out = 'X'; break;
		case 0x38: out = 'Y'; break;
		case 0x39: out = 'Z'; break;
		case 0x3A: out = VK_OEM_2; break;
		case 0x3B: out = VK_OEM_1; break;
		case 0x3C: out = VK_OEM_PLUS; break;
		case 0x3D: out = VK_OEM_MINUS; break;
		case 0x3E: out = VK_OEM_3; break;
		case 0x3F: out = VK_OEM_4; break;

		case 0x40: out = VK_OEM_6; break;
		case 0x41: out = VK_OEM_102; break;
		case 0x42: out = VK_OEM_5; break;
		case 0x43: out = VK_OEM_COMMA; break;
		case 0x44: out = VK_OEM_PERIOD; break;
		case 0x45: out = VK_OEM_7; break;
		case 0x46: out = VK_MULTIPLY ; break;
		case 0x47: out = VK_DIVIDE; break;
		case 0x48: out = VK_ADD; break;
		case 0x49: out = VK_SUBTRACT; break;
		case 0x4A: out = VK_DECIMAL; break;

		default : out = 0; 
	}
#else
	switch(code){
		case 0x00: out = DIK_UP; break;
		case 0x01: out = DIK_DOWN; break;
		case 0x02: out = DIK_LEFT; break;
		case 0x03: out = DIK_RIGHT; break;
		case 0x04: out = DIK_NUMPAD0; break;
		case 0x05: out = DIK_NUMPAD1; break;
		case 0x06: out = DIK_NUMPAD2; break;
		case 0x07: out = DIK_NUMPAD3; break;
		case 0x08: out = DIK_NUMPAD4; break;
		case 0x09: out = DIK_NUMPAD5; break;
		case 0x0A: out = DIK_NUMPAD6; break;
		case 0x0B: out = DIK_NUMPAD7; break;
		case 0x0C: out = DIK_NUMPAD8; break;
		case 0x0D: out = DIK_NUMPAD9; break;
		case 0x0E: out = DIK_BACK; break;
		case 0x0F: out = DIK_RETURN; break;

		case 0x10: out = DIK_TAB; break;
		case 0x11: out = DIK_SPACE; break;
		case 0x12: out = -1; break;				//MOUSE L
		case 0x13: out = -2; break;				//MOUSE R
		case 0x14: out = -3; break;				//DIK_LSHIFT / DIK_RSHIFT
		case 0x15: out = -4; break;				//DIK_LCONTROL / DIK_RCONTROL
		case 0x16: out = DIK_0; break;
		case 0x17: out = DIK_1; break;
		case 0x18: out = DIK_2; break;
		case 0x19: out = DIK_3; break;
		case 0x1A: out = DIK_4; break;
		case 0x1B: out = DIK_5; break;
		case 0x1C: out = DIK_6; break;
		case 0x1D: out = DIK_7; break;
		case 0x1E: out = DIK_8; break;
		case 0x1F: out = DIK_9; break;

		case 0x20: out = DIK_A; break;
		case 0x21: out = DIK_B; break;
		case 0x22: out = DIK_C; break;
		case 0x23: out = DIK_D; break;
		case 0x24: out = DIK_E; break;
		case 0x25: out = DIK_F; break;
		case 0x26: out = DIK_G; break;
		case 0x27: out = DIK_H; break;
		case 0x28: out = DIK_I; break;
		case 0x29: out = DIK_J; break;
		case 0x2A: out = DIK_K; break;
		case 0x2B: out = DIK_L; break;
		case 0x2C: out = DIK_M; break;
		case 0x2D: out = DIK_N; break;
		case 0x2E: out = DIK_O; break;
		case 0x2F: out = DIK_P; break;

		case 0x30: out = DIK_Q; break;
		case 0x31: out = DIK_R; break;
		case 0x32: out = DIK_S; break;
		case 0x33: out = DIK_T; break;
		case 0x34: out = DIK_U; break;
		case 0x35: out = DIK_V; break;
		case 0x36: out = DIK_W; break;
		case 0x37: out = DIK_X; break;
		case 0x38: out = DIK_Y; break;
		case 0x39: out = DIK_Z; break;
		case 0x3A: out = DIK_SLASH; break;
		case 0x3B: out = DIK_COLON; break;
		case 0x3C: out = DIK_SEMICOLON; break;
		case 0x3D: out = DIK_MINUS; break;
		case 0x3E: out = DIK_AT; break;
		case 0x3F: out = DIK_LBRACKET; break;

		case 0x40: out = DIK_RBRACKET; break;
		case 0x41: out = DIK_BACKSLASH; break;
		case 0x42: out = DIK_YEN; break;
		case 0x43: out = DIK_COMMA; break;
		case 0x44: out = DIK_PERIOD; break;
		case 0x45: out = DIK_EQUALS; break;
		case 0x46: out = DIK_NUMPADSTAR; break;
		case 0x47: out = DIK_NUMPADSLASH; break;
		case 0x48: out = DIK_NUMPADPLUS; break;
		case 0x49: out = DIK_NUMPADMINUS; break;
		case 0x4A: out = DIK_NUMPADPERIOD; break;

		default : out = 0; 
	}
#endif

	return out;
}

//! @brief 左右キーのキーコード取得
//! @param id Shiftキー：0　Ctrlキー：1
//! @param *CodeL 左側キーのキーコードを受け取るポインタ
//! @param *CodeR 右側キーのキーコードを受け取るポインタ
//! @return 成功：true　失敗：false　
bool GetDoubleKeyCode(int id, int *CodeL, int *CodeR)
{
	if( (CodeL == NULL)||(CodeR == NULL) ){ return false; }

#if INPUT_INTERFACE != 2
	//未使用引数対策
	UNREFERENCED_PARAMETER(id);
#else
	if( id == 0 ){
		*CodeL = DIK_LSHIFT;
		*CodeR = DIK_RSHIFT;
		return true;
	}
	if( id == 1 ){
		*CodeL = DIK_LCONTROL;
		*CodeR = DIK_RCONTROL;
		return true;
	}
#endif

	//エラー
	*CodeL = 0x00;
	*CodeR = 0x00;
	return false;
}

//! @brief Escキーのキーコード取得
//! @return キーコード
int GetEscKeycode()
{
#if INPUT_INTERFACE != 2
	return VK_ESCAPE;
#else
	return DIK_ESCAPE;
#endif
}

//! @brief Homeキーのキーコード取得
//! @return キーコード
int GetHomeKeycode()
{
#if INPUT_INTERFACE != 2
	return VK_HOME;
#else
	return DIK_HOME;
#endif
}

//! @brief ファンクションキー（F1～F12）のキーコードを取得
//! @param key 番号（1～12）
//! @return キーコード
int GetFunctionKeycode(int key)
{
	int out = 0;

#if INPUT_INTERFACE != 2
	switch(key){
		case 1: out = VK_F1; break;
		case 2: out = VK_F2; break;
		case 3: out = VK_F3; break;
		case 4: out = VK_F4; break;
		case 5: out = VK_F5; break;
		case 6: out = VK_F6; break;
		case 7: out = VK_F7; break;
		case 8: out = VK_F8; break;
		case 9: out = VK_F9; break;
		case 10: out = VK_F10; break;
		case 11: out = VK_F11; break;
		case 12: out = VK_F12; break;

		default : out = 0; 
	}
#else
	switch(key){
		case 1: out = DIK_F1; break;
		case 2: out = DIK_F2; break;
		case 3: out = DIK_F3; break;
		case 4: out = DIK_F4; break;
		case 5: out = DIK_F5; break;
		case 6: out = DIK_F6; break;
		case 7: out = DIK_F7; break;
		case 8: out = DIK_F8; break;
		case 9: out = DIK_F9; break;
		case 10: out = DIK_F10; break;
		case 11: out = DIK_F11; break;
		case 12: out = DIK_F12; break;

		default : out = 0; 
	}
#endif

	return out;
}