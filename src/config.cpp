//! @file config.cpp
//! @brief configクラスの定義 

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

#include "config.h"

//! @brief コンストラクタ
Config::Config()
{
	//各種メンバー変数初期化
	for(int i=0; i<TOTAL_ControlKey; i++){
		Keycode[i] = 0;
	}
	MouseSensitivity = 0;
	FullscreenFlag = false;
	SoundFlag = false;
	BloodFlag = false;
	Brightness = 0;
	InvertMouseFlag = false;
	FrameskipFlag = false;
	AnotherGunsightFlag = false;
	strcpy(PlayerName, "");

	Language = 0;
	ShowArmFlag = false;
	ScreenWidth = 0;
	ScreenHeight = 0;
	MouseLimitFlag = true;
	mastervolume = 0.0f;
}

//! @brief ディストラクタ
Config::~Config()
{}

//! @brief 設定ファイルを読み込む
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int Config::LoadFile(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	char buf;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Config File", fname);
#endif

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	fname = ChangePathDelimiter(fname);
#endif

	//ファイルを開く
	fp = fopen(fname, "rb");
	if( fp == NULL ){ return 1; }

	//キーコード
	for(int i=0; i<TOTAL_ControlKey; i++){
		fread(&buf, 1, 1, fp);
		Keycode[i] = (int)buf;
	}

	//マウス感度
	fread(&buf, 1, 1, fp);
	MouseSensitivity = (int)buf;

	//フルスクリーン有効
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ FullscreenFlag = false; }
	else{ FullscreenFlag = true; }

	//効果音有効
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ SoundFlag = false; }
	else{ SoundFlag = true; }

	//出血有効
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ BloodFlag = false; }
	else{ BloodFlag = true; }

	//画面の明るさ
	fread(&buf, 1, 1, fp);
	Brightness = (int)buf;

	//マウス反転
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ InvertMouseFlag = false; }
	else{ InvertMouseFlag = true; }

	//フレームスキップ
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ FrameskipFlag = false; }
	else{ FrameskipFlag = true; }

	//別の照準を使用
	fread(&buf, 1, 1, fp);
	if( buf == 0x00 ){ AnotherGunsightFlag = false; }
	else{ AnotherGunsightFlag = true; }

	//プレイヤー名
	fread(PlayerName, 1, MAX_PLAYERNAME, fp);

	//ファイルハンドルを閉じる
	fclose(fp);

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif
	return 0;
}

//! @brief 設定ファイルに書き込む
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int Config::SaveFile(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	char buf;

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Config File (Save)", fname);
#endif

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	fname = ChangePathDelimiter(fname);
#endif

	//ファイルを開く
	fp = fopen(fname, "wb");
	if( fp == NULL ){ return 1; }

	//キーコード
	for(int i=0; i<TOTAL_ControlKey; i++){
		buf = (char)Keycode[i];
		fwrite(&buf, 1, 1, fp);
	}

	//マウス感度
	buf = (char)MouseSensitivity;
	fwrite(&buf, 1, 1, fp);

	//フルスクリーン有効
	if( FullscreenFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//効果音有効
	if( SoundFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//出血有効
	if( BloodFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//画面の明るさ
	buf = (char)Brightness;
	fwrite(&buf, 1, 1, fp);

	//マウス反転
	if( InvertMouseFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//フレームスキップ
	if( FrameskipFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//別の照準を使用
	if( AnotherGunsightFlag == false ){ buf = 0x00; }
	else{ buf = 0x01; }
	fwrite(&buf, 1, 1, fp);

	//プレイヤー名
	fwrite(PlayerName, 1, MAX_PLAYERNAME, fp);

	//ファイルハンドルを閉じる
	fclose(fp);

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif
	return 0;
}

//! @brief 設定値をデフォルトの値で初期化
void Config::SetDefaultConfig()
{
	Keycode[KEY_TURNUP]			= 0;
	Keycode[KEY_TURNDOWN]		= 1;
	Keycode[KEY_TURNLEFT]		= 2;
	Keycode[KEY_TURNRIGHT]		= 3;
	Keycode[KEY_MOVEFORWARD]	= 54;
	Keycode[KEY_MOVEBACKWARD]	= 50;
	Keycode[KEY_MOVELEFT]		= 32;
	Keycode[KEY_MOVERIGHT]		= 35;
	Keycode[KEY_WALK]			= 16;
	Keycode[KEY_JUMP]			= 17;
	Keycode[KEY_RELOAD]			= 49;
	Keycode[KEY_DROPWEAPON]		= 38;
	Keycode[KEY_ZOOMSCOPE]		= 20;
	Keycode[KEY_SHOTMODE]		= 55;
	Keycode[KEY_SWITCHWEAPON]	= 57;
	Keycode[KEY_WEAPON1]		= 23;
	Keycode[KEY_WEAPON2]		= 24;
	Keycode[KEY_SHOT]			= 18;
	MouseSensitivity = 25;
	FullscreenFlag = true;
	SoundFlag = true;
	BloodFlag = true;
	Brightness = 4;
	InvertMouseFlag = false;
	FrameskipFlag = false;
	AnotherGunsightFlag = false;
	strcpy(PlayerName, "xopsplayer");
}

//! @brief オリジナルキーコードを取得
//! @param id 定数
//! @return オリジナルキーコード
int Config::GetKeycode(int id)
{
	if( (id < 0)||(TOTAL_ControlKey <= id) ){ return 0; }

	return Keycode[id];
}

//! @brief オリジナルキーコードを設定
//! @param id 定数
//! @param key オリジナルキーコード
void Config::SetKeycode(int id, int key)
{
	if( (id < 0)||(TOTAL_ControlKey <= id) ){ return; }

	Keycode[id] = key;
}

//! @brief マウス感度取得
//! @return 生の値
int Config::GetMouseSensitivity()
{
	return MouseSensitivity;
}

//! @brief マウス感度設定
//! @param param 生の値
void Config::SetMouseSensitivity(int param)
{
	MouseSensitivity = param;
}

//! @brief 画面表示モード取得
//! @return ウィンドウ：false　フルスクリーン：true
bool Config::GetFullscreenFlag()
{
	return FullscreenFlag;
}

//! @brief 画面表示モード設定
//! @param flag フラグ
void Config::SetFullscreenFlag(bool flag)
{
	FullscreenFlag = flag;
}

//! @brief 効果音設定取得
//! @return 無効：false　有効：true
bool Config::GetSoundFlag()
{
	return SoundFlag;
}

//! @brief 効果音設定
//! @param flag フラグ
void Config::SetSoundFlag(bool flag)
{
	SoundFlag = flag;
}

//! @brief 出血設定取得
//! @return 無効：false　有効：true
bool Config::GetBloodFlag()
{
	return BloodFlag;
}

//! @brief 出血設定
//! @param flag フラグ
void Config::SetBloodFlag(bool flag)
{
	BloodFlag = flag;
}

//! @brief 画面の明るさ設定取得
//! @return 生の値
int Config::GetBrightness()
{
	return Brightness;
}

//! @brief 画面の明るさ設定
//! @param param 生の値
void Config::SetBrightness(int param)
{
	Brightness = param;
}


//! @brief マウス反転設定取得
//! @return 無効：false　有効：true
bool Config::GetInvertMouseFlag()
{
	return InvertMouseFlag;
}

//! @brief マウス反転設定
//! @param flag フラグ
void Config::SetInvertMouseFlag(bool flag)
{
	InvertMouseFlag = flag;
}

//! @brief フレームスキップ設定取得
//! @return 無効：false　有効：true
bool Config::GetFrameskipFlag()
{
	return FrameskipFlag;
}

//! @brief フレームスキップ設定
//! @param flag フラグ
void Config::SetFrameskipFlag(bool flag)
{
	FrameskipFlag = flag;
}

//! @brief 別の照準を使用設定取得
//! @return 無効：false　有効：true
bool Config::GetAnotherGunsightFlag()
{
	return AnotherGunsightFlag;
}

//! @brief 別の照準を使用設定
//! @param flag フラグ
void Config::SetAnotherGunsightFlag(bool flag)
{
	AnotherGunsightFlag = flag;
}

//! @brief プレイヤー名取得
//! @param out_str 受け取る文字列型ポインタ
//! @return プレイヤー名文字数
int Config::GetPlayerName(char *out_str)
{
	if( out_str == NULL ){ return 0; }

	//ポインタにコピーする
	strcpy(out_str, PlayerName);

	//文字数を返す
	return (int)strlen(PlayerName);
}

//! @brief プレイヤー名設定
//! @param in_str プレイヤー名
void Config::SetPlayerName(char *in_str)
{
	if( in_str == NULL ){ return; }

	if( strlen(in_str) > (MAX_PLAYERNAME - 1) ){
		in_str[MAX_PLAYERNAME - 1] = 0x00;
	}

	//文字列をコピーする
	strcpy(PlayerName, in_str);
}

//! @brief オリジナルキーコードの名前を取得
//! @param code オリジナルキーコード
//! @return キーの名前
const char* Config::GetOriginalkeycodeString(int code)
{
	const char *out;

	switch(code){
		case 0x00: out = "UP"; break;
		case 0x01: out = "DOWN"; break;
		case 0x02: out = "LEFT"; break;
		case 0x03: out = "RIGHT"; break;
		case 0x04: out = "NUM0"; break;
		case 0x05: out = "NUM1"; break;
		case 0x06: out = "NUM2"; break;
		case 0x07: out = "NUM3"; break;
		case 0x08: out = "NUM4"; break;
		case 0x09: out = "NUM5"; break;
		case 0x0A: out = "NUM6"; break;
		case 0x0B: out = "NUM7"; break;
		case 0x0C: out = "NUM8"; break;
		case 0x0D: out = "NUM9"; break;
		case 0x0E: out = "BACKSPACE"; break;
		case 0x0F: out = "ENTER"; break;

		case 0x10: out = "TAB"; break;
		case 0x11: out = "SPACE"; break;
		case 0x12: out = "MOUSE L"; break;
		case 0x13: out = "MOUSE R"; break;
		case 0x14: out = "SHIFT"; break;
		case 0x15: out = "CTRL"; break;
		case 0x16: out = "0"; break;
		case 0x17: out = "1"; break;
		case 0x18: out = "2"; break;
		case 0x19: out = "3"; break;
		case 0x1A: out = "4"; break;
		case 0x1B: out = "5"; break;
		case 0x1C: out = "6"; break;
		case 0x1D: out = "7"; break;
		case 0x1E: out = "8"; break;
		case 0x1F: out = "9"; break;

		case 0x20: out = "A"; break;
		case 0x21: out = "B"; break;
		case 0x22: out = "C"; break;
		case 0x23: out = "D"; break;
		case 0x24: out = "E"; break;
		case 0x25: out = "F"; break;
		case 0x26: out = "G"; break;
		case 0x27: out = "H"; break;
		case 0x28: out = "I"; break;
		case 0x29: out = "J"; break;
		case 0x2A: out = "K"; break;
		case 0x2B: out = "L"; break;
		case 0x2C: out = "M"; break;
		case 0x2D: out = "N"; break;
		case 0x2E: out = "O"; break;
		case 0x2F: out = "P"; break;

		case 0x30: out = "Q"; break;
		case 0x31: out = "R"; break;
		case 0x32: out = "S"; break;
		case 0x33: out = "T"; break;
		case 0x34: out = "U"; break;
		case 0x35: out = "V"; break;
		case 0x36: out = "W"; break;
		case 0x37: out = "X"; break;
		case 0x38: out = "Y"; break;
		case 0x39: out = "Z"; break;
		case 0x3A: out = "/"; break;
		case 0x3B: out = ":"; break;
		case 0x3C: out = ";"; break;
		case 0x3D: out = "-"; break;
		case 0x3E: out = "@"; break;
		case 0x3F: out = "["; break;

		case 0x40: out = "]"; break;
		case 0x41: out = "BACKSLASH"; break;
		case 0x42: out = "\\"; break;
		case 0x43: out = ","; break;
		case 0x44: out = "."; break;
		case 0x45: out = "^"; break;
		case 0x46: out = "NUM*" ; break;
		case 0x47: out = "NUM/"; break;
		case 0x48: out = "NUM+"; break;
		case 0x49: out = "NUM-"; break;
		case 0x4A: out = "NUM."; break;

		default : out = "NULL";
	}

	return out;
}


//! @brief 追加設定ファイルを生成する
//! @param fname ファイル名
//! @return 成功：true　失敗/不要：false
bool Config::CreateExtConfig(const char *fname)
{
#ifdef ENABLE_ADDCONFIG
	if( fname == NULL ){ return false; }

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	fname = ChangePathDelimiter(fname);
#endif

	FILE *fp;

	//ファイルが存在するなら失敗（ファイル生成不要）
	fp = fopen(fname, "r");
	if( fp != NULL ){
		fclose(fp);
		return false;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Config File (Create .ini)", fname);
#endif

	fp = fopen(fname, "w");
	if( fp == NULL ){ return false; }

	fprintf(fp, "[Game]\n");
	fprintf(fp, ";Game language.\n");
	fprintf(fp, ";  0: English + Japanese  (default)\n");
	fprintf(fp, ";  1: English + Korean\n");
	fprintf(fp, "Language = 0\n");
	fprintf(fp, "\n");
	fprintf(fp, ";Show player arm and weapon.\n");
	fprintf(fp, ";  0: OFF (default)\n");
	fprintf(fp, ";  1: ON\n");
	fprintf(fp, "ShowArm = 0\n");
	fprintf(fp, "\n");
	fprintf(fp, "\n");

	fprintf(fp, "[Graphics]\n");
	fprintf(fp, ";WindowSize\n");
	fprintf(fp, ";  0: 640x480  [4:3] (default)\n");
	fprintf(fp, ";  1: 800x600  [4:3]\n");
	fprintf(fp, ";  2:1024x768  [4:3]\n");
	fprintf(fp, ";  3:1280x960  [4:3]\n");
	fprintf(fp, ";  4:1600x1200 [4:3]\n");
	fprintf(fp, "; 10:1280x720  [16:9]\n");
	fprintf(fp, "; 11:1600x900  [16:9]\n");
	fprintf(fp, "; 12:1920x1080 [16:9]\n");
	//fprintf(fp, "; 13:2560x1440 [16:9]\n");
	//fprintf(fp, "; 14:3840x2160 [16:9]\n");
	fprintf(fp, "WindowSize = 0\n");
	fprintf(fp, "\n");
	fprintf(fp, "\n");

	fprintf(fp, "[Input]\n");
	fprintf(fp, ";Limit mouse input control.\n");
	fprintf(fp, ";  0: OFF\n");
	fprintf(fp, ";  1: ON (default)\n");
	fprintf(fp, "MouseLimit = 1\n");
	fprintf(fp, "\n");
	fprintf(fp, "\n");

	fprintf(fp, "[Sounds]\n");
	fprintf(fp, ";Game sound volume. (0.0 ~ 1.0)\n");
	fprintf(fp, "MasterVolume = 1.0\n");
	fprintf(fp, "\n");

	fclose(fp);

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif
	return true;
#else
	return false;
#endif
}

//! @brief 追加設定ファイルを読み込む
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int Config::LoadExtFile(const char *fname)
{
#ifdef ENABLE_ADDCONFIG
	if( fname == NULL ){ return 1; }

	INIFileInterface INIConfig;
	int mode;
	
#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Config File (.ini)", fname);
#endif

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	fname = ChangePathDelimiter(fname);
#endif

	INIConfig.LoadINIFile(fname);

	//表示言語取得
	Language = INIConfig.GetINIFileInt("Game", "Language", 0, NULL);

	//腕表示フラグ取得
	mode = INIConfig.GetINIFileInt("Game", "ShowArm", 0, NULL);
	if( mode == 0 ){ ShowArmFlag = false; }
	else{ ShowArmFlag = true; }

	//解像度取得
	mode = INIConfig.GetINIFileInt("Graphics", "WindowSize", 0, NULL);
	switch(mode){
		case  0: ScreenWidth =  640; ScreenHeight =  480; break;
		case  1: ScreenWidth =  800; ScreenHeight =  600; break;
		case  2: ScreenWidth = 1024; ScreenHeight =  768; break;
		case  3: ScreenWidth = 1280; ScreenHeight =  960; break;
		case  4: ScreenWidth = 1600; ScreenHeight = 1200; break;
		case 10: ScreenWidth = 1280; ScreenHeight =  720; break;
		case 11: ScreenWidth = 1600; ScreenHeight =  900; break;
		case 12: ScreenWidth = 1920; ScreenHeight = 1080; break;
		case 13: ScreenWidth = 2560; ScreenHeight = 1440; break;
		case 14: ScreenWidth = 3840; ScreenHeight = 2160; break;
		default: ScreenWidth = DEFAULT_SCREEN_WIDTH; ScreenHeight = DEFAULT_SCREEN_HEIGHT; break;
	}

	//マウス移動量制限フラグ
	mode = INIConfig.GetINIFileInt("Input", "MouseLimit", 1, NULL);
	if( mode == 0 ){ MouseLimitFlag = false; }
	else{ MouseLimitFlag = true; }

	//音量取得
	mastervolume = INIConfig.GetINIFileFloat("Sounds", "MasterVolume", 1.0f, NULL);
	if( mastervolume < 0.0f ){ mastervolume = 0.0f; }
	if( mastervolume > 1.0f ){ mastervolume = 1.0f; }

	INIConfig.ReleaseINIFile();

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

#else
	ShowArmFlag = false;
	ScreenWidth = DEFAULT_SCREEN_WIDTH;
	ScreenHeight = DEFAULT_SCREEN_HEIGHT;
	mastervolume = 1.0f;
#endif
	return 0;
}

//! @brief 追加設定値をデフォルトの値で初期化
void Config::SetDefaultExtConfig()
{
	Language = 0;
	ShowArmFlag = false;
	ScreenWidth = DEFAULT_SCREEN_WIDTH;
	ScreenHeight = DEFAULT_SCREEN_HEIGHT;
	MouseLimitFlag = true;
	mastervolume = 1.0f;
}

//! @brief 言語設定を取得
//! @return 値
int Config::GetLanguage()
{
	return Language;
}

//! @brief 腕表示フラグを取得
//! @return フラグ
bool Config::GetShowArmFlag()
{
	return ShowArmFlag;
}

//! @brief 画面解像度（幅）を取得
//! @return 値
int Config::GetScreenWidth()
{
	return ScreenWidth;
}

//! @brief 画面解像度（高さ）を取得
//! @return 値
int Config::GetScreenHeight()
{
	return ScreenHeight;
}

//! @brief マウス移動量制限フラグ
//! @return 値
bool Config::GetMouseLimitFlag()
{
	return MouseLimitFlag;
}

//! @brief 音量設定を取得
//! @return 値
float Config::GetVolume()
{
	return mastervolume;
}
