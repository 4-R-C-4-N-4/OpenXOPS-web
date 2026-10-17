//! @file soundmanager.h
//! @brief SoundManagerクラスの宣言

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

#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#define MAX_SOUNDMGR_LIST 100		//!< サウンドリストの最大数

#define MAX_SOUNDHITMAP 95			//!< マップ着弾音の最大音量
#define MAX_SOUNDHITSMALLOBJ 110	//!< 小物着弾音の最大音量
#define MAX_SOUNDHITHUMAN 75		//!< 人着弾（被弾）音の最大音量
#define MAX_SOUNDPASSING 80			//!< 弾が横切る音の最大音量
#define MAX_SOUNDCCOGRENADE 100		//!< 手榴弾 バウンド・跳ね返り音の最大音量
#define MAX_SOUNDHITGRENADE 120		//!< 手榴弾 爆発音の最大音量

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 2		//!< Select include file.
#endif
#include "main.h"

//! サウンドリスト「paramid」用定数
enum SoundmgrID
{
	SHOT_WEAPON = 0,		//!< 発砲音
	SHOT_WEAPON_PLAYER,		//!< プレイヤー自身の発砲音
	HIT_MAP,				//!< マップ着弾音
	HIT_HUMAN_HEAD,			//!< 被弾音（頭）
	HIT_HUMAN_UP,			//!< 被弾音（上半身）
	HIT_HUMAN_LEG,			//!< 被弾音（下半身）
	HIT_HUMAN_ZOMBIE,		//!< ゾンビ攻撃音
	HIT_SMALLOBJECT,		//!< 小物破壊音
	BULLET,					//!< 銃弾の音・横切る音
	GRE_BOUND,				//!< 手榴弾 バウンド音
	GRE_EXPLOSION,			//!< 手榴弾 爆発音
	FOOTSTEPS_WALK,			//!< 足音・歩き
	FOOTSTEPS_FORWARD,		//!< 足音・走る音（前進）
	FOOTSTEPS_BACK,			//!< 足音・走る音（後退）
	FOOTSTEPS_SIDE,			//!< 足音・走る音（左右）
	FOOTSTEPS_JUMP,			//!< 足音・ジャンプ
	FOOTSTEPS_LANDING,		//!< 足音・着地音
	WEAPON_RELOAD			//!< リロード音
};

//! サウンドリスト用構造体
typedef struct
{
	int paramid;	//!< 音源の種類番号・SoundmgrID定数
	int dataid;		//!< データ番号
	float x;		//!< X座標
	float y;		//!< Y座標
	float z;		//!< Z座標
	float move_x;	//!< 1フレーム後の X座標移動量
	float move_y;	//!< 1フレーム後の Y座標移動量
	float move_z;	//!< 1フレーム後の Z座標移動量
	int teamid;		//!< チーム番号
	int cnt;		//!< カウント値
} soundlist;

//! @brief サウンド管理クラス
//! @details 各サウンドの初期化・計算・再生などを行い管理します。
class SoundManager
{
	class SoundControl *SoundCtrl;		//!< サウンド再生クラス
	class ResourceManager *Resource;	//!< リソース管理クラス
	class ParameterInfo *Param;			//!< 設定値を管理するクラス
	soundlist *soundlistA;		//!< サウンドリスト A
	soundlist *soundlistB;		//!< サウンドリスト B
	bool changeAB;				//!< サウンドリストの反転
	int listAdatas;				//!< サウンドリスト A の登録数
	int listBdatas;				//!< サウンドリスト B の登録数

	bool GetNewList(soundlist **plist);
	int GetTargetList(soundlist **plist);
	bool CheckApproach(soundlist *plist, float camera_x, float camera_y, float camera_z, float *min_x, float *min_y, float *min_z);
	void PlaySound(soundlist *plist, float camera_x, float camera_y, float camera_z, int teamID);

public:
	SoundManager(SoundControl *in_SoundCtrl = NULL, ResourceManager *in_Resource = NULL, ParameterInfo *in_Param = NULL);
	~SoundManager();
	void SetClass(SoundControl *in_SoundCtrl, ResourceManager *in_Resource, ParameterInfo *in_Param);
	void InitWorldSound();
	bool ShotWeapon(float x, float y, float z, int id, int teamID, bool player);
	bool HitMap(float x, float y, float z, int teamID);
	bool HitHuman(float x, float y, float z, int Hit_id, int teamID);
	bool HitSmallObject(float x, float y, float z, int id, int teamID);
	bool PassingBullet(float x, float y, float z, float move_x, float move_y, float move_z, int teamID);
	bool GrenadeBound(float x, float y, float z, int teamID);
	bool GrenadeExplosion(float x, float y, float z, int teamID);
	bool SetFootsteps(float x, float y, float z, int id, int MoveMode, int teamID, int cnt);
	bool ReloadWeapon(float x, float y, float z, int teamID);
	int GetTotalSoundList();
	int GetWorldSound(float pos_x, float pos_y, float pos_z, int teamID, soundlist *psoundlist);
	void PlayWorldSound(float camera_x, float camera_y, float camera_z, float camera_rx, int teamID);
};

#endif