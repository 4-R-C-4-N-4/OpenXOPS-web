//! @file sound-directsound.cpp
//! @brief SoundControlクラスの定義（DirectSound版）

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

#include "sound.h"

#if (SOUND_ENGINE == 1 || SOUND_ENGINE == 2)

//! @brief コンストラクタ
SoundControl::SoundControl()
{
	pDSound = NULL;
	for(int i=0;i<MAX_LOADSOUND; i++){
		for(int j=0; j<MAX_SOUNDLISTS; j++){
			pDSBuffer[i][j] = NULL;
		}
	}
	p3DListener = NULL;
	mastervolume = 1.0f;
	volume_dBper = 1.0f;
}

//! @brief ディストラクタ
SoundControl::~SoundControl()
{
	DestroySound();
}

//! @brief 初期化
//! @param WindowCtrl WindowControlクラスのポインタ
//! @return 成功：0　失敗：1
int SoundControl::InitSound(WindowControl *WindowCtrl)
{
	if( WindowCtrl == NULL ){ return 1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_INIT, "Sound", "DirectSound");
#endif

	//DirectSoundオブジェクトを生成
	if( FAILED( DirectSoundCreate8(NULL, &pDSound, NULL) ) ){
		return 1;
	}

	//協調レベルの設定
	if( FAILED( pDSound->SetCooperativeLevel(WindowCtrl->GethWnd(), DSSCL_EXCLUSIVE) ) ){
		return 1;
	}

	//プライマリ バッファを作成し、リスナーインターフェイスを取得する
	LPDIRECTSOUNDBUFFER pPrimary;
	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
	if( SUCCEEDED(pDSound->CreateSoundBuffer(&dsbd, &pPrimary, NULL)) ){
		pPrimary->QueryInterface(IID_IDirectSound3DListener8, (LPVOID *)&p3DListener);
		pPrimary->Release();
	}

	//ドップラー効果を無効に
	p3DListener->SetDopplerFactor(DS3D_MINDOPPLERFACTOR, DS3D_IMMEDIATE);

	//ロール・オフ（減衰度合い）設定
	p3DListener->SetRolloffFactor(0.05f, DS3D_IMMEDIATE);

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", "");
#endif

	return 0;
}

//! @brief 解放
//! @attention 本関数は自動的に呼び出されますが、明示的に呼び出すことも可能です。
void SoundControl::DestroySound()
{
	if( pDSound == NULL ){ return; }

	for(int i=0;i<MAX_LOADSOUND; i++){
		for(int j=0; j<MAX_SOUNDLISTS; j++){
			if( pDSBuffer[i][j] != NULL ){
				pDSBuffer[i][j]->Release();
				pDSBuffer[i][j] = NULL;
			}
		}
	}
	if( pDSound != NULL ){
		pDSound->Release();
		pDSound = NULL;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CLEANUP, "Sound", "DirectSound");
#endif
}

//! @brief 再生音量を設定
//! @param volume 再生音量　（0.0＝無音　1.0＝100％）
//! @attention 0.01（1％）未満は 0.00（0％）として処理されます。
void SoundControl::SetVolume(float volume)
{
	mastervolume = volume;
	if( mastervolume < 0.01f ){ mastervolume = 0.0f; }
	if( mastervolume > 1.00f ){ mastervolume = 1.0f; }

	//dBを考慮して最大音量を決定
	if( mastervolume == 0.0f ){ volume_dBper = 0.0f; return; }
	if( mastervolume == 1.0f ){ volume_dBper = 1.0f; return; }
	volume_dBper = 1.0f - log10f(mastervolume)*-0.2f;
}

//! @brief カメラの座標と角度を設定
//! @param x カメラのX座標
//! @param y カメラのY座標
//! @param z カメラのZ座標
//! @param rx カメラのX軸角度　（予約）
//! @warning 毎フレーム呼び出して、最新のカメラ座標を設定（適用）してください。
void SoundControl::SetCamera(float x, float y, float z, float rx)
{
	p3DListener->SetPosition(x, y, z, DS3D_IMMEDIATE);
	p3DListener->SetOrientation(cosf(rx), 0.0f, sinf(rx), 0.0f, 1.0f, 0.0f, DS3D_IMMEDIATE);
}

//! @brief サウンドを読み込む
//! @param filename ファイル名
//! @return 成功：0以上のデータ番号　失敗：-1
//! @attention 2チャンネル（ステレオ）データが指定された場合、右側のデータだけ取得され、左側のデータは無視されます。
int SoundControl::LoadSound(const char* filename)
{
	if( pDSound == NULL ){ return -1; }
	if( filename == NULL ){ return -1; }

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_LOAD, "Sound", filename);
#endif

	//開いている番号を探す
	int id=0;
	for(id=0; id<MAX_LOADSOUND; id++){
		if( pDSBuffer[id][0] == NULL ){ break; }
	}
	if( id == MAX_LOADSOUND ){ return -1; }

	WAVEFORMATEX* pwfex;
	int WavSize = 0;
	int Wavoffset = 0;
	bool d2channels = false;

	//Waveファイルの情報を取得
	if( CheckSoundFile(filename, &WavSize, &Wavoffset, &pwfex) == true ){
		return -1;
	}
	pwfex->cbSize = 0;

	//非対応フォーマットなら失敗
	if( (pwfex->wFormatTag != WAVE_FORMAT_PCM)&&(pwfex->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) ){ return -1; }
	if( (pwfex->nChannels != 1)&&(pwfex->nChannels != 2) ){ return -1; }
	if( (pwfex->wBitsPerSample != 8)&&(pwfex->wBitsPerSample != 16)
		&&(pwfex->wBitsPerSample != 24)&&(pwfex->wBitsPerSample != 32) ){ return -1; }

	//もしステレオデータなら
	if( pwfex->nChannels == 2 ){
		d2channels = true;		//フラグを設定
		WavSize /= 2;			//サイズを半分に

		//モノラルとして強制的に再計算
		pwfex->nChannels = 1;
		pwfex->nAvgBytesPerSec /= 2;
		pwfex->nBlockAlign /= 2;
	}

	// DirectSoundセカンダリーバッファー作成
	DSBUFFERDESC dsbd;  
	ZeroMemory( &dsbd, sizeof(DSBUFFERDESC) );
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME;
	dsbd.dwBufferBytes = WavSize;
	dsbd.guid3DAlgorithm = DS3DALG_NO_VIRTUALIZATION;
	dsbd.lpwfxFormat = pwfex;
	if( FAILED( pDSound->CreateSoundBuffer(&dsbd, &pDSBuffer[id][0], NULL) ) ){
		return -1;	//セカンダリバッファー作成失敗
	}

	//ロックする
	VOID* pBuffer = NULL;
	DWORD dwBufferSize = 0; 
	if( FAILED(pDSBuffer[id][0]->Lock(0, WavSize, &pBuffer, &dwBufferSize, NULL, NULL, 0) ) ){
		return -1;	//バッファのロック失敗
	}

	//一時領域を確保
	BYTE* pWavData;
	int dwSize = dwBufferSize;
	if( dwSize > WavSize ){ dwSize = WavSize; }
	if( d2channels == true ){
		dwSize = dwSize * 2;
	}
	pWavData = new BYTE[dwSize];
	if( pWavData == NULL ){
		//WAVEファイルを読み込むメモリーが確保できない
		return -1;
	}

#ifdef ENABLE_PATH_DELIMITER_SLASH
	//パス区切り文字を変換
	filename = ChangePathDelimiter(filename);
#endif

	//波形データを取り込む
	FILE* fp;
	fp = fopen(filename, "rb");
	fseek(fp, Wavoffset, SEEK_SET);
	fread(pWavData, 1, dwSize, fp);
	fclose(fp);

	//一時領域からセカンダリバッファーへコピー
	if( d2channels == false ){
		//モノラルデータなら1バイトづつコピーする
		for(int i=0; i<dwSize; i++){
			((BYTE*)pBuffer)[i] = pWavData[i];
		}
	}
	else{
		//サンプリングバイト数を取得
		int samplingbits = pwfex->wBitsPerSample;

		if( samplingbits == 8 ){
			int byte = 0;
			for(int i=0; i<dwSize; i+=2){
				/*
				int mix = (pWavData[i] + pWavData[i + 1])/2;
				((BYTE*)pBuffer)[byte] = (BYTE)mix;
				*/

				//ステレオデータなら、左側のデータだけ格納
				((BYTE*)pBuffer)[byte] = (BYTE)pWavData[i];
				byte += 1;
			}
		}
		if( samplingbits == 16 ){
			int byte = 0;
			for(int i=0; i<dwSize; i+=4){
				/*
				//int mix = (int)(Get2ByteBigEndian(&(pWavData[i + 0])) + Get2ByteBigEndian(&(pWavData[i + 2])))/2;
				int mix = (int)Get2ByteBigEndian(&(pWavData[i + 0]));
				((BYTE*)pBuffer)[byte+0] = (BYTE)((mix>>8)&0x00FF);
				((BYTE*)pBuffer)[byte+1] = (BYTE)(mix)&0x00FF;
				byte += 2;
				*/

				//ステレオデータなら、左側のデータだけ格納
				((BYTE*)pBuffer)[byte+0] = (BYTE)pWavData[i + 0];
				((BYTE*)pBuffer)[byte+1] = (BYTE)pWavData[i + 1];
				byte += 2;
			}
		}
		if( samplingbits == 24 ){
			int byte = 0;
			for(int i=0; i<dwSize; i+=6){
				//ステレオデータなら、左側のデータだけ格納
				((BYTE*)pBuffer)[byte+0] = (BYTE)pWavData[i + 0];
				((BYTE*)pBuffer)[byte+1] = (BYTE)pWavData[i + 1];
				((BYTE*)pBuffer)[byte+2] = (BYTE)pWavData[i + 2];
				byte += 3;
			}
		}
		if( samplingbits == 32 ){
			int byte = 0;
			for(int i=0; i<dwSize; i+=8){
				//ステレオデータなら、左側のデータだけ格納
				((BYTE*)pBuffer)[byte+0] = (BYTE)pWavData[i + 0];
				((BYTE*)pBuffer)[byte+1] = (BYTE)pWavData[i + 1];
				((BYTE*)pBuffer)[byte+2] = (BYTE)pWavData[i + 2];
				((BYTE*)pBuffer)[byte+3] = (BYTE)pWavData[i + 3];
				byte += 4;
			}
		}
	}

	//ロック解除
	pDSBuffer[id][0]->Unlock(pBuffer, dwBufferSize, NULL, 0);

	//一時領域を解放
	delete [] pWavData;

	//フォーマット情報解放
	delete pwfex;

	//セカンダリバッファーのコピーを作成
	for(int i=1; i<MAX_SOUNDLISTS; i++){
		if( pDSound->DuplicateSoundBuffer(pDSBuffer[id][0], &(pDSBuffer[id][i])) != DS_OK ){
			CleanupSound(id);
			return -1;
		}
	}

	//音量最小で一度再生しておく
	pDSBuffer[id][0]->SetVolume(DSBVOLUME_MIN);
	pDSBuffer[id][0]->Play(NULL, 0, NULL);

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_COMPLETE, "", id);
#endif

	return id;
}

//! @brief サウンドを再生（非3D再生）
//! @param id データ番号
//! @param volume 再生ボリューム
//! @param pan ＜無効＞
//! @return 成功：1　失敗：0
//! @note 用途：プレイヤー自身が発生する音・ゲーム空間全体で均一に鳴らす音・BGM
int SoundControl::PlaySound(int id, int volume, int pan)
{
	if( pDSound == NULL ){ return 0; }
	if( (id < 0)||(MAX_LOADSOUND <= id) ){ return 0; }
	if( pDSBuffer[id][0] == NULL ){ return 0; }

	DWORD status = 0;

	for(int i=0; i<MAX_SOUNDLISTS; i++){
		//再生状況を取得
		pDSBuffer[id][i]->GetStatus(&status);

		//サウンドが停止中ならば
		if( (status & DSBSTATUS_PLAYING) == 0x00 ){
			LPDIRECTSOUND3DBUFFER pDS3DBuffer;
			if( FAILED(pDSBuffer[id][i]->QueryInterface(IID_IDirectSound3DBuffer8, (VOID**)&pDS3DBuffer)) ){
				//IDirectSound3DBuffer8を取得できない
				return 0;
			}

			//3D再生を無効
			pDS3DBuffer->SetMode(DS3DMODE_DISABLE, DS3D_IMMEDIATE);

			//ボリュームを設定し再生
			pDSBuffer[id][i]->SetVolume( GetDSVolume(volume) );
			pDSBuffer[id][i]->Play(NULL, 0, NULL);
			return 1;
		}
	}

	return 0;
}

//! @brief サウンドを再生（3D再生）
//! @param id データ番号
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param volume 再生ボリューム
//! @return 成功：1　失敗：0
//! @note 用途：絶対的な位置を持ち距離により減衰する、一般的な効果音。
int SoundControl::Play3DSound(int id, float x, float y, float z, int volume)
{
	if( pDSound == NULL ){ return 0; }
	if( (id < 0)||(MAX_LOADSOUND <= id) ){ return 0; }
	if( pDSBuffer[id][0] == NULL ){ return 0; }

#if SOUND_ENGINE == 1

	if( volume < -100 ){ volume = -100; }
	if( volume > 100 ){ volume = 100; }

	float dist;
	int playvolume;
	int pan = 0;
	
	//距離による再生音量決定
	if( CheckSourceDist(x, y, z, false, &dist) == false ){
		return 0;
	}
	playvolume = CalculationVolume(volume, dist, false);

	return PlaySound(id, playvolume, pan);

#else	//#if SOUND_ENGINE

	DWORD status = 0;

	for(int i=0; i<MAX_SOUNDLISTS; i++){
		//再生状況を取得
		pDSBuffer[id][i]->GetStatus(&status);

		//サウンドが停止中ならば
		if( (status & DSBSTATUS_PLAYING) == 0x00 ){

			LPDIRECTSOUND3DBUFFER pDS3DBuffer;
			if( FAILED(pDSBuffer[id][i]->QueryInterface(IID_IDirectSound3DBuffer8, (VOID**)&pDS3DBuffer)) ){
				//IDirectSound3DBuffer8を取得できない
				return 0;
			}

			//最小距離と最大距離の設定
			pDS3DBuffer->SetMinDistance(1, DS3D_IMMEDIATE);
			pDS3DBuffer->SetMaxDistance(MAX_SOUNDDIST, DS3D_IMMEDIATE);

			//音源の座標を設定
			pDS3DBuffer->SetPosition(x, y, z, DS3D_IMMEDIATE);

			//3D再生を有効
			pDS3DBuffer->SetMode(DS3DMODE_NORMAL, DS3D_IMMEDIATE);

			//ボリュームを設定し再生
			pDSBuffer[id][i]->SetVolume( GetDSVolume(volume) );
			pDSBuffer[id][i]->Play(NULL, 0, NULL);
			return 1;
		}
	}

	return 0;

#endif	//#if SOUND_ENGINE
}

//! @brief 読み込み済みのサウンド数を取得
//! @return サウンド数
int SoundControl::GetTotalSounds()
{
	int cnt = 0;

	//使用中の要素を数える
	for(int i=0; i<MAX_LOADSOUND; i++){
		if( pDSBuffer[i][0] != NULL ){ cnt += 1; }
	}

	return cnt;
}

//! @brief サウンドを解放
//! @param id データ番号
void SoundControl::CleanupSound(int id)
{
	if( pDSound == NULL ){ return; }
	if( (id < 0)||(MAX_LOADSOUND <= id) ){ return; }
	if( pDSBuffer[id][0] == NULL ){ return; }

	DWORD status = 0;

	for(int i=0; i<MAX_SOUNDLISTS; i++){
		//再生状況を取得
		pDSBuffer[id][i]->GetStatus(&status);

		//サウンドが再生中ならば停止する
		if( (status & DSBSTATUS_PLAYING) != 0x00 ){
			pDSBuffer[id][i]->Stop();
		}

		//対象のセカンダリバッファーを解放
		if( pDSBuffer[id][i] != NULL ){ pDSBuffer[id][i]->Release(); }
		pDSBuffer[id][i] = NULL;
	}

#ifdef ENABLE_DEBUGLOG
	//ログに出力
	OutputLog.WriteLog(LOG_CLEANUP, "Sound", id);
#endif
}

//! @brief Waveファイルの情報を調べる
//! @param filename ファイル名
//! @param filesize ファイルサイズを受け取るポインタ
//! @param fileoffset データオフセットを受け取るポインタ
//! @param pwfex WAVEFORMATEX型を受け取る二重ポインタ
//! @return 成功：false　失敗：true
bool SoundControl::CheckSoundFile(const char* filename, int *filesize, int *fileoffset, WAVEFORMATEX** pwfex)
{
	HMMIO hMmio = NULL;
	MMCKINFO ckInfo;
	MMCKINFO riffckInfo;
	PCMWAVEFORMAT pcmWaveFormat; 
	bool errorflag;

	hMmio = mmioOpen((LPSTR)filename, NULL, MMIO_ALLOCBUF | MMIO_READ);
	if( hMmio == NULL ){
		//ファイルがない
		return true;
	}

	ZeroMemory(&riffckInfo, sizeof(MMCKINFO));

	//WAVEファイルか確認
	errorflag = true;
	if( mmioDescend( hMmio, &riffckInfo, NULL, 0 ) == MMSYSERR_NOERROR ){
		if( (mmioFOURCC('R','I','F','F') == riffckInfo.ckid) && (mmioFOURCC('W','A','V','E') == riffckInfo.fccType) ){
			errorflag = false;
		}
	}
	if( errorflag == true ){
		mmioClose(hMmio, MMIO_FHOPEN);
		return true;	//WAVEファイルでない
	}

	//フォーマット情報取得
	*pwfex = NULL;
	ckInfo.ckid = mmioFOURCC('f','m','t',' ');
	if( mmioDescend(hMmio, &ckInfo, &riffckInfo, MMIO_FINDCHUNK) == MMSYSERR_NOERROR ){
		if( mmioRead(hMmio, (HPSTR) &pcmWaveFormat, sizeof(pcmWaveFormat)) == sizeof(pcmWaveFormat) ){
			if( (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)||(pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ){
				if( (pcmWaveFormat.wf.nChannels == 1)||(pcmWaveFormat.wf.nChannels == 2) ){
					*pwfex = new WAVEFORMATEX;
					if( *pwfex != NULL ){
						memcpy( *pwfex, &pcmWaveFormat, sizeof(pcmWaveFormat) );
						//pwfex->cbSize = 0;
					}
				}
			}
		}
	}
	if( *pwfex == NULL ){
		mmioClose(hMmio, MMIO_FHOPEN);
		return true;	//ファイルフォーマットが対応してない
	}
	if( mmioAscend(hMmio, &ckInfo, 0) != MMSYSERR_NOERROR ){
		delete pwfex;
		mmioClose(hMmio, MMIO_FHOPEN);
		return true;	//チャンク制御失敗
	}

	//データ領域を調べる
	ckInfo.ckid = mmioFOURCC('d','a','t','a');
	if( mmioDescend(hMmio, &ckInfo, &riffckInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR ){
		mmioClose(hMmio, MMIO_FHOPEN);
		return true;	//ファイルフォーマットが対応してない
	}

	mmioClose(hMmio, MMIO_FHOPEN);

	*filesize = ckInfo.cksize;
	*fileoffset = ckInfo.dwDataOffset;
	return false;
}

//! @brief SetVolume()用　1/100 dB (デシベル) を計算
//! @param volume 音量（-100～100）
//! @return 1/100 dB (デシベル).
int SoundControl::GetDSVolume(int volume)
{
	if( volume <= -100 ){ return DSBVOLUME_MIN; }
	if( volume >=  100 ){ return DSBVOLUME_MAX; }

	//float volume2 = 1.0f/200 * (volume + 100);
	float volume2 = (1.0f/200 * (volume + 100)) * volume_dBper;
	int retn = (int)((DSBVOLUME_MIN-DSBVOLUME_MAX) * (1.0f - volume2));
	return retn;
}

#if SOUND_ENGINE == 1
//! @brief 音源との距離を調べる
//! @param x 音源のX座標
//! @param y 音源のY座標
//! @param z 音源のZ座標
//! @param snear 近距離音源
//! @param out_dist 距離
//! @return 有効（内）：true　無効（外）：false
bool SoundControl::CheckSourceDist(float x, float y, float z, bool snear, float *out_dist)
{
	D3DXVECTOR3 camera;
	float dx, dy, dz, dist;
	int max_dist;

	p3DListener->GetPosition(&camera);

	if( snear == false ){
		max_dist = MAX_SOUNDDIST;
	}
	else{
		max_dist = 30;
	}

	dx = camera.x - x;
	dy = camera.y - y;
	dz = camera.z - z;
	dist = dx*dx + dy*dy + dz*dz;
	if( dist > max_dist * max_dist ){
		*out_dist = 0.0f;
		return false;
	}

	*out_dist = sqrtf(dist);
	return true;
}

//! @brief 音量を計算
//! @param MaxVolume 音源の最大音量
//! @param dist 音源との距離
//! @param snear 近距離音源
int SoundControl::CalculationVolume(int MaxVolume, float dist, bool snear)
{
	int max_dist;
	if( snear == false ){
		max_dist = MAX_SOUNDDIST;
	}
	else{
		max_dist = 30;
	}
	return (int)( (float)MaxVolume/max_dist*dist*-1 + MaxVolume );
}
#endif

#endif	//SOUND_ENGINE