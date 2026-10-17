//! @file ai.cpp
//! @brief AIcontrolクラスの定義

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

#include "ai.h"

//! @brief コンストラクタ
AIcontrol::AIcontrol(class ObjectManager *in_ObjMgr, int in_ctrlid, class BlockDataInterface *in_blocks, class PointDataInterface *in_Points, class ParameterInfo *in_Param, class Collision *in_CollD, class SoundManager *in_GameSound)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;
	if( in_ObjMgr != NULL ){ ctrlhuman = in_ObjMgr->GetHumanObject(in_ctrlid); }
	else{ ctrlhuman = NULL; }
	enemyhuman = NULL;
	blocks = in_blocks;
	Param = in_Param;
	CollD = in_CollD;
	GameSound = in_GameSound;

	AIlevel = 0;
	battlemode = AI_NORMAL;
	NoFight = false;
	posx = 0.0f;
	posy = 0.0f;
	posz = 0.0f;
	rx = 0.0f;
	ry = 0.0f;
	cautionback_posx = 0.0f;
	cautionback_posz = 0.0f;
	FaceCaution_flag = false;
	FaceCaution_rx = 0.0f;
	total_move = 0.0f;
	waitcnt = 0;
	movejumpcnt = 1*((int)GAMEFPS);
	gotocnt = 0;
	cautioncnt = 0;
	actioncnt = 0;
	longattack = false;
	EventWeaponShot = false;
	LevelParam = NULL;

	MoveNavi = new AIMoveNavi;
	ObjDriver = new AIObjectDriver;
	MoveNavi->SetClass(in_ObjMgr, in_ctrlid, in_Points);
	ObjDriver->SetClass(in_ObjMgr, in_ctrlid);
}

//! @brief ディストラクタ
AIcontrol::~AIcontrol()
{
	delete MoveNavi;
	delete ObjDriver;
}

//! @brief 対象クラスを設定
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void AIcontrol::SetClass(class ObjectManager *in_ObjMgr, int in_ctrlid, class BlockDataInterface *in_blocks, class PointDataInterface *in_Points, class ParameterInfo *in_Param, class Collision *in_CollD, class SoundManager *in_GameSound)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;
	ctrlhuman = in_ObjMgr->GetHumanObject(in_ctrlid);
	blocks = in_blocks;
	Param = in_Param;
	CollD = in_CollD;
	GameSound = in_GameSound;

	MoveNavi->SetClass(in_ObjMgr, in_ctrlid, in_Points);
	ObjDriver->SetClass(in_ObjMgr, in_ctrlid);
}

//! @brief 目標地点に移動しているか確認
//! @return 到達：true　非到達：false
bool AIcontrol::CheckTargetPos(bool back)
{
	float target_posx, target_posz;
	int movemode;
	if( back == false ){
		MoveNavi->GetTargetPos(&target_posx, &target_posz, NULL, &movemode, NULL);
	}
	else{
		target_posx = cautionback_posx;
		target_posz = cautionback_posz;
		movemode = AI_NAVI_MOVE_RUN;
	}

	//距離を算出
	float x = posx - target_posx;
	float z = posz - target_posz;
	float r = x * x + z * z;

	if( movemode == AI_NAVI_MOVE_TRACKING ){	//追尾中なら
		if( r < AI_ARRIVALDIST_TRACKING * AI_ARRIVALDIST_TRACKING ){
			return true;
		}
	}
	else{							//それ以外なら
		if( r < (AI_ARRIVALDIST_PATH * AI_ARRIVALDIST_PATH) ){
			return true;
		}
	}

	return false;
}

//! @brief 目標地点に移動
void AIcontrol::MoveTarget(bool back)
{
	float r, atan;
	int paramid;
	HumanParameter Paraminfo;
	bool zombie;

	float target_posx, target_posz;	
	int movemode;
	if( back == false ){
		MoveNavi->GetTargetPos(&target_posx, &target_posz, NULL, &movemode, NULL);
	}
	else{
		target_posx = cautionback_posx;
		target_posz = cautionback_posz;
		movemode = AI_NAVI_MOVE_RUN;
	}

	//ゾンビかどうか判定
	ctrlhuman->GetParamData(&paramid, NULL, NULL, NULL);
	Param->GetHuman(paramid, &Paraminfo);
	if( Paraminfo.type == 2 ){
		zombie = true;
	}
	else{
		zombie = false;
	}

	//一度全ての動きを止める
	ObjDriver->ResetMode();

	//目標地点への角度を求める
	CheckTargetAngle(posx, 0.0f, posz, rx*-1 + (float)M_PI/2, 0.0f, target_posx, 0.0f, target_posz, 0.0f, &atan, NULL, &r);

	//旋回
	if( atan > DegreeToRadian(0.5f) ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
	}
	if( atan < DegreeToRadian(-0.5f) ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
	}

	//前進する
	if( zombie == true ){
		if( fabsf(atan) < DegreeToRadian(20) ){
			ObjDriver->SetModeFlag(AI_CTRL_MOVEWALK);
		}
	}
	else if( movemode == AI_NAVI_MOVE_RUN ){
		if( fabsf(atan) < DegreeToRadian(50) ){
			ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
		}
	}
	else if( movemode == AI_NAVI_MOVE_RUN2 ){
		if( fabsf(atan) < DegreeToRadian(50) ){
			ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
		}
	}
	else if( movemode == AI_NAVI_MOVE_WALK ){
		if( fabsf(atan) < DegreeToRadian(6) ){
			ObjDriver->SetModeFlag(AI_CTRL_MOVEWALK);
		}
	}
	else if( movemode == AI_NAVI_MOVE_TRACKING ){
		if( fabsf(atan) < DegreeToRadian(20) ){
			if( r < (AI_ARRIVALDIST_WALKTRACKING * AI_ARRIVALDIST_WALKTRACKING) ){
				ObjDriver->SetModeFlag(AI_CTRL_MOVEWALK);
			}
			else{
				ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
			}
		}
	}

	//ジャンプ
	if( GetRand(16) == 0 ){
		MoveJump();
	}

	//引っ掛かっていたら、左右への回転をランダムに行う
	if( GetRand(28) == 0 ){
		if( ctrlhuman->GetMovemode(true) != 0 ){
			if( ctrlhuman->GetTotalMove() - total_move < 0.1f ){
				if( GetRand(2) == 0 ){ ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT); }
				else{ ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT); }
			}
		}
	}
	total_move = ctrlhuman->GetTotalMove();
}

//! @brief 目標地点に移動（優先的な走り用）
void AIcontrol::MoveTarget2(bool back)
{
	float atan;

	float target_posx, target_posz;	
	if( back == false ){
		MoveNavi->GetTargetPos(&target_posx, &target_posz, NULL, NULL, NULL);
	}
	else{
		target_posx = cautionback_posx;
		target_posz = cautionback_posz;
	}

	//目標地点への角度を求める
	CheckTargetAngle(posx, 0.0f, posz, rx*-1 + (float)M_PI/2, 0.0f, target_posx, 0.0f, target_posz, 0.0f, &atan, NULL, NULL);

	//前後移動の処理
	if( fabsf(atan) < DegreeToRadian(56) ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
	}
	if( fabsf(atan) > DegreeToRadian(123.5f) ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
	}

	//左右移動の処理
	if( (DegreeToRadian(-146) < atan)&&(atan < DegreeToRadian(-33)) ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVERIGHT);
	}
	if( (DegreeToRadian(33) < atan)&&(atan < DegreeToRadian(146)) ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVELEFT);
	}

	//ジャンプ
	if( GetRand(16) == 0 ){
		MoveJump();
	}

	//引っ掛かっていたら、左右への回転をランダムに行う
	if( GetRand(28) == 0 ){
		if( ctrlhuman->GetMovemode(true) != 0 ){
			if( ctrlhuman->GetTotalMove() - total_move < 0.1f ){
				if( GetRand(2) == 0 ){ ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT); }
				else{ ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT); }
			}
		}
	}
	total_move = ctrlhuman->GetTotalMove();
}

//! @brief 前後左右ランダムに移動（攻撃中用）
void AIcontrol::MoveRandom()
{
	int forwardstart, backstart, sidestart;

	if( longattack == false ){
		forwardstart = 80;
		backstart = 90;
		sidestart = 70;
	}
	else{
		forwardstart = 120;
		backstart = 150;
		sidestart = 130;
	}

	//ランダムに移動を始める
	if( GetRand(forwardstart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
	}
	if( GetRand(backstart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
	}
	if( GetRand(sidestart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVELEFT);
	}
	if( GetRand(sidestart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_MOVERIGHT);
	}

	//武器を持っておらず、手ぶらならば
	if( ctrlhuman->GetMainWeaponTypeNO() == ID_WEAPON_NONE ){
		// 1/80の確率で下がり始める
		if( GetRand(80) == 0 ){
			ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
		}
	}

	// 1/3の確率か、移動フラグが設定されていたら
	if( (GetRand(3) == 0)||(ObjDriver->GetModeFlag(AI_CTRL_MOVEFORWARD | AI_CTRL_MOVEBACKWARD | AI_CTRL_MOVELEFT | AI_CTRL_MOVERIGHT)) ){
		float vx, vz;
		float Dist;

		if( GetRand(2) == 0 ){
			//前方向のベクトルを計算
			vx = cosf(rx*-1 + (float)M_PI/2);
			vz = sinf(rx*-1 + (float)M_PI/2);
			if(
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy + HUMAN_MAPCOLLISION_HEIGHT, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == true)||		//腰の高さにブロックがある（ぶつかる）
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy - 1.0f, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == false)							//足元にブロックがない（落ちる）
			){
				//前進フラグを削除し、後退フラグを設定
				ObjDriver->DelModeFlag(AI_CTRL_MOVEFORWARD);
				ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
			}

			//後方向のベクトルを計算
			vx = cosf(rx*-1 + (float)M_PI/2 + (float)M_PI);
			vz = sinf(rx*-1 + (float)M_PI/2 + (float)M_PI);
			if(
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy + HUMAN_MAPCOLLISION_HEIGHT, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == true)||		//腰の高さにブロックがある（ぶつかる）
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy - 1.0f, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == false)							//足元にブロックがない（落ちる）
			){
				//後退フラグを削除し、前進フラグを設定
				ObjDriver->DelModeFlag(AI_CTRL_MOVEBACKWARD);
				ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
			}
		}
		else{
			//
			vx = cosf(rx*-1);
			vz = sinf(rx*-1);
			if(
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy + HUMAN_MAPCOLLISION_HEIGHT, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == true)||		//腰の高さにブロックがある（ぶつかる）
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy - 1.0f, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == false)							//足元にブロックがない（落ちる）
			){
				//右移動フラグを削除し、左移動フラグを設定
				ObjDriver->DelModeFlag(AI_CTRL_MOVERIGHT);
				ObjDriver->SetModeFlag(AI_CTRL_MOVELEFT);
			}

			vx = cosf(rx*-1 + (float)M_PI);
			vz = sinf(rx*-1 + (float)M_PI);
			if(
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy + HUMAN_MAPCOLLISION_HEIGHT, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == true)||		//腰の高さにブロックがある（ぶつかる）
				(CollD->CheckALLBlockIntersectDummyRay(posx, posy - 1.0f, posz, vx, 0, vz, NULL, NULL, &Dist, HUMAN_MAPCOLLISION_R) == false)							//足元にブロックがない（落ちる）
			){
				//左移動フラグを削除し、右移動フラグを設定
				ObjDriver->DelModeFlag(AI_CTRL_MOVELEFT);
				ObjDriver->SetModeFlag(AI_CTRL_MOVERIGHT);
			}
		}
	}

	//攻撃対象がいるなら
	if( enemyhuman != NULL ){
		float tx, ty, tz;

		enemyhuman->GetPosData(&tx, &ty, &tz, NULL);

		float x = posx - tx;
		float y = posy - ty;
		float z = posz - tz;
		float r = x * x + y * y + z * z;

		//敵に近づきすぎたなら後退する
		if( r < 20.0f * 20.0f ){
			ObjDriver->DelModeFlag(AI_CTRL_MOVEFORWARD);
			if( GetRand(70) == 0 ){
				ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
			}
		}
	}
}

//! @brief その場を見まわす
void AIcontrol::TurnSeen()
{
	int turnstart, turnstop;

	float target_rx;
	int pointmode;
	MoveNavi->GetTargetPos(NULL, NULL, &target_rx, NULL, &pointmode);

	//撃たれて警戒したなら、撃たれた方向を向く
	if( (battlemode == AI_CAUTION)&&(FaceCaution_flag == true) ){
		float tr;

		//方向を計算
		tr = FaceCaution_rx - rx;
		tr = AngleNormalization(tr);

		//旋回
		if( tr > DegreeToRadian(2.5f) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
		}
		if( tr < DegreeToRadian(-2.5f) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
		}

		//特定方向に向き終われば、普通の警戒に移行する
		if( fabsf(tr) <= DegreeToRadian(2.5f) ){
			FaceCaution_flag = false;
		}

		return;
	}

	//回転の開始・終了確率を設定
	if( battlemode == AI_ACTION ){
		return;
	}
	else if( battlemode == AI_CAUTION ){
		turnstart = 20;
		turnstop = 20;
	}
	else{
		if( pointmode == AI_NAVI_POINT_TRACKING ){ turnstart = 65; }
		else{ turnstart = 85; }
		turnstop = 18;
	}

	//ランダムに回転を始める
	if( GetRand(turnstart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
	}
	if( GetRand(turnstart) == 0 ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
	}

	if( (battlemode == AI_NORMAL)&&(pointmode == AI_NAVI_POINT_WAIT) ){
		//ランダムにポイントの方を向こうとする
		//「ポイントの方向を少し重視する」の再現 
		if( GetRand(80) == 0 ){
			float tr;
			tr = target_rx - rx;
			tr = AngleNormalization(tr);

			if( tr > 0.0f ){
				ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
			}
			if( tr < 0.0f ){
				ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
			}
		}
	}

	//回転をランダムに止める
	if( GetRand(turnstop) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNRIGHT);
	}
	if( GetRand(turnstop) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNLEFT);
	}
}

//! @brief 特定の方向を見続ける
bool AIcontrol::StopSeen()
{
	float tr;
	bool returnflag = false;

	float target_rx;
	MoveNavi->GetTargetPos(NULL, NULL, &target_rx, NULL, NULL);

	tr = target_rx - rx;
	tr = AngleNormalization(tr);

	ObjDriver->DelModeFlag(AI_CTRL_TURNRIGHT);
	ObjDriver->DelModeFlag(AI_CTRL_TURNLEFT);

	//旋回
	if( tr > DegreeToRadian(2.5f) ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
		returnflag = false;
	}
	if( tr < DegreeToRadian(-2.5f) ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
		returnflag = false;
	}

	//特定方向に向けているか判定
	if( fabsf(tr) <= DegreeToRadian(2.5f) ){
		returnflag = true;
	}

	return returnflag;
}

//! @brief 進行方向に障害物があればジャンプする
bool AIcontrol::MoveJump()
{
	//立ち止まっていれば処理しない
	//　※AIObjectDriver(ObjDriver)の移動フラグでチェックしても良い。
	if( ctrlhuman->GetMovemode(false) == 0 ){ return false; }

	float dist_dummy;

	float check_rx = 0.0f;
	float new_posx, new_posy, new_posz;

	//進行方向を算出
	if( ObjDriver->GetModeFlag(AI_CTRL_MOVEWALK) == true ){
		check_rx = DegreeToRadian(0.0f);
	}
	if( ObjDriver->GetModeFlag(AI_CTRL_MOVEFORWARD) == true ){
		check_rx = DegreeToRadian(0.0f);
	}
	if( ObjDriver->GetModeFlag(AI_CTRL_MOVEBACKWARD) == true ){
		check_rx = DegreeToRadian(180.0f);
	}
	if( ObjDriver->GetModeFlag(AI_CTRL_MOVELEFT) == true ){
		check_rx = DegreeToRadian(90.0f);
	}
	if( ObjDriver->GetModeFlag(AI_CTRL_MOVERIGHT) == true ){
		check_rx = DegreeToRadian(-90.0f);
	}

	//腰付近の当たり判定
	new_posx = posx + cosf(rx*-1 + (float)M_PI/2 + check_rx) * (AI_CHECKJUMP_DIST + HUMAN_MAPCOLLISION_R);
	new_posy = posy + HUMAN_MAPCOLLISION_HEIGHT;
	new_posz = posz + sinf(rx*-1 + (float)M_PI/2 + check_rx) * (AI_CHECKJUMP_DIST + HUMAN_MAPCOLLISION_R);
	if( CollD->CheckALLBlockInside(new_posx, new_posy, new_posz) == true ){
		ObjMgr->MoveJump(ctrlid);
		return true;
	}

	//体全体の当たり判定
	new_posx = posx + cosf(rx*-1 + (float)M_PI/2 + check_rx) * AI_CHECKJUMP_DIST;
	new_posy = posy + AI_CHECKJUMP_HEIGHT;
	new_posz = posz + sinf(rx*-1 + (float)M_PI/2 + check_rx) * AI_CHECKJUMP_DIST;
	if( CollD->CheckALLBlockInside(new_posx, new_posy, new_posz) == true ){
		ObjMgr->MoveJump(ctrlid);
		return true;
	}
	else if( CollD->CheckALLBlockIntersectRay(new_posx, new_posy, new_posz, 0.0f, 1.0f, 0.0f, NULL, NULL, &dist_dummy, HUMAN_HEIGHT - AI_CHECKJUMP_HEIGHT) == true ){
		ObjMgr->MoveJump(ctrlid);
		return true;
	}

	return false;
}

//! @brief 攻撃
//! @todo ゾンビの相手を捕まえる処理
void AIcontrol::Action()
{
	human* EnemyHuman = NULL;
	int paramid;
	HumanParameter Paraminfo;
	bool zombie;
	float posy2;
	float tx, ty, tz;

	EnemyHuman = enemyhuman;

	//座標を取得
	EnemyHuman->GetPosData(&tx, &ty, &tz, NULL);
	posy2 = posy + VIEW_HEIGHT;
	ty += VIEW_HEIGHT;

	//ゾンビかどうか判定
	ctrlhuman->GetParamData(&paramid, NULL, NULL, NULL);
	Param->GetHuman(paramid, &Paraminfo);
	if( Paraminfo.type == 2 ){
		zombie = true;
	}
	else{
		zombie = false;
	}

	//所持している武器の種類を取得
	int weaponid = ctrlhuman->GetMainWeaponTypeNO();

	float atanx, atany, r;

	//ターゲットの位置を補正
	if( zombie == true ){		//ゾンビの場合
		float x = posx - tx;
		float z = posz - tz;
		float r = sqrtf(x * x + z * z);
		float scale;
		float mx, mz;

		if( r > 180.0f ){ r = 180.0f; }		//もしかしたら 1800.0f 上限かも

		//追従性のパラメーターを取得
		switch( Paraminfo.AItrackability ){
			case 0: scale = (0.05f * 2); break;
			case 1: scale = (0.10f * 2); break;
			case 2: scale = (0.13f * 2); break;
			case 3: scale = (0.17f * 2); break;
			default: scale = 0.0f;
		}

		//敵の移動を見超す
		EnemyHuman->GetMovePos(&mx, NULL, &mz);
		tx += mx * r * scale;
		tz += mz * r * scale;
	}
	else{		//ゾンビでない場合
		//自分が手榴弾を持っていれば～
		if( weaponid == ID_WEAPON_GRENADE ){
			if( zombie == false ){
				float x = posx - tx;
				float z = posz - tz;
				float r = sqrtf(x * x + z * z);
				float scale;
				if( longattack == false ){ scale = 0.12f; }
				else{ scale = 0.4f; }

				//距離に応じて高さを変える
				ty += (r - AI_CHECKSHORTATTACK_DIST) * scale;
			}
		}
		else{
			float mx, mz;
			float scale;
			EnemyHuman->GetMovePos(&mx, NULL, &mz);
			if( longattack == false ){ scale = 1.5f; }
			else{
				float x = posx - tx;
				float z = posz - tz;
				float r = sqrtf(x * x + z * z);

				scale = r * 0.12f;
			}

			//敵の移動を見超す
			tx += mx * scale;
			tz += mz * scale;
		}
	}

	//目標地点への角度を求める
	CheckTargetAngle(posx, posy2, posz, rx*-1 + (float)M_PI/2, ry, tx, ty, tz, 0.0f, &atanx, &atany, &r);

	//向きを変えるタイミングか決定
	int randr = LevelParam->aiming;
	if( longattack == false ){ randr += 1; }
	else{ randr += 2; }

	if( randr != 0 ){
		//旋回
		if( atanx > 0.0f ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
			ObjDriver->DelModeFlag(AI_CTRL_TURNRIGHT);
		}
		if( atanx < 0.0f ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
			ObjDriver->DelModeFlag(AI_CTRL_TURNLEFT);
		}

		//腕の角度
		if( zombie == true ){
			//ry = AI_ZOMBIEATTACK_ARMRY;

			//旋回
			if( ry < AI_ZOMBIEATTACK_ARMRY ){
				ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
				ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);
			}
			if( ry > AI_ZOMBIEATTACK_ARMRY ){
				ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
				ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
			}
		}
		else{
			//自分が手ぶらならば～
			if( weaponid == ID_WEAPON_NONE ){
				if( EnemyHuman->GetMainWeaponTypeNO() == ID_WEAPON_NONE ){	//敵も手ぶらならば～
					//下に向け続ける
					ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
					ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
				}
				else{														//敵が武器を持っていれば～
					//上に向け続ける
					ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
					ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);
				}
			}
			else{
				//旋回
				if( atany > 0.0f ){
					ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
					ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);
				}
				if( atany < 0.0f ){
					ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
					ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
				}
			}
		}
	}

	//ゾンビ以外で手ぶらならば
	if( zombie == false ){
		if( weaponid == ID_WEAPON_NONE ){
			//一定の確率で後退する
			if( GetRand(80) == 0 ){
				ObjDriver->SetModeFlag(AI_CTRL_MOVEBACKWARD);
			}
		}
	}

	
	if( zombie == true ){	//ゾンビの攻撃
		float y = posy2 - ty;

		//もし走っていれば、一度歩きに切り替える
		if( ObjDriver->GetModeFlag(AI_CTRL_MOVEFORWARD) ){
			ObjDriver->DelModeFlag(AI_CTRL_MOVEFORWARD);
			ObjDriver->SetModeFlag(AI_CTRL_MOVEWALK);
		}

		//敵に向かって前進する
		if( fabsf(atanx) <= DegreeToRadian(25) ){
			if( (fabsf(atanx) <= DegreeToRadian(15)) && (r < 24.0f*24.0f) && (actioncnt%50 > 20) ){
				//歩きを取り消し、走る
				ObjDriver->SetModeFlag(AI_CTRL_MOVEFORWARD);
				ObjDriver->DelModeFlag(AI_CTRL_MOVEWALK);
			}
			else{
				ObjDriver->SetModeFlag(AI_CTRL_MOVEWALK);
			}
		}

		/*
		//ジャンプ
		if( GetRand(16) == 0 ){
			MoveJump();
		}
		*/

		if( (r < 9.0f*9.0f)&&( fabsf(y) < 10.0f) ){
			float x = posx - tx;
			float z = posz - tz;

			//捕まえる　（敵を引き付ける）
			EnemyHuman->AddPosOrder(atan2f(z, x), 0.0f, 0.5f);

			//敵の視点をランダムに動かす
			float erx, ery;
			EnemyHuman->GetRxRy(&erx, &ery);
			switch(GetRand(3)){
				case 0: erx -= DegreeToRadian(2); break;
				case 1: erx += DegreeToRadian(2); break;
				default: break;
			}
			switch(GetRand(3)){
				case 0: ery -= DegreeToRadian(2); break;
				case 1: ery += DegreeToRadian(2); break;
				default: break;
			}
			EnemyHuman->SetRxRy(erx, ery);
		}

		if( actioncnt%50 == 0){
			if( ObjMgr->CheckZombieAttack(ctrlhuman, EnemyHuman) == true ){
				ObjMgr->HitZombieAttack(ctrlhuman, EnemyHuman);
			}
		}
	}
	else{					//発砲する
		int scopemode = 0;
		ScopeParameter sparam;
		float ShotAngle;

		//武器のスコープ設定を取得
		if( weaponid != ID_WEAPON_NONE ){
			WeaponParameter wparam;
			Param->GetWeapon(weaponid, &wparam);
			scopemode = wparam.scopemode;
		}

		if( longattack == false ){
			//敵を捉えたと判定する、許容誤差を計算する
			Param->GetScopeParam(scopemode, &sparam);
			ShotAngle = sparam.AI_ShotAngle;

			//AIレベルごとに調整
			ShotAngle += DegreeToRadian(0.5f) * LevelParam->limitserror;

			if( MoveNavi->GetRun2() == true ){
				ShotAngle *= 1.5f;
			}
		}
		else{
			//敵を捉えたと判定する、許容誤差を計算する
			Param->GetScopeParam(scopemode, &sparam);
			ShotAngle = sparam.AI_ShotAngleLong;

			//AIレベルごとに調整
			ShotAngle += DegreeToRadian(0.2f) * LevelParam->limitserror;
		}

		//敵を捉えていれば
		float atanxy = fabsf(atanx) + fabsf(atany);
		if( atanxy < ShotAngle ){
			int rand = LevelParam->attack;
			if( longattack == true ){ rand += 1; }

			//発砲
			if( GetRand(rand) == 0 ){
				ObjMgr->ShotWeapon(ctrlid);
			}
		}
	}

	//距離に応じて近距離・遠距離を切り替える
	//　※ AI_CHECKSHORTATTACK_DIST ピッタリで設定値維持
	if( r < AI_CHECKSHORTATTACK_DIST * AI_CHECKSHORTATTACK_DIST ){
		longattack = false;
	}
	if( (r > AI_CHECKSHORTATTACK_DIST * AI_CHECKSHORTATTACK_DIST)&&(MoveNavi->GetRun2() == false) ){
		longattack = true;
	}

	if( zombie == false ){
		//ランダムに移動
		MoveRandom();
	}

	actioncnt += 1;
}

//! @brief 攻撃をキャンセル
bool AIcontrol::ActionCancel()
{
	//非戦闘化フラグが有効なら終了
	if( NoFight == true ){
		return true;
	}

	//敵が死亡したら終了
	if( enemyhuman->GetDeadFlag() == true ){
		return true;
	}

	//距離を取得
	float tx, ty, tz;
	enemyhuman->GetPosData(&tx, &ty, &tz, NULL);
	float x = posx - tx;
	float y = posy - ty;
	float z = posz - tz;
	float r = x*x + y*y + z*z;

	//距離が離れ過ぎていたら終了
	if( r > 620.0f*620.0f ){
		return true;
	}

	if( longattack == false ){
		//適当なタイミングで敵が見えるか確認
		if( GetRand(40) == 0 ){
			//ブロックが遮っていた（＝見えない）ならば終了
			if( CheckLookEnemy(enemyhuman, AI_SEARCH_RX, AI_SEARCH_RY, 620.0f, NULL) == false ){
				return true;
			}
		}

		//強制的に終了
		if( GetRand(550) == 0 ){
			return true;
		}
	}
	else{
		//適当なタイミングで敵が見えるか確認
		if( GetRand(30) == 0 ){
			//ブロックが遮っていた（＝見えない）ならば終了
			if( CheckLookEnemy(enemyhuman, AI_SEARCH_RX, AI_SEARCH_RY, 620.0f, NULL) == false ){
				return true;
			}
		}

		//強制的に終了
		if( GetRand(450) == 0 ){
			return true;
		}
	}

	return false;
}

//! @brief 武器を持つ
int AIcontrol::HaveWeapon()
{
	int selectweapon;
	class weapon *weapon[TOTAL_HAVEWEAPON];
	int lnbslist[TOTAL_HAVEWEAPON];
	int nbslist[TOTAL_HAVEWEAPON];
	int nbs = 0;

	for(int i=0; i<TOTAL_HAVEWEAPON; i++){
		weapon[i] = NULL;
		lnbslist[i] = 0;
		nbslist[i] = 0;
	}

	//武器の情報を取得
	ctrlhuman->GetWeapon(&selectweapon, weapon, lnbslist, nbslist);

	//武器を持っていないにも関わらず、装弾数があるなら切り替えない
	if( (weapon[selectweapon] == NULL)&&(nbslist[selectweapon] > 0) ){
		return 0;
	}

	//武器を持っていれば、合計弾数を取得
	if( weapon[selectweapon] != NULL ){
		weapon[selectweapon]->GetParamData(NULL, NULL, &nbs);
	}

	//武器を持っていないか、弾が0ならば
	if( (weapon[selectweapon] == NULL)||(nbs == 0) ){
		//次の武器を指定
		int notselectweapon = selectweapon + 1;
		if( notselectweapon == TOTAL_HAVEWEAPON ){ notselectweapon = 0; }

		//次の武器を持っていれば
		if( weapon[notselectweapon] != NULL ){
			//次の武器の弾数が0でなければ
			weapon[notselectweapon]->GetParamData(NULL, NULL, &nbs);
			if( nbs > 0 ){
				//持ち替える
				ObjMgr->ChangeHaveWeapon(ctrlid);
				return 1;
			}
		}
	}

	return 0;
}

//! @brief 移動や方向転換をランダムに終了
void AIcontrol::CancelMoveTurn()
{
	int forward, back, side, updown, rightleft;

	if( battlemode == AI_ACTION ){			//攻撃中
		if( MoveNavi->GetRun2() == true ){	//優先的な走り
			ObjDriver->DelModeFlag(AI_CTRL_MOVEFORWARD);
			ObjDriver->DelModeFlag(AI_CTRL_MOVEBACKWARD);
			ObjDriver->DelModeFlag(AI_CTRL_MOVELEFT);
			ObjDriver->DelModeFlag(AI_CTRL_MOVERIGHT);
			ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
			ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);
			ObjDriver->DelModeFlag(AI_CTRL_TURNLEFT);
			ObjDriver->DelModeFlag(AI_CTRL_TURNRIGHT);
			if( GetRand(3) == 0 ){
				ObjDriver->DelModeFlag(AI_CTRL_MOVEWALK);
			}
			return;
		}
		else{									//優先的な走り 以外
			if( longattack == false ){
				forward = 6;
				back = 6;
				side = 7;
				updown = 5;
				rightleft = 6;
			}
			else{
				forward = 5;
				back = 4;
				side = 5;
				updown = 3;
				rightleft = 3;
			}
		}
	}
	else if( battlemode == AI_CAUTION ){	//警戒中
		forward = 10;
		back = 10;
		side = 10;
		updown = 14;
		rightleft = 20;
	}
	else{
		forward = 12;
		back = 12;
		side = 12;
		updown = 15;
		rightleft = 18;
	}

	//移動をランダムに止める
	if( GetRand(forward) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_MOVEFORWARD);
	}
	if( GetRand(back) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_MOVEBACKWARD);
	}
	if( GetRand(side) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_MOVELEFT);
	}
	if( GetRand(side) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_MOVERIGHT);
	}
	if( GetRand(3) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_MOVEWALK);
	}

	//回転をランダムに止める
	if( GetRand(updown) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
	}
	if( GetRand(updown) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);
	}
	if( GetRand(rightleft) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNLEFT);
	}
	if( GetRand(rightleft) == 0 ){
		ObjDriver->DelModeFlag(AI_CTRL_TURNRIGHT);
	}
}

//! @brief 武器をリロード・捨てる
//! @return 捨てる：1　リロード：2　持ち替える：3　FULL/SEMI切り替え：4　何もしない：0
int AIcontrol::ControlWeapon()
{
	int selectweapon;
	class weapon *weapon[TOTAL_HAVEWEAPON];
	int weaponid, lnbs, nbs;
	WeaponParameter paramdata;
	int burstcntS, burstcntN;

	for(int i=0; i<TOTAL_HAVEWEAPON; i++){
		weapon[i] = NULL;
	}

	//武器の情報を取得
	ctrlhuman->GetWeapon(&selectweapon, weapon, NULL, NULL);
	if( weapon[selectweapon] == NULL ){ return 0; }
	weapon[selectweapon]->GetParamData(&weaponid, &lnbs, &nbs);

	//武器の性能を取得
	if( Param->GetWeapon(weaponid, &paramdata) == 1 ){ return 0; }

	//スコープ解除
	ctrlhuman->SetDisableScope();

	//「ケース」なら何もしない
	if( weaponid == ID_WEAPON_CASE ){ return 0; }

	//戦闘中にグレネードを持っていれば
	if( battlemode == AI_ACTION ){
		if( weaponid == ID_WEAPON_GRENADE ){
			int nextselectweapon, nextnds;

			//次の武器の弾数を取得
			nextselectweapon = selectweapon + 1;
			nextnds = 0;
			if( nextselectweapon == TOTAL_HAVEWEAPON ){
				nextselectweapon = 0;
			}
			if( weapon[nextselectweapon] != NULL ){
				weapon[nextselectweapon]->GetParamData(NULL, NULL, &nextnds);
			}

			if( longattack == false ){
				// 1/100の確率で持ち替える
				if( (GetRand(100) == 0)&&(nextnds > 0) ){
					ObjMgr->ChangeHaveWeapon(ctrlid);
					return 3;
				}
			}
			else{
				// 1/66の確率で持ち替える
				if( (GetRand(66) == 0)&&(nextnds > 0) ){
					ObjMgr->ChangeHaveWeapon(ctrlid);
					return 3;
				}
			}
		}
	}

	//残弾数が無ければ
	if( lnbs == 0 ){
		int ways;

		//処理確率決定
		if( battlemode == AI_NORMAL ){ ways = 1; }
		else if( battlemode == AI_CAUTION ){ ways = 10; }
		else{ ways = 8; }

		// 1/waysの確率で処理
		if( GetRand(ways) == 0 ){
			int under;

			//リロード確率
			if( battlemode == AI_NORMAL ){
				ways = 1;
				under = 0;
			}
			else if( battlemode == AI_CAUTION ){
				ways = 5;
				under = 3;
			}
			else{
				if( longattack == false ){
					// 確率は　3/4
					ways = 4;
					under = 2;
				}
				else{
					// 確率は　2/3
					ways = 3;
					under = 1;
				}
			}

			//弾が無ければ捨てる
			if( nbs == 0 ){
				ObjMgr->DumpWeapon(ctrlid);
				return 1;
			}

			//ランダムに リロード実行 or 武器を持ちかえ
			if( GetRand(ways) <= under ){
				ObjMgr->ReloadWeapon(ctrlid);
				return 2;
			}
			//else{
				ObjMgr->ChangeHaveWeapon(ctrlid);
				return 3;
			//}
		}
	}

	//連射切り替えが可能な武器なら
	if( paramdata.ChangeWeaponID != -1 ){
		//現在の武器の連射設定を取得
		burstcntS = paramdata.burstcnt;

		//新たな武器の連射設定を取得
		if( Param->GetWeapon(paramdata.ChangeWeaponID, &paramdata) == 1 ){ return 0; }
		burstcntN = paramdata.burstcnt;

		if( longattack == false ){
			//近距離攻撃中で、現在SEMI・切り替えるとFULLになるなら
			if( (burstcntS == 1)&&(burstcntN != 1) ){
				ObjMgr->ChangeWeaponID(ctrlid);	//切り替える
				return 4;
			}
		}
		else{
			//遠距離攻撃中で、現在FULL・切り替えるとSEMIになるなら
			if( (burstcntS != 1)&&(burstcntN == 1) ){
				ObjMgr->ChangeWeaponID(ctrlid);	//切り替える
				return 4;
			}
		}
	}

	return 0;
}

//! @brief 手榴弾を投げる
//! @return 処理中：0　投げ終わった：1　手榴弾を持っていない：2
//! @attention 手榴弾を投げるパスに応じた処理です。敵発見時の攻撃はAction()関数を参照してください。
int AIcontrol::ThrowGrenade()
{
	int selectweapon;
	class weapon *weapon[TOTAL_HAVEWEAPON];
	int weaponid, nbs, i;
	pointdata pdata;
	float posy2;
	float atan_rx, atan_ry;

	for(int i=0; i<TOTAL_HAVEWEAPON; i++){
		weapon[i] = NULL;
	}

	//手榴弾を持っているか確認
	ctrlhuman->GetWeapon(&selectweapon, weapon, NULL, NULL);
	for(i=0; i<TOTAL_HAVEWEAPON; i++){
		if( weapon[i] != NULL ){
			weapon[i]->GetParamData(&weaponid, NULL, &nbs);
			if( weaponid == ID_WEAPON_GRENADE ){
				break;
			}
		}
	}

	//手榴弾に切り替えられるなら、切り替える
	if( (i != TOTAL_HAVEWEAPON)&&(i != selectweapon) ){
		ObjMgr->ChangeHaveWeapon(ctrlid, i);
	}

	//パスと人の高さを取得
	MoveNavi->GetPathPointData(&pdata);
	posy2 = posy + VIEW_HEIGHT;

	//一度全ての動きを止める
	ObjDriver->ResetMode();

	//目標地点への角度を求める
	CheckTargetAngle(posx, posy2, posz, rx*-1 + (float)M_PI/2, ry, pdata.x, pdata.y, pdata.z, 0.0f, &atan_rx, &atan_ry, NULL);

	//手榴弾を持っていなければ
	if( i == TOTAL_HAVEWEAPON ){
		//※手榴弾を持っていない場合でも、このフレームではその方向へ向かせるよう処理する。
		//　この処理により、手榴弾を持っていない状態で かつ手榴弾投げの無限ループに陥った場合、手榴弾投げパスの方角を向き続ける。
		//　（本家XOPSの仕様）

		//旋回
		if( atan_rx > 0.0f ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
		}
		if( atan_rx < 0.0f ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
		}
		ArmAngle();

		return 2;
	}

	//旋回
	if( atan_rx > 0.0f ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNLEFT);
	}
	if( atan_rx < 0.0f ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNRIGHT);
	}
	if( atan_ry > 0.0f ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
	}
	if( atan_ry < 0.0f ){
		ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
	}

	//照準に定まっているか確認
	if( (fabsf(atan_rx) < DegreeToRadian(1.5f))&&(fabsf(atan_ry) < DegreeToRadian(1.5f)) ){
		//連射させない
		if( (ctrlhuman->GetWeaponBlazingmode() == false)&&(EventWeaponShot == true) ){ return 0; }

		//角度を設定
		ctrlhuman->SetRxRy(rx, ry);

		//投げる
		if( ObjMgr->ShotWeapon(ctrlid) != 0 ){
			EventWeaponShot = true;
			return 1;
		}
	}

	return 0;
}

//! @brief 腕の角度を設定
void AIcontrol::ArmAngle()
{
	ObjDriver->DelModeFlag(AI_CTRL_TURNUP);
	ObjDriver->DelModeFlag(AI_CTRL_TURNDOWN);

	if( ctrlhuman->GetMainWeaponTypeNO() == ID_WEAPON_NONE ){	//手ぶら
		//下に向け続ける
		ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
	}
	else if( (battlemode == AI_CAUTION)&&(cautioncnt > 0) ){	//警戒中
		float addry2 = 0.0f - ry;

		//旋回
		if( addry2 > DegreeToRadian(1.0f) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
		}
		if( addry2 < DegreeToRadian(-1.0f) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
		}
	}
	else{									//平常時で武器所有中
		//旋回
		if( ry < DegreeToRadian(-32) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNUP);
		}
		if( ry > DegreeToRadian(-28) ){
			ObjDriver->SetModeFlag(AI_CTRL_TURNDOWN);
		}
	}
}

//! @brief 敵を探す
int AIcontrol::SearchEnemy()
{
	//非戦闘化フラグが有効なら敵を見つけない
	if( NoFight == true ){
		return 0;
	}

	if( battlemode == AI_ACTION ){ return 0; }

	int weaponid = ctrlhuman->GetMainWeaponTypeNO();
	int weaponscope = 0;
	ScopeParameter sparam;
	int searchloops;
	float maxDist;
	float A_rx, A_ry, B_rx, B_ry;

	//武器を持っていれば、スコープモードを取得
	if( weaponid != ID_WEAPON_NONE ){
		WeaponParameter Wparam;

		Param->GetWeapon(weaponid, &Wparam);
		weaponscope = Wparam.scopemode;
	}

	//敵の探索回数と探索範囲（距離と角度）を設定
	if( battlemode == AI_NORMAL ){
		searchloops = (LevelParam->search) * AI_TOTALHUMAN_SCALE;

		Param->GetScopeParam(weaponscope, &sparam);
		maxDist = sparam.AI_AddSearchEnemyMaxDist_Normal;
		maxDist += 12.0f*((LevelParam->search)-2) + 350.0f;
		A_rx = DegreeToRadian(110);
		A_ry = DegreeToRadian(60);
		B_rx = DegreeToRadian(60);
		B_ry = DegreeToRadian(40);
	}
	else {	//battlemode == AI_CAUTION
		searchloops = (LevelParam->search) * AI_TOTALHUMAN_SCALE + 4;

		Param->GetScopeParam(weaponscope, &sparam);
		maxDist = sparam.AI_AddSearchEnemyMaxDist_Caution;
		maxDist += 15.0f*((LevelParam->search)-2) + 420.0f;
		A_rx = DegreeToRadian(130);
		A_ry = DegreeToRadian(80);
		B_rx = DegreeToRadian(80);
		B_ry = DegreeToRadian(50);
	}

	//指定回数、敵を探索
	for(int i=0; i<searchloops; i++){
		int targetid = GetRand(MAX_HUMAN);

		if( CheckLookEnemy(targetid, A_rx, A_ry, AI_CHECKSHORTATTACK_DIST, NULL) == true ){
			longattack = false;
			return 1;
		}

		if( CheckLookEnemy(targetid, B_rx, B_ry, maxDist, NULL) == true ){
			if( GetRand(4) == 0 ){
				if( MoveNavi->GetRun2() == true ){ longattack = false; }
				else{ longattack = true; }
				return 2;
			}
		}
	}
	return 0;
}

//! @brief 敵を探す（遠距離攻撃中に近距離を探す）
int AIcontrol::SearchShortEnemy()
{
	float A_rx, A_ry;
	A_rx = DegreeToRadian(100);
	A_ry = DegreeToRadian(52);

	for(int i=0; i<3; i++){
		int targetid = GetRand(MAX_HUMAN);

		if( CheckLookEnemy(targetid, A_rx, A_ry, AI_CHECKSHORTATTACK_DIST, NULL) == true ){
			longattack = false;
			return 1;
		}
	}
	return 0;
}

//! @brief 敵が見えるか判定
bool AIcontrol::CheckLookEnemy(int id, float search_rx, float search_ry, float maxDist, float *out_minDist)
{
	if( ObjMgr == NULL ){ return false; }

	//人のオブジェクトを取得
	class human* thuman;
	thuman = ObjMgr->GetHumanObject(id);
	if( thuman == NULL ){ return false; }

	//同名関数をオーバーロード
	return CheckLookEnemy(thuman, search_rx, search_ry, maxDist, out_minDist);
}

//! @brief 敵が見えるか判定
bool AIcontrol::CheckLookEnemy(class human* thuman, float search_rx, float search_ry, float maxDist, float *out_minDist)
{
	//return false;

	//無効なデータなら終了
	if( ctrlhuman == NULL ){ return false; }
	if( thuman == NULL ){ return false; }
	if( thuman->GetEnableFlag() == false ){ return false; }
	if( thuman->GetDeadFlag() == true ){ return false; }

	//自分と敵の座標を取得
	int ctrlteam, targetteam;
	float tx, ty, tz;
	ctrlhuman->GetParamData(NULL, NULL, NULL, &ctrlteam);
	thuman->GetParamData(NULL, NULL, NULL, &targetteam);
	thuman->GetPosData(&tx, &ty, &tz, NULL);

	//自分と敵が同一人物でなければ
	if( ctrlteam != targetteam ){
		float mrx, mry;
		float Dist2 , Dist;
		float Dist_dummy; 

		//距離を判定し、角度も取得
		if( CheckTargetAngle(posx, posy, posz, rx*-1 + (float)M_PI/2, 0.0f, tx, ty, tz, maxDist, &mrx, &mry, &Dist2) == true ){
			//角度上、視界に入っていれば
			if( (fabsf(mrx) < search_rx/2)&&(fabsf(mry) < search_ry/2) ){
				float vx, vy, vz;

				Dist = sqrtf(Dist2);

				//ベクトルを取得
				vx = (tx - posx)/Dist;
				vy = (ty - posy)/Dist;
				vz = (tz - posz)/Dist;

				//ブロックが遮っていなければ　（レイで当たり判定を行い、当たっていなければ）
				if( CollD->CheckALLBlockIntersectRay(posx, posy + VIEW_HEIGHT, posz, vx, vy, vz, NULL, NULL, &Dist_dummy, Dist) == false ){
					if( out_minDist != NULL ){ *out_minDist = Dist; }
					enemyhuman = thuman;
					return true;
				}
			}
		}
	}
	return false;
}

//! @brief 死体があるか確認
bool AIcontrol::CheckCorpse(int id)
{
	//クラス設定がおかしければ処理しない
	if( ObjMgr == NULL ){ return false; }
	if( ctrlhuman == NULL ){ return false; }

	if( (id < 0)||(MAX_HUMAN <= id) ){ return false; }

	//ターゲットのクラスを取得
	class human* thuman;
	thuman = ObjMgr->GetHumanObject(id);
	if( thuman == NULL ){ return false; }
	if( thuman->GetEnableFlag() == false ){ return false; }

	//死亡していれば
	if( thuman->GetDeadFlag() == true ){

		//チーム番号を取得
		int ctrlteam, targetteam;
		ctrlhuman->GetParamData(NULL, NULL, NULL, &ctrlteam);
		thuman->GetParamData(NULL, NULL, NULL, &targetteam);

		//味方ならば
		if( ctrlteam == targetteam ){
			float tposx, tposy, tposz;
			float atan;

			//座標から距離を取得
			thuman->GetPosData(&tposx, &tposy, &tposz, NULL);

			//距離と角度を計算
			if( CheckTargetAngle(posx, posy, posz, rx*-1 + (float)M_PI/2, 0.0f, tposx, tposy, tposz, 22.0f, &atan, NULL, NULL) == true ){
				if( fabsf(atan) < DegreeToRadian(40) ){
					return true;
				}
			}
		}
	}
	return false;
}

//! @brief パスによる移動
void AIcontrol::MovePath()
{
	int movemode, pointmode;
	MoveNavi->GetTargetPos(NULL, NULL, NULL, &movemode, &pointmode);

	if( movemode == AI_NAVI_MOVE_NULL ){			//異常なパス
		return;
	}

	if( CheckTargetPos(false) == false ){
		MoveTarget(false);
	}
	else{
		if( (pointmode == AI_NAVI_POINT_WAIT)||(pointmode == AI_NAVI_POINT_TRACKING) ){
			TurnSeen();
		}
		else if( pointmode == AI_NAVI_POINT_STOP_5SEC ){
			if( waitcnt < ((int)GAMEFPS)*5 ){
				if( StopSeen() == true ){
					waitcnt += 1;
				}
			}
			else{
				waitcnt = 0;
				MoveNavi->MovePathNextState();
				MoveNavi->MovePathNowState();
			}
		}
		else{	//pointmode == AI_NAVI_POINT_NULL
			waitcnt = 0;
			MoveNavi->MovePathNextState();
			MoveNavi->MovePathNowState();
		}
	}
}

//! @brief 攻撃メイン処理
//! @return 不変：false　変更：true
bool AIcontrol::ActionMain()
{
	int newbattlemode = AI_ACTION;

	//攻撃処理
	Action();

	//ランダムパスなら処理実行　※「優先的な走り」を想定
	if( MoveNavi->GetMoveMode() == AI_RANDOM ){
		MoveNavi->MovePathNextState();
		MoveNavi->MovePathNowState();
	}

	if( MoveNavi->GetRun2() == true ){				//優先的な走り
		//目標地点へ移動
		if( CheckTargetPos(false) == true ){
			//newbattlemode = AI_NORMAL;

			//この目標地点を、警戒状態解除時の戻る位置に設定する。
			MoveNavi->GetTargetPos(&cautionback_posx, &cautionback_posz, NULL, NULL, NULL);

			MoveNavi->MovePathNextState();
			MoveNavi->MovePathNowState();
		}
		else{
			MoveTarget2(false);
		}
	}
	else{									//優先的な走り 以外
		//遠距離攻撃中なら、近くの敵を探す
		if( longattack == true ){
			SearchShortEnemy();
		}
	}

	//攻撃終了判定
	if( ActionCancel() == true ){
		enemyhuman = NULL;

		if( MoveNavi->GetRun2() == true ){
			newbattlemode = AI_NORMAL;
		}
		else{
			newbattlemode = AI_CAUTION;
			cautioncnt = 160;
			FaceCaution_flag = false;
			FaceCaution_rx = 0.0f;
		}
	}

	//設定を判定
	if( battlemode != newbattlemode ){
		battlemode = newbattlemode;
		return true;
	}
	return false;
}

//! @brief 警戒メイン処理
//! @return 不変：false　変更：true
bool AIcontrol::CautionMain()
{
	int newbattlemode = AI_CAUTION;
	float caution_rx;

	//座標とチーム番号を取得
	int teamid;
	ctrlhuman->GetParamData(NULL, NULL, NULL, &teamid);

	//被弾と音の状況を取得
	bool HitFlag = ctrlhuman->CheckHit(&caution_rx);
	soundlist soundlist[MAX_SOUNDMGR_LIST];
	int soundlists = GameSound->GetWorldSound(posx, posy + VIEW_HEIGHT, posz, teamid, soundlist);

	//非戦闘化フラグが有効なら、音は聞こえないことにする
	if( NoFight == true ){
		soundlists = 0;
	}

	//メイン処理
	if( enemyhuman != NULL ){		//既に敵を見つけていれば
		newbattlemode = AI_ACTION;
		actioncnt = 0;
	}
	else if( SearchEnemy() != 0 ){	//敵が見つかれば
		newbattlemode = AI_ACTION;
		actioncnt = 0;
	}
	else if( HitFlag == true ){		//被弾した
		cautioncnt = 160;					//警戒を再開
		FaceCaution_flag = true;
		FaceCaution_rx = caution_rx;
	}
	else if( soundlists > 0 ){		//音が聞こえた
		cautioncnt = 160;					//警戒を再開
	}
	else if( cautioncnt == 0 ){		//警戒を終了するなら
		if( CheckTargetPos(true) == false ){				//警戒開始地点より離れているか
			MoveTarget(true);				//警戒開始地点に近づく
		}
		else{
			newbattlemode = AI_NORMAL;
			FaceCaution_flag = false;
			FaceCaution_rx = 0.0f;

			//警戒待ちパスなら次へ進める
			pointdata pdata;
			MoveNavi->GetPathPointData(&pdata);
			if( (pdata.p1 == POINT_P1TYPE_AIPATH)&&(pdata.p2 == 4) ){
				MoveNavi->MovePathNextState();
				MoveNavi->MovePathNowState();
			}
		}
	}
	else if( (cautioncnt < 100)&&(FaceCaution_flag == false) ){		//100フレームを切ったら、ランダムに警戒終了（カウント：0に）
		if( GetRand(50) == 0 ){ cautioncnt = 0; }
	}
	else{ cautioncnt -= 1; }

	//追尾中で対象から離れすぎたら、ランダムに警戒終了
	if( (MoveNavi->GetMoveMode() == AI_TRACKING)&&(GetRand(3) == 0) ){
		float x, z;
		float tx, tz;
		class human *targethuman = ObjMgr->GetHumanObject(MoveNavi->GetTargetHumanID());
		targethuman->GetPosData(&tx, NULL, &tz, NULL);
		x = posx - tx;
		z = posz - tz;
		if( (x*x + z*z) > 25.0f*25.0f ){
			cautioncnt = 0;
		}
	}

	//回転と腕の角度
	TurnSeen();
	ArmAngle();

	//設定を判定
	if( battlemode != newbattlemode ){
		battlemode = newbattlemode;
		return true;
	}
	return false;
}

//! @brief 通常メイン処理
//! @return 不変：false　変更：true
bool AIcontrol::NormalMain()
{
	int newbattlemode = AI_NORMAL;
	float caution_rx;
	int EventThrowGrenade = 0;

	MoveNavi->MovePathNowState();
	enemyhuman = NULL;

	//座標とチーム番号を取得
	int teamid;
	ctrlhuman->GetParamData(NULL, NULL, NULL, &teamid);

	//被弾と音の状況を取得
	bool HitFlag = ctrlhuman->CheckHit(&caution_rx);
	soundlist soundlist[MAX_SOUNDMGR_LIST];
	int soundlists = GameSound->GetWorldSound(posx, posy + VIEW_HEIGHT, posz, teamid, soundlist);

	//非戦闘化フラグが有効なら、音は聞こえないことにする
	if( NoFight == true ){
		soundlists = 0;
	}

	//ランダムパスなら処理実行
	if( MoveNavi->GetMoveMode() == AI_RANDOM ){
		MoveNavi->MovePathNextState();
		MoveNavi->MovePathNowState();
	}

	//手榴弾パス
	if( MoveNavi->GetMoveMode() == AI_GRENADE ){
		EventThrowGrenade = ThrowGrenade();
		if( EventThrowGrenade != 0 ){
			MoveNavi->MovePathNextState();
			MoveNavi->MovePathNowState();
		}
	}
	if( EventThrowGrenade != 1 ){
		EventWeaponShot = false;
	}

	if( MoveNavi->GetMoveMode() == AI_RUN2 ){		//優先的な走りの処理
		//敵を見つけたら攻撃に入る
		if( SearchEnemy() != 0 ){
			newbattlemode = AI_ACTION;
			//cautionback_posx = posx;
			//cautionback_posz = posz;
		}
		else{
			MovePath();		//移動実行
		}
	}
	else{							//優先的な走り以外の処理
		//警戒判定に入る処理
		if( HitFlag == true ){							//被弾した
			newbattlemode = AI_CAUTION;
			cautioncnt = 160;
			cautionback_posx = posx;
			cautionback_posz = posz;
			FaceCaution_flag = true;
			FaceCaution_rx = caution_rx;
		}
		else if(
			(SearchEnemy() != 0)||						//敵を見つけた
			(soundlists > 0)||							//音が聞こえた
			(CheckCorpse( GetRand(MAX_HUMAN) ) == true)	//死体を見つけた
		){
			newbattlemode = AI_CAUTION;
			cautioncnt = 160;
			cautionback_posx = posx;
			cautionback_posz = posz;
			FaceCaution_flag = false;
			FaceCaution_rx = 0.0f;
		}
		else{
			MovePath();		//移動実行
		}
	}

	//腕の角度を設定
	if( MoveNavi->GetMoveMode() != AI_GRENADE ){
		ArmAngle();
	}

	//設定を判定
	if( battlemode != newbattlemode ){
		battlemode = newbattlemode;
		return true;
	}
	return false;
}

//! @brief 初期化系関数
void AIcontrol::Init()
{
	//クラス設定がおかしければ処理しない
	if( ctrlhuman == NULL ){ return; }
	if( blocks == NULL ){ return; }
	if( CollD == NULL ){ return; }

	//使用されていない人なら処理しない
	if( ctrlhuman->GetEnableFlag() == false ){ return; }

	//ステートを初期化
	NoFight = false;
	battlemode = AI_NORMAL;
	enemyhuman = NULL;
	FaceCaution_flag = false;
	FaceCaution_rx = 0.0f;
	total_move = 0.0f;
	waitcnt = 0;
	gotocnt = 0;
	cautioncnt = 0;
	actioncnt = 0;
	longattack = false;
	EventWeaponShot = false;

	MoveNavi->Init();
	ObjDriver->Init();

	//AIレベルと設定値を取得
	int paramid;
	HumanParameter paramdata;
	ctrlhuman->GetParamData(&paramid, NULL, NULL, NULL);
	Param->GetHuman(paramid, &paramdata);
	AIlevel = paramdata.AIlevel;
	Param->GetAIlevel(AIlevel, &LevelParam);

	//次のポイントを検索
	//MoveNavi->MovePathNextState();
	MoveNavi->MovePathNowState();
}

//! @brief AIレベルを設定
//! @param level AIレベル
void AIcontrol::SetAIlevel(int level)
{
	if( (level < -1)||(TOTAL_PARAMETERINFO_AILEVEL <= level) ){ return; }

	if( level == -1 ){
		//AIレベルを取得
		int paramid;
		HumanParameter paramdata;
		ctrlhuman->GetParamData(&paramid, NULL, NULL, NULL);
		Param->GetHuman(paramid, &paramdata);
		AIlevel = paramdata.AIlevel;
	}
	else{
		AIlevel = level;
	}

	//設定値を取得
	Param->GetAIlevel(AIlevel, &LevelParam);
}

//! @brief AIレベルを取得
//! @return AIレベル
int AIcontrol::GetAIlevel()
{
	return AIlevel;
}

//! @brief 指定した場所へ待機させる
//! @param px X座標
//! @param pz Z座標
//! @param rx 重視する向き
//! @attention 移動パスに関わらず、指定した座標への待機を強制します。Init()関数を再度実行するまで元に戻せません。
void AIcontrol::SetHoldWait(float px, float pz, float rx)
{
	MoveNavi->SetHoldWait(px, pz, rx);
}

//! @brief 指定した人を追尾させる
//! @param id 人のデータ番号
//! @attention 移動パスに関わらず、指定した人への追尾を強制します。Init()関数を再度実行するまで元に戻せません。
void AIcontrol::SetHoldTracking(int id)
{
	if( (0 <= id)&&(id < MAX_HUMAN) ){ MoveNavi->SetHoldTracking(id); }
}

//! @brief 強制的に警戒させる
//! @warning 優先的な走り を実行中の場合、この関数は何もしません。
void AIcontrol::SetCautionMode()
{
	//優先的な走りならば何もしない
	if( MoveNavi->GetRun2() == true ){ return; }

	if( battlemode == AI_NORMAL ){
		cautionback_posx = posx;
		cautionback_posz = posz;
	}
	battlemode = AI_CAUTION;
	cautioncnt = 160;
	FaceCaution_flag = false;
	FaceCaution_rx = 0.0f;
}

//! @brief 非戦闘化フラグを設定
//! @param flag true：戦闘を行わない（非戦闘化）　false：戦闘を行う（通常）
//! @attention フラグを有効にすると敵を認識しなくなります。
void AIcontrol::SetNoFightFlag(bool flag)
{
	NoFight = flag;
}

//! @brief 戦闘モード取得
//! @param *mode 戦闘モード（数字）を受け取るポインタ
//! @param *modestr 戦闘モード名の文字列を受け取るポインタ
void AIcontrol::GetBattleMode(int *mode, char *modestr)
{
	if( mode != NULL ){
		*mode = battlemode;
	}
	if( modestr != NULL ){
		switch(battlemode){
			case 0: strcpy(modestr, "AI_DEAD"); break;
			case 1: strcpy(modestr, "AI_ACTION"); break;
			case 2: strcpy(modestr, "AI_CAUTION"); break;
			case 3: strcpy(modestr, "AI_NORMAL"); break;
			default: strcpy(modestr, "");
		}
	}
}

//! @brief 攻撃対象の人データ番号
//! @return 人データ番号（ターゲットがいない場合は -1）
int AIcontrol::GetEnemyHumanID()
{
	if( enemyhuman == NULL ){
		return -1;
	}
	//else{
		return ObjMgr->GetHumanObjectID(enemyhuman);
	//}
}

//! @brief 移動する目標地点（ターゲット）を取得
//! @param posx 目標地点のX座標を受け取るポインタ
//! @param posz 目標地点のZ座標を受け取るポインタ
//! @param movemode 目標地点への移動モードを受け取るポインタ
void AIcontrol::GetMoveTargetPos(float *posx, float *posz, int *movemode)
{
	MoveNavi->GetTargetPos(posx, posz, NULL, movemode, NULL);

	//「優先的な走り」ではない＋攻撃中・警戒中ならば、警戒後に戻る位置を返す。
	if( MoveNavi->GetRun2() == false ){
		if( (battlemode == AI_ACTION)||(battlemode == AI_CAUTION) ){
			*posx = cautionback_posx;
			*posz = cautionback_posz;
		}
	}
}

//! @brief 現在読み込んでいるポイントデータパス
//! @param out_data ポイントデータを受け取るポインタ
void AIcontrol::GetPathPointData(pointdata *out_data)
{
	MoveNavi->GetPathPointData(out_data);
}

//! @brief 処理系関数
void AIcontrol::Process()
{
	//クラス設定がおかしければ処理しない
	if( ctrlhuman == NULL ){ return; }
	if( blocks == NULL ){ return; }
	if( CollD == NULL ){ return; }

	//無効な人クラスなら処理しない
	if( ctrlhuman->GetEnableFlag() == false ){ return; }

	//死亡したら
	if( ctrlhuman->GetHP() <= 0 ){
		battlemode = AI_DEAD;
		return;
	}

	//HPが0でないのに 死亡したことになってる　・・・生き返った？
	if( battlemode == AI_DEAD ){
		battlemode = AI_NORMAL;
		MoveNavi->MovePathNowState();
	}

	//座標と角度を取得
	ctrlhuman->GetPosData(&posx, &posy, &posz, NULL);
	ctrlhuman->GetRxRy(&rx, &ry);

	//ランダムに動作を止める
	CancelMoveTurn();

	//攻撃中か警戒中ならば
	if( (battlemode == AI_ACTION)||(battlemode == AI_CAUTION) ){
		//武器を持つ
		HaveWeapon();
	}

	//主計算実行
	if( battlemode == AI_ACTION ){			//攻撃中
		ActionMain();
	}
	else if( battlemode == AI_CAUTION ){	//警戒中
		CautionMain();
	}
	else{									//平常時
		NormalMain();
	}

	//移動・方向転換処理
	ObjDriver->ControlObject();

	//武器を取り扱い
	ControlWeapon();
}

//! @brief コンストラクタ
AIMoveNavi::AIMoveNavi(class ObjectManager *in_ObjMgr, int in_ctrlid, class PointDataInterface *in_Points)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;
	Points = in_Points;

	movemode = AI_WAIT;
	hold = false;
	path_pointid = 0;
	target_humanid = -1;
	target_posx = 0.0f;
	target_posz = 0.0f;
	target_rx = 0.0f;
}

//! @brief ディストラクタ
AIMoveNavi::~AIMoveNavi()
{}

//! @brief 対象クラスを設定
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void AIMoveNavi::SetClass(class ObjectManager *in_ObjMgr, int in_ctrlid, class PointDataInterface *in_Points)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;
	Points = in_Points;
}

//! @brief 初期化系関数
void AIMoveNavi::Init()
{
	movemode = AI_NULL;
	hold = false;
	path_pointid = 0;
	target_humanid = -1;

	ObjMgr->GetHumanObject(ctrlid)->GetParamData(NULL, &path_pointid, NULL, NULL);
	MovePathNextState();
}

//! @brief 移動する目標地点を適用
//! @return 完了：true　失敗：false
//! @attention 追尾時に対象人物の座標を反映させるため、本関数を毎フレーム呼び出して、最新の位置情報を計算する必要があります。
bool AIMoveNavi::MovePathNowState()
{
	if( hold == true ){
		if( movemode == AI_TRACKING ){
			class human *targethuman;
			targethuman = ObjMgr->GetHumanObject(target_humanid);
			targethuman->GetPosData(&target_posx, NULL, &target_posz, &target_rx);
		}
		return true;
	}


	//ポイントの情報を取得
	pointdata pdata;
	if( Points->Getdata(&pdata, path_pointid) != 0 ){
		movemode = AI_NULL;
		return false;
	}

	//移動パスなら～
	if( pdata.p1 == POINT_P1TYPE_AIPATH ){
		//移動ステート設定
		switch(pdata.p2){
			case 0: movemode = AI_WALK; break;
			case 1: movemode = AI_RUN; break;
			case 2: movemode = AI_WAIT; break;
			case 3: movemode = AI_TRACKING; break;
			case 4: movemode = AI_WAIT; break;
			case 5: movemode = AI_STOP_5SEC; break;
			case 6: movemode = AI_GRENADE; break;
			case 7: movemode = AI_RUN2; break;
			default: break;
		}

		if( movemode == AI_TRACKING ){
			class human *targethuman;

			if( target_humanid == -1 ){
				signed short int nextpointp4 = pdata.p3;

				//ポイント（人）の情報を取得
				if( Points->SearchPointdata(&pdata, 0x08, 0, 0, 0, nextpointp4, 0) == 0 ){
					return false;
				}

				//人を検索してクラスを取得
				targethuman = ObjMgr->SearchHuman(pdata.p4);
				if( targethuman == NULL ){ return false; }

				//人のデータ番号を取得
				target_humanid = ObjMgr->GetHumanObjectID(targethuman);
			}

			targethuman = ObjMgr->GetHumanObject(target_humanid);
			targethuman->GetPosData(&target_posx, NULL, &target_posz, &target_rx);
		}
		else{
			//情報適用
			target_posx = pdata.x;
			target_posz = pdata.z;
			target_rx = pdata.r;
		}

		return true;
	}

	//ランダムパスなら
	if( pdata.p1 == POINT_P1TYPE_RAND_AIPATH ){
		movemode = AI_RANDOM;
		return false;
	}

	movemode = AI_NULL;
	return false;
}

//! @brief 次の目標地点を検索
//! @return 完了：true　失敗：false
bool AIMoveNavi::MovePathNextState()
{
	//ポイントの情報を取得
	pointdata pdata;
	if( Points->Getdata(&pdata, path_pointid) != 0 ){
		movemode = AI_NULL;
		return false;
	}

	signed short int nextpointp4 = pdata.p3;

	//ランダムパス処理
	if( pdata.p1 == POINT_P1TYPE_RAND_AIPATH ){
		if( GetRand(2) == 0 ){
			nextpointp4 = pdata.p2;
		}
		else{
			nextpointp4 = pdata.p3;
		}
		movemode = AI_RANDOM;
	}

	//ポイントを検索
	if( Points->SearchPointdata(&pdata, 0x08, 0, 0, 0, nextpointp4, 0) == 0 ){
		return false;
	}

	path_pointid = pdata.id;

	return true;
}

//! @brief 指定した場所へ待機させる
//! @param px X座標
//! @param pz Z座標
//! @param rx 重視する向き
//! @attention 移動パスに関わらず、指定した座標への待機を強制します。Init()関数を再度実行するまで元に戻せません。
void AIMoveNavi::SetHoldWait(float px, float pz, float rx)
{
	movemode = AI_WAIT;
	hold = true;
	target_posx = px;
	target_posz = pz;
	target_rx = rx;
}

//! @brief 指定した人を追尾させる
//! @param id 人のデータ番号
//! @attention 移動パスに関わらず、指定した人への追尾を強制します。Init()関数を再度実行するまで元に戻せません。
void AIMoveNavi::SetHoldTracking(int id)
{
	if( (id < 0)||(MAX_HUMAN <= id) ){ return; }

	movemode = AI_TRACKING;
	hold = true;
	target_humanid = id;
}

//! @brief 移動モードを取得
//! @return 移動モード
int AIMoveNavi::GetMoveMode()
{
	return movemode;
}

//! @brief 優先的な走りフラグを取得
//! @return 優先的な走りである：true　優先的な走りでない：false
bool AIMoveNavi::GetRun2()
{
	if( movemode == AI_RUN2 ){
		return true;
	}
	//else{
		return false;
	//}
}

//! @brief ターゲット（人）のデータ番号を取得
//! @return 人のデータ番号
int AIMoveNavi::GetTargetHumanID()
{
	return target_humanid;
}

//! @brief 現在読み込んでいるポイントデータパス
//! @param out_data ポイントデータを受け取るポインタ
void AIMoveNavi::GetPathPointData(pointdata *out_data)
{
	Points->Getdata(out_data, path_pointid);
}

//! @brief 移動する目標地点（ターゲット）を取得
//! @param posx 目標地点のX座標を受け取るポインタ
//! @param posz 目標地点のZ座標を受け取るポインタ
//! @param rx 目標地点の水平角度を受け取るポインタ
//! @param out_movemode 目標地点への移動モードを受け取るポインタ
//! @param out_pointmode 目標地点のポイントモードを受け取るポインタ
//! @warning 本関数では座標は更新されません。先に MovePathNowState() 関数で座標を更新した後、本関数を呼び出してください。
void AIMoveNavi::GetTargetPos(float *posx, float *posz, float *rx, int *out_movemode, int *out_pointmode)
{
	if( posx != NULL ){ *posx = target_posx; }
	if( posz != NULL ){ *posz = target_posz; }
	if( rx != NULL ){ *rx = target_rx; }

	if( out_movemode != NULL ){
		if( (movemode == AI_WALK)||(movemode == AI_WAIT)||(movemode == AI_STOP_5SEC) ){
			*out_movemode = AI_NAVI_MOVE_WALK;
		}
		else if( movemode == AI_RUN ){
			*out_movemode = AI_NAVI_MOVE_RUN;
		}
		else if( movemode == AI_RUN2 ){
			*out_movemode = AI_NAVI_MOVE_RUN2;
		}
		else if( movemode == AI_TRACKING ){
			*out_movemode = AI_NAVI_MOVE_TRACKING;
		}
		else{
			*out_movemode = AI_NAVI_MOVE_NULL;
		}
	}

	if( out_pointmode != NULL ){
		if( movemode == AI_WAIT ){
			*out_pointmode = AI_NAVI_POINT_WAIT;
		}
		else if( movemode == AI_STOP_5SEC ){
			*out_pointmode = AI_NAVI_POINT_STOP_5SEC;
		}
		else if( movemode == AI_TRACKING ){
			*out_pointmode = AI_NAVI_POINT_TRACKING;
		}
		else if( movemode == AI_GRENADE ){	//手榴弾パス
			*out_pointmode = AI_NAVI_POINT_GRENADE;
		}
		else{								//移動パス・異常なパス
			*out_pointmode = AI_NAVI_POINT_NULL;
		}
	}
}

//! @brief コンストラクタ
AIObjectDriver::AIObjectDriver(class ObjectManager *in_ObjMgr, int in_ctrlid)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;

	moveturn_mode = 0;
	addrx = 0.0f;
	addry = 0.0f;
}

//! @brief ディストラクタ
AIObjectDriver::~AIObjectDriver()
{}

//! @brief 対象クラスを設定
//! @attention この関数で設定を行わないと、クラス自体が正しく機能しません。
void AIObjectDriver::SetClass(class ObjectManager *in_ObjMgr, int in_ctrlid)
{
	ObjMgr = in_ObjMgr;
	ctrlid = in_ctrlid;
}

//! @brief 初期化系関数
void AIObjectDriver::Init()
{
	addrx = 0.0f;
	addry = 0.0f;

	ResetMode();
}

//! @brief 移動回転制御フラグをクリア（一度全ての動きを止める）
void AIObjectDriver::ResetMode()
{
	moveturn_mode = 0;
}

//! @brief 移動回転制御フラグ 設定
//! @param flag 操作モードを表す定数（AIcontrolFlag列挙型）
void AIObjectDriver::SetModeFlag(int flag)
{
	SetFlag(moveturn_mode, flag);
}

//! @brief 移動回転制御フラグ 解除
//! @param flag 操作モードを表す定数（AIcontrolFlag列挙型）
void AIObjectDriver::DelModeFlag(int flag)
{
	DelFlag(moveturn_mode, flag);
}

//! @brief 移動回転制御フラグ 取得
//! @param flag 操作モードを表す定数（AIcontrolFlag列挙型）
bool AIObjectDriver::GetModeFlag(int flag)
{
	if( GetFlag(moveturn_mode, flag) == 0 ){
		return false;
	}
	//else{
		return true;
	//}
}

//! @brief 移動回転制御を実行
//! @attention 毎フレーム1回だけ呼び出してください。
void AIObjectDriver::ControlObject()
{
	float rx, ry;

	//移動の実行
	if( GetFlag(moveturn_mode, AI_CTRL_MOVEFORWARD) ){
		ObjMgr->MoveForward(ctrlid);
	}
	if( GetFlag(moveturn_mode, AI_CTRL_MOVEBACKWARD) ){
		ObjMgr->MoveBack(ctrlid);
	}
	if( GetFlag(moveturn_mode, AI_CTRL_MOVELEFT) ){
		ObjMgr->MoveLeft(ctrlid);
	}
	if( GetFlag(moveturn_mode, AI_CTRL_MOVERIGHT) ){
		ObjMgr->MoveRight(ctrlid);
	}
	if( GetFlag(moveturn_mode, AI_CTRL_MOVEWALK) ){
		ObjMgr->MoveWalk(ctrlid);
	}

	//方向転換の実行（回転速度の加算）
	if( GetFlag(moveturn_mode, AI_CTRL_TURNUP) ){
		addry += AI_ADDTURNRAD;
	}
	if( GetFlag(moveturn_mode, AI_CTRL_TURNDOWN) ){
		addry -= AI_ADDTURNRAD;
	}
	if( GetFlag(moveturn_mode, AI_CTRL_TURNLEFT) ){
		addrx -= AI_ADDTURNRAD;
	}
	if( GetFlag(moveturn_mode, AI_CTRL_TURNRIGHT) ){
		addrx += AI_ADDTURNRAD;
	}

	//角度を取得
	ObjMgr->GetHumanObject(ctrlid)->GetRxRy(&rx, &ry);

	//角度に加算
	rx += addrx;
	ry += addry;

	//縦の回転範囲を収める
	if( ry > DegreeToRadian(70) ){ ry = DegreeToRadian(70); }
	if( ry < DegreeToRadian(-70) ){ ry = DegreeToRadian(-70); }

	//角度を設定
	ObjMgr->GetHumanObject(ctrlid)->SetRxRy(rx, ry);

	//回転速度の減衰
	addrx *= 0.8f;
	addry *= 0.8f;

	//0.0fへ補正
	if( fabsf(addrx) < DegreeToRadian(0.2f) ){ addrx = 0.0f; }
	if( fabsf(addry) < DegreeToRadian(0.2f) ){ addry = 0.0f; }
}