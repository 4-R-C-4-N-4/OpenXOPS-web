//! @file sound-stub.cpp
//! @brief [OpenXOPS-web] Silent stub of SoundControl for the Phase 0 native build.
//!        LoadSound() only checks that the file can be opened and hands out a
//!        slot number, so the resource bookkeeping in game logic stays the same.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#include "../../sound.h"

#include <stdio.h>

SoundControl::SoundControl()
{
	for(int i=0; i<MAX_LOADSOUND; i++){
		useflag[i] = false;
	}
	camera_x = 0.0f;
	camera_y = 0.0f;
	camera_z = 0.0f;
	camera_rx = 0.0f;
	mastervolume = 1.0f;
}

SoundControl::~SoundControl()
{
	DestroySound();
}

//! @return 成功：0　失敗：1
int SoundControl::InitSound(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 1; }
	return 0;
}

void SoundControl::DestroySound()
{
	for(int i=0; i<MAX_LOADSOUND; i++){
		useflag[i] = false;
	}
}

void SoundControl::SetVolume(float volume)
{
	mastervolume = volume;
}

void SoundControl::SetCamera(float x, float y, float z, float rx)
{
	camera_x = x;
	camera_y = y;
	camera_z = z;
	camera_rx = rx;
}

//! @return 成功：0以上のデータ番号　失敗：-1
int SoundControl::LoadSound(const char* filename)
{
	if( filename == NULL ){ return -1; }

	for(int i=0; i<MAX_LOADSOUND; i++){
		if( useflag[i] == false ){
#ifdef ENABLE_PATH_DELIMITER_SLASH
			filename = ChangePathDelimiter(filename);
#endif
			FILE *fp = fopen(filename, "rb");
			if( fp == NULL ){ return -1; }
			fclose(fp);

			useflag[i] = true;
			return i;
		}
	}
	return -1;
}

//! @return 成功：1～3　失敗：0
int SoundControl::PlaySound(int id, int volume, int pan)
{
	UNREFERENCED_PARAMETER(volume);
	UNREFERENCED_PARAMETER(pan);
	if( (id < 0)||(MAX_LOADSOUND <= id) ){ return 0; }
	if( useflag[id] == false ){ return 0; }
	return 1;
}

//! @return 成功：1～3　失敗：0
int SoundControl::Play3DSound(int id, float x, float y, float z, int volume)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);
	UNREFERENCED_PARAMETER(z);
	return PlaySound(id, volume, 0);
}

int SoundControl::GetTotalSounds()
{
	int cnt = 0;
	for(int i=0; i<MAX_LOADSOUND; i++){
		if( useflag[i] == true ){ cnt += 1; }
	}
	return cnt;
}

void SoundControl::CleanupSound(int id)
{
	if( (id < 0)||(MAX_LOADSOUND <= id) ){ return; }
	useflag[id] = false;
}
