//! @file common.cpp
//! @brief フレームレート・時刻・乱数などの補助関数（プラットフォーム非依存部）

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

//[OpenXOPS-web] Moved from window.cpp (upstream 1.080) so that every window backend
//shares them. Only change: ControlFps() uses nanosleep() instead of Sleep() off Windows.

#include "../window.h"

#ifndef _WIN32
 #include <errno.h>
 #include <time.h>
#endif

//! @brief fps（Frames Per Second：フレームレート）計算
//! @param getcnt fpsを取得する周期（フレーム単位）
//! @return fps数
float GetFps(int getcnt)
{
	if( getcnt <= 0 ){ return 0.0f; }

	static unsigned int ptimeg = 0;
	unsigned int nowtime;
	static float pfps = 0.0f;
	static int cnt = 0;

	if( cnt == 0 ){
		ptimeg = GetTimeMS();
	}
	if( cnt == getcnt ){
		nowtime = GetTimeMS();
		pfps = 1000.0f / ((nowtime - ptimeg)/getcnt);
		cnt = 0;
	}
	else{
		cnt += 1;
	}

	return pfps;
}

//! @brief fps（Frames Per Second：フレームレート）調整
//! @return 調整を実施：true　　調整を実施せず：false
//! @attention WindowControlクラスを初期化しないと、タイマーの精度が出ません。
bool ControlFps()
{
	static unsigned int ptimec = 0;
	unsigned int nowtime;
	unsigned int waittime;

	nowtime = GetTimeMS();
	waittime = GAMEFRAMEMS - (nowtime - ptimec);
	if( (0 < waittime)&&(waittime <= GAMEFRAMEMS) ){
		//timeBeginPeriod(1);
#ifdef _WIN32
		Sleep(waittime);
#else
		//[OpenXOPS-web] Sleep() → nanosleep() (resumed after signal interruption)
		struct timespec req, rem;
		req.tv_sec = waittime / 1000;
		req.tv_nsec = (long)(waittime % 1000) * 1000000L;
		while( nanosleep(&req, &rem) == -1 && errno == EINTR ){ req = rem; }
#endif
		//timeEndPeriod(1);
		
		ptimec = GetTimeMS();
		return true;
	}

	ptimec = nowtime;
	return false;
}

//! @brief 日時による文字列を取得
//! @param str 文字列を受け取るポインタ
void GetTimeName(char *str)
{
	if( str == NULL ){ return; }

	time_t timer;
	struct tm *local;

	timer = time(NULL);
	local = localtime(&timer);

	sprintf(str, "%04d%02d%02d%02d%02d%02d", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
}

//! @brief 乱数を初期化
void InitRand()
{
	srand(GetTimeMS());
}

//! @brief ランダムな整数値を返す
//! @param num 範囲
//! @return 0～num-1
int GetRand(int num)
{
	if( num <= 0 ){ return 0; }

	return rand()%num;

	//return rand() / (RAND_MAX/num);

	//本家XOPSのアルゴリズム？
	//static int memory = GetTimeMS();
	//int x;
	//memory = memory * 214013 + 2745024;
	//x = memory >> 16;
	//x = x & 0x00007FFF;
	//return x%num;
}

//! @brief 四捨五入
//! @param r 値
//! @return 値
float Round(float r)
{
	return (r > 0.0f) ? floorf(r + 0.5f) : ceilf(r - 0.5f);
}

#ifdef ENABLE_PATH_DELIMITER_SLASH
//! @brief パス区切り文字を変換
//! @param *str ファイルパスのポインタ
//! @return 新しいファイルパスのポインタ
//! @note '\'を'/'へ置き換えます。
char* ChangePathDelimiter(const char *str)	//[OpenXOPS-web] const: callers pass const char*
{
	if( str == NULL ){ return NULL; }

	static char newstr[MAX_PATH];
	strcpy(newstr, str);

	for(int i=0; i<(int)strlen(newstr); i++){
		if( newstr[i] == '\\' ){
			newstr[i] = '/';
		}
	}

	return newstr;
}
#endif
