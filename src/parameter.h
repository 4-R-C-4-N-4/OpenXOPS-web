//! @file parameter.h
//! @brief ParameterInfoクラスの宣言

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

// 人・武器・小物 設定データの確認作業
// 　Mandaさん　（2014年12月）

#ifndef PARAMETER_H
#define PARAMETER_H

#define TOTAL_PARAMETERINFO_HUMAN 48	//[OpenXOPS-web] was 43; +5 free-pack types			//!< 人の設定数
#define TOTAL_HUMANTEXTURE 34	//[OpenXOPS-web] was 30					//!< 人のテクスチャの合計枚数
#define TOTAL_PARAMETERINFO_WEAPON 28	//[OpenXOPS-web] was 23; +5 free-pack weapons			//!< 武器の設定数
#define TOTAL_PARAMETERINFO_SMALLOBJECT 12	//!< 小物の設定数
#define TOTAL_PARAMETERINFO_BULLET 6	//[OpenXOPS-web] was 2; +3 laser bolts, +1 thrown rock		//!< 銃弾オブジェクトの設定数
#define TOTAL_OFFICIALMISSION 58			//!< 標準ミッションの数
#define TOTAL_PARAMETERINFO_AILEVEL 6		//!< AIレベルの設定数
#define TOTAL_PARAMETERINFO_SCOPE 4			//!< スコープの設定数

#define ID_WEAPON_NONE 0		//!< 武器：なし として取り扱う武器番号
#define ID_WEAPON_GRENADE 13	//!< 手榴弾として取り扱う武器番号
#define ID_WEAPON_CASE 15		//!< ケースとして取り扱う武器番号

#define OFFICIALMISSION_BD1 "temp.bd1"		//!< 標準ミッションのBD1ファイル名

#define SMALLOBJECT_SCALE 5.0f				//!< 小物の描画倍率

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 1		//!< Select include file.
#endif
#include "main.h"

//! 人設定用の構造体
typedef struct
{
	int texture;		//!< テクスチャ番号
	int model;			//!< モデル番号
	int hp;				//!< 体力
	int AIlevel;		//!< AIレベル
	int AItrackability;	//!< AI攻撃時の追従性（ゾンビのみ）
	int Weapon[TOTAL_HAVEWEAPON];	//!< 武器
	int type;			//!< 種類
} HumanParameter;

//! 武器設定用の構造体
typedef struct
{
	const char *name;		//!< 武器名
	const char *model;		//!< モデルファイル名
	const char *texture;	//!< テクスチャファイル名
	int attacks;		//!< 攻撃力
	int penetration;	//!< 貫通力
	int blazings;		//!< 連射間隔
	int speed;			//!< 弾速
	int nbsmax;			//!< 装弾数
	int reloads;		//!< リロード時間
	int reaction;		//!< 反動角度
	int ErrorRangeMIN;	//!< 弾道誤差 最小
	int ErrorRangeMAX;	//!< 弾道誤差 最大
	float mx;		//!< 銃の位置：X
	float my;		//!< 銃の位置：Y
	float mz;		//!< 銃の位置：Z
	float flashx;	//!< マズルフラッシュの位置：X
	float flashy;	//!< マズルフラッシュの位置：Y
	float flashz;	//!< マズルフラッシュの位置：Z
	float yakkyou_px;	//!< 薬莢排出 位置：X
	float yakkyou_py;	//!< 薬莢排出 位置：Y
	float yakkyou_pz;	//!< 薬莢排出 位置：Z
	float yakkyou_sx;	//!< 薬莢排出 初速：X
	float yakkyou_sy;	//!< 薬莢排出 初速：Y
	float yakkyou_size;	//!< 薬莢 描画倍率（※負数で攻撃力から算出）
	int yakkyou_delay;	//!< 薬莢排出 遅延カウント
	int burstcnt;		//!< 連射弾数（0：フルオート　1：セミオート　2以上：バーストモード）
	int scopemode;		//!< スコープモード
	float size;			//!< 描画倍率
	int soundid;		//!< 発砲音の番号
	int soundvolume;	//!< 発砲音の音量
	bool silencer;		//!< サイレンサーの有無
	int WeaponP;		//!< 装備方法
	int ChangeWeaponID;		//!< 切り替える武器対象
	int ChangeWeaponCnt;	//!< 武器切り替え時間
	int pellet;			//!< 発射弾数（ペレット数）
	int bullettype;		//!< [OpenXOPS-web] bullet visual (Bullet[] index; 0 = normal)
	bool throwaway;		//!< [OpenXOPS-web] thrown item: no reserve, leaves the hand when empty
} WeaponParameter;

//! 小物設定用の構造体
typedef struct
{
	const char *model;		//!< モデル名
	const char *texture;	//!< テクスチャ名
	int decide;		//!< 当たり判定の大きさ
	int hp;			//!< 耐久力
	int sound;		//!< 命中時の効果音番号
	int jump;		//!< 飛び具合
} SmallObjectParameter;

//! 銃弾オブジェクト用の構造体
typedef struct
{
	const char *model;		//!< モデル名
	const char *texture;	//!< テクスチャ名
	float size;				//!< 描画倍率
} BulletParameter;

//! 標準ミッション設定用の構造体
typedef struct
{
	const char *name;				//!< ミッション識別名
	const char *fullname;			//!< ミッション正式名称
	const char *directory;			//!< データディレクトリ
	const char *txt;				//!< ファイル名（.pd1/.txt）
	bool collision;			//!< 追加の当たり判定を示すフラグ
	bool screen;			//!< 画面を暗くするフラグ
} OfficialMissionParameter;

//! AIレベルによる設定（性能値）用構造体
typedef struct
{
	int aiming;			//!< エイミング能力
	int attack;			//!< 攻撃性
	int search;			//!< 発見能力
	int limitserror;	//!< 許容誤差
} AIParameter;

//! スコープ設定用の構造体
typedef struct
{
	float ViewAngle;				//!< 視野角
	float MouseRange;				//!< 操作感度
	int ScopeGunsight;				//!< スコープ使用時の照準表示（0：通常、1：簡易、2：精密）
	bool NoScopeGunsight;			//!< 通常時の照準表示
	int WeaponNoScopeReaction;		//!< 通常発砲時の反動
	float WeaponMotionReactionY;	//!< 発砲時の反動（モーション）
	bool WeaponRecoil_ScopeOnly;	//!< スコープ 発砲時の反動 スコープ時のみ
	float WeaponRecoil_ScopeRx;		//!< スコープ 発砲時の反動 横軸　※度数法 0.1f刻み
	float WeaponRecoil_ScopeRyMin;	//!< スコープ 発砲時の反動 縦軸最小　※度数法 0.1f刻み
	float WeaponRecoil_ScopeRyMax;	//!< スコープ 発砲時の反動 縦軸最大　※度数法 0.1f刻み
	float AI_ShotAngle;							//!< AI 発砲時に敵を捉える角度
	float AI_ShotAngleLong;						//!< AI 発砲時に敵を捉える角度（長距離）
	float AI_AddSearchEnemyMaxDist_Normal;		//!< AI 敵を見つける距離 加算分（通常時）
	float AI_AddSearchEnemyMaxDist_Caution;		//!< AI 敵を見つける距離 加算分（警戒時）
} ScopeParameter;

//! @brief 設定された値を管理するクラス
//! @details 予め固定された設定値を管理します。
//! @details 具体的には、人・武器・小物から、標準ミッションに関係する設定値などです。
class ParameterInfo
{
	HumanParameter *Human;								//!< 人の情報を格納するポインタ
	const char *HumanTexturePath[TOTAL_HUMANTEXTURE];	//!< 人のテクスチャを格納するポインタ
	WeaponParameter *Weapon;							//!< 武器の情報を格納するポインタ
#ifdef ENABLE_BUG_HUMANWEAPON
	WeaponParameter *BugWeapon;							//!< バグ武器の情報を格納するポインタ
#endif
	SmallObjectParameter *SmallObject;					//!< 小物の情報を格納するポインタ
	BulletParameter *Bullet;							//!< 銃弾オブジェクトの情報を格納するポインタ
	OfficialMissionParameter *MissionData;				//!< 標準ミッションの情報を格納するポインタ
	AIParameter *AIlevel;								//!< AIの性能値
	ScopeParameter *Scope;								//!< スコープ設定を格納するポインタ

public:
	ParameterInfo();
	~ParameterInfo();
	void InitInfo();
	void DestroyInfo();
	int GetHuman(int id, HumanParameter *out_data);
	int GetHumanTexturePath(int id, char *out_str);
	int GetWeapon(int id, WeaponParameter *out_data);
#ifdef ENABLE_BUG_HUMANWEAPON
	int GetBugWeapon(int id, WeaponParameter *out_data);
#endif
	int GetSmallObject(int id, SmallObjectParameter *out_data);
	int GetBullet(int id, BulletParameter *out_data);
	int GetOfficialMission(int id, char *name, char *fullname, char* directory, char *txt, bool *collisionflag, bool *screenflag);
	int GetOfficialMissionBriefingPath(int id, char *path);	//!< [OpenXOPS-web]
	int GetOfficialMissionCount();	//!< Playable standard missions (data-driven, see docs/MISSIONS.md)
	int GetAIlevel(int level, AIParameter **out_AIlevel);
	int GetScopeParam(int id, ScopeParameter *out_data);
};

#endif