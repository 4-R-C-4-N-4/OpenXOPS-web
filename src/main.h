//! @file main.h
//! @brief 最上位のヘッダーファイル

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

#ifndef MAIN_H
#define MAIN_H

#define MAINICON 101		//!< Icon

#ifdef _DEBUG
 #define _CRTDBG_MAP_ALLOC
 #include <crtdbg.h>
#endif

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 3		//!< Select include file.
#endif

#pragma warning(disable:4996)		//VC++警告防止
//#pragma warning(disable:4244)

//定数
#define GAMENAME "OpenXOPS"				//!< ゲーム名
#define GAMEVERSION "1.080"			//!< ゲームのバージョン
#define GAMEFRAMEMS 30					//!< フレームあたりの処理時間（ms）
#define GAMEFPS (1000.0f/GAMEFRAMEMS)	//!< FPS（フレームレート）　1000 / 30 = 33.333[FPS]
#define DEFAULT_SCREEN_WIDTH 640		//!< スクリーンの幅
#define DEFAULT_SCREEN_HEIGHT 480		//!< スクリーンの高さ

#define ADDONDIR "addon"		//!< ADDONを入れるディレクトリ名
#define MAX_ADDONLIST 128		//!< ADDONを読み込む最大数

#define TOTAL_HAVEWEAPON 2		//!< 持てる武器の数

//システムのインクルードファイル
#define _USE_MATH_DEFINES	 //!< math.h 'M_PI' enable
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <direct.h>

#define ENABLE_BUG_HUMANWEAPON			//!< 範囲外の人・武器種類番号を用いたバグを再現する（コメント化で無効）
#define ENABLE_BUG_TEAMID				//!< チーム番号を負数に設定した際のバグを再現する（コメント化で無効）
#define ENABLE_WEAPONNAME_SCALING		//!< 武器名が長い場合に表示を縮小する（コメント化で無効）
#define ENABLE_ADDOBJ_PARAM8BIT			//!< 追加小物の設定値をchar型 8bitにする（コメント化で無効）
#define ENABLE_DEBUGCONSOLE				//!< デバック用コンソールの有効化（コメント化で機能無効）
#define ENABLE_DEBUGLOG					//!< デバック用ログ出力の有効化（コメント化で機能無効）
#define ENABLE_CHECKOPENXOPSEVENT		//!< OpenXOPS動作判定用イベントの有効化（コメント化で機能無効）
#define ENABLE_MENUOPTIONS				//!< メニューにオプション画面追加（コメント化で機能無効）
//#define ENABLE_AUTOCREATECONFIG		//!< config.datが見つからない場合、同ファイルを自動生成する。
//#define ENABLE_PATH_DELIMITER_SLASH	//!< パス区切り文字を、'\'から‘/’へ変換する。
#define ENABLE_AUTOLOADMIF				//!< .mifが引数に指定された場合、自動的にロードする（コメント化で機能無効）
#define ENABLE_ADDCONFIG				//!< iniファイルによる追加設定を取得する（コメント化で機能無効）
#define ENABLE_CHECKDATADIR				//!< 起動時にdataフォルダーの有無をチェックする（コメント化で機能無効）

//windows.hを使用しないならば
#ifndef _MAX_PATH
 #define _MAX_PATH 260		//!< _MAX_PATH is 260
#endif
#ifndef MAX_PATH
 #define MAX_PATH 260		//!< MAX_PATH is 260
#endif

//BorlandC++ Compiler用の処理
#ifdef __BORLANDC__
 //#define sqrtf(df) sqrt(df)
 int _matherr(struct _exception  *__e) { return 1; }
#endif

//未使用引数の対策用マクロ
#ifndef MAIN_H
 #define UNREFERENCED_PARAMETER(P) ((P)=(P))
#endif

//float型の算術関数用意
#ifndef _MSC_VER
 #define sinf(x) (float)sin(x)				//!< sinf(x)
 #define cosf(x) (float)cos(x)				//!< cosf(x)
 #define asinf(x) (float)asin(x)			//!< asinf(x)
 #define acosf(x) (float)acos(x)			//!< acosf(x)
 #define atan2f(y,x) (float)atan2(y,x)		//!< atan2f(y,x)
 #define sqrtf(x) (float)sqrt(x)			//!< sqrtf(x)
 #define floorf(x) (float)floor(x)			//!< floorf(x)
 #define ceilf(x) (float)ceil(x)			//!< ceilf(x)
 #define fabsf(x) (float)fabs(x)			//!< fabsf(x)
#endif

#define SetFlag(value, bit) value = value | bit		//!< ビットによるフラグ 設定
#define DelFlag(value, bit) value = value & (~bit)	//!< ビットによるフラグ 解除
#define GetFlag(value, bit) (value & bit)			//!< ビットによるフラグ 取得

#define DegreeToRadian(value) ((float)M_PI/180*value)	//!< 度からラジアンへの変換

//低レイヤー
#if H_LAYERLEVEL >= 1
 #include "debug.h"
 #include "window.h"
 #include "config.h"
 #include "datafile.h"
 #include "d3dgraphics.h"
 #include "input.h"
 #include "sound.h"
 #include "parameter.h"
#endif

//中間レイヤー
#if H_LAYERLEVEL >= 2
 #include "resource.h"
 #include "collision.h"
 #include "object.h"
 #include "soundmanager.h"
 #include "gui-object.h"
#endif

//高レイヤー
#if H_LAYERLEVEL >= 3
 #include "objectmanager.h"
 #include "scene.h"
 #include "gamemain.h"
 #include "ai.h"
 #include "statemachine.h"
 #include "event.h"
#endif

//追加ライブラリ
// not .lib

#ifdef ENABLE_DEBUGLOG
 extern class DebugLog OutputLog;
#endif
extern class StateMachine GameState;
extern class Config GameConfig;

bool CheckCommandParameter(const char *param, const char *cmd);
float AngleNormalization(float r);
unsigned int PointParamToInt(signed short int value);
void Set2ByteLittleEndian(unsigned char *outdata, unsigned int value);
void Set4ByteLittleEndian(unsigned char *outdata, unsigned int value);
void Set2ByteBigEndian(unsigned char *outdata, unsigned int value);
void Set4ByteBigEndian(unsigned char *outdata, unsigned int value);
unsigned int Get2ByteLittleEndian(unsigned char *indata);
unsigned int Get4ByteLittleEndian(unsigned char *indata);
unsigned int Get2ByteBigEndian(unsigned char *indata);
unsigned int Get4ByteBigEndian(unsigned char *indata);

#endif