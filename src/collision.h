//! @file collision.h
//! @brief Collisionクラスの宣言

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

#ifndef COLLISION_H
#define COLLISION_H

#define COLLISION_ADDSIZE 0.01f			//!< 当たり判定範囲の加算量

#ifndef H_LAYERLEVEL
 #define H_LAYERLEVEL 2		//!< Select include file.
#endif
#include "main.h"

#include <float.h>

//! 当たり判定用ブロックデータ構造体
typedef struct
{
	float x[8];					//!< ブロック X座標
	float y[8];					//!< ブロック Y座標
	float z[8];					//!< ブロック Z座標
	float polygon_center_x[6];	//!< ブロックの面の中心 X座標
	float polygon_center_y[6];	//!< ブロックの面の中心 Y座標
	float polygon_center_z[6];	//!< ブロックの面の中心 Z座標
	float min_x;				//!< ブロック X座標の最小値
	float min_y;				//!< ブロック Y座標の最小値
	float min_z;				//!< ブロック Z座標の最小値
	float max_x;				//!< ブロック X座標の最大値
	float max_y;				//!< ブロック Y座標の最大値
	float max_z;				//!< ブロック Z座標の最大値
	bool BoardBlock;			//!< ブロック が厚さ0で板状になっているか
	int worldgroup;				//!< 空間分割のグループ
} Coll_Blockdata;

//! @brief 当たり判定を行うクラス
//! @details マップとして使用されるブロックデータへの当たり判定（衝突検出）を行います。
class Collision
{
	class BlockDataInterface* BlockDataIF;		//!< 読み込んだブロックデータが格納されたクラスへのポインタ
	Coll_Blockdata *cbdata;						//!< ブロックデータ

	bool CheckIntersectTri(int blockid, int face, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, float *out_Dist);

public:
	Collision();
	~Collision();
	int InitCollision(BlockDataInterface* in_BlockDataIF);
	void GetBlockPosMINMAX(int id, float *min_x, float *min_y, float *min_z, float *max_x, float *max_y, float *max_z);
	int GetWorldGroup(float x, float z);
	bool CheckBlockAABB(int id, float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);
	bool CheckALLBlockAABB(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);
	bool CheckPolygonFront(int id, int face, float x, float y, float z);
	bool CheckPolygonFrontRx(int id, int face, float rx);
	bool CheckBlockInside(int blockid, float x, float y, float z, bool worldgroup, int *planeid);
	bool CheckALLBlockInside(float x, float y, float z);
	bool CheckBlockIntersectRay(int blockid, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, int *face, float *Dist, float maxDist);
	bool CheckALLBlockIntersectRay(float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, int *id, int *face, float *Dist, float maxDist);
	bool CheckALLBlockIntersectDummyRay(float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, int *id, int *face, float *Dist, float maxDist);
	void ScratchVector(int id, int face, float in_vx, float in_vy, float in_vz, float *out_vx, float *out_vy, float *out_vz);
	void ReflectVector(int id, int face, float in_vx, float in_vy, float in_vz, float *out_vx, float *out_vy, float *out_vz);
	bool AngleVector(int id, int face, float in_vx, float in_vy, float in_vz, float *out_angle);
	bool AngleVector2(int id, int face, float in_vx, float in_vy, float in_vz, float *out_angle);
};

void GetAABBRay(float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, float maxDist, float *min_x, float *min_y, float *min_z, float *max_x, float *max_y, float *max_z);
bool CollideBoxAABB(float box1_min_x, float box1_min_y, float box1_min_z, float box1_max_x, float box1_max_y, float box1_max_z, float box2_min_x, float box2_min_y, float box2_min_z, float box2_max_x, float box2_max_y, float box2_max_z);
bool CollideCylinder(float c1_x, float c1_y, float c1_z, float c1_r, float c1_h, float c2_x, float c2_y, float c2_z, float c2_r, float c2_h, float *angle, float *length);
bool CollideSphereInside(float s_x, float s_y, float s_z, float s_r, float p_x, float p_y, float p_z);
bool CollideSphereRay(float s_x, float s_y, float s_z, float s_r, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, float *Dist, float maxDist);
bool CollideAABBRay(float box_min_x, float box_min_y, float box_min_z, float box_max_x, float box_max_y, float box_max_z, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, float *Dist, float maxDist);
bool CollideCylinderInside(float c_x, float c_y, float c_z, float c_r, float c_h, float p_x, float p_y, float p_z);
bool CollideCylinderRay(float c_x, float c_y, float c_z, float c_r, float c_h, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z, float *Dist, float maxDist);
float DistancePosRay(float Pos_x, float Pos_y, float Pos_z, float RayPos_x, float RayPos_y, float RayPos_z, float RayDir_x, float RayDir_y, float RayDir_z);
bool Collide2DLine(int A1x, int A1y, int A2x, int A2y, int B1x, int B1y, int B2x, int B2y, int *out_x, int *out_y);
bool Get2DLineInBox(int line_x1, int line_y1, int line_x2, int line_y2, int box_x1, int box_y1, int box_x2, int box_y2, int *out_line_x1, int *out_line_y1, int *out_line_x2, int *out_line_y2);

bool CheckTargetAngle(float pos_x, float pos_y, float pos_z, float rx, float ry, float target_x, float target_y, float target_z, float checkdist, float *out_rx, float *out_ry, float *out_dist2);

float VectorNormalization(float *vx, float *vy, float *vz);

#endif