//! @file event.cpp
//! @brief EventControlクラスの定義

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

#include "event.h"

//! @brief コンストラクタ
EventControl::EventControl(PointDataInterface *in_Point, ObjectManager *in_ObjMgr)
{
	Point = in_Point;
	ObjMgr = in_ObjMgr;
	nextp4 = 0;
	waitcnt = 0;
}

//! @brief ディストラクタ
EventControl::~EventControl()
{}

//! @brief 対象クラスを設定
//! @param in_Point ポイントデータ管理クラス
//! @param in_ObjMgr オブジェクト管理クラス
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void EventControl::SetClass(PointDataInterface *in_Point, ObjectManager *in_ObjMgr)
{
	Point = in_Point;
	ObjMgr = in_ObjMgr;
}

//! @brief リセット
//! @param EntryP4 イベント処理を開始する識別番号　（-100、-110、-120　など）
void EventControl::Reset(signed short int EntryP4)
{
	nextp4 = EntryP4;
	waitcnt = 0;
}

//! @brief 次に処理する識別番号
//! @return P4：識別番号
signed short int EventControl::GetNextP4()
{
	return nextp4;
}

//! @brief 対象人物の死亡判定
//! @param targetP4 ターゲットの識別番号
//! @param reverse 条件反転（生存判定化）
//! @return true：次のイベントへ進む　false：次へ進まない
//! @attention 対象人物が存在しない場合、falseになります。
bool EventControl::CheckDead(signed short int targetP4, bool reverse)
{
	human* thuman;

	thuman = ObjMgr->SearchHuman(targetP4);
	if( thuman == NULL ){ return false; }

	if( reverse == false ){
		if( thuman->GetDeadFlag() == false ){ return false; }
	}
	else{
		if( thuman->GetDeadFlag() == true ){ return false; }
	}
	return true;
}

//! @brief 対象人物の到着判定
//! @param pos_x 判定するX座標
//! @param pos_y 判定するY座標
//! @param pos_z 判定するZ座標
//! @param dist_r 判定する距離（半径）
//! @param targetP4 ターゲットの識別番号
//! @param reverse 条件反転（未着判定化）
//! @return true：次のイベントへ進む　false：次へ進まない
//! @attention 対象人物が存在しない場合、falseになります。
bool EventControl::CheckArrival(float pos_x, float pos_y, float pos_z, float dist_r, signed short int targetP4, bool reverse)
{
	human* thuman;
	float hx, hy, hz;
	float x, y, z;

	thuman = ObjMgr->SearchHuman(targetP4);
	if( thuman == NULL ){ return false; }
	thuman->GetPosData(&hx, &hy, &hz, NULL);
	x = hx - pos_x;
	y = hy - pos_y;
	z = hz - pos_z;

	if( reverse == false ){
		if( sqrtf(x*x + y*y+ + z*z) > dist_r ){ return false; }
	}
	else{
		if( sqrtf(x*x + y*y+ + z*z) < dist_r ){ return false; }
	}
	return true;
}

//! @brief 対象人物の指定武器所有判定
//! @param WeaponID 武器の種類番号
//! @param targetP4 ターゲットの識別番号
//! @param reverse 条件反転（未所有判定化）
//! @return true：次のイベントへ進む　false：次へ進まない
//! @attention 対象人物が存在しない場合、falseになります。
bool EventControl::CheckHaveWeapon(int WeaponID, signed short int targetP4, bool reverse)
{
	human* thuman;
	int selectweapon;
	weapon *out_weapon[TOTAL_HAVEWEAPON];
	int now_weaponid;
	bool flag = false;			//false:未所有、true:所有

	thuman = ObjMgr->SearchHuman(targetP4);
	if( thuman == NULL ){ return false; }

	//所有する武器オブジェクトを全て取得
	thuman->GetWeapon(&selectweapon, out_weapon, NULL, NULL);

	//全て調べる
	for(int i=0; i<TOTAL_HAVEWEAPON; i++){
		if( out_weapon[i] != NULL ){

			//武器の種類番号を取得し、それが対象の武器かどうか。
			out_weapon[i]->GetParamData(&now_weaponid, NULL, NULL);
			if( now_weaponid == WeaponID ){
				flag = true;
			}

		}
	}

	if( reverse == false ){
		if( flag == false ){ return false; }
	}
	else{
		if( flag == true ){ return false; }
	}
	return true;
}

//! @brief 対象移動パスの移動モード書き換え
//! @param targetP4 ターゲットの識別番号
//! @param newmode 移動モード
//! @return 成功：true　失敗：false
bool EventControl::SetMovePathMode(signed short int targetP4, int newmode)
{
	pointdata pathdata;
	int pid;

	if( Point->SearchPointdata(&pid, 0x08, 0, 0, 0, targetP4, 0) > 0 ){

		//対象がAIパスならば、強制的にパラメータを書き換える
		Point->Getdata(&pathdata, pid);
		if( (pathdata.p1 == POINT_P1TYPE_AIPATH)||(pathdata.p1 == POINT_P1TYPE_RAND_AIPATH) ){
			pathdata.p2 = (signed short int)newmode;
			Point->SetParam(pid, pathdata.p1, pathdata.p2, pathdata.p3, pathdata.p4);
			return true;
		}

	}
	return false;
}

//! @brief 対象人物のチーム番号変更
//! @param targetP4 ターゲットの識別番号
//! @param TeamID チーム番号
//! @return 成功：true　失敗：false
bool EventControl::SetTeamID(signed short int targetP4, int TeamID)
{
	human* thuman;

	thuman = ObjMgr->SearchHuman(targetP4);
	if( thuman == NULL ){ return false; }

	thuman->SetTeamID(TeamID);
	return true;
}

//! @brief 対象小物の破壊判定
//! @param targetP4 ターゲットの識別番号
//! @param reverse 条件反転（未破壊判定化）
//! @return true：次のイベントへ進む　false：次へ進まない
//! @attention 対象小物が存在しない場合、破壊済みとして処理されます。
bool EventControl::CheckBreakSmallObject(signed short int targetP4, bool reverse)
{
	smallobject* tsmallobject;
	bool flag = true;			//false:未破壊、true:破壊済みor存在しない

	tsmallobject = ObjMgr->SearchSmallobject(targetP4);
	if( tsmallobject != NULL ){
		if( tsmallobject->GetEnableFlag() == true ){ flag = false; }
	}

	if( reverse == false ){
		if( flag == false ){ return false; }
	}
	else{
		if( flag == true ){ return false; }
	}
	return true;
}

//! @brief 時間待ち（秒単位）
//! @param sec 秒
//! @return true：次のイベントへ進む　false：次へ進まない
bool EventControl::WaitSec(int sec)
{
	if( ((int)GAMEFPS * sec) > waitcnt ){
		waitcnt += 1;
		return false;
	}
	waitcnt = 0;
	return true;
}

//! @brief メッセージ表示
//! @param SetID メッセージ番号
//! @param MessageID MessageIDのポインタ
//! @param SetMessageID SetMessageIDのポインタ
//! @attention 範囲外のメッセージ番号が指定されて場合も、新たなメッセージが指定されたものとして処理します。
void EventControl::SetMessage(int SetID, int *MessageID, bool *SetMessageID)
{
	if( (0 <= SetID)&&(SetID < MAX_POINTMESSAGES) ){
		*MessageID = SetID;
	}
	*SetMessageID = true;
}

//! @brief 処理を実行
//! @param SkipFlag イベントタスクのスキップフラグ
//! @param Param イベントのパラメーターを受け渡しする構造体のポインタ
//! @return 実行したステップ数
//! @attention 渡されたパラメーター（構造体）に変更がない場合、渡されたパラメーターは操作されません。
//! @attention 構造体のSetMessageID は、メッセージイベントが実行された場合に true になります。<b>前回から変更されたとは限りません。</b>
int EventControl::ProcessEventPoint(bool SkipFlag, EventParameter *Param)
{
	if( Param == NULL ){ return 0; }

	pointdata data;
	int cnt = 0;

	for(int i=0; i<TOTAL_EVENTFRAMESTEP; i++){
		cnt = i;

		//次のポイントを探す
		if( Point->SearchPointdata(&data, 0x08, 0, 0, 0, nextp4, 0) == 0 ){ return cnt; }

		//無効な番号なら処理しない
		if( (data.p1 < 10)||(19 < data.p1) ){
#ifdef ENABLE_CHECKOPENXOPSEVENT
			if( data.p1 != 29 ){ return cnt; }
#else
			return cnt;
#endif
		}

		//ループ1回目でスキップフラグが有効ならば、次イベントへ進む。
		if( (cnt == 0)&&(SkipFlag == true) ){
			nextp4 = data.p3;
			continue;
		}

		switch(data.p1){
			case 10:	//任務達成
				Param->endcnt += 1;
				Param->complete = true;
				return cnt;

			case 11:	//任務失敗
				Param->endcnt += 1;
				Param->complete = false;
				return cnt;

			case 12:	//死亡待ち
				if( CheckDead(data.p2, false) == false ){ return cnt; }
				nextp4 = data.p3;
				break;

			case 13:	//到着待ち
				if( CheckArrival(data.x, data.y, data.z, DISTANCE_CHECKPOINT, data.p2, false) == false ){ return cnt; }
				nextp4 = data.p3;
				break;

			case 14:	//歩きに変更
				SetMovePathMode(data.p2, 0);
				nextp4 = data.p3;
				break;

			case 15:	//小物破壊待ち
				if( CheckBreakSmallObject(data.p2, false) == false ){ return cnt; }
				nextp4 = data.p3;
				break;

			case 16:	//ケース待ち
				if( CheckArrival(data.x, data.y, data.z, DISTANCE_CHECKPOINT, data.p2, false) == false ){ return cnt; }
				if( CheckHaveWeapon(ID_WEAPON_CASE, data.p2, false) == false ){ return cnt; }
				nextp4 = data.p3;
				break;

			case 17:	//時間待ち
				if( WaitSec(PointParamToInt(data.p2)) == false ){ return cnt; }
				nextp4 = data.p3;
				break;

			case 18:	//メッセージ
				SetMessage(PointParamToInt(data.p2), &(Param->MessageID), &(Param->SetMessageID));
				nextp4 = data.p3;
				break;

			case 19:	//チーム変更
				SetTeamID(data.p2, 0);
				nextp4 = data.p3;
				break;

#ifdef ENABLE_CHECKOPENXOPSEVENT
			case 29:	//OpenXOPS判定
				if( data.p2 != 0 ){ return cnt; }
				nextp4 = data.p3;
				break;
#endif

			//新たなイベントポイントを追加する場合、ここに書く。
			//　※ 種類番号の競合 厳禁
			//　※ 本switch文直前にある、条件判定（範囲チェック）も書き換えること。
		}
	}
	return cnt;
}