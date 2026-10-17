//! @file resource.cpp
//! @brief ResourceManagerクラスの定義

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

#include "resource.h"

//! @brief コンストラクタ
ResourceManager::ResourceManager()
{
	ParamInfo = NULL;
	d3dg = NULL;
	SoundCtrl = NULL;

	for(int i=0; i<TOTAL_UPMODE; i++){
		human_upmodel[i] = -1;
	}
	for(int i=0; i<TOTAL_ARMMODE; i++){
		human_armmodel[i] = -1;
	}
	human_legmodel = -1;
	for(int i=0; i<TOTAL_WALKMODE; i++){
		human_walkmodel[i] = -1;
	}
	for(int i=0; i<TOTAL_RUNMODE; i++){
		human_runmodel[i] = -1;
	}
	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		human_texture_Param[i] = -1;
		human_texture_d3dg[i] = -1;
	}
	for(int i=0; i<TOTAL_PARAMETERINFO_WEAPON; i++){
		weapon_model[i] = -1;
		weapon_texture[i] = -1;
		weapon_sound[i] = -1;
	}
	weapon_reloadsound = -1;
	for(int i=0; i<(TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT); i++){
		smallobject_model[i] = -1;
		smallobject_texture[i] = -1;
		smallobject_sound[i] = -1;
	}
	for(int i=0; i<TOTAL_PARAMETERINFO_BULLET; i++){
		bullet_model[i] = -1;
		bullet_texture[i] = -1;
	}

	scopetexture = -1;
	skymodel = -1;
	skytexture = -1;
	bullet_hitsoundA = -1;
	bullet_hitsoundB = -1;
	bullet_humanhitsound = -1;
	bullet_passingsound = -1;
	grenade_bang = -1;
	grenade_cco = -1;
	effecttexture_blood = -1;
	effecttexture_mflash = -1;
	effecttexture_smoke = -1;
	effecttexture_yakkyou = -1;
}

//! @brief ディストラクタ
ResourceManager::~ResourceManager()
{
	DestroyResource();
}

//! @brief 参照するParameterInfoクラスを設定
//! @param *_ParamInfo ParameterInfoクラスのポインタ
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void ResourceManager::SetParameterInfo(ParameterInfo *_ParamInfo)
{
	ParamInfo = _ParamInfo;
}

//! @brief 参照するD3DGraphicsクラスを設定
//! @param *_d3dg D3DGraphicsクラスのポインタ
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void ResourceManager::SetD3DGraphics(D3DGraphics *_d3dg)
{
	d3dg = _d3dg;
}

//! @brief 参照するSoundControlクラスを設定
//! @param *_SoundCtrl SoundControlクラスのポインタ
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void ResourceManager::SetSoundControl(SoundControl *_SoundCtrl)
{
	SoundCtrl = _SoundCtrl;
}

//! @brief 解放
//! @attention 本関数は自動的に呼び出されますが、明示的に呼び出すことも可能です。
void ResourceManager::DestroyResource()
{
	CleanupHumanModel();
	CleanupHumanTexture();
	CleanupWeaponModelTexture();
	CleanupWeaponSound();
	CleanupSmallObjectModelTexture();
	CleanupSmallObjectSound();

	CleanupScopeTexture();
	CleanupSkyModelTexture();
	CleanupBulletModelTexture();
	CleanupBulletSound();
	CleanupEffectTexture();
}

//! @brief 人のモデルを読み込み
//! @return 成功：0　失敗：1
int ResourceManager::LoadHumanModel()
{
	if( d3dg == NULL ){ return 1; }

	human_upmodel[0] = d3dg->LoadModel("data\\model\\up0.x");
	human_upmodel[1] = d3dg->LoadModel("data\\model\\up1.x");
	human_upmodel[2] = d3dg->LoadModel("data\\model\\up2.x");
	human_upmodel[3] = d3dg->LoadModel("data\\model\\up3.x");
	human_upmodel[4] = d3dg->LoadModel("data\\model\\up4.x");
	human_upmodel[5] = d3dg->LoadModel("data\\model\\up5.x");
	human_armmodel[0] = d3dg->LoadModel("data\\model\\arm00.x");
	human_armmodel[1] = d3dg->LoadModel("data\\model\\arm01.x");
	human_armmodel[2] = d3dg->LoadModel("data\\model\\arm02.x");
	human_legmodel = d3dg->LoadModel("data\\model\\leg.x");
	human_walkmodel[0] = d3dg->LoadModel("data\\model\\walk01.x");
	human_walkmodel[2] = d3dg->LoadModel("data\\model\\walk02.x");
	human_walkmodel[4] = d3dg->LoadModel("data\\model\\walk03.x");
	human_walkmodel[6] = d3dg->LoadModel("data\\model\\walk04.x");
	human_runmodel[0] = d3dg->LoadModel("data\\model\\run01.x");
	human_runmodel[2] = d3dg->LoadModel("data\\model\\run02.x");
	human_runmodel[4] = d3dg->LoadModel("data\\model\\run03.x");
	human_runmodel[6] = d3dg->LoadModel("data\\model\\run04.x");
	human_runmodel[8] = d3dg->LoadModel("data\\model\\run05.x");
	human_runmodel[10] = d3dg->LoadModel("data\\model\\run06.x");

#ifdef ENABLE_HUMANMODEL_MORPHING
	//モーフィング処理を実行する
	human_walkmodel[1] = d3dg->MorphingModel(human_walkmodel[0], human_walkmodel[2]);
	human_walkmodel[3] = d3dg->MorphingModel(human_walkmodel[2], human_walkmodel[4]);
	human_walkmodel[5] = d3dg->MorphingModel(human_walkmodel[4], human_walkmodel[6]);
	human_walkmodel[7] = d3dg->MorphingModel(human_walkmodel[6], human_walkmodel[0]);
	human_runmodel[1] = d3dg->MorphingModel(human_runmodel[0], human_runmodel[2]);
	human_runmodel[3] = d3dg->MorphingModel(human_runmodel[2], human_runmodel[4]);
	human_runmodel[5] = d3dg->MorphingModel(human_runmodel[4], human_runmodel[6]);
	human_runmodel[7] = d3dg->MorphingModel(human_runmodel[6], human_runmodel[8]);
	human_runmodel[9] = d3dg->MorphingModel(human_runmodel[8], human_runmodel[10]);
	human_runmodel[11] = d3dg->MorphingModel(human_runmodel[10], human_runmodel[0]);
#else
	//モーフィング処理を実行しない
	human_walkmodel[1] = human_walkmodel[0];
	human_walkmodel[3] = human_walkmodel[2];
	human_walkmodel[5] = human_walkmodel[4];
	human_walkmodel[7] = human_walkmodel[6];
	human_runmodel[1] = human_runmodel[0];
	human_runmodel[3] = human_runmodel[2];
	human_runmodel[5] = human_runmodel[4];
	human_runmodel[7] = human_runmodel[6];
	human_runmodel[9] = human_runmodel[8];
	human_runmodel[11] = human_runmodel[10];
#endif

	return 0;
}

//! @brief 人のモデルのデータ番号を取得
//! @param out_upmodel[] 上半身のモデル　（配列数：TOTAL_UPMODE）
//! @param out_armmodel[] 腕のモデル　（配列数：TOTAL_ARMMODE）
//! @param *legmodel 足（静止状態）のモデルのポインタ
//! @param out_walkmodel[] 足（歩き）のモデル　（配列数：TOTAL_WALKMODE）
//! @param out_runmodel[] 足（走り）のモデル　（配列数：TOTAL_RUNMODE）
void ResourceManager::GetHumanModel(int out_upmodel[], int out_armmodel[], int *legmodel, int out_walkmodel[], int out_runmodel[])
{
	for(int i=0; i<TOTAL_UPMODE; i++){
		out_upmodel[i] = human_upmodel[i];
	}
	for(int i=0; i<TOTAL_ARMMODE; i++){
		out_armmodel[i] = human_armmodel[i];
	}
	*legmodel = human_legmodel;
	for(int i=0; i<TOTAL_WALKMODE; i++){
		out_walkmodel[i] = human_walkmodel[i];
	}
	for(int i=0; i<TOTAL_RUNMODE; i++){
		out_runmodel[i] = human_runmodel[i];
	}
}

//! @brief 人のモデルを解放
void ResourceManager::CleanupHumanModel()
{
	if( d3dg == NULL ){ return; }

	for(int i=0; i<TOTAL_UPMODE; i++){
		d3dg->CleanupModel(human_upmodel[i]);
		human_upmodel[i] = -1;
	}
	for(int i=0; i<TOTAL_ARMMODE; i++){
		d3dg->CleanupModel(human_armmodel[i]);
		human_armmodel[i] = -1;
	}
	d3dg->CleanupModel(human_legmodel);
	for(int i=0; i<TOTAL_WALKMODE; i++){
		d3dg->CleanupModel(human_walkmodel[i]);
		human_walkmodel[i] = -1;
	}
	for(int i=0; i<TOTAL_RUNMODE; i++){
		d3dg->CleanupModel(human_runmodel[i]);
		human_runmodel[i] = -1;
	}
}

//! @brief 人のテクスチャを読み込む
//! @param id 人の種類ID
//! @return 成功：0以上　失敗：-1
int ResourceManager::AddHumanTexture(int id)
{
	if( d3dg == NULL ){ return -1; }
	if( (id < 0)||(TOTAL_PARAMETERINFO_HUMAN <= id ) ){ return -1; }

	HumanParameter data;
	char path[MAX_PATH];
	if( ParamInfo->GetHuman(id, &data) == 1 ){ return -1; }

	//既に読み込んだテクスチャかどうか調べる
	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		if( human_texture_Param[i] == data.texture ){
			return -1;
		}
	}

	//テクスチャを読み込む
	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		if( human_texture_Param[i] == -1 ){
			int TexturePathID = data.texture;
			if( ParamInfo->GetHumanTexturePath(TexturePathID, path) == 1 ){ return -1; }
			human_texture_Param[i] = TexturePathID;				//データ番号を記録
			human_texture_d3dg[i] = d3dg->LoadTexture(path, false, false);	//テクスチャを読み込む
			return i;
		}
	}

	return -1;
}

//! @brief 読み込んでいる人のテクスチャ数を取得
//! @return テクスチャ数（最大 MAX_LOADHUMANTEXTURE）
int ResourceManager::GetHumanTextures()
{
	int Textures = 0;

	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		if( human_texture_Param[i] != -1 ){
			Textures += 1;
		}
	}

	return Textures;
}

//! @brief 人のテクスチャ番号を取得
//! @param id 人の種類ID
//! @return 成功：テクスチャID　失敗：-1
int ResourceManager::GetHumanTexture(int id)
{
	if( (id < 0)||(TOTAL_PARAMETERINFO_HUMAN <= id ) ){ return -1; }

	HumanParameter data;
	if( ParamInfo->GetHuman(id, &data) == 1 ){ return -1; }

	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		if( human_texture_Param[i] == data.texture ){
			return human_texture_d3dg[i];
		}
	}

	return -1;
}

//! @brief 読み込んでいる人のテクスチャ情報を取得
//! @param dataid データ番号
//! @param ParamID 人の種類IDを受け取るポインタ（NULL可）
//! @param TextureID テクスチャIDを受け取るポインタ（NULL可）
//! @return 成功：true　失敗：false
//! @attention デバック用の関数です。通常は使用しません。
bool ResourceManager::GetHumanTextureInfo(int dataid, int *ParamID, int *TextureID)
{
	if( (dataid < 0)||(MAX_LOADHUMANTEXTURE <= dataid ) ){ return false; }

	if( ParamID != NULL ){ *ParamID = human_texture_Param[dataid]; }
	if( TextureID != NULL ){ *TextureID = human_texture_d3dg[dataid]; }
	return true;
}

//! @brief 人のテクスチャを一括解放
void ResourceManager::CleanupHumanTexture()
{
	if( d3dg == NULL ){ return; }

	for(int i=0; i<MAX_LOADHUMANTEXTURE; i++){
		if( human_texture_Param[i] != -1 ){
			d3dg->CleanupTexture(human_texture_d3dg[i]);
			human_texture_Param[i] = -1;
			human_texture_d3dg[i] = -1;
		}
	}
}

//! @brief 武器のモデルやテクスチャを一括読み込み
//! @return 成功：0　失敗：1以上
int ResourceManager::LoadWeaponModelTexture()
{
	int cnt = 0;

	if( d3dg == NULL ){ return 1; }

	for(int i=0; i<TOTAL_PARAMETERINFO_WEAPON; i++){
		WeaponParameter data;
		if( ParamInfo->GetWeapon(i, &data) == 0 ){
			//モデルとテクスチャを読み込み、エラーが出ればカウントする。
			weapon_model[i] = d3dg->LoadModel(data.model);
			weapon_texture[i] = d3dg->LoadTexture(data.texture, false, false);
			if( weapon_model[i] == -1 ){ cnt += 1; }
			if( weapon_texture[i] == -1 ){ cnt += 1; }
		}
		else{
			//設定データがおかしければ、モデルとテクスチャ 2つともエラー。
			cnt += 2;
		}
	}

	return cnt;
}

//! @brief 武器のモデルとテクスチャを取得
//! @return 成功：0　失敗：1
int ResourceManager::GetWeaponModelTexture(int id, int *model, int *texture)
{
	if( model == NULL ){ return 1; }
	if( texture == NULL ){ return 1; }

	if( (id < 0)||(TOTAL_PARAMETERINFO_WEAPON <= id ) ){
#ifdef ENABLE_BUG_HUMANWEAPON
		return GetBugWeaponModelTexture(id, model, texture);
#else
		return 1;
#endif
	}

	*model = weapon_model[id];
	*texture = weapon_texture[id];
	return 0;
}

#ifdef ENABLE_BUG_HUMANWEAPON
//! @brief バグ武器のモデルとテクスチャを取得
//! @return 成功：0　失敗：1
//! @warning 先に GetWeaponModelTexture()関数 を実行して取得に失敗した時に限り、この関数を使ってください。
//! @attention バグ武器を追加する場合は、ParameterInfoクラスの GetBugWeapon() 関数も編集してください。
int ResourceManager::GetBugWeaponModelTexture(int id, int *model, int *texture)
{
	if( d3dg == NULL ){ return 1; }
	if( model == NULL ){ return 1; }
	if( texture == NULL ){ return 1; }

	if( id == 23 ){
		*model = human_upmodel[0];
		*texture = d3dg->GetMapTextureID(0);
		return 0;
	}
	if( id == 24 ){
		*model = human_upmodel[0];
		*texture = d3dg->GetMapTextureID(3);
		return 0;
	}
	if( id == 30 ){
		*model = human_upmodel[0];
		*texture = d3dg->GetMapTextureID(2);
		return 0;
	}
	if( id == 53 ){
		*model = human_upmodel[0];
		*texture = d3dg->GetMapTextureID(8);
		return 0;
	}

	return 1;
}
#endif

//! @brief 武器のモデルやテクスチャを一括解放
void ResourceManager::CleanupWeaponModelTexture()
{
	if( d3dg == NULL ){ return; }

	for(int i=0; i<TOTAL_PARAMETERINFO_WEAPON; i++){
		d3dg->CleanupModel(weapon_model[i]);
		d3dg->CleanupTexture(weapon_texture[i]);
		weapon_model[i] = -1;
		weapon_texture[i] = -1;
	}
}

//! @brief 武器のサウンドを読み込む
//! @return 成功：0　失敗：1
int ResourceManager::LoadWeaponSound()
{
	if( SoundCtrl == NULL ){ return 1; }

	int soundid[6];
	int reloadid;

	//サウンドファイルをまとめて読み込む
	soundid[0] = SoundCtrl->LoadSound("data\\sound\\bang1.wav");
	soundid[1] = SoundCtrl->LoadSound("data\\sound\\bang2.wav");
	soundid[2] = SoundCtrl->LoadSound("data\\sound\\bang3.wav");
	soundid[3] = SoundCtrl->LoadSound("data\\sound\\bang4.wav");
	soundid[4] = SoundCtrl->LoadSound("data\\sound\\bang5.wav");
	soundid[5] = SoundCtrl->LoadSound("data\\sound\\ka.wav");
	//reloadid = SoundCtrl->LoadSound("reload.wav");
	reloadid = -1;

	for(int i=0; i<TOTAL_PARAMETERINFO_WEAPON; i++){
		WeaponParameter data;
		if( ParamInfo->GetWeapon(i, &data) == 0 ){
			//サウンド番号に合わせて、効果音を割り当てる。
			if( data.soundid == 0 ){
				weapon_sound[i] = soundid[0];
			}
			else if( data.soundid == 1 ){
				weapon_sound[i] = soundid[1];
			}
			else if( data.soundid == 2 ){
				weapon_sound[i] = soundid[2];
			}
			else if( data.soundid == 3 ){
				weapon_sound[i] = soundid[3];
			}
			else if( data.soundid == 4 ){
				weapon_sound[i] = soundid[4];
			}
			else if( data.soundid == 13 ){
				weapon_sound[i] = soundid[5];
			}
			else{
				weapon_sound[i] = -1;
			}
		}
	}
	weapon_reloadsound = reloadid;

	//ファイル読み込み時にエラーが出なかったか調べる
	for(int i=0; i<6; i++){
		if( soundid[i] == -1 ){ return 1; }
	}
	if( reloadid == -1 ){ return 1; }
	return 0;
}

//! @brief 武器のサウンドを取得
//! @param id 0以上で武器のデータ番号、-1でリロード音
//! @return 成功：データ番号　失敗：-1
int ResourceManager::GetWeaponSound(int id)
{
	if( SoundCtrl == NULL ){ return -1; }
	if( id == -1 ){ return weapon_reloadsound; }

	if( (id < 0)||(TOTAL_PARAMETERINFO_WEAPON <= id ) ){ return -1; }
	return weapon_sound[id];
}

//! @brief 武器のサウンドを一括解放
void ResourceManager::CleanupWeaponSound()
{
	if( SoundCtrl == NULL ){ return; }

	for(int i=0; i<TOTAL_PARAMETERINFO_WEAPON; i++){
		SoundCtrl->CleanupSound(weapon_sound[i]);
		weapon_sound[i] = -1;
	}
	SoundCtrl->CleanupSound(weapon_reloadsound);
	weapon_reloadsound = -1;
}

//! @brief 小物のモデルやテクスチャを一括読み込み
//! @return 成功：0　失敗：1以上
int ResourceManager::LoadSmallObjectModelTexture()
{
	int cnt = 0;

	if( d3dg == NULL ){ return 1; }

	for(int i=0; i<TOTAL_PARAMETERINFO_SMALLOBJECT; i++){
		SmallObjectParameter data;
		if( ParamInfo->GetSmallObject(i, &data) == 0 ){
			//モデルとテクスチャを読み込み、エラーが出ればカウントする。
			smallobject_model[i] = d3dg->LoadModel(data.model);
			smallobject_texture[i] = d3dg->LoadTexture(data.texture, false, false);
			if( smallobject_model[i] == -1 ){ cnt += 1; }
			if( smallobject_texture[i] == -1 ){ cnt += 1; }
		}
		else{
			//設定データがおかしければ、モデルとテクスチャ 2つともエラー。
			cnt += 2;
		}
	}

	return cnt;
}

//! @brief 小物のモデルとテクスチャを取得
//! @return 成功：0　失敗：1
int ResourceManager::GetSmallObjectModelTexture(int id, int *model, int *texture)
{
	if( model == NULL ){ return 1; }
	if( texture == NULL ){ return 1; }
	if( (id < 0)||((TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT) <= id ) ){ return 1; }

	*model = smallobject_model[id];
	*texture = smallobject_texture[id];
	return 0;
}

//! @brief 小物のモデルやテクスチャを一括解放
void ResourceManager::CleanupSmallObjectModelTexture()
{
	if( d3dg == NULL ){ return; }

	for(int i=0; i<(TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT); i++){
		d3dg->CleanupModel(smallobject_model[i]);
		d3dg->CleanupTexture(smallobject_texture[i]);
		smallobject_model[i] = -1;
		smallobject_texture[i] = -1;
	}
}

//! @brief 小物のサウンドを読み込む
//! @return 成功：0　失敗：1
int ResourceManager::LoadSmallObjectSound()
{
	if( SoundCtrl == NULL ){ return 1; }

	int soundid[2];

	//サウンドファイルをまとめて読み込む
	soundid[0] = SoundCtrl->LoadSound("data\\sound\\can.wav");
	soundid[1] = SoundCtrl->LoadSound("data\\sound\\dan.wav");

	for(int i=0; i<TOTAL_PARAMETERINFO_SMALLOBJECT; i++){
		SmallObjectParameter data;
		if( ParamInfo->GetSmallObject(i, &data) == 0 ){
			//サウンド番号に合わせて、効果音を割り当てる。
			if( data.sound == 0 ){
				smallobject_sound[i] = soundid[0];
			}
			else if( data.sound == 1 ){
				smallobject_sound[i] = soundid[1];
			}
			else{
				smallobject_sound[i] = -1;
			}
		}
	}

	//ファイル読み込み時にエラーが出なかったか調べる
	for(int i=0; i<2; i++){
		if( soundid[i] == -1 ){ return 1; }
	}
	return 0;
}

//! @brief 小物のサウンドを取得
//! @param id 小物のデータ番号
//! @return 成功：データ番号　失敗：-1
int ResourceManager::GetSmallObjectSound(int id)
{
	if( SoundCtrl == NULL ){ return -1; }
	if( (id < 0)||((TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT) <= id ) ){ return -1; }
	return smallobject_sound[id];
}

//! @brief 小物のサウンドを一括解放
void ResourceManager::CleanupSmallObjectSound()
{
	if( SoundCtrl == NULL ){ return; }

	for(int i=0; i<(TOTAL_PARAMETERINFO_SMALLOBJECT + MAX_ADDSMALLOBJECT); i++){
		SoundCtrl->CleanupSound(smallobject_sound[i]);
		smallobject_sound[i] = -1;
	}
}

//! @brief 追加小物のモデル・テクスチャ・サウンドを取得
//! @param id データ番号（通常は 0 固定）
//! @param modelpath モデルデータのパス
//! @param texturepath テクスチャデータのパス
//! @param soundpath サウンドデータのパス
//! @return 成功：0　失敗：1以上
int ResourceManager::LoadAddSmallObject(int id, char *modelpath, char *texturepath, char *soundpath)
{
	if( d3dg == NULL ){ return 1; }
	if( SoundCtrl == NULL ){ return 1; }
	if( modelpath == NULL ){ return 1; }
	if( texturepath == NULL ){ return 1; }
	if( soundpath == NULL ){ return 1; }
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return 1; }

	int dataid = TOTAL_PARAMETERINFO_SMALLOBJECT + id;
	int cnt = 0;

	smallobject_model[dataid] = d3dg->LoadModel(modelpath);
	smallobject_texture[dataid] = d3dg->LoadTexture(texturepath, false, false);
	if( smallobject_model[dataid] == -1 ){ cnt += 1; }
	if( smallobject_texture[dataid] == -1 ){ cnt += 1; }
	smallobject_sound[dataid] = SoundCtrl->LoadSound(soundpath);
	if( smallobject_sound[dataid] == -1 ){ cnt += 1; }

	return cnt;
}

//! @brief 追加小物のモデル・テクスチャ・サウンドを解放
//! @param id データ番号（通常は 0 固定）
void ResourceManager::CleanupAddSmallObject(int id)
{
	if( d3dg == NULL ){ return; }
	if( SoundCtrl == NULL ){ return; }
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return; }

	int dataid = TOTAL_PARAMETERINFO_SMALLOBJECT + id;

	d3dg->CleanupModel(smallobject_model[dataid]);
	d3dg->CleanupTexture(smallobject_texture[dataid]);
	SoundCtrl->CleanupSound(smallobject_sound[dataid]);
}

//! @brief 弾のモデルとテクスチャを読み込む
//! @return 成功：0　失敗：1以上
int ResourceManager::LoadBulletModelTexture()
{
	int cnt = 0;

	if( d3dg == NULL ){ return 1; }

	for(int i=0; i<TOTAL_PARAMETERINFO_BULLET; i++){
		BulletParameter data;
		if( ParamInfo->GetBullet(i, &data) == 0 ){
			//モデルとテクスチャを読み込み、エラーが出ればカウントする。
			bullet_model[i] = d3dg->LoadModel(data.model);
			bullet_texture[i] = d3dg->LoadTexture(data.texture, false, false);
			if( bullet_model[i] == -1 ){ cnt += 1; }
			if( bullet_texture[i] == -1 ){ cnt += 1; }
		}
		else{
			//設定データがおかしければ、モデルとテクスチャ 2つともエラー。
			cnt += 2;
		}
	}

	return cnt;
}

//! @brief 弾のモデルとテクスチャを取得
//! @return 成功：0　失敗：1
int ResourceManager::GetBulletModelTexture(int id, int *model, int *texture)
{
	if( model == NULL ){ return 1; }
	if( texture == NULL ){ return 1; }
	if( (id < 0)||(TOTAL_PARAMETERINFO_BULLET <= id ) ){ return 1; }

	*model = bullet_model[id];
	*texture = bullet_texture[id];

	return 0;
}

//! @brief 弾のモデルとテクスチャを解放
void ResourceManager::CleanupBulletModelTexture()
{
	if( d3dg == NULL ){ return; }

	for(int i=0; i<TOTAL_PARAMETERINFO_BULLET; i++){
		d3dg->CleanupModel(bullet_model[i]);
		bullet_model[i] = -1;

		d3dg->CleanupTexture(bullet_texture[i]);
		bullet_texture[i] = -1;
	}
}

//! @brief スコープテクスチャを読み込む
//! @return 成功：0　失敗：1
int ResourceManager::LoadScopeTexture()
{
	if( d3dg == NULL ){ return 1; }

	scopetexture = d3dg->LoadTexture("data\\scope.dds", false, false);
	if( scopetexture == -1 ){ return 1; }
	return 0;
}

//! @brief スコープテクスチャを取得
//! @return テクスチャのデータ番号
int ResourceManager::GetScopeTexture()
{
	return scopetexture;
}

//! @brief スコープテクスチャを解放
void ResourceManager::CleanupScopeTexture()
{
	if( d3dg == NULL ){ return; }
	if( scopetexture == -1 ){ return; }

	d3dg->CleanupTexture(scopetexture);
	scopetexture = -1;
}

//! @brief 背景空のテクスチャを読み込む
//! @param id 空の番号（1～5）
//! @return 成功：0　失敗：1
int ResourceManager::LoadSkyModelTexture(int id)
{
	if( d3dg == NULL ){ return 1; }
	if( (id < 1)||(5 < id) ){ return 1; }

	//モデルを読み込む（固定）
	skymodel = d3dg->LoadModel("data\\sky\\sky.x");

	//テクスチャ名を生成して読み込む
	char path[MAX_PATH];
	sprintf(path, "data\\sky\\sky%d.bmp", id);
	skytexture = d3dg->LoadTexture(path, false, false);

	if( skymodel == -1 ){ return 1; }
	if( skytexture == -1 ){ return 1; }
	return 0;
}

//! @brief 背景空のモデルとテクスチャを取得
void ResourceManager::GetSkyModelTexture(int *model, int *texture)
{
	if( model == NULL ){ return; }
	if( texture == NULL ){ return; }

	*model = skymodel;
	*texture = skytexture;
}

//! @brief 背景空のモデルとテクスチャを解放
void ResourceManager::CleanupSkyModelTexture()
{
	if( d3dg == NULL ){ return; }

	if( skymodel != -1 ){
		d3dg->CleanupModel(skymodel);
		skymodel = -1;
	}
	if( skytexture != -1 ){
		d3dg->CleanupTexture(skytexture);
		skytexture = -1;
	}
}

//! @brief 弾・手榴弾のサウンドを読み込む
//! @return 成功：0　失敗：1
int ResourceManager::LoadBulletSound()
{
	if( SoundCtrl == NULL ){ return 1; }

	bullet_hitsoundA = SoundCtrl->LoadSound("data\\sound\\hit1.wav");
	bullet_hitsoundB = SoundCtrl->LoadSound("data\\sound\\hit3.wav");
	bullet_humanhitsound = SoundCtrl->LoadSound("data\\sound\\hit2.wav");
	bullet_passingsound = SoundCtrl->LoadSound("data\\sound\\hyu.wav");
	grenade_bang = SoundCtrl->LoadSound("data\\sound\\bang.wav");
	grenade_cco = SoundCtrl->LoadSound("data\\sound\\cco.wav");

	if( bullet_hitsoundA == -1 ){ return 1; }
	if( bullet_hitsoundB == -1 ){ return 1; }
	if( bullet_humanhitsound == -1 ){ return 1; }
	if( bullet_passingsound == -1 ){ return 1; }
	if( grenade_bang == -1 ){ return 1; }
	if( grenade_cco == -1 ){ return 1; }
	return 0;
}

//! @brief 弾・手榴弾のサウンドを取得
//! @param hitsoundA 弾ヒットサウンド：A　（NULL可）
//! @param hitsoundB 弾ヒットサウンド：B　（NULL可）
//! @param humanhitsound 弾ヒットサウンド：人　（NULL可）
//! @param passingsound 弾通過サウンド　（NULL可）
//! @param grenadebang 手榴弾 爆発サウンド　（NULL可）
//! @param grenadecco 手榴弾バウンド サウンド　（NULL可）
void ResourceManager::GetBulletSound(int *hitsoundA, int *hitsoundB, int *humanhitsound, int *passingsound, int *grenadebang, int *grenadecco)
{
	if( SoundCtrl == NULL ){ return; }

	if( hitsoundA != NULL ){ *hitsoundA = bullet_hitsoundA; }
	if( hitsoundB != NULL ){ *hitsoundB = bullet_hitsoundB; }
	if( humanhitsound != NULL ){ *humanhitsound = bullet_humanhitsound; }
	if( passingsound != NULL ){ *passingsound = bullet_passingsound; }
	if( grenadebang != NULL ){ *grenadebang = grenade_bang; }
	if( grenadecco != NULL ){ *grenadecco = grenade_cco; }
}

//! @brief 弾・手榴弾のサウンドを一括解放
void ResourceManager::CleanupBulletSound()
{
	if( SoundCtrl == NULL ){ return; }

	SoundCtrl->CleanupSound(bullet_hitsoundA);
	SoundCtrl->CleanupSound(bullet_hitsoundB);
	SoundCtrl->CleanupSound(bullet_humanhitsound);
	SoundCtrl->CleanupSound(bullet_passingsound);
	SoundCtrl->CleanupSound(grenade_bang);
	SoundCtrl->CleanupSound(grenade_cco);

	bullet_hitsoundA = -1;
	bullet_hitsoundB = -1;
	bullet_humanhitsound = -1;
	bullet_passingsound = -1;
	grenade_bang = -1;
	grenade_cco = -1;
}

//! @brief エフェクトのテクスチャを読み込む
//! @return 成功：0　失敗：1
int ResourceManager::LoadEffectTexture()
{
	if( d3dg == NULL ){ return 1; }

	effecttexture_blood = d3dg->LoadTexture("data\\blood.dds", false, false);
	effecttexture_mflash = d3dg->LoadTexture("data\\mflash.dds", false, false);
	effecttexture_smoke = d3dg->LoadTexture("data\\smoke.dds", false, false);
	effecttexture_yakkyou = d3dg->LoadTexture("data\\yakkyou.dds", false, false);

	if( effecttexture_blood == -1 ){ return 1; }
	if( effecttexture_mflash == -1 ){ return 1; }
	if( effecttexture_smoke == -1 ){ return 1; }
	if( effecttexture_yakkyou == -1 ){ return 1; }
	return 0;
}

//! @brief 血テクスチャ取得
//! @return テクスチャのデータ番号
int ResourceManager::GetEffectBloodTexture()
{
	return effecttexture_blood;
}

//! @brief フラッシュテクスチャ取得
//! @return テクスチャのデータ番号
int ResourceManager::GetEffectMflashTexture()
{
	return effecttexture_mflash;
}

//! @brief 煙テクスチャ取得
//! @return テクスチャのデータ番号
int ResourceManager::GetEffectSmokeTexture()
{
	return effecttexture_smoke;
}

//! @brief 薬莢テクスチャ取得
//! @return テクスチャのデータ番号
int ResourceManager::GetEffectYakkyouTexture()
{
	return effecttexture_yakkyou;
}

//! @brief エフェクトのテクスチャを解放
void ResourceManager::CleanupEffectTexture()
{
	if( d3dg == NULL ){ return; }

	if( effecttexture_blood != -1 ){
		d3dg->CleanupTexture(effecttexture_blood);
		effecttexture_blood = -1;
	}
	if( effecttexture_mflash != -1 ){
		d3dg->CleanupTexture(effecttexture_mflash);
		effecttexture_mflash = -1;
	}
	if( effecttexture_smoke != -1 ){
		d3dg->CleanupTexture(effecttexture_smoke);
		effecttexture_smoke = -1;
	}
	if( effecttexture_yakkyou != -1 ){
		d3dg->CleanupTexture(effecttexture_yakkyou);
		effecttexture_yakkyou = -1;
	}
}