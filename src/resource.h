//! @file resource.h
//! @brief ResourceManagerクラスの宣言

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

#ifndef RESOURCE_H
#define RESOURCE_H

#define TOTAL_UPMODE 6				//!< 上半身の合計モデル
#define TOTAL_ARMMODE 3				//!< 腕の合計モデル
#define TOTAL_WALKMODE 8			//!< 足（歩き）の合計モデル
#define TOTAL_RUNMODE 12			//!< 足（走り）の合計モデル

#define MAX_LOADHUMANTEXTURE 10		//!< 人のテクスチャを読み込む最大枚数

#define ENABLE_HUMANMODEL_MORPHING	//!< 人モデルのモーフィング処理有効化（コメント化で機能無効）

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 2		//!< Select include file.
#endif
#include "main.h"

//! @brief リソース管理クラス
//! @details 主に使用されるモデルやテクスチャおよびサウンドを管理します。　（一部を除く）
class ResourceManager
{
	class ParameterInfo *ParamInfo;				//!< 管理する設定値情報クラス
	class D3DGraphics *d3dg;					//!< 管理する描画クラス
	class SoundControl *SoundCtrl;				//!< 管理するサウンドクラス

	int human_upmodel[TOTAL_UPMODE];			//!< 上半身のモデル
	int human_armmodel[TOTAL_ARMMODE];			//!< 腕のモデル
	int human_legmodel;							//!< 足（静止状態）のモデル
	int human_walkmodel[TOTAL_WALKMODE];		//!< 足（歩き）のモデル
	int human_runmodel[TOTAL_RUNMODE];			//!< 足（走り）のモデル
	int human_texture_Param[MAX_LOADHUMANTEXTURE];		//!< 人のテクスチャの対応番号
	int human_texture_d3dg[MAX_LOADHUMANTEXTURE];		//!< 人のテクスチャのデータ番号
	int weapon_model[TOTAL_PARAMETERINFO_WEAPON];		//!< 武器のモデルのデータ番号
	int weapon_texture[TOTAL_PARAMETERINFO_WEAPON];		//!< 武器のテクスチャのデータ番号
	int weapon_sound[TOTAL_PARAMETERINFO_WEAPON];		//!< 武器のサウンドのデータ番号
	int weapon_reloadsound;								//!< 武器のリロードサウンドのデータ番号
	int smallobject_model[TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT];		//!< 小物のモデルのデータ番号
	int smallobject_texture[TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT];		//!< 小物のテクスチャのデータ番号
	int smallobject_sound[TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT];		//!< 小物のサウンドのデータ番号
	int bullet_model[TOTAL_PARAMETERINFO_BULLET];		//!< 弾のモデル
	int bullet_texture[TOTAL_PARAMETERINFO_BULLET];		//!< 弾のテクスチャ

	int scopetexture;			//!< スコープのテクスチャ
	int skymodel;				//!< 空背景のモデル
	int skytexture;				//!< 空背景のテクスチャ
	int bullet_hitsoundA;		//!< 弾のヒットサウンド（A）
	int bullet_hitsoundB;		//!< 弾のヒットサウンド（B）
	int bullet_humanhitsound;	//!< 弾のヒットサウンド（人）
	int bullet_passingsound;	//!< 弾の通過サウンド
	int grenade_bang;			//!< 手榴弾の爆発音
	int grenade_cco;			//!< 手榴弾のバウンド音
	int effecttexture_blood;	//!< エフェクト用・血テクスチャ
	int effecttexture_mflash;	//!< エフェクト用・フラッシュテクスチャ
	int effecttexture_smoke;	//!< エフェクト用・煙テクスチャ
	int effecttexture_yakkyou;	//!< エフェクト用・薬莢テクスチャ

public:
	ResourceManager();
	~ResourceManager();
	void SetParameterInfo(ParameterInfo *_ParamInfo);
	void SetD3DGraphics(D3DGraphics *_d3dg);
	void SetSoundControl(SoundControl *_SoundCtrl);
	void DestroyResource();

	int LoadHumanModel();
	void GetHumanModel(int out_upmodel[], int out_armmodel[], int *legmodel, int out_walkmodel[], int out_runmodel[]);
	void CleanupHumanModel();
	int AddHumanTexture(int id);
	int GetHumanTextures();
	int GetHumanTexture(int id);
	bool GetHumanTextureInfo(int dataid, int *ParamID, int *TextureID);
	void CleanupHumanTexture();
	int LoadWeaponModelTexture();
	int GetWeaponModelTexture(int id, int *model, int *texture);
#ifdef ENABLE_BUG_HUMANWEAPON
	int GetBugWeaponModelTexture(int id, int *model, int *texture);
#endif
	void CleanupWeaponModelTexture();
	int LoadWeaponSound();
	int GetWeaponSound(int id);
	void CleanupWeaponSound();
	int LoadSmallObjectModelTexture();
	int GetSmallObjectModelTexture(int id, int *model, int *texture);
	void CleanupSmallObjectModelTexture();
	int LoadSmallObjectSound();
	int GetSmallObjectSound(int id);
	void CleanupSmallObjectSound();
	int LoadAddSmallObject(int id, char *modelpath, char *texturepath, char *soundpath);
	void CleanupAddSmallObject(int id);
	int LoadBulletModelTexture();
	int GetBulletModelTexture(int id, int *model, int *texture);
	void CleanupBulletModelTexture();

	int LoadScopeTexture();
	int GetScopeTexture();
	void CleanupScopeTexture();
	int LoadSkyModelTexture(int id);
	void GetSkyModelTexture(int *model, int *texture);
	void CleanupSkyModelTexture();
	int LoadBulletSound();
	void GetBulletSound(int *hitsoundA, int *hitsoundB, int *humanhitsound, int *passingsound, int *grenadebang, int *grenadecco);
	void CleanupBulletSound();
	int LoadEffectTexture();
	int GetEffectBloodTexture();
	int GetEffectMflashTexture();
	int GetEffectSmokeTexture();
	int GetEffectYakkyouTexture();
	void CleanupEffectTexture();
};

#endif