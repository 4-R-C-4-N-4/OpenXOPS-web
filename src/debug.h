//! @file debug.h
//! @brief DebugLogクラスの宣言

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

#ifndef DEBUG_H
#define DEBUG_H

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 1		//!< Select include file.
#endif
#include "main.h"

#ifdef ENABLE_DEBUGLOG
//! @brief デバック情報を出力するクラス
//! @details デバック情報をファイルに出力します。
class DebugLog
{
	bool OutputFlag;		//!< 出力有効
	char fname[MAX_PATH];	//!< ファイル名
	bool error;				//!< エラーフラグ（次回エラーを記録する）

public:
	DebugLog();
	~DebugLog();
	void EnableOutputLog();
	void SetLogName(const char* name);
	bool MakeLog();
	bool WriteLog(int tag, const char* title, int id);
	bool WriteLog(int tag, const char* title, const char* text);
};

//! ログの出力モードを表す定数
enum LogTag
{
	LOG_CHECK,
	LOG_INIT,
	LOG_LOAD,
	LOG_CLEANUP,
	LOG_COMPLETE,
	LOG_ERROR
};
#endif	//ENABLE_DEBUGLOG

#endif