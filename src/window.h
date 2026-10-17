//! @file window.h
//! @brief window.cpp関係のヘッダーファイル

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

#ifndef WINDOW_H
#define WINDOW_H

class WindowControl;	//プロトタイプ宣言

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 1		//!< Select include file.
#endif
#include "main.h"

#include <windows.h>
#include <mmsystem.h>
#include <time.h>

#pragma comment(lib, "winmm.lib")

//! @brief ウィンドウ制御クラス
//! @details ゲーム画面を表示するウィンドウを制御し管理します。
class WindowControl
{
	HINSTANCE hInstance;	//!< インスタンス ハンドル
	int nCmdShow;			//!< ウィンドウの表示状態
	HWND hWnd;				//!< ウィンドウハンドル

	static LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	WindowControl();
	~WindowControl();
	void SetParam(HINSTANCE in_hInstance, int in_nCmdShow);
	bool InitWindow(const char* title, int width, int height, bool fullscreen);
	bool ChangeWindowMode(bool fullscreen);
	HWND GethWnd();
	int CheckMainLoop();
	void ErrorInfo(const char *str);
	void CloseWindow();
};

#ifdef ENABLE_DEBUGLOG
 void GetOperatingEnvironment();
#endif

float GetFps(int getcnt);
bool ControlFps();
unsigned int GetTimeMS();
void GetTimeName(char *str);
void InitRand();
int GetRand(int num);
float Round(float r);

#ifdef ENABLE_PATH_DELIMITER_SLASH
 char* ChangePathDelimiter(char *str);
#endif

#endif