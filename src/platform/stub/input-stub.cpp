//! @file input-stub.cpp
//! @brief [OpenXOPS-web] Headless stub of InputControl for the Phase 0 native build.
//!        Reports nothing pressed and no mouse movement. The query methods
//!        (CheckKeyNow() ... CheckInputAll()) and the VK key-id helpers are the
//!        upstream ones in platform/input-common.cpp.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#include "../../input.h"

#include <string.h>

InputControl::InputControl()
{
	InitFlag = false;
	MouseLimitFlag = false;
	memset(keys, 0, sizeof(keys));
	memset(keys_lt, 0, sizeof(keys_lt));
	mx = 0;
	my = 0;
	mbl = false;
	mbr = false;
	mbl_lt = false;
	mbr_lt = false;
}

InputControl::~InputControl()
{
	DestroyInput();
}

//! @return 成功：0　失敗：1
int InputControl::InitInput(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 1; }

	InitFlag = true;
	return 0;
}

void InputControl::DestroyInput()
{
	InitFlag = false;
}

void InputControl::SetMouseLimitFlag(bool flag)
{
	MouseLimitFlag = flag;
}

//! @brief 入力状態を更新（スタブ：常に何も押されていない）
void InputControl::GetInputState(bool mousemode)
{
	UNREFERENCED_PARAMETER(mousemode);

	//前回の状態を保存（上流と同じ流れ）
	memcpy(keys_lt, keys, sizeof(keys));
	mbl_lt = mbl;
	mbr_lt = mbr;

	memset(keys, 0, sizeof(keys));
	mx = 0;
	my = 0;
	mbl = false;
	mbr = false;
}

void InputControl::MoveMouseCenter()
{}
