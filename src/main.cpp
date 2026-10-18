//! @file main.cpp
//! @brief WinMain()関数の定義およびテストプログラム 

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

//Doxygen設定ファイル
#include "doxygen.h"

#include "main.h"
#include "platform/vfs.h"

#ifdef __EMSCRIPTEN__
extern "C" void openxops_apply_controls(int preset);
#endif

#if defined(_DEBUG) && defined(_WIN32)
 #include <shlwapi.h>
 #pragma comment(lib, "Shlwapi.lib")
#endif

#ifdef ENABLE_DEBUGLOG
 //! ログ出力
 DebugLog OutputLog;
#endif

//! ステートマシン
StateMachine GameState;

//! ゲーム設定データ
Config GameConfig;

//! メインウィンドウ
WindowControl MainWindow;

//[OpenXOPS-web] The body of upstream WinMain() now lives in GameEntry(), shared by
//WinMain() (Windows) and main() (other platforms). __argc/__argv became argc/argv;
//the Win32-only parts (MessageBox in _DEBUG, SetParam) stay behind _WIN32.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
static int GameEntry(int argc, char **argv);

#ifdef _WIN32
//! @brief WinMain()関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//未使用引数対策
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//[OpenXOPS-web] moved up from just before InitWindow(); SetParam() only stores the two values
	MainWindow.SetParam(hPrevInstance, nCmdShow);

	return GameEntry(__argc, __argv);
}
#else
//! @brief main()関数
int main(int argc, char **argv)
{
	return GameEntry(argc, argv);
}
#endif

//! @brief ゲームの起動から終了まで（旧 WinMain() の本体）
//! @param argc 引数の数
//! @param argv 引数
//! @return 終了コード
static int GameEntry(int argc, char **argv)
{
	char path[MAX_PATH];

#ifdef ENABLE_AUTOLOADMIF
	bool NoBriefingFlag = false;
	int gamemode = 0;
#endif

	//乱数初期化
	InitRand();

	//実行ファイルのある場所を、カレントディレクトリにする。
	GetFileDirectory(argv[0], path);
	chdir(path);

#ifdef _DEBUG
	//メモリリークの検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	char str[255];
	strcpy(str, "[Information]\nThe compiler is the Debug mode.\nIf release the software, Switch compiler to Release mode.");
 #ifdef _WIN32
	MessageBox(NULL, str, GAMENAME, MB_OK);
 #else
	MainWindow.ErrorInfo(str);
 #endif
#endif

	//引数処理
	//メモ：Windows環境なら、lpCmdLine引数とかGetCommandLine()関数でも取れますけどね・・。
	strcpy(path, "");
	if( argc > 1 ){
		for(int param=1; param<argc; param++){
#ifdef ENABLE_DEBUGLOG
			//"Log"が与えられていたら、ログ出力を有効化
			if( CheckCommandParameter(argv[param], "-Log") == true ){
				//ログ出力を有効化
				OutputLog.EnableOutputLog();

				MainWindow.ErrorInfo("Enable Debug Log...");
				continue;
			}
#endif

#ifdef ENABLE_AUTOLOADMIF
			//"Dir"が与えられていたら、カレントディレクトリ移動
			if( CheckCommandParameter(argv[param], "-Dir") == true ){
				if( (param+1) < argc ){
					chdir(argv[param+1]);
					param += 1;
				}
				continue;
			}

			//"NoBriefing"が与えられていたら、（.mif指定時は）ブリーフィングを省略する
			if( CheckCommandParameter(argv[param], "-NoBriefing") == true ){
				NoBriefingFlag = true;
				continue;
			}

			//それ以外の引数ならば拡張子を判定してみる
			if( CheckFileExtension(argv[param], ".mif") == true ){
				//.mifであればファイルパスとして記憶
				strcpy(path, argv[param]);
			}
#endif
		}

#ifdef ENABLE_AUTOLOADMIF
		//何かしらの.mifが指定されているならば、ブリーフィングフラグ反映
		if( path[0] != '\0' ){
			if( NoBriefingFlag == false ){ gamemode = 1; }
			else{ gamemode = 2; }
		}
#endif
	}

	// Mount game data only after -Dir has selected the working directory.
#ifdef __EMSCRIPTEN__
 // browser-archive.cpp set OPENXOPS_DATA_ZIP to the validated layer list.
 setenv("OPENXOPS_DATA_ZIP","/xops096.zip",0);
#endif
 vfs_init_from_args(NULL);

#ifdef ENABLE_DEBUGLOG
	//ファイル作成
	OutputLog.MakeLog();

	//ユーザー環境を出力
	GetOperatingEnvironment();

	char infostr[64];

	/*
	//ログに出力
	sprintf(infostr, "Total Parameters : %d", argc);
	OutputLog.WriteLog(LOG_CHECK, "Parameter", infostr);
	for(int i=0; i<argc; i++){
		sprintf(infostr, "Parameter[%d]", i);
		OutputLog.WriteLog(LOG_CHECK, infostr, argv[i]);
	}
	*/

	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Startup", "Start program entry point");
#endif

#ifdef _DEBUG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Startup", "Visual C++ debug mode");
#endif

#ifdef _DEBUG
	char path2[MAX_PATH];
	getcwd(path2, MAX_PATH);
	sprintf(infostr, "EXE directory path length : %d", strlen(path2));

	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Environment", infostr);
#endif

	//データフォルダチェック
#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Directory", "data folder");
	if( CheckDirectory("data") == true ){
		OutputLog.WriteLog(LOG_COMPLETE, "", "");
	}
	else{
		OutputLog.WriteLog(LOG_ERROR, "", "");
	}
#endif
#ifdef ENABLE_CHECKDATADIR
	if( CheckDirectory("data") == false ){
		MainWindow.ErrorInfo("Data directory not exist.\n\nPlease download and copy \"X operations 0.96\".");
		return 1;
	}
#endif

	//設定ファイル読み込み
	if( GameConfig.LoadFile("config.dat") == 1 ){
#ifndef ENABLE_AUTOCREATECONFIG
		MainWindow.ErrorInfo("config data open failed");
		return 1;
#else
		//デフォルト値を反映し、設定ファイルに保存する
		GameConfig.SetDefaultConfig();
		if( GameConfig.SaveFile("config.dat") == 1 ){
			MainWindow.ErrorInfo("config data save failed");
			return 1;
		}
#endif
	}

	//追加設定を読み込む
#ifdef ENABLE_ADDCONFIG
	GameConfig.CreateExtConfig("config-openxops.ini");
	GameConfig.LoadExtFile("config-openxops.ini");
#else
	GameConfig.LoadExtFile(NULL);
#endif

#ifdef __EMSCRIPTEN__
	// Apply browser choice only after both original and extension config loaded.
	openxops_apply_controls(EM_ASM_INT({ return Module.browserInputPreset === 'modern' ? 1 : 0; }));
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	sprintf(infostr, "Display resolution : %d x %d", GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight());
	OutputLog.WriteLog(LOG_CHECK, "Environment", infostr);
	if( GameConfig.GetFullscreenFlag() == false ){
		OutputLog.WriteLog(LOG_CHECK, "Environment", "Window mode : Window");
	}
	else{
		OutputLog.WriteLog(LOG_CHECK, "Environment", "Window mode : Fullscreen");
	}
	sprintf(infostr, "Game language mode : %d", GameConfig.GetLanguage());
	OutputLog.WriteLog(LOG_CHECK, "Environment", infostr);
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Startup", "Start game initialize");
#endif

	//ウィンドウ初期化
	MainWindow.InitWindow(GAMENAME, GameConfig.GetScreenWidth(), GameConfig.GetScreenHeight(), GameConfig.GetFullscreenFlag());

	//基本的な初期化処理
#ifdef ENABLE_AUTOLOADMIF
	if( InitGame(&MainWindow, gamemode, path) ){
#else
	if( InitGame(&MainWindow, 0, "") ){
#endif
		return 1;
	}

 static opening Opening;
 static mainmenu MainMenu;
 static briefing Briefing;
 static maingame MainGame;
 static result Result;
	InitScreen(&MainWindow, &Opening, &MainMenu, &Briefing, &MainGame, &Result);

#ifdef ENABLE_AUTOLOADMIF
	//メインゲームのみ表示するモードならば、標準でデバック表示を有効にする。
	if( gamemode == 2 ){ MainGame.SetShowInfoFlag(true); }
#endif


#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Startup", "Start main loop");
#endif

#ifdef __EMSCRIPTEN__
 emscripten_set_main_loop([](){
   static unsigned int framecnt=0;
   static double last=0;
   if(EM_ASM_INT({ return !!Module.browserPaused; })) return;
   double now=emscripten_get_now();
   if(now-last<GAMEFRAMEMS)return;
   last=now;
   int flag=MainWindow.CheckMainLoop();
   if(flag<0){emscripten_cancel_main_loop();CleanupGame();return;}
   if(flag==1)ProcessScreen(&MainWindow,&Opening,&MainMenu,&Briefing,&MainGame,&Result,framecnt++);
 },0,1);
 return 0;
#else
 unsigned int framecnt = 0;
 for(int flag = 0; flag != -1; flag = MainWindow.CheckMainLoop()){
		if( flag == 1 ){
			//メイン処理
			ProcessScreen(&MainWindow, &Opening, &MainMenu, &Briefing, &MainGame, &Result, framecnt);

			//FPS調整
			ControlFps();

			framecnt++;
		}
	}

#endif // native main loop
#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Exit", "Exit main loop");
#endif


	//基本的な解放処理
	CleanupGame();

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Exit", "Exit program entry point");
#endif
	return 0;
}

//! @brief コマンド引数判定
//! @param param 元の文字列
//! @param cmd 判定する引数
//! @return 該当：true　非該当：false
//! @warning cmd引数の最初の一文字は無視されます。
//! @attention 次の引数を判定します。＞/[引数]、-[引数]、/[引数（小文字）]、-[引数（小文字）]
bool CheckCommandParameter(const char *param, const char *cmd)
{
	if( param == NULL ){ return false; }
	if( cmd == NULL ){ return false; }
	if( strlen(param) >= 16 ){ return false; }
	if( strlen(cmd) >= 16 ){ return false; }
	if( strlen(param) != strlen(cmd) ){ return false; }

	char str[16];
	strcpy(str, cmd);

	str[0] = '/';
	if( strcmp(param, str) == 0 ){ return true; }
	str[0] = '-';
	if( strcmp(param, str) == 0 ){ return true; }

	for(int i=0; i<(int)strlen(str); i++){ str[i] = (char)tolower((int)(str[i])); }

	str[0] = '/';
	if( strcmp(param, str) == 0 ){ return true; }
	str[0] = '-';
	if( strcmp(param, str) == 0 ){ return true; }

	return false;
}

//! @brief 角度（ラジアン）正規化
//! @param r 角度
//! @return 正規化後の角度
//! @note -PI～PI の間に正規化します。
float AngleNormalization(float r)
{
	for(; r > (float)M_PI;    r -= (float)M_PI*2){}
	for(; r < (float)M_PI*-1; r += (float)M_PI*2){}
	return r;
}

//! @brief ポイントパラメーターの値をunsigned int型に変換
//! @param value 値
//! @return 整数値
unsigned int PointParamToInt(signed short int value)
{
	return (unsigned int)((unsigned char)value);
}

//! @brief 2バイトに数値設定（リトルエンディアン）
//! @param outdata 2バイトのポインタ
//! @param value 数値
void Set2ByteLittleEndian(unsigned char *outdata, unsigned int value)
{
	if( outdata == NULL ){ return; }

	outdata[0] = (unsigned char)(( value >> 0 ) & 0x00FF);
	outdata[1] = (unsigned char)(( value >> 8 ) & 0x00FF);
}

//! @brief 4バイトに数値設定（リトルエンディアン）
//! @param outdata 4バイトのポインタ
//! @param value 数値
void Set4ByteLittleEndian(unsigned char *outdata, unsigned int value)
{
	if( outdata == NULL ){ return; }

	outdata[0] = (unsigned char)(( value >> 0 ) & 0x000000FF);
	outdata[1] = (unsigned char)(( value >> 8 ) & 0x000000FF);
	outdata[2] = (unsigned char)(( value >> 16 ) & 0x000000FF);
	outdata[3] = (unsigned char)(( value >> 24 ) & 0x000000FF);
}

//! @brief 2バイトに数値設定（ビッグエンディアン）
//! @param outdata 2バイトのポインタ
//! @param value 数値
void Set2ByteBigEndian(unsigned char *outdata, unsigned int value)
{
	if( outdata == NULL ){ return; }

	outdata[0] = (unsigned char)(( value >> 8 ) & 0x00FF);
	outdata[1] = (unsigned char)(( value >> 0 ) & 0x00FF);
}

//! @brief 4バイトに数値設定（ビッグエンディアン）
//! @param outdata 4バイトのポインタ
//! @param value 数値
void Set4ByteBigEndian(unsigned char *outdata, unsigned int value)
{
	if( outdata == NULL ){ return; }

	outdata[0] = (unsigned char)(( value >> 24 ) & 0x000000FF);
	outdata[1] = (unsigned char)(( value >> 16 ) & 0x000000FF);
	outdata[2] = (unsigned char)(( value >> 8 ) & 0x000000FF);
	outdata[3] = (unsigned char)(( value >> 0 ) & 0x000000FF);
}

//! @brief 2バイトの数値取得（リトルエンディアン）
//! @param indata 2バイトのポインタ
//! @return 数値
unsigned int Get2ByteLittleEndian(unsigned char *indata)
{
	if( indata == NULL ){ return 0; }

	return (unsigned int)( ((indata[1]<<8)&0xFF00) + ((indata[0]<<0)&0x00FF) );
}

//! @brief 4バイトの数値取得（リトルエンディアン）
//! @param indata 4バイトのポインタ
//! @return 数値
unsigned int Get4ByteLittleEndian(unsigned char *indata)
{
	if( indata == NULL ){ return 0; }

	return (unsigned int)( ((indata[3]<<24)&0xFF000000) + ((indata[2]<<16)&0x00FF0000) + ((indata[1]<<8)&0x0000FF00) + ((indata[0]<<0)&0x000000FF) );
}

//! @brief 2バイトの数値取得（ビッグエンディアン）
//! @param indata 2バイトのポインタ
//! @return 数値
unsigned int Get2ByteBigEndian(unsigned char *indata)
{
	if( indata == NULL ){ return 0; }

	return (unsigned int)( ((indata[0]<<8)&0xFF00) + ((indata[1]<<0)&0x00FF) );
}

//! @brief 4バイトの数値取得（ビッグエンディアン）
//! @param indata 4バイトのポインタ
//! @return 数値
unsigned int Get4ByteBigEndian(unsigned char *indata)
{
	if( indata == NULL ){ return 0; }

	return (unsigned int)( ((indata[0]<<24)&0xFF000000) + ((indata[1]<<16)&0x00FF0000) + ((indata[2]<<8)&0x0000FF00) + ((indata[3]<<0)&0x000000FF) );
}
