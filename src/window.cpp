//! @file window.cpp
//! @brief メインウィンドウの作成・制御

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

#include "window.h"

#ifdef _WIN32	//[OpenXOPS-web] Win32 window backend only

//! @brief コンストラクタ
WindowControl::WindowControl()
{
	hInstance = 0;
	nCmdShow = 0;
	hWnd = NULL;

	timeBeginPeriod(1);		//timeEndPeriodと対
}

//! @brief ディストラクタ
WindowControl::~WindowControl()
{
	timeEndPeriod(1);		//timeBeginPeriodと対
}

//! @brief アプリケーションの情報を設定
//! @param in_hInstance インスタンス ハンドル
//! @param in_nCmdShow ウィンドウの表示状態
void WindowControl::SetParam(HINSTANCE in_hInstance, int in_nCmdShow)
{
	hInstance = in_hInstance;
	nCmdShow = in_nCmdShow;
}

//! @brief メインウィンドウ作成
//! @param title ウィンドウタイトル
//! @param width ウィンドウの幅
//! @param height ウィンドウの高さ
//! @param fullscreen false：ウィンドウ表示　true：フルスクリーン用表示
//! @return 成功：true　失敗：false
//! @warning 先にSetParam()関数で設定しておく必要があります。
bool WindowControl::InitWindow(const char* title, int width, int height, bool fullscreen)
{
	if( title == NULL ){ return false; }
	if( (width <= 0)||(height <= 0) ){ return false; }

	WNDCLASS wc;
	int x, y;
	RECT Rect;
	DWORD dwStyle;

	//ウィンドウクラスの登録
	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= WindowProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance	= hInstance;
	wc.hIcon		= LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(MAINICON));
	wc.hCursor		= NULL;
	wc.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName	= "MainWindow";
	if( RegisterClass(&wc) == 0 ){
		return false;
	}

	if( fullscreen == false ){
		dwStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;
	}
	else{
		dwStyle = WS_POPUP;
	}

	//ウィンドウサイズを計算
	Rect.left = 0;
	Rect.top = 0;
	Rect.right = width;
	Rect.bottom = height;
	AdjustWindowRect(&Rect, dwStyle, FALSE);
	width = Rect.right - Rect.left;
	height = Rect.bottom - Rect.top;

	//表示位置
	x = 0;
	y = 0;

	/*
	//ウィンドウモードなら、ウィンドウを画面の中心に配置する
	if( fullscreen == false ){
		SystemParametersInfo(SPI_GETWORKAREA, 0, &Rect, 0);
		x = (Rect.right - width)/2;
		y = (Rect.bottom - height)/2;
	}
	*/

	//ウィンドウ作成
	hWnd = CreateWindow( "MainWindow", title, dwStyle, x, y, width, height, NULL, NULL, hInstance, NULL );
	if( hWnd == NULL ){
		return false;
	}

	//表示
	ShowWindow( hWnd, nCmdShow );

	return true;
}

//! @brief メインウィンドウのウィンドウプロシージャ
LRESULT WINAPI WindowControl::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg ){
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

//! @brief ウィンドウの表示モード切り替え
//! @param fullscreen false：ウィンドウ表示　true：フルスクリーン用表示
//! @return 成功：true　失敗：false
bool WindowControl::ChangeWindowMode(bool fullscreen)
{
	if( hWnd == NULL ){ return false; }

	DWORD dwStyle;
	RECT Rect;
	int width, height;

	//ウィンドウサイズを取得
	GetClientRect(hWnd, &Rect);

	if( fullscreen == false ){
		ChangeDisplaySettings(NULL, 0);		//ディスプレイ解像度を戻す

		dwStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;
	}
	else{
		dwStyle = WS_POPUP;
	}

	//ウィンドウサイズを計算
	AdjustWindowRect(&Rect, dwStyle, FALSE);
	width = Rect.right - Rect.left;
	height = Rect.bottom - Rect.top;

	//反映
	SetWindowLong(hWnd, GWL_STYLE, dwStyle);
	SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	return true;
}

//! @brief ウィンドウハンドルを取得
//! @return ウィンドウハンドル
HWND WindowControl::GethWnd()
{
	return hWnd;
}

//! @brief ウィンドウメッセージを処理（メインループ判定）
//! @return アイドル状態：0　ゲームのメイン処理実行：1　ウィンドウが閉じた：-1
//! @attention 本関数から「1」が返された場合に限り、ゲームのメイン処理を実行してください。
//! @attention 本関数から「-1」が返された場合、各リソースやインターフェースを解放し、ゲームを終了してください。
int WindowControl::CheckMainLoop()
{
	MSG msg = {0};

	if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) ){
		//ウィンドウメッセージが来ている

		TranslateMessage( &msg );
		DispatchMessage( &msg );

		if( msg.message == WM_QUIT ){
			return -1;
		}
		return 0;
	}
	else if( GetActiveWindow() == hWnd ){
		//ゲームのフレーム処理
		return 1;
	}
	//else{
		//ウィンドウメッセージが来るまで待つ
		WaitMessage();
		return 0;
	//}
}

//! @brief エラーメッセージ表示
//! @param *str メッセージ
void WindowControl::ErrorInfo(const char *str)
{
	if( str == NULL ){ return; }

	MessageBox(hWnd, str, "error", MB_OK);
}

//! @brief ウィンドウを閉じるように要求
void WindowControl::CloseWindow()
{
	PostMessage(hWnd, WM_CLOSE, 0L, 0L);
}

#ifdef ENABLE_DEBUGLOG
//! @brief デバック用ログにユーザー環境を出力
//! @note 現時点ではOS情報と言語設定のみ
void GetOperatingEnvironment()
{
	char str[256];
	char OSname[128];
	char OSver[128];
	char OSbit[128];
	int strsize;
	HKEY hKey = NULL;

	//レジストリからOS情報取得
	strcpy(OSname, "");
	strcpy(OSver, "");
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_EXECUTE | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS ){
		strsize = sizeof(OSname);
		RegQueryValueEx(hKey, "ProductName", 0, NULL, (LPBYTE)OSname, (LPDWORD)&strsize);
		strsize = sizeof(OSver);
		RegQueryValueEx(hKey, "DisplayVersion", 0, NULL, (LPBYTE)OSver, (LPDWORD)&strsize);
		RegCloseKey(hKey);
	}

	//OSビット数取得
	SYSTEM_INFO SystemInfo = {0};
	GetNativeSystemInfo(&SystemInfo);
	switch(SystemInfo.wProcessorArchitecture){
		case PROCESSOR_ARCHITECTURE_AMD64: strcpy(OSbit, "64bit"); break;
		case PROCESSOR_ARCHITECTURE_IA64:  strcpy(OSbit, "64bit"); break;
		case PROCESSOR_ARCHITECTURE_INTEL: strcpy(OSbit, "32bit"); break;
		default: strcpy(OSbit, "unknown bit");
	}

	//OS情報をログに出力
	sprintf(str, "OS Name：%s %s (%s)", OSname, OSver, OSbit);
	OutputLog.WriteLog(LOG_CHECK, "Environment", str);


	//言語情報をログに出力
	sprintf(str, "OS Language：0x%04hx / 0x%04hx", GetSystemDefaultLangID(), GetUserDefaultLangID());
	OutputLog.WriteLog(LOG_CHECK, "Environment", str);
}
#endif

//! @brief ミリ秒単位を返す
//! @return ミリ秒
//! @attention WindowControlクラスを初期化しないと、タイマーの精度が出ません。
unsigned int GetTimeMS()
{
	unsigned int time;

	//timeBeginPeriod(1);
	time = timeGetTime();
	//timeEndPeriod(1);

	return time;
}

//[OpenXOPS-web] GetFps(), ControlFps(), GetTimeName(), InitRand(), GetRand(), Round() and
//ChangePathDelimiter() are platform-neutral and were moved to platform/common.cpp.

#endif
