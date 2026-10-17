//! @file datafile.cpp
//! @brief データ管理クラスの定義

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

#include "datafile.h"
#include "platform/vfs.h"

#ifndef _WIN32
 #include <dirent.h>	//[OpenXOPS-web] opendir()/readdir() for AddonList::GetMIFlist()
#endif

//! @brief コンストラクタ
DataInterface::DataInterface()
{
	datas = 0;
}

//! @brief ディストラクタ
DataInterface::~DataInterface()
{}

//! @brief データファイルを読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int DataInterface::LoadFiledata(const char *fname)
{
	//未使用引数対策
	UNREFERENCED_PARAMETER(fname);

	if( fname == NULL ){ return 1; }

	return 0;
}

//! @brief 合計データ数の取得
//! @return 合計データ数
int DataInterface::GetTotaldatas()
{
	return datas;
}

//! @brief データを取得
//! @param out_data 受け取るポインタ
//! @param id データ番号
//! @return 成功：0　失敗：0以外
int DataInterface::Getdata(void *out_data, int id)
{
	//未使用引数対策
	UNREFERENCED_PARAMETER(out_data);
	UNREFERENCED_PARAMETER(id);

	return 0;
}

//! @brief コンストラクタ
BlockDataInterface::BlockDataInterface()
{
	//blockdata構造体初期化
	data = new blockdata[MAX_BLOCKS];

	//テクスチャ設定初期化
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		strcpy(texture[i], "");
	}
}

//! @brief ディストラクタ
BlockDataInterface::~BlockDataInterface()
{
	//blockdata構造体解放
	if( data != NULL ){ delete [] data; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	//OutputLog.WriteLog(LOG_CLEANUP, "BD1", "");
#endif
}

//! @brief ブロックデータファイルを読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int BlockDataInterface::LoadFiledata(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	unsigned char bdata_header[2];
	float bdata_main[80];
	char bdata_mainb[30];

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "BD1", fname);
#endif

	//ファイルを読み込む
	fp = vfs_fopen(fname, "rb");
	if( fp == NULL ){ return 1; }

	//テクスチャを取得
	for(int i=0; i<TOTAL_BLOCKTEXTURE; i++){
		fread(texture[i], 1, 31, fp );
	}

	//データ数を取得
	fread( bdata_header, 1, 2, fp );
	datas = (int)Get2ByteLittleEndian(&(bdata_header[0]));
	if( datas > MAX_BLOCKS ){ datas = MAX_BLOCKS; }

	//ブロックデータ数分取得
	for(int i=0; i<datas; i++){
		//ブロックのを頂点情報と面情報を取得
		fread( bdata_main, 4, 24+48, fp );
		fread( bdata_mainb, 1, 28, fp );

		//データ番号を設定
		data[i].id = i;

		//ブロックの頂点情報に設定
		for(int j=0; j<8; j++){
			data[i].x[j] = bdata_main[j];
			data[i].y[j] = bdata_main[j+8];
			data[i].z[j] = bdata_main[j+16];
		}

		//ブロックの面情報を設定
		for(int j=0; j<6; j++){
			data[i].material[j].textureID = (int)bdata_mainb[j*4];
			data[i].material[j].u[0] = bdata_main[j*4+24];
			data[i].material[j].v[0] = bdata_main[j*4+48];
			data[i].material[j].u[1] = bdata_main[j*4+25];
			data[i].material[j].v[1] = bdata_main[j*4+49];
			data[i].material[j].u[2] = bdata_main[j*4+26];
			data[i].material[j].v[2] = bdata_main[j*4+50];
			data[i].material[j].u[3] = bdata_main[j*4+27];
			data[i].material[j].v[3] = bdata_main[j*4+51];
		}
	}

	//ファイルハンドルを解放
	fclose( fp );

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	return 0;
}

//! @brief ブロックデータの法線・影を算出する
//! @param darkflag ブロックを暗くする
//! @attention LoadBlockdata()関数で読みこんだ後、一度だけ実行。
void BlockDataInterface::CalculationBlockdata(bool darkflag)
{
	int vID[4];
	int uvID[4];
	float g;
	float xs, ys, zs;
	float g1;
	float xs1, ys1, zs1;
	float g2;
	float xs2, ys2, zs2;
	float lx, ly, lz;
	float rx, ry, rz, a;

	//光源の角度を設定
	lx = cosf(LIGHT_RX);
	ly = sinf(LIGHT_RY);
	lz = sinf(LIGHT_RX);

	//各ブロックの面情報分処理する
	for(int i=0; i<datas; i++){
		for(int j=0; j<6; j++){

			//ブロック頂点データの関連付けを取得
			blockdataface(j, vID, uvID);

			//中心点初期化
			data[i].material[j].center_x = 0.0f;
			data[i].material[j].center_y = 0.0f;
			data[i].material[j].center_z = 0.0f;

			//4頂点の中心点を算出
			for(int k=0; k<4; k++){
				data[i].material[j].center_x += data[i].x[ vID[k] ];
				data[i].material[j].center_y += data[i].y[ vID[k] ];
				data[i].material[j].center_z += data[i].z[ vID[k] ];
			}
			data[i].material[j].center_x /= 4;
			data[i].material[j].center_y /= 4;
			data[i].material[j].center_z /= 4;

			//面の法線（ベクトル）と、その長さを求める
			xs1 = ((data[i].y[ vID[3] ] - data[i].y[ vID[2] ]) * (data[i].z[ vID[0] ] - data[i].z[ vID[2] ])) - ((data[i].y[ vID[0] ] - data[i].y[ vID[2] ]) * (data[i].z[ vID[3] ] - data[i].z[ vID[2] ]));
			ys1 = ((data[i].z[ vID[3] ] - data[i].z[ vID[2] ]) * (data[i].x[ vID[0] ] - data[i].x[ vID[2] ])) - ((data[i].z[ vID[0] ] - data[i].z[ vID[2] ]) * (data[i].x[ vID[3] ] - data[i].x[ vID[2] ]));
			zs1 = ((data[i].x[ vID[3] ] - data[i].x[ vID[2] ]) * (data[i].y[ vID[0] ] - data[i].y[ vID[2] ])) - ((data[i].x[ vID[0] ] - data[i].x[ vID[2] ]) * (data[i].y[ vID[3] ] - data[i].y[ vID[2] ]));
			g1 = sqrtf(xs1 * xs1 + ys1 * ys1 + zs1 * zs1);

			//もう一方の三角形でも計算する
			xs2 = ((data[i].y[ vID[1] ] - data[i].y[ vID[0] ]) * (data[i].z[ vID[2] ] - data[i].z[ vID[0] ])) - ((data[i].y[ vID[2] ] - data[i].y[ vID[0] ]) * (data[i].z[ vID[1] ] - data[i].z[ vID[0] ]));
			ys2 = ((data[i].z[ vID[1] ] - data[i].z[ vID[0] ]) * (data[i].x[ vID[2] ] - data[i].x[ vID[0] ])) - ((data[i].z[ vID[2] ] - data[i].z[ vID[0] ]) * (data[i].x[ vID[1] ] - data[i].x[ vID[0] ]));
			zs2 = ((data[i].x[ vID[1] ] - data[i].x[ vID[0] ]) * (data[i].y[ vID[2] ] - data[i].y[ vID[0] ])) - ((data[i].x[ vID[2] ] - data[i].x[ vID[0] ]) * (data[i].y[ vID[1] ] - data[i].y[ vID[0] ]));
			g2 = sqrtf(xs2 * xs2 + ys2 * ys2 + zs2 * zs2);

			//正しい計算結果を採用する
			if( g1 > g2 ){
				xs = xs1;
				ys = ys1;
				zs = zs1;
				g = g1;
			}
			else{
				xs = xs2;
				ys = ys2;
				zs = zs2;
				g = g2;
			}

			//法線（ベクトル）を正規化
			data[i].material[j].vx = xs / g;
			data[i].material[j].vy = ys / g;
			data[i].material[j].vz = zs / g;

			//面の明るさを求める
			rx = data[i].material[j].vx + lx;
			ry = data[i].material[j].vy + ly;
			rz = data[i].material[j].vz + lz;
			//a = sqrtf(fabsf(rx*rx + ry*ry + rz*rz)) / 2.0f;
			//data[i].material[j].shadow = a/2 + 0.5f;
			a = sqrtf(rx*rx + ry*ry + rz*rz) / 3.0f;
			data[i].material[j].shadow = a/2;
			if( darkflag == false ){
				data[i].material[j].shadow += 0.5f;
			}
			else{
				data[i].material[j].shadow += 0.3f;
			}
		}
	}
}

//! @brief ブロックデータに設定されたテクスチャのファイル名を取得
//! @param fname ファイル名を受け取る文字列型ポインタ
//! @param id テクスチャ番号
//! @return 成功：0　失敗：0以外
int BlockDataInterface::GetTexture(char *fname, int id)
{
	if( data == NULL ){ return 1; }
	if( fname == NULL ){ return 2; }
	if( (id < 0)||(TOTAL_BLOCKTEXTURE <= id) ){ return 2; }

	//ポインタにテクスチャ名をコピー
	strcpy(fname, texture[id]);

	return 0;
}

//! @brief ブロックデータを取得
//! @param out_data 受け取るblockdata型ポインタ
//! @param id データ番号
//! @return 成功：0　失敗：0以外
int BlockDataInterface::Getdata(blockdata *out_data, int id)
{
	if( data == NULL ){ return 1; }
	if( out_data == NULL ){ return 2; }
	if( (id < 0)||(datas <= id) ){ return 2; }

	//ブロックデータを取得
	memcpy(out_data, &(data[id]), sizeof(blockdata));

	return 0;
}

//! @brief ブロック頂点データの関連付けを取得
//
//	    3 ・----・2
//	     /|    /|
//	    / |   / |
//	0 ・----・1 |
//	  |7 ・-| --・6
//	  |  /  |  /
//	  | /   | /
//	4 ・----・5
//
//	C-----D
//	|     |
//	|     |
//	B-----A
//
//	上[0](ABCD) =  (1032)
//	下[1]		=  (6745)
//	前[2]		=  (5401)
//	右[3]		=  (6512)
//	奥[4]		=  (7623)
//	左[5]		=  (4730)
bool blockdataface(int faceID, int* vID, int* uvID)
{
	if( faceID == 0 ){
		if( vID != NULL ){
			vID[0] = 1;		vID[1] = 0;		vID[2] = 3;		vID[3] = 2;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	if( faceID == 1 ){
		if( vID != NULL ){
			vID[0] = 6;		vID[1] = 7;		vID[2] = 4;		vID[3] = 5;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	if( faceID == 2 ){
		if( vID != NULL ){
			vID[0] = 5;		vID[1] = 4;		vID[2] = 0;		vID[3] = 1;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	if( faceID == 3 ){
		if( vID != NULL ){
			vID[0] = 6;		vID[1] = 5;		vID[2] = 1;		vID[3] = 2;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	if( faceID == 4 ){
		if( vID != NULL ){
			vID[0] = 7;		vID[1] = 6;		vID[2] = 2;		vID[3] = 3;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	if( faceID == 5 ){
		if( vID != NULL ){
			vID[0] = 4;		vID[1] = 7;		vID[2] = 3;		vID[3] = 0;
		}
		if( uvID != NULL ){
			uvID[0] = 2;	uvID[1] = 3;	uvID[2] = 0;	uvID[3] = 1;
		}
		return true;
	}
	return false;
}

//! @brief コンストラクタ
PointDataInterface::PointDataInterface()
{
	//pointdata構造体初期化
	data = new pointdata[MAX_POINTS];

	//イベントメッセージ初期化
	for(int i=0; i<MAX_POINTMESSAGES; i++){
		text[i] = new char[MAX_POINTMESSAGEBYTE];
		text[i][0] = '\0';
	}
}

//! @brief ディストラクタ
PointDataInterface::~PointDataInterface()
{
	//pointdata構造体解放
	if( data != NULL ){ delete [] data; }

	//イベントメッセージ解放
	for(int i=0; i<MAX_POINTMESSAGES; i++){
		if( text[i] != NULL ){ delete [] text[i]; }
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	//OutputLog.WriteLog(LOG_CLEANUP, "PD1", "");
#endif
}

//! @brief ポイントデータファイルを読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int PointDataInterface::LoadFiledata(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	unsigned char pdata_header[2];
	float pdata_mainf[4];
	signed char pdata_mainc[4];
	char fname2[MAX_PATH];

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "PD1", fname);
#endif

	//ファイルを読み込む
	fp = vfs_fopen( fname, "rb" );
	if( fp == NULL ){ return 1; }

	//データ数を取得
	fread( pdata_header, 1, 2, fp );
	datas = (int)Get2ByteLittleEndian(&(pdata_header[0]));
	if( datas > MAX_POINTS ){ datas = MAX_POINTS; }

	//ポイントデータ数分処理する
	for(int i=0; i<datas; i++){
		//データ番号設定
		data[i].id = i;

		//座標データ取得
		fread( pdata_mainf, 4, 4, fp );
		data[i].x = pdata_mainf[0];
		data[i].y = pdata_mainf[1];
		data[i].z = pdata_mainf[2];
		data[i].r = pdata_mainf[3];

		//パラメータ取得
		fread( pdata_mainc, 1, 4, fp );
		data[i].p1 = (signed short int)pdata_mainc[0];
		data[i].p2 = (signed short int)pdata_mainc[1];
		data[i].p3 = (signed short int)pdata_mainc[2];
		data[i].p4 = (signed short int)pdata_mainc[3];
	}

	//ファイルポインタを閉じる
	fclose( fp );

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	//イベントメッセージ初期化
	for(int i=0; i<MAX_POINTMESSAGES; i++){
		text[i][0] = '\0';
	}

	//「同ファイル名.msg」を生成
	strcpy(fname2, fname);
	//PathRemoveExtension(fname2);
	for(int i=(int)strlen(fname2)-1; i>0; i--){
		if( fname2[i] == '.' ){
			fname2[i] = '\0';
			break;
		}
	}
	strcat(fname2, ".msg");

	LoadMSGFiledata(fname2);

	return 0;
}

//! @brief メッセージデータファイルを読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int PointDataInterface::LoadMSGFiledata(char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;


#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "MSG", fname);
#endif

	//ファイルを読み込み
	fp = vfs_fopen( fname, "r" );
	if( fp != NULL ){
		//メッセージデータを取得
		for(int i=0; i<MAX_POINTMESSAGES; i++){
			if( fgets(text[i], MAX_POINTMESSAGEBYTE, fp) == NULL ){ break; }

			//'\r'があれば'\n'に置き換える
			for(int j=0; j<MAX_POINTMESSAGEBYTE; j++){
				if( text[i][j] == '\r' ){ text[i][j] = '\n'; }
			}
		}

		//ファイルポインタを解放
		fclose( fp );

#ifdef ENABLE_DEBUGLOG
		//ログに出力
		OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif
		return 0;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_ERROR, "", "");
#endif
	return 1;
}

//! @brief ポイントデータを取得
//! @param out_data 受け取るpointdata型ポインタ
//! @param id データ番号
//! @return 成功：0　失敗：0以外
int PointDataInterface::Getdata(pointdata *out_data, int id)
{
	if( data == NULL ){ return 1; }
	if( out_data == NULL ){ return 2; }
	if( (id < 0)||(datas <= id) ){ return 2; }

	//データをポインタにコピー
	memcpy(out_data, &(data[id]), sizeof(pointdata));

	return 0;
}

//! @brief ポイントのパラメーターを書き換え
//! @param id データ番号
//! @param p1 第1パラメータ
//! @param p2 第2パラメータ
//! @param p3 第3パラメータ
//! @param p4 第4パラメータ
//! @return 成功：0　失敗：0以外
//! @warning AIが使用するパス（p1:3 or p1:8）以外への使用は、保証されておらず推奨しません。
int PointDataInterface::SetParam(int id, signed short int p1, signed short int p2, signed short int p3, signed short int p4)
{
	if( data == NULL ){ return 1; }
	if( (id < 0)||(datas <= id) ){ return 2; }

	//パラメータを上書き
	data[id].p1 = p1;
	data[id].p2 = p2;
	data[id].p3 = p3;
	data[id].p4 = p4;

	return 0;
}

//! @brief メッセージ（1行）を取得
//! @param str 文字列を受け取るポインタ
//! @param id データ番号
//! @return 成功：0　失敗：1
int PointDataInterface::GetMessageText(char *str, int id)
{
	if( str == NULL ){ return 1; }
	if( (id < 0)||(MAX_POINTMESSAGES <= id) ){ return 1; }

	//ポインタにメッセージをコピー
	strcpy(str, text[id]);
	return 0;
}

//! @brief ポイントデータを検索
//! @param id 最初に該当したデータ番号を受け取るポインタ　（NULL 可）
//! @param pmask パラメータのマスク
//! @param p1 第1パラメータ
//! @param p2 第2パラメータ
//! @param p3 第3パラメータ
//! @param p4 第4パラメータ
//! @param offset 検索を開始するデータ
//! @return 該当ポイント数
//! @note パラメータのマスクは、p1～p4の検索対象を指定します。
//! @note 1～4ビット目までを使用し、p1：1ビット目、p2：2ビット目、p3：3ビット目、p4：4ビット目　をそれぞれ意味します。
//! @note 検索の対象ビット：1　対象外のビット：0　に設定してください。　なお、7～5ビット目は無視されます。
//! @code
//! //16進数で記述の場合―
//! 0x02	// p2のみを検索対象にする
//! 0x05	// p1とp3を検索対象にする
//! 0xF1	// p1のみを検索対象にする　（7～5ビット目は無視）
//! @endcode
int PointDataInterface::SearchPointdata(int* id, unsigned char pmask, signed short int p1, signed short int p2, signed short int p3, signed short int p4, int offset)
{
	int cnt = 0;

	if( offset < 0 ){ offset = 0; }
	if( offset >= datas ){ offset = datas; }

	//オフセット値からデータ数分処理
	for(int i=offset; i<datas; i++){

		if( ((pmask&0x01) == 0)||(p1 == data[i].p1) ){		//マスクで指定されていないか、p1が一致
			if( ((pmask&0x02) == 0)||(p2 == data[i].p2) ){		//マスクで指定されていないか、p2が一致
				if( ((pmask&0x04) == 0)||(p3 == data[i].p3) ){		//マスクで指定されていないか、p3が一致
					if( ((pmask&0x08) == 0)||(p4 == data[i].p4) ){		//マスクで指定されていないか、p4が一致

						if( (cnt == 0)&&(id != NULL) ){			//最初に該当し、idがNULLでない。
							*id = i;
						}

						//検索該当数を+1
						cnt += 1;

					}
				}
			}
		}

	}

	//検索該当数を返す
	return cnt;
}

//! @brief ポイントデータを検索
//! @param out_data 最初に該当したデータを受け取るpointdata型ポインタ　（NULL 可）
//! @param pmask パラメータのマスク
//! @param p1 第1パラメータ
//! @param p2 第2パラメータ
//! @param p3 第3パラメータ
//! @param p4 第4パラメータ
//! @param offset 検索を開始するデータ
//! @return 該当ポイント数
//! @attention この関数の詳しい説明は、同オーバーロード関数をご覧ください。
int PointDataInterface::SearchPointdata(pointdata *out_data, unsigned char pmask, signed short int p1, signed short int p2, signed short int p3, signed short int p4, int offset)
{
	int id, total;

	//同条件でポイントを検索
	total = SearchPointdata(&id, pmask, p1, p2, p3, p4, offset);
	
	//該当すればデータを取得
	if( total > 0 ){
		Getdata(out_data, id);
	}

	//検索該当数を返す
	return total;
}

//! @brief コンストラクタ
MIFInterface::MIFInterface()
{
	datas = 0;

	mif = false;
	strcpy(mission_name, "");
	strcpy(mission_fullname, "");
	strcpy(blockfile_path, "");
	strcpy(pointfile_path, "");
	skynumber = 0;
	strcpy(picturefileA_path, "!");
	strcpy(picturefileB_path, "!");
	strcpy(addsmallobject_path, "");
	strcpy(briefingtext, "");
	collision = false;
	screen = false;
	AddSmallObjectData = new addsmallobject[MAX_ADDSMALLOBJECT];
}

//! @brief ディストラクタ
MIFInterface::~MIFInterface()
{
	if( AddSmallObjectData != NULL ){ delete [] AddSmallObjectData; }
}

//! @brief データファイルを読みこむ
//! @param fname ファイル名
//! @param addfile 追加ファイルも読み込む
//! @return 成功：0　失敗：1
int MIFInterface::LoadFiledata(const char *fname, bool addfile)
{
	if( fname == NULL ){ return 1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "MIF", fname);
#endif

	mif = true;

	//拡張子が.txtならば
	if( CheckFileExtension(fname, ".txt") == true ){
		//MIFフラグを無効に
		mif = false;
	}


	if( mif == false ){
		if( LoadDefaultTextFiledata(fname) != 0 ){
			//briefing data open failed
			return 1;
		}
	}
	else{
		if( LoadMissionInfoFiledata(fname) != 0 ){
			//briefing data open failed
			return 1;
		}
	}

#ifdef ENABLE_AUTOLOADMIF
	//ドライブレターから指定されている（＝フルパス）ならば
	if( CheckFullPath(fname) == true ){
		char path[MAX_PATH];

		//mifのディレクトリ取得
		GetFileDirectory(fname, path);

		//各ファイルパスをフルパスに書き換える
		ChangeExePathToFullPath(path, blockfile_path);
		ChangeExePathToFullPath(path, pointfile_path);
		ChangeExePathToFullPath(path, picturefileA_path);
		ChangeExePathToFullPath(path, picturefileB_path);
		ChangeExePathToFullPath(path, addsmallobject_path);
	}
#endif

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	//追加小物情報を初期値へ
	for(int i=0; i<MAX_ADDSMALLOBJECT; i++){
		strcpy(AddSmallObjectData[i].modelpath, "");
		strcpy(AddSmallObjectData[i].texturepath, "");
		AddSmallObjectData[i].decide = 0;
		AddSmallObjectData[i].hp = 0;
		strcpy(AddSmallObjectData[i].soundpath, "");
		AddSmallObjectData[i].jump = 0;
	}

	//何かしらの追加小物情報ファイルが指定されていれば
	if( (addfile == true)&&(strcmp(addsmallobject_path, "") != 0)&&(strcmp(addsmallobject_path, "!") != 0) ){
		LoadAddSmallObjectFiledata(addsmallobject_path);
	}
#ifdef ENABLE_DEBUGLOG
	else{
		//ログに出力
		OutputLog.WriteLog(LOG_CHECK, "(Add SmallObject)", "File not found");
	}
#endif

#ifdef ENABLE_AUTOLOADMIF
	//ドライブレターから指定されている（＝フルパス）ならば
	if( CheckFullPath(fname) == true ){
		char path[MAX_PATH];

		//mifのディレクトリ取得
		GetFileDirectory(fname, path);

		//各ファイルパスをフルパスに書き換える
		for(int i=0; i<MAX_ADDSMALLOBJECT; i++){
			ChangeExePathToFullPath(path, AddSmallObjectData[i].modelpath);
			ChangeExePathToFullPath(path, AddSmallObjectData[i].texturepath);
			ChangeExePathToFullPath(path, AddSmallObjectData[i].soundpath);
		}
	}
#endif

	return 0;
}

//! @brief ミッションファイル（.txt）を読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int MIFInterface::LoadDefaultTextFiledata(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	char str[64];

	//ファイルを開く
	fp = vfs_fopen( fname, "r" );
	if( fp != NULL ){
		//画像Aを取得
		fgets(str, 64, fp);
		DeleteLinefeed(str);
		if( strcmp(str, "!") == 0 ){
			strcpy(picturefileA_path, "!");
		}
		else{
			//「data\\briefing\\　～　.bmp」を生成
			strcpy(picturefileA_path, "data\\briefing\\");
			strcat(picturefileA_path, str);
			strcat(picturefileA_path, ".bmp");
		}

		//画像Bを取得
		fgets(str, 64, fp);
		DeleteLinefeed(str);
		if( strcmp(str, "!") == 0 ){
			strcpy(picturefileB_path, "!");
		}
		else{
			//「data\\briefing\\　～　.bmp」を生成
			strcpy(picturefileB_path, "data\\briefing\\");
			strcat(picturefileB_path, str);
			strcat(picturefileB_path, ".bmp");
		}

		//背景空の番号
		fgets(str, 16, fp);
		DeleteLinefeed(str);
		skynumber = atoi(str);

		//ブリーフィングテキストを取得
		strcpy(briefingtext, "");
		for(int i=0; i<17; i++ ){
			if( fgets(str, 50, fp) == NULL ){ break; }
			strcat(briefingtext, str);
			datas += 1;
		}

		//ファイルハンドルを解放
		fclose( fp );

		//取得できない値の初期化
		strcpy(mission_name, "");
		strcpy(mission_fullname, "");
		strcpy(blockfile_path, "");
		strcpy(pointfile_path, "");
		strcpy(addsmallobject_path, "");
		collision = false;
		screen = false;

		return 0;
	}

	return 1;
}

//! @brief ミッションファイル（.mif）を読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int MIFInterface::LoadMissionInfoFiledata(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	char str[64];

	//ファイルを開く
	fp = vfs_fopen( fname, "r" );
	if( fp != NULL ){
		//ミッション識別名
		fgets(mission_name, 24, fp);
		DeleteLinefeed(mission_name);

		//ミッション正式名称
		fgets(mission_fullname, 64, fp);
		DeleteLinefeed(mission_fullname);

		//ブロックデータファイル
		fgets(blockfile_path, _MAX_PATH, fp);
		DeleteLinefeed(blockfile_path);

		//ポイントデータファイル
		fgets(pointfile_path, _MAX_PATH, fp);
		DeleteLinefeed(pointfile_path);

		//背景空の番号
		fgets(str, 16, fp);
		DeleteLinefeed(str);
		skynumber = atoi(str);

		//当たり判定・画面設定の取得
		fgets(str, 16, fp);
		DeleteLinefeed(str);
		if( strcmp(str, "1") == 0 ){
			collision = true;
			screen = false;
		}
		else if( strcmp(str, "2") == 0 ){
			collision = false;
			screen = true;
		}
		else if( strcmp(str, "3") == 0 ){
			collision = true;
			screen = true;
		}
		else{	// == "0"
			collision = false;
			screen = false;
		}

		//追加小物情報ファイル取得
		fgets(addsmallobject_path, _MAX_PATH, fp);
		DeleteLinefeed(addsmallobject_path);

		//画像Aを取得
		fgets(picturefileA_path, _MAX_PATH, fp);
		DeleteLinefeed(picturefileA_path);

		//画像Bを取得
		fgets(picturefileB_path, _MAX_PATH, fp);
		DeleteLinefeed(picturefileB_path);

		//ブリーフィングテキストを取得
		strcpy(briefingtext, "");
		for(int i=0; i<17; i++ ){
			if( fgets(str, 50, fp) == NULL ){ break; }
			strcat(briefingtext, str);
			datas += 1;
		}

		//ファイルハンドルを解放
		fclose( fp );

		return 0;
	}

	return 1;
}

//! @brief 追加小物情報ファイルを読みこむ
//! @param fname ファイル名
//! @return 成功：0　失敗：1
int MIFInterface::LoadAddSmallObjectFiledata(const char *fname)
{
	if( fname == NULL ){ return 1; }

	FILE *fp;
	char str1[_MAX_PATH];
	char str2[_MAX_PATH];
	char str3[32];
	char str4[32];
	char str5[_MAX_PATH];
	char str6[32];

	char str[64];


#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "(Add SmallObject)", fname);
#endif

	//ファイルを開く
	fp = vfs_fopen( fname, "r" );
	if( fp != NULL ){
		for(int i=0; i<MAX_ADDSMALLOBJECT; i++){
			//6行取得
			//6行すべて取得できなければ失敗
			if( fgets(str1, _MAX_PATH, fp) == NULL ){ break; }
			if( fgets(str2, _MAX_PATH, fp) == NULL ){ break; }
			if( fgets(str3, 16, fp) == NULL ){ break; }
			if( fgets(str4, 16, fp) == NULL ){ break; }
			if( fgets(str5, _MAX_PATH, fp) == NULL ){ break; }
			if( fgets(str6, 16, fp) == NULL ){ break; }


			//モデルデータパス
			DeleteLinefeed(str1);
			strcpy(AddSmallObjectData[i].modelpath, str1);

			//テクスチャパス
			DeleteLinefeed(str2);
			strcpy(AddSmallObjectData[i].texturepath, str2);

			//当たり判定の大きさ
			DeleteLinefeed(str3);
			AddSmallObjectData[i].decide = atoi(str3);
#ifdef ENABLE_ADDOBJ_PARAM8BIT
			AddSmallObjectData[i].decide = AddSmallObjectData[i].decide & 0x0000007F;
#endif

			//耐久力
			DeleteLinefeed(str4);
			AddSmallObjectData[i].hp = atoi(str4);
#ifdef ENABLE_ADDOBJ_PARAM8BIT
			AddSmallObjectData[i].hp = AddSmallObjectData[i].hp & 0x0000007F;
#endif

			//サウンドデータパス
			DeleteLinefeed(str5);
			strcpy(AddSmallObjectData[i].soundpath, str5);

			//飛び具合
			DeleteLinefeed(str6);
			AddSmallObjectData[i].jump = atoi(str6);
#ifdef ENABLE_ADDOBJ_PARAM8BIT
			AddSmallObjectData[i].jump = AddSmallObjectData[i].jump & 0x000000FF;
#endif

			//次の追加小物情報の前にある、ダミー 1行を読み出し
			if( i+1 < MAX_ADDSMALLOBJECT ){
				if( fgets(str, 32, fp) == NULL ){ break; }
			}
		}

		//ファイルハンドルを解放
		fclose( fp );

#ifdef ENABLE_DEBUGLOG
		//ログに出力
		OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif
		return 0;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_ERROR, "", "");
#endif
	return 1;
}

//! @brief addonフォルダを参照する相対パスを.exe基準→.mif基準へ変更
//! @param dir .mifファイルまでのフルパス
//! @param fname 処理対象のファイルパス
//! @return 変換済み：true　未変換：false
//! @attention fnameのポインタを直接書き換えます。
bool MIFInterface::ChangeExePathToFullPath(char *dir, char *fname)
{
	if( dir == NULL ){ return false; }
	if( fname == NULL ){ return false; }

	char str[MAX_PATH];
	int index = 0;

	//無効なファイルパスならば処理しない
	if( strcmp(fname, "") == 0 ){ return false; }
	if( strcmp(fname, "!") == 0 ){ return false; }

	//addonフォルダ内を参照しているか判定
	sprintf(str, "%s", ADDONDIR);
	if( memcmp(fname, str, strlen(str)) == 0 ){ index = (int)strlen(str); }
	sprintf(str, "\\%s", ADDONDIR);
	if( memcmp(fname, str, strlen(str)) == 0 ){ index = (int)strlen(str); }
	sprintf(str, "/%s", ADDONDIR);
	if( memcmp(fname, str, strlen(str)) == 0 ){ index = (int)strlen(str); }
	sprintf(str, ".\\%s", ADDONDIR);
	if( memcmp(fname, str, strlen(str)) == 0 ){ index = (int)strlen(str); }
	sprintf(str, "./%s", ADDONDIR);
	if( memcmp(fname, str, strlen(str)) == 0 ){ index = (int)strlen(str); }

	//addonフォルダ内を参照しているならば
	if( index != 0 ){
		//合成後の文字数がMAX_PATHを超えるようなら処理中断
		if( strlen(dir)+strlen(fname)-index+1 >= MAX_PATH -1 ){ return false; }

		//addonの文字を除去
		strcpy(str, fname);
		for(int i=0; i<(int)strlen(str)-index+1; i++){ str[i] = str[i+index]; }

		//.mifまでのパス＋処理対象のファイルパス
		strcpy(fname, dir);
		strcat(fname, str);
		return true;
	}
	return false;
}

//! @brief 読み込んだデータファイルの形式を取得
//! @return 標準形式：false　MIF形式：true
//! @attention ファイルを正常に読み込んだ後に実行してください。
bool MIFInterface::GetFiletype()
{
	return mif;
}

//! @brief ミッション識別名を取得
//! @return 識別名のポインタ（最大：24）
char* MIFInterface::GetMissionName()
{
	return mission_name;
}

//! @brief ミッション正式名称を取得
//! @return 正式名称のポインタ（最大：64）
char* MIFInterface::GetMissionFullname()
{
	return mission_fullname;
}

//! @brief ブロックデータとポイントデータのパスを取得
//! @param *blockfile ブロックデータを受け取るポインタ
//! @param *pointfile ポイントデータを受け取るポインタ
void MIFInterface::GetDatafilePath(char *blockfile, char *pointfile)
{
	if( blockfile == NULL ){ return; }
	if( pointfile == NULL ){ return; }

	strcpy(blockfile, blockfile_path);
	strcpy(pointfile, pointfile_path);
}

//! @brief 背景空を取得
//! @return 空の番号（0～5）
//! @attention 番号 0 は「背景なし」を意味します。
int MIFInterface::GetSkynumber()
{
	return skynumber;
}

//! @brief ブリーフィング画像ファイルのパスを取得
//! @param *picturefileA 画像ファイルAのパスを受け取るポインタ
//! @param *picturefileB 画像ファイルBのパスを受け取るポインタ
//! @attention 設定されていない場合は「!」が返されます。
//! @attention 画像を1枚しか使用しない場合、画像ファイルBは「!」を返します。
void MIFInterface::GetPicturefilePath(char *picturefileA, char *picturefileB)
{
	if( picturefileA == NULL ){ return; }
	if( picturefileB == NULL ){ return; }

	strcpy(picturefileA, picturefileA_path);
	strcpy(picturefileB, picturefileB_path);
}

//! @brief ブリーフィング文章（本文）を取得
//! @return 文章のポインタ（最大：816）
//! @attention 改行コードも含めて、最大17行分が一度に返されます。
//! @attention 行数は GetTotaldatas() で取得できます。
char* MIFInterface::GetBriefingText()
{
	return briefingtext;
}

//! @brief 追加の当たり判定を示すフラグを取得
//! @return 有効：true　無効：false
bool MIFInterface::GetCollisionFlag()
{
	return collision;
}

//! @brief 画面を暗くを示すフラグを取得
//! @return 有効：true　無効：false
bool MIFInterface::GetScreenFlag()
{
	return screen;
}

//! @brief 追加小物情報ファイルパスを取得
//! @return 追加小物情報ファイルパスのポインタ（最大：_MAX_PATH）
//! @attention 本関数は通常必要ありません。モデルデータパスなどのファイル内の情報は、他の関数で取得できます。
char* MIFInterface::GetAddSmallobjectFile()
{
	return addsmallobject_path;
}

//! @brief 追加小物のモデルデータパスを取得
//! @param id データ番号（通常は 0 固定）
//! @return モデルデータパスのポインタ（最大：_MAX_PATH）
char* MIFInterface::GetAddSmallobjectModelPath(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return NULL; }
	return AddSmallObjectData[id].modelpath;
}

//! @brief 追加小物のテクスチャパスを取得
//! @param id データ番号（通常は 0 固定）
//! @return テクスチャパスのポインタ（最大：_MAX_PATH）
char* MIFInterface::GetAddSmallobjectTexturePath(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return NULL; }
	return AddSmallObjectData[id].texturepath;
}

//! @brief 追加小物の当たり判定の大きさを取得
//! @param id データ番号（通常は 0 固定）
//! @return 当たり判定の大きさ
int MIFInterface::GetAddSmallobjectDecide(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return 0; }
	return AddSmallObjectData[id].decide;
}

//! @brief 追加小物の耐久力を取得
//! @param id データ番号（通常は 0 固定）
//! @return 耐久力
int MIFInterface::GetAddSmallobjectHP(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return 0; }
	return AddSmallObjectData[id].hp;
}

//! @brief 追加小物のサウンドデータパスを取得
//! @param id データ番号（通常は 0 固定）
//! @return サウンドデータパスのポインタ（最大：_MAX_PATH）
char* MIFInterface::GetAddSmallobjectSoundPath(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return NULL; }
	return AddSmallObjectData[id].soundpath;
}

//! @brief 追加小物の飛び具合を取得
//! @param id データ番号（通常は 0 固定）
//! @return 飛び具合
int MIFInterface::GetAddSmallobjectJump(int id)
{
	if( (id < 0)||(MAX_ADDSMALLOBJECT <= id) ){ return 0; }
	return AddSmallObjectData[id].jump;
}

//! @brief コンストラクタ
AddonList::AddonList()
{
	datas = 0;

	for(int i=0; i<MAX_ADDONLIST; i++){
		strcpy(filename[i], "");
		strcpy(mission_name[i], "");
	}
}

//! @brief ディストラクタ
AddonList::~AddonList()
{}

//! @brief .mifファイルを取得
void AddonList::GetMIFlist(const char *dir)
{
	if( dir == NULL ){ return; }
	datas = vfs_list(dir, ".mif", filename, MAX_ADDONLIST);
}

//! @brief ミッション名を取得
void AddonList::GetMissionName(const char *dir)
{
	if( dir == NULL ){ return; }

	for(int i=0; i<datas; i++){
		char str[_MAX_PATH];
		MIFInterface mifdata;

		//ファイル名を生成
		strcpy(str, dir);
		strcat(str, "\\");
		strcat(str, filename[i]);

		//MIFInterfaceで読み込む
		mifdata.LoadFiledata(str, false);

		//ミッション名を取得
		strcpy(mission_name[i], mifdata.GetMissionName());
	}
}

//! @brief ミッション名をソートする
void AddonList::Sort()
{
	if( datas <= 1 ){ return; }

	char mission_name_c[MAX_ADDONLIST][24];
	char temp[_MAX_PATH];
	int cmp;

	//ミッション名を一度小文字に変換する
	for(int i=0; i<datas; i++){
		for(int j=0; j<(int)strlen(mission_name[i]); j++){
			mission_name_c[i][j] = (char)tolower(mission_name[i][j]);
		}
		mission_name_c[i][strlen(mission_name[i])] = '\0';
	}

	//低速なバブルソート　（＾＾；
	for(int i=0; i<datas-1; i++){
		for(int j=i+1; j<datas; j++){
			cmp = strcmp(mission_name_c[i], mission_name_c[j]);
			if(cmp > 0){
				strcpy(temp, mission_name_c[i]);
				strcpy(mission_name_c[i], mission_name_c[j]);
				strcpy(mission_name_c[j], temp);

				strcpy(temp, mission_name[i]);
				strcpy(mission_name[i], mission_name[j]);
				strcpy(mission_name[j], temp);

				strcpy(temp, filename[i]);
				strcpy(filename[i], filename[j]);
				strcpy(filename[j], temp);
			}
		}
	}

}

//! @brief ADDONリストを取得する
//! @param dir ADDON（.mif）が入ったディレクトリ　（標準："addon\\"）
//! @return addonの総数
int AddonList::LoadFiledata(const char *dir)
{
	if( dir == NULL ){ return 0; }

	datas = 0;

	//.mifファイルを取得
	GetMIFlist(dir);

	//ミッション名を取得
	GetMissionName(dir);

	//ミッション名をソートする
	Sort();

#ifdef ENABLE_DEBUGLOG
	char str[128];
	sprintf(str, "Research addon directory (Directory name : [%s]  Total : %d )", dir, datas);

	//ログに出力
	OutputLog.WriteLog(LOG_CHECK, "Directory", str);
#endif

	return datas;
}

//! @brief ミッション名を取得
//! @param id データ番号
//! @return ミッション名
char* AddonList::GetMissionName(int id)
{
	if( (id < 0)||(MAX_ADDONLIST <= id ) ){ return NULL; }
	return mission_name[id];
}

//! @brief ファイル名を取得
//! @param id データ番号
//! @return ファイル名
char* AddonList::GetFileName(int id)
{
	if( (id < 0)||(MAX_ADDONLIST <= id ) ){ return NULL; }
	return filename[id];
}

//! @brief コンストラクタ
INIFileInterface::INIFileInterface()
{
	inifp = NULL;
}

//! @brief ディストラクタ
INIFileInterface::~INIFileInterface()
{
	ReleaseINIFile();
}

//! @brief INIファイルを読み込む
//! @param fname ファイル名
//! @return 成功：false　失敗：true
//! @attention 既にINIファイルを読み込んでいる場合、ReleaseINIFile()関数を使うまで本関数は失敗します。
bool INIFileInterface::LoadINIFile(const char *fname)
{
	if( fname == NULL ){ return true; }
	if( inifp != NULL ){ return true; }


	inifp = vfs_fopen(fname, "rb");
	if( inifp == NULL ){ return true; }

	return false;
}

//! @brief 文字列を取得
//! @param sectionname セクション名（NULL可）
//! @param keyname キー名
//! @param defaultvalue デフォルト値
//! @param valuestr 値
//! @param strbuflen 最大文字数（-1で無効）
//! @return エラーコード
//! @attention ファイルが開かれていないか キーが見つからなければ、デフォルト値を返します。
//! @attention 取得した文字列が最大文字数を超えている場合、最大文字数まで切り取ります。
//! @note エラーコード　1：引数異常、2：ファイルが開かれていない、3：値が見つからない
int INIFileInterface::GetINIFileString(const char *sectionname, const char *keyname, const char *defaultvalue, char *valuestr, int strbuflen)
{
	if( valuestr == NULL ){ return 1; }
	if( defaultvalue == NULL ){ strcpy(valuestr, ""); return 1; }
	if( keyname == NULL ){ strcpy(valuestr, defaultvalue); return 1; }

	int state = 0;
	char readline[512];
	char buf[512];

	//ファイルが読み込まれていなければ失敗
	if( inifp == NULL ){
		strcpy(valuestr, defaultvalue);
		return 2;
	}

	fseek(inifp, 0, SEEK_SET);
	strcpy(readline, "");

	//セクション名が未指定ならば、セクション名判定を無効化
	if( sectionname == NULL ){
		state = 1;
	}
	else if( sectionname[0] == '\0' ){
		state = 1;
	}

	//データサイズ変数チェック
	if( strbuflen <= -2 ){ strbuflen = 0; }
	if( strbuflen == -1 ){ strbuflen = 512; }
	if( strbuflen > 512 ){ strbuflen = 512; }

	while( fgets(readline, 512, inifp) != NULL ){
		//コメント行なら無効
		if( readline[0] == ';' ){ continue; }

		//セクション名判定
		if( state == 0 ){//if( sectionname != NULL ){
			char str[64];
			sprintf(str, "[%s]", sectionname);
			strcpy(buf, readline);
			buf[ strlen(str) ] = '\0';
			if( strcmp(buf, str) == 0 ){
				state = 1;
				continue;
			}
		}


		//キー名判定
		if( state == 1 ){
			//一文字目が"["なら無効
			if( readline[0] == '[' ){ break; }

			char readline2[512];
			int strcnt = 0;
			int quotationmode = 0;
			char str[64];

			//スペース/コメント/"" などがない文字を生成
			strcpy(readline2, "");
			for(int i=0; i<(int)strlen(readline); i++){
				if( (readline[i] == '\r')||(readline[i] == '\n') ){ break; }

				if( quotationmode == 0 ){
					if( (readline[i] == ' ')||(readline[i] == '\t') ){ continue; }
					if( readline[i] == ';' ){ break; }

					if( readline[i] == '\'' ){
						quotationmode = 1;
						continue;
					}
					if( readline[i] == '"' ){
						quotationmode = 2;
						continue;
					}
				}
				if( (quotationmode == 1)&&(readline[i] == '\'') ){
					quotationmode = 0;
					continue;
				}
				if( (quotationmode == 2)&&(readline[i] == '"') ){
					quotationmode = 0;
					continue;
				}

				readline2[strcnt] = readline[i];
				readline2[strcnt+1] = '\0';
				strcnt += 1;
			}

			//"="の後ろの文字を取得し判定
			sprintf(str, "%s=", keyname);
			strcpy(buf, readline2);
			buf[ strlen(str) ] = '\0';
			if( strcmp(buf, str) == 0 ){
				state = 1;

				//データサイズ調整
				strcpy(buf, &(readline2[strlen(str)]));
				buf[strbuflen-1] = '\0';

				//値を返す
				strcpy(valuestr, buf);
				return 0;
			}
		}
	}

	//値が見つからなければエラーを返す
	strcpy(valuestr, defaultvalue);
	return 3;
}

//! @brief int値を取得
//! @param sectionname セクション名（NULL可）
//! @param keyname キー名
//! @param defaultvalue デフォルト値
//! @param errorcode エラーコードを取得するポインタ（NULL可）
//! @return 値
//! @attention ファイルが開かれていないか キーが見つからなければ、デフォルト値を返します。
//! @note エラーコード　1：引数異常、2：ファイルが開かれていない、3：値が見つからない
int INIFileInterface::GetINIFileInt(const char *sectionname, const char *keyname, int defaultvalue, int *errorcode)
{
	if( keyname == NULL ){ return defaultvalue; }

	char defaultint[64];
	char buf[64];
	int error;

	sprintf(defaultint, "%d", defaultvalue);
	error = GetINIFileString(sectionname, keyname, defaultint, buf, 64);
	if( errorcode != NULL ){ *errorcode = error; }
	return atoi(buf);
}

//! @brief float値を取得
//! @param sectionname セクション名（NULL可）
//! @param keyname キー名
//! @param defaultvalue デフォルト値
//! @param errorcode エラーコードを取得するポインタ（NULL可）
//! @return 値
//! @attention ファイルが開かれていないか キーが見つからなければ、デフォルト値を返します。
//! @note エラーコード　1：引数異常、2：ファイルが開かれていない、3：値が見つからない
float INIFileInterface::GetINIFileFloat(const char *sectionname, const char *keyname, float defaultvalue, int *errorcode)
{
	if( keyname == NULL ){ return defaultvalue; }

	char defaultfloat[64];
	char buf[64];
	int error;

	sprintf(defaultfloat, "%f", defaultvalue);
	error = GetINIFileString(sectionname, keyname, defaultfloat, buf, 64);
	if( errorcode != NULL ){ *errorcode = error; }
	for(int i=(int)strlen(buf)-1; i>0; i--){
		if( buf[i] == 'f' ){ buf[i] = '\0'; }
		if( buf[i] == 'F' ){ buf[i] = '\0'; }
	}
	return (float)atof(buf);
}

//! @brief bool値を取得
//! @param sectionname セクション名（NULL可）
//! @param keyname キー名
//! @param defaultvalue デフォルト値
//! @param errorcode エラーコードを取得するポインタ（NULL可）
//! @return 値
//! @attention ファイルが開かれていないか キーが見つからなければ、デフォルト値を返します。
//! @note エラーコード　1：引数異常、2：ファイルが開かれていない、3：値が見つからない
bool INIFileInterface::GetINIFileBool(const char *sectionname, const char *keyname, bool defaultvalue, int *errorcode)
{
	if( keyname == NULL ){ return defaultvalue; }

	char defaultbool[64];
	char buf[64];
	int error;

	if( defaultvalue == false ){
		strcpy(defaultbool, "false");
	}
	else{
		strcpy(defaultbool, "true");
	}
	error = GetINIFileString(sectionname, keyname, defaultbool, buf, 64);
	if( errorcode != NULL ){ *errorcode = error; }
	for(int j=0; j<=(int)strlen(buf); j++){ if(('A'<=buf[j])&&(buf[j]<='Z')){ buf[j] += 32; } }
	if( strcmp(buf, "false") == 0 ){
		return false;
	}
	else if( strcmp(buf, "0") == 0 ){
		return false;
	}
	//else{
		return true;
	//}
}

//! @brief INIファイルを解放
void INIFileInterface::ReleaseINIFile()
{
	if( inifp == NULL ){ return; }

	fclose(inifp);
	inifp = NULL;
}


//! @brief fgets()用　改行コードを取り除く
//! @param str 文字列
//! @return 置き換えあり：0　置き換えなし：1
int DeleteLinefeed(char *str)
{
	if( str == NULL ){ return 1; }

	char *pstr;
	bool flag = false;

	pstr = strchr(str, '\n');
	if( pstr ){
		*pstr = '\0';
		flag = true;
	}

	pstr = strchr(str, '\r');
	if( pstr ){
		*pstr = '\0';
		flag = true;
	}

	if( flag == true ){
		return 0;
	}
	return 1;
}

//! @brief ファイルパスがフルパスか判定する
//! @param path ファイルパス
//! @return フルパスである（絶対パス）：true　フルパスでない（相対パス）：false
//! @warning Windows環境しか想定されていません。
bool CheckFullPath(const char *path)
{
	if( path == NULL ){ return false; }
	if( strlen(path) < 2 ){ return false; }

	if( path[1] == ':' ){ return true; }
	return false;
}

//! @brief ファイルパスからファイル名を取り除く
//! @param path ファイルパス（ポインタ）
//! @param dir ディレクトリ名を受け取るポインタ
void GetFileDirectory(const char *path, char *dir)
{
	if( path == NULL ){ return; }
	if( dir == NULL ){ return; }

	strcpy(dir, path);

	//終端から'\\'か'/'を探し、'\0'に置き換える
	for(int i=(int)strlen(dir)-1; i>0; i--){
		if( dir[i] == '\\' ){ dir[i+1] = '\0'; break; }
		if( dir[i] == '/' ){ dir[i+1] = '\0'; break; }
	}
}

//! @brief ファイル拡張子を判定する
//! @param filepath ファイル名
//! @param checkstr 拡張子名
//! @return 該当：true　非該当：false
//! @attention 拡張子は"."込みで指定してください。大文字・小文字は区別しません。
bool CheckFileExtension(const char *filepath, const char *checkstr)
{
	if( filepath == NULL ){ return false; }
	if( checkstr == NULL ){ return false; }
	if( strlen(filepath) >= MAX_PATH ){ return false; }
	if( strlen(checkstr) >= 16 ){ return false; }
	if( strlen(filepath) < strlen(checkstr) ){ return false; }

	char filepath2[MAX_PATH];
	char checkstr2[16];

	//ファイル名を小文字へ変換
	for(int i=0; i<(int)strlen(filepath); i++){
		filepath2[i] = (char)tolower((int)filepath[i]);
	}
	filepath2[ strlen(filepath) ] = '\0';

	//拡張子名を小文字へ変換
	for(int i=0; i<(int)strlen(checkstr); i++){
		checkstr2[i] = (char)tolower((int)checkstr[i]);
	}
	checkstr2[ strlen(checkstr) ] = '\0';

	//拡張子判定
	for(int i=(int)strlen(filepath2)-1; i>0; i--){
		if( filepath2[i] == '.' ){
			if( strcmp(&(filepath2[i]), checkstr2) == 0 ){ return true; }
			return false;
		}
	}
	return false;
}

//! @brief ディレクトリの存在を判定
//! @param dir ディレクトリ名
//! @return 存在する：true　存在しない：false
bool CheckDirectory(const char *dir)
{
	return vfs_dir_exists(dir);
}
