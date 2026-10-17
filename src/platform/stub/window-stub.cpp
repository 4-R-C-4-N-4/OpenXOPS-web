//! @file window-stub.cpp
//! @brief [OpenXOPS-web] Headless stub of WindowControl and GetTimeMS() for the
//!        Phase 0 native build. No window is created. CheckMainLoop() runs the
//!        game for OPENXOPS_STUB_FRAMES frames (default 1) and then reports
//!        "window closed" (-1). ErrorInfo() prints to stderr.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#include "../../window.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/utsname.h>

//! @brief Number of game frames to run before CheckMainLoop() returns -1
static unsigned int StubMaxFrames()
{
	static int cached = -1;
	if( cached < 0 ){
		cached = 1;
		const char *env = getenv("OPENXOPS_STUB_FRAMES");
		if( env != NULL ){
			char *end = NULL;
			long v = strtol(env, &end, 10);
			if( (end != env)&&(v >= 0)&&(v <= 0x7FFFFFFF) ){ cached = (int)v; }
		}
	}
	return (unsigned int)cached;
}

WindowControl::WindowControl()
{
	width = 0;
	height = 0;
	fullscreenflag = false;
	closeflag = false;
	frames = 0;
}

WindowControl::~WindowControl()
{}

bool WindowControl::InitWindow(const char* title, int in_width, int in_height, bool fullscreen)
{
	if( title == NULL ){ return false; }
	if( (in_width <= 0)||(in_height <= 0) ){ return false; }

	width = in_width;
	height = in_height;
	fullscreenflag = fullscreen;

	fprintf(stderr, "[stub] InitWindow(\"%s\", %d x %d, %s)\n", title, width, height, fullscreen ? "fullscreen" : "window");
	return true;
}

bool WindowControl::ChangeWindowMode(bool fullscreen)
{
	fullscreenflag = fullscreen;
	return true;
}

//! @return アイドル状態：0　ゲームのメイン処理実行：1　ウィンドウが閉じた：-1
int WindowControl::CheckMainLoop()
{
	if( closeflag == true ){ return -1; }
	if( frames >= StubMaxFrames() ){
		fprintf(stderr, "[stub] CheckMainLoop: %u frame(s) done, closing\n", frames);
		return -1;
	}
	frames += 1;
	return 1;
}

void WindowControl::ErrorInfo(const char *str)
{
	if( str == NULL ){ return; }

	fprintf(stderr, "error: %s\n", str);
}

void WindowControl::CloseWindow()
{
	closeflag = true;
}

#ifdef ENABLE_DEBUGLOG
//! @brief デバック用ログにユーザー環境を出力
void GetOperatingEnvironment()
{
	char str[256];
	struct utsname u;

	if( uname(&u) == 0 ){
		snprintf(str, sizeof(str), "OS Name：%s %s (%s)", u.sysname, u.release, u.machine);
	}
	else{
		snprintf(str, sizeof(str), "OS Name：unknown");
	}
	OutputLog.WriteLog(LOG_CHECK, "Environment", str);
}
#endif

//! @brief ミリ秒単位を返す
//! @return ミリ秒（CLOCK_MONOTONIC、32bitで折り返す。timeGetTime() と同じ扱い）
unsigned int GetTimeMS()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long long ms = (unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)(ts.tv_nsec / 1000000L);
	return (unsigned int)ms;
}

//[OpenXOPS-web] gl_present.h contract: the stub window has no GL context.
#include "platform/gl_present.h"
bool Platform_GLAvailable(){ return false; }
void Platform_GLSwap(){}
void Platform_GLDrawableSize(int *w, int *h){ if( w ){ *w = 0; } if( h ){ *h = 0; } }
