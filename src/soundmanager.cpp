//! @file soundmanager.cpp
//! @brief SoundManagerクラスの定義

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

#include "soundmanager.h"

//! @brief コンストラクタ
SoundManager::SoundManager(SoundControl *in_SoundCtrl, ResourceManager *in_Resource, ParameterInfo *in_Param)
{
	SoundCtrl = in_SoundCtrl;
	Resource = in_Resource;
	Param = in_Param;
	soundlistA = new soundlist[MAX_SOUNDMGR_LIST];
	soundlistB = new soundlist[MAX_SOUNDMGR_LIST];
	changeAB = false;
	listAdatas = 0;
	listBdatas = 0;
}

//! @brief ディストラクタ
SoundManager::~SoundManager()
{
	if( soundlistA != NULL ){ delete [] soundlistA; }
	if( soundlistB != NULL ){ delete [] soundlistB; }
}

//! @brief 使用するクラスを設定
//! @param in_SoundCtrl サウンドコントロールクラス
//! @param in_Resource リソース管理クラス
//! @param in_Param 設定値管理クラス
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void SoundManager::SetClass(SoundControl *in_SoundCtrl, ResourceManager *in_Resource, ParameterInfo *in_Param)
{
	SoundCtrl = in_SoundCtrl;
	Resource = in_Resource;
	Param = in_Param;
}

//! @brief 空間の音源を初期化
void SoundManager::InitWorldSound()
{
	changeAB = false;
	listAdatas = 0;
	listBdatas = 0;
}

//! @brief 空間に発砲音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param id 武器の種類番号
//! @param teamID チーム番号
//! @param player プレイヤーによる発砲かどうか（trueでプレイヤー）
//! @return 成功：true　失敗：false
bool SoundManager::ShotWeapon(float x, float y, float z, int id, int teamID, bool player)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	if( player == true ){
		plist->paramid = SHOT_WEAPON_PLAYER;
	}
	else{
		plist->paramid = SHOT_WEAPON;
	}
	plist->dataid = id;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間にマップ着弾音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::HitMap(float x, float y, float z, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = HIT_MAP;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間に被弾音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param Hit_id 被弾箇所（頭：0　胴体：1　足：2　ゾンビ攻撃：3）
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::HitHuman(float x, float y, float z, int Hit_id, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	if( (Hit_id < 0)||(3 < Hit_id) ){ return false; }

	if( Hit_id == 0 ){ plist->paramid = HIT_HUMAN_HEAD; }
	if( Hit_id == 1 ){ plist->paramid = HIT_HUMAN_UP; }
	if( Hit_id == 2 ){ plist->paramid = HIT_HUMAN_LEG; }
	if( Hit_id == 3 ){ plist->paramid = HIT_HUMAN_ZOMBIE; }
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間に小物着弾音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param id 小物の種類番号
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::HitSmallObject(float x, float y, float z, int id, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = HIT_SMALLOBJECT;
	plist->dataid = id;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 弾の通過・横切る音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param move_x 音源のX軸移動量
//! @param move_y 音源のY軸移動量
//! @param move_z 音源のZ軸移動量
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
//! @attention move_x・move_y・move_zの移動量は、1フレーム分を指定してください。実際に座標が移動するわけではありません。
bool SoundManager::PassingBullet(float x, float y, float z, float move_x, float move_y, float move_z, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = BULLET;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->move_x = move_x;
	plist->move_y = move_y;
	plist->move_z = move_z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間に手榴弾バウンド・跳ね返り音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::GrenadeBound(float x, float y, float z, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = GRE_BOUND;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間に手榴弾爆発音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::GrenadeExplosion(float x, float y, float z, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = GRE_EXPLOSION;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間に足音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param id 足音の種類番号
//! @param MoveMode 移動モード（歩き：0　前進：1　後退：2　左右：3　ジャンプ：4　着地：5）
//! @param teamID チーム番号
//! @param cnt カウント値
//! @return 成功：true　失敗：false
bool SoundManager::SetFootsteps(float x, float y, float z, int id, int MoveMode, int teamID, int cnt)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	switch(MoveMode){
		case 0: plist->paramid = FOOTSTEPS_WALK; break;
		case 1: plist->paramid = FOOTSTEPS_FORWARD; break;
		case 2: plist->paramid = FOOTSTEPS_BACK; break;
		case 3: plist->paramid = FOOTSTEPS_SIDE; break;
		case 4: plist->paramid = FOOTSTEPS_JUMP; break;
		case 5: plist->paramid = FOOTSTEPS_LANDING; break;
		default: return false;
	}
	plist->dataid = id;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;
	plist->cnt = cnt;

	return true;
}

//! @brief 空間に武器リロード音を追加
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param teamID チーム番号
//! @return 成功：true　失敗：false
bool SoundManager::ReloadWeapon(float x, float y, float z, int teamID)
{
	soundlist *plist = NULL;
	if( GetNewList(&plist) == false ){ return false; }

	plist->paramid = WEAPON_RELOAD;
	plist->x = x;
	plist->y = y;
	plist->z = z;
	plist->teamid = teamID;

	return true;
}

//! @brief 空間（サウンドリスト）上で有効な音源数を取得
//! @return 有効な音源数
int SoundManager::GetTotalSoundList()
{
	if( changeAB == false ){
		return listBdatas;
	}
	//else{
		return listAdatas;
	//}
}

//! @brief 指定した位置の周辺にある音源を取得
//! @param pos_x 音源のX座標
//! @param pos_y 音源のY座標
//! @param pos_z 音源のZ座標
//! @param teamID チーム番号
//! @param psoundlist サウンドリストを受け取るポインタ
//! @return 返す（サウンドリストの）音源の数
int SoundManager::GetWorldSound(float pos_x, float pos_y, float pos_z, int teamID, soundlist *psoundlist)
{
	if( psoundlist == NULL ){ return 0; }

	int lists;
	soundlist *getlist = NULL;
	int newlists = 0;
	WeaponParameter WParam;

	//処理するリストを決定
	lists = GetTargetList(&getlist);

	for(int i=0; i<lists; i++){
		float x, y, z;
		float maxdist = 20.0f;	//判定距離の標準値

		//音源の種類により判定距離を変える
		if( (getlist[i].paramid == SHOT_WEAPON)||(getlist[i].paramid == SHOT_WEAPON_PLAYER) ){
			Param->GetWeapon(getlist[i].dataid, &WParam);

			if( getlist[i].teamid == teamID ){ maxdist = 20.0f; }
			else if( WParam.silencer == true ){ maxdist = 30.0f; }
			else{ maxdist = 120.0f; }
		}
		if( getlist[i].paramid == HIT_MAP ){
			maxdist = 30.0f;
		}
		if( getlist[i].paramid == HIT_HUMAN_HEAD ){
			maxdist = 60.0f;
		}
		if( getlist[i].paramid == HIT_HUMAN_UP ){
			maxdist = 50.0f;
		}
		if( getlist[i].paramid == HIT_HUMAN_LEG ){
			maxdist = 40.0f;
		}
		if( getlist[i].paramid == HIT_HUMAN_ZOMBIE ){
			maxdist = 40.0f;
		}
		if( getlist[i].paramid == FOOTSTEPS_WALK ){
			continue;
		}
		if( getlist[i].paramid == FOOTSTEPS_FORWARD ){
			if( getlist[i].teamid == teamID ){ continue; }
			else{ maxdist = 40.0f; }
		}
		if( getlist[i].paramid == FOOTSTEPS_BACK ){
			if( getlist[i].teamid == teamID ){ continue; }
			else{ maxdist = 30.0f; }
		}
		if( getlist[i].paramid == FOOTSTEPS_SIDE ){
			if( getlist[i].teamid == teamID ){ continue; }
			else{ maxdist = 35.0f; }
		}
		if( (getlist[i].paramid == FOOTSTEPS_JUMP)||(getlist[i].paramid == FOOTSTEPS_LANDING) ){
			continue;
		}
		if( getlist[i].paramid == GRE_BOUND ){
			continue;
		}
		if( getlist[i].paramid == GRE_EXPLOSION ){
			maxdist = 80.0f;
		}
		if( getlist[i].paramid == WEAPON_RELOAD ){
			continue;
		}

#ifdef ENABLE_BUG_TEAMID
		//チーム番号が負数、かつ音源のチーム番号が大きいなら、音源を無視
		if( (teamID < 0)&&(getlist[i].teamid < 0) ){
			if( teamID < getlist[i].teamid ){
				continue;
			}
		}
#endif

		//銃弾ならば
		if( getlist[i].paramid == BULLET ){
			float min_x, min_y, min_z;

			//銃弾が最も近づく座標を算出
			if( CheckApproach(&(getlist[i]), pos_x, pos_y, pos_z, &min_x, &min_y, &min_z) == false ){ continue; }

			//音源との距離が範囲内ならば～
			x = min_x - pos_x;
			y = min_y - pos_y;
			z = min_z - pos_z;
			if( x*x + y*y + z*z < maxdist*maxdist ){
				//出力先のリストに追加
				psoundlist[newlists].paramid = getlist[i].paramid;
				psoundlist[newlists].dataid = getlist[i].dataid;
				psoundlist[newlists].x = min_x;
				psoundlist[newlists].y = min_y;
				psoundlist[newlists].z = min_z;
				newlists += 1;
			}
		}
		else{
			//音源との距離が範囲内ならば～
			x = getlist[i].x - pos_x;
			y = getlist[i].y - pos_y;
			z = getlist[i].z - pos_z;
			if( x*x + y*y + z*z < maxdist*maxdist ){
				//出力先のリストに追加
				psoundlist[newlists].paramid = getlist[i].paramid;
				psoundlist[newlists].dataid = getlist[i].dataid;
				psoundlist[newlists].x = getlist[i].x;
				psoundlist[newlists].y = getlist[i].y;
				psoundlist[newlists].z = getlist[i].z;
				newlists += 1;
			}
		}
	}

	return newlists;
}

//! @brief 空間上のサウンドを再生
//! @param camera_x カメラのX座標
//! @param camera_y カメラのY座標
//! @param camera_z カメラのZ座標
//! @param camera_rx カメラのX軸角度　（予約）
//! @param teamID チーム番号
//! @warning 毎フレーム呼び出してください。
void SoundManager::PlayWorldSound(float camera_x, float camera_y, float camera_z, float camera_rx, int teamID)
{
	int lists;
	soundlist *getlist = NULL;

	//フラグを切り替える
	if( changeAB == false ){
		listBdatas = 0;
		changeAB = true;
	}
	else{
		listAdatas = 0;
		changeAB = false;
	}

	//カメラ座標を設定
	SoundCtrl->SetCamera(camera_x, camera_y, camera_z, camera_rx);

	//処理するリストを決定
	lists = GetTargetList(&getlist);

	for(int i=0; i<lists; i++){
		float x = getlist[i].x - camera_x;
		float y = getlist[i].y - camera_y;
		float z = getlist[i].z - camera_z;

		//範囲内の音源ならば再生を試みる
		if( x*x + y*y + z*z < MAX_SOUNDDIST*MAX_SOUNDDIST ){
			PlaySound( &(getlist[i]), camera_x, camera_y, camera_z, teamID );
		}
	}
}

//! @brief 新しいサウンドリスト（単一）を取得
//! @param plist 新しいサウンドリスト（単一）の二重ポインタ
//! @return 成功：true　失敗：false
bool SoundManager::GetNewList(soundlist **plist)
{
	if( changeAB == false ){
		if( (listAdatas + 1) >= MAX_SOUNDMGR_LIST ){
			return false;
		}
		*plist = &(soundlistA[listAdatas]);
		listAdatas += 1;
	}
	else{
		if( (listBdatas + 1) >= MAX_SOUNDMGR_LIST ){
			return false;
		}
		*plist = &(soundlistB[listBdatas]);
		listBdatas += 1;
	}

	return true;
}

//! @brief 処理対象のサウンドリストを取得
//! @param plist 処理対象のサウンドリストの二重ポインタ
//! @return 取得したサウンドリストに含まれる音源の数
int SoundManager::GetTargetList(soundlist **plist)
{
	if( changeAB == false ){
		*plist = soundlistB;
		return listBdatas;
	}
	//else{
		*plist = soundlistA;
		return listAdatas;
	//}
}

//! @brief 音源がカメラ（視点）に最も近づくか確認
//! @param plist 処理対象のサウンドリスト（単一）のポインタ
//! @param camera_x カメラのX座標
//! @param camera_y カメラのY座標
//! @param camera_z カメラのZ座標
//! @param min_x 最短距離のX座標を受け取るポインタ
//! @param min_y 最短距離のY座標を受け取るポインタ
//! @param min_z 最短距離のZ座標を受け取るポインタ
//! @return 通り過ぎた：true　通り過ぎていない：false
bool SoundManager::CheckApproach(soundlist *plist, float camera_x, float camera_y, float camera_z, float *min_x, float *min_y, float *min_z)
{
	float x, y, z;
	float dist1, dist2, dist3;

	//1フレーム前の距離
	x = camera_x - (plist->x - plist->move_x);
	y = camera_y - (plist->y - plist->move_y);
	z = camera_z - (plist->z - plist->move_z);
	dist1 = x*x + y*y + z*z;

	//現在位置の距離
	x = camera_x - plist->x;
	y = camera_y - plist->y;
	z = camera_z - plist->z;
	dist2 = x*x + y*y + z*z;

	//1フレーム後の距離
	x = camera_x - (plist->x + plist->move_x);
	y = camera_y - (plist->y + plist->move_y);
	z = camera_z - (plist->z + plist->move_z);
	dist3 = x*x + y*y + z*z;

	//現在位置の距離が最も近ければ～
	if( (dist1 > dist2)&&(dist2 < dist3) ){
		float speed;
		float min_dist, dist;

		//移動速度を求める
		speed = sqrtf(plist->move_x*plist->move_x + plist->move_y*plist->move_y + plist->move_z*plist->move_z);

		//最短距離の座標を求める
		min_dist = DistancePosRay(camera_x, camera_y, camera_z, plist->x, plist->y, plist->z, plist->move_x/speed, plist->move_y/speed, plist->move_z/speed);

		//最短距離時の座標を求める
		dist = sqrtf(dist2 - min_dist*min_dist);
		*min_x = plist->x + plist->move_x/speed * dist;
		*min_y = plist->y + plist->move_y/speed * dist;
		*min_z = plist->z + plist->move_z/speed * dist;

		return true;
	}

	return false;
}

//! @brief 指定したサウンドリスト（1音源）を再生
//! @param plist 再生するサウンドリスト（単一）のポインタ
//! @param camera_x カメラのX座標
//! @param camera_y カメラのY座標
//! @param camera_z カメラのZ座標
//! @param teamID チーム番号
void SoundManager::PlaySound(soundlist *plist, float camera_x, float camera_y, float camera_z, int teamID)
{
	WeaponParameter WParam;
	int hitsoundA, hitsoundB;
	int hitsound;
	int ccosound;
	int bangsound;

	int id = -1;
	int volume = 0;

	//再生条件を設定
	switch(plist->paramid){
		case SHOT_WEAPON:		//発砲音
			Param->GetWeapon(plist->dataid, &WParam);
			if( WParam.soundvolume == 0 ){ return; }

			id = Resource->GetWeaponSound(plist->dataid);
			volume = WParam.soundvolume;
			break;

		case SHOT_WEAPON_PLAYER:	//プレイヤー自身の発砲音
			Param->GetWeapon(plist->dataid, &WParam);
			if( WParam.soundvolume == 0 ){ return; }

			//3D音源とせずにそのまま再生して終了
			SoundCtrl->PlaySound(Resource->GetWeaponSound(plist->dataid), WParam.soundvolume, 0);
			return;

		case HIT_MAP:			//マップ着弾音
			Resource->GetBulletSound(&hitsoundA, &hitsoundB, NULL, NULL, NULL, NULL);
			if( GetRand(3) == 0 ){
				id = hitsoundB;
			}
			else{
				id = hitsoundA;
			}
			volume = MAX_SOUNDHITMAP;
			break;

		case HIT_HUMAN_HEAD:			//被弾音
		case HIT_HUMAN_UP:
		case HIT_HUMAN_LEG:
		case HIT_HUMAN_ZOMBIE:
			Resource->GetBulletSound(NULL, NULL, &hitsound, NULL, NULL, NULL);
			id = hitsound;
			volume = MAX_SOUNDHITHUMAN;
			break;

		case HIT_SMALLOBJECT:	//小物破壊音
			id = Resource->GetSmallObjectSound(plist->dataid);
			volume = MAX_SOUNDHITSMALLOBJ;
			break;

		case BULLET:			//銃弾の音・横切る音
			float new_x, new_y, new_z;
			int passingsound;

			if( CheckApproach(plist, camera_x, camera_y, camera_z, &new_x, &new_y, &new_z) == false ){ return; }

			//味方の弾なら何もせず終了
			if( plist->teamid == teamID ){ return; }

			//そのまま再生して終了
			Resource->GetBulletSound(NULL, NULL, NULL, &passingsound, NULL, NULL);
			SoundCtrl->Play3DSound(passingsound, new_x, new_y, new_z, MAX_SOUNDPASSING);
			return;

		case GRE_BOUND:			//手榴弾 バウンド音
			Resource->GetBulletSound(NULL, NULL, NULL, NULL, NULL, &ccosound);
			id = ccosound;
			volume = MAX_SOUNDCCOGRENADE;
			break;

		case GRE_EXPLOSION:		//手榴弾 爆発音
			Resource->GetBulletSound(NULL, NULL, NULL, NULL, &bangsound, NULL);
			id = bangsound;
			volume = MAX_SOUNDHITGRENADE;
			break;

		case FOOTSTEPS_WALK:			//足音
		case FOOTSTEPS_FORWARD:
		case FOOTSTEPS_BACK:
		case FOOTSTEPS_SIDE:
			//足音を再生する処理を書く
			//　別途足音のサウンドデータを読み込んだ上で、
			//　plist->cnt（移動モーションのカウント値）を元に一定間隔で、
			//　plist->dataid（足音の種類番号）に合わせた音を再生すればよい。
			//break;
			return;	//何も再生せずに返す

		case FOOTSTEPS_JUMP:			//ジャンプ時の音
		case FOOTSTEPS_LANDING:			//着地時の音
			//ジャンプ音・着地音を再生する処理を書く
			//　plist->dataid（足音の種類番号）も参照できる。
			//break;
			return;	//何も再生せずに返す

		case WEAPON_RELOAD:			//リロード音
			id = Resource->GetWeaponSound(-1);
			volume = 100;
			break;

		default:
			return;
	}

	//再生
	SoundCtrl->Play3DSound(id, plist->x, plist->y, plist->z, volume);
}