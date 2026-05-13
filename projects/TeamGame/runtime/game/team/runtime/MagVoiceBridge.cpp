#include "MagVoiceBridge.h"

#include <algorithm>
#include <cmath>
#include <combaseapi.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

//========================================
// コンストラクタ / デストラクタ
//========================================

MagVoiceBridge::MagVoiceBridge() {
	// NOTE: メンバ変数は宣言時に初期化リスト経由で初期化されます
}

MagVoiceBridge::~MagVoiceBridge() {
	// NOTE: Shutdown() は冪等性を持つため、二重解放の心配なく呼び出せます
	Shutdown();
}

//========================================
// 初期化 / シャットダウン
//========================================

bool MagVoiceBridge::Initialize() {
	HRESULT result = S_OK;

	// NOTE: COM ライブラリを初期化する
	result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(result)) {
		return false;
	}

	// NOTE: デバイス列挙用オブジェクトを生成する
	result = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		nullptr,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void **>(&deviceEnumerator_));

	if (FAILED(result)) {
		return false;
	}

	// NOTE: デフォルトの録音デバイスを取得する
	result = deviceEnumerator_->GetDefaultAudioEndpoint(
		eCapture,
		eConsole,
		&captureDevice_);

	if (FAILED(result)) {
		return false;
	}

	// NOTE: 録音デバイスの IAudioClient インターフェースを取得する
	result = captureDevice_->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL,
		nullptr,
		reinterpret_cast<void **>(&audioClient_));

	if (FAILED(result)) {
		return false;
	}

	// NOTE: マイクのネイティブオーディオフォーマットを取得する
	result = audioClient_->GetMixFormat(&waveFormat_);
	if (FAILED(result)) {
		return false;
	}

	// NOTE: オーディオフォーマット情報をメンバ変数に保存する
	sampleRate_ = waveFormat_->nSamplesPerSec;
	channelCount_ = waveFormat_->nChannels;
	bitsPerSample_ = waveFormat_->wBitsPerSample;

	// REVIEW: サンプリングレート 0 はあり得ない。妥当性チェック
	if (sampleRate_ == 0 || channelCount_ == 0) {
		return false;
	}

	// NOTE: shared mode でオーディオクライアントを初期化する
	// OPTIMIZE: バッファ期間 10000000 (1秒) は環境に応じて調整可能
	REFERENCE_TIME bufferDuration = 10000000;

	result = audioClient_->Initialize(
		AUDCLNT_SHAREMODE_SHARED, // shared mode
		0,						  // フラグなし
		bufferDuration,			  // バッファ期間
		0,						  // デバイス期間（shared mode では 0）
		waveFormat_,			  // オーディオフォーマット
		nullptr);				  // GUID なし

	if (FAILED(result)) {
		return false;
	}

	// NOTE: IAudioCaptureClient インターフェースを取得する
	result = audioClient_->GetService(
		__uuidof(IAudioCaptureClient),
		reinterpret_cast<void **>(&captureClient_));

	if (FAILED(result)) {
		return false;
	}

	// NOTE: 初期化完了フラグを設定する
	isInitialized_ = true;
	return true;
}

bool MagVoiceBridge::Start() {
	// REVIEW: Initialize() の前に Start() が呼ばれる場合を想定
	if (!isInitialized_) {
		return false;
	}

	// PURPOSE: オーディオストリームを開始する
	// REASON: WASAPI からのデータキャプチャを開始するため
	HRESULT result = audioClient_->Start();
	if (FAILED(result)) {
		return false;
	}

	// NOTE: キャプチャ中フラグを設定する
	isCapturing_ = true;
	return true;
}

void MagVoiceBridge::Stop() {
	// NOTE: オーディオストリームを停止する
	if (audioClient_ && isCapturing_) {
		audioClient_->Stop();
		isCapturing_ = false;
	}
}

void MagVoiceBridge::Shutdown() {
	// NOTE: キャプチャを停止する
	Stop();

	// NOTE: COM インターフェースをリリースする
	if (captureClient_) {
		captureClient_->Release();
		captureClient_ = nullptr;
	}

	if (audioClient_) {
		audioClient_->Release();
		audioClient_ = nullptr;
	}

	if (captureDevice_) {
		captureDevice_->Release();
		captureDevice_ = nullptr;
	}

	if (deviceEnumerator_) {
		deviceEnumerator_->Release();
		deviceEnumerator_ = nullptr;
	}

	// NOTE: waveFormat_ のメモリを解放する
	if (waveFormat_) {
		CoTaskMemFree(waveFormat_);
		waveFormat_ = nullptr;
	}

	// NOTE: COM ライブラリをクリーンアップする
	CoUninitialize();

	// NOTE: 初期化フラグをリセットする
	isInitialized_ = false;
	isCapturing_ = false;
}

//========================================
// メイン処理
//========================================

void MagVoiceBridge::Update() {
	// NOTE: Start() 前の無駄な処理を回避する
	if (!isCapturing_) {
		return;
	}

	// NOTE: WASAPI からリアルタイムのオーディオデータを取得する
	CaptureAudioData();

	// NOTE: 最新のサンプルから音量を計算する
	float rmsLevel = CalculateRMSLevel();
	currentVolume_ = rmsLevel;

	// NOTE: RMS をデシベルに変換する（計算式: dB = 20 * log10(RMS)）
	float volumeDB = 20.0f * std::log10(std::max(0.00001f, rmsLevel));
	currentVolumeDB_ = volumeDB;

	// NOTE: ピークレベル（最大振幅）を計算する
	float peakLevel = CalculatePeakLevel();
	peakVolume_ = peakLevel;

	// NOTE: ピークレベルを dB に変換する
	float peakDB = 20.0f * std::log10(std::max(0.00001f, peakLevel));
	peakVolumeDB_ = peakDB;

	// NOTE: スムージング処理を適用する（式: smoothed = smoothed * (1 - factor) + current * factor）
	float currentSmoothed = smoothedVolume_.load();
	float newSmoothed = currentSmoothed * (1.0f - smoothingFactor_) + rmsLevel * smoothingFactor_;
	smoothedVolume_ = newSmoothed;

	float currentSmoothedDB = smoothedVolumeDB_.load();
	float newSmoothedDB = currentSmoothedDB * (1.0f - smoothingFactor_) + volumeDB * smoothingFactor_;
	smoothedVolumeDB_ = newSmoothedDB;

	// NOTE: 音声特性スコアを計算する
	// NOTE: ZCR が低い（0.0に近い）ほど高スコア（1.0に近い）= 人の声らしい
	float zeroCrossingRate = CalculateZeroCrossingRate();
	float voiceScore = std::max(0.0f, 1.0f - (zeroCrossingRate / ZERO_CROSSING_RATE_THRESHOLD));
	voiceCharacteristicScore_ = voiceScore;

	// NOTE: ゼロクロス率と音量から人の声かどうかを判定する
	bool voiceDetected = IsVoiceDetected();
	isVoiceDetected_ = voiceDetected;
}

void MagVoiceBridge::CaptureAudioData() {
	// NOTE: WASAPI バッファに利用可能なフレーム数を取得する
	uint32_t packetLength = 0;
	HRESULT result = captureClient_->GetNextPacketSize(&packetLength);

	// REVIEW: データが利用できない場合は処理をスキップ
	if (FAILED(result) || packetLength == 0) {
		return;
	}

	// NOTE: WASAPI バッファからオーディオデータを取得する
	BYTE *pData = nullptr;
	DWORD flags = 0;
	uint32_t numFramesAvailable = 0;

	result = captureClient_->GetBuffer(
		&pData,				 // データへのポインタ
		&numFramesAvailable, // 利用可能なフレーム数
		&flags,				 // フラグ（サイレンス判定など）
		nullptr,			 // デバイス位置（不使用）
		nullptr);			 // バッファ位置（不使用）

	// REVIEW: データ取得失敗時または nullptr の場合は処理をスキップ
	if (FAILED(result) || !pData) {
		return;
	}

	// NOTE: フレーム数を更新する
	packetLength = numFramesAvailable;

	{
		std::lock_guard<std::mutex> lock(sampleMutex_);

		// NOTE: WASAPI Shared Mode では 32-bit float でデータが返される
		float *pFloatData = reinterpret_cast<float *>(pData);

		// NOTE: マルチチャンネルの場合、全チャンネルを合成する
		for (uint32_t frame = 0; frame < packetLength; ++frame) {
			float channelAverage = 0.0f;

			// NOTE: 全チャンネルの絶対値を加算する
			for (uint16_t ch = 0; ch < channelCount_; ++ch) {
				uint32_t sampleIndex = frame * channelCount_ + ch;
				channelAverage += std::abs(pFloatData[sampleIndex]);
			}

			// NOTE: チャンネル数で平均化してモノラルに統一する
			channelAverage /= static_cast<float>(channelCount_);

			// NOTE: サンプルをバッファに追加する
			sampleBuffer_.push_back(channelAverage);
		}

		// NOTE: サンプルバッファが肥大化しないようサイズを制限する
		if (sampleBuffer_.size() > MAX_SAMPLE_BUFFER_SIZE) {
			// NOTE: 古いサンプルを削除して新しいサンプルのみを保持
			sampleBuffer_.erase(
				sampleBuffer_.begin(),
				sampleBuffer_.end() - MAX_SAMPLE_BUFFER_SIZE);
		}
	}

	// NOTE: WASAPI バッファを解放する
	captureClient_->ReleaseBuffer(packetLength);
}

float MagVoiceBridge::CalculateZeroCrossingRate() {
	std::lock_guard<std::mutex> lock(sampleMutex_);

	// NOTE: サンプルバッファが空の場合は 0 を返す
	if (sampleBuffer_.size() < 2) {
		return 0.0f;
	}

	// NOTE: 最新の 2048 サンプルでゼロクロス率を計算する
	const size_t ANALYSIS_WINDOW = 2048;
	const size_t startIdx = sampleBuffer_.size() > ANALYSIS_WINDOW
								? sampleBuffer_.size() - ANALYSIS_WINDOW
								: 0;

	// NOTE: ゼロクロッシング（符号変化）の回数を数える
	int zeroCrossingCount = 0;
	for (size_t i = startIdx + 1; i < sampleBuffer_.size(); ++i) {
		// NOTE: 前後のサンプルで符号が変わっているかを確認
		if ((sampleBuffer_[i - 1] < 0.0f && sampleBuffer_[i] >= 0.0f) ||
			(sampleBuffer_[i - 1] >= 0.0f && sampleBuffer_[i] < 0.0f)) {
			zeroCrossingCount++;
		}
	}

	// NOTE: ゼロクロス率を計算する（0.0～1.0、計算式: ZCR = ゼロクロッシング数 / (サンプル数 - 1)）
	size_t sampleCount = sampleBuffer_.size() - startIdx;
	float zeroCrossingRate = static_cast<float>(zeroCrossingCount) / static_cast<float>(sampleCount);

	return zeroCrossingRate;
}

float MagVoiceBridge::CalculateRMSLevel() {
	std::lock_guard<std::mutex> lock(sampleMutex_);

	// NOTE: サンプルバッファが空の場合は 0 を返す
	if (sampleBuffer_.empty()) {
		return 0.0f;
	}

	// NOTE: 最新の 2048 サンプルで RMS を計算する
	const size_t ANALYSIS_WINDOW = 2048;
	const size_t startIdx = sampleBuffer_.size() > ANALYSIS_WINDOW
								? sampleBuffer_.size() - ANALYSIS_WINDOW
								: 0;

	// NOTE: RMS（二乗平均平方根）を計算する（計算式: RMS = sqrt( sum(sample^2) / N )）
	float sumOfSquares = 0.0f;
	for (size_t i = startIdx; i < sampleBuffer_.size(); ++i) {
		float sample = sampleBuffer_[i];
		sumOfSquares += sample * sample;
	}

	size_t sampleCount = sampleBuffer_.size() - startIdx;
	float rmsLevel = std::sqrt(sumOfSquares / static_cast<float>(sampleCount));

	return rmsLevel;
}

float MagVoiceBridge::CalculatePeakLevel() {
	std::lock_guard<std::mutex> lock(sampleMutex_);

	// NOTE: サンプルバッファが空の場合は 0 を返す
	if (sampleBuffer_.empty()) {
		return 0.0f;
	}

	// NOTE: 最新の 2048 サンプルで最大振幅を計算する
	const size_t ANALYSIS_WINDOW = 2048;
	const size_t startIdx = sampleBuffer_.size() > ANALYSIS_WINDOW
								? sampleBuffer_.size() - ANALYSIS_WINDOW
								: 0;

	// NOTE: 最大振幅を見つける
	float maxAbsValue = 0.0f;
	for (size_t i = startIdx; i < sampleBuffer_.size(); ++i) {
		float absValue = std::abs(sampleBuffer_[i]);
		if (absValue > maxAbsValue) {
			maxAbsValue = absValue;
		}
	}

	return maxAbsValue;
}

//========================================
// ゲッター
//========================================

std::vector<float> MagVoiceBridge::GetSamples() {
	// NOTE: 現在のサンプルバッファをコピーして返す
	std::lock_guard<std::mutex> lock(sampleMutex_);
	return sampleBuffer_;
}

void MagVoiceBridge::ClearSamples() {
	// NOTE: サンプルバッファをクリアする
	std::lock_guard<std::mutex> lock(sampleMutex_);
	sampleBuffer_.clear();
}

float MagVoiceBridge::GetCurrentVolume() {
	// NOTE: 現在の RMS 音量を 0.0～1.0 で返す
	return currentVolume_.load();
}

float MagVoiceBridge::GetCurrentVolumeDB() {
	// NOTE: 現在の RMS 音量を dB 単位で返す
	return currentVolumeDB_.load();
}

uint32_t MagVoiceBridge::GetSampleRate() const {
	// NOTE: オーディオフォーマットのサンプリングレートを返す
	// WARNING: Initialize() 前に呼び出すと 0 が返されます
	return sampleRate_;
}

uint16_t MagVoiceBridge::GetChannelCount() const {
	// NOTE: オーディオフォーマットのチャンネル数を返す
	// WARNING: Initialize() 前に呼び出すと 0 が返されます
	return channelCount_;
}

bool MagVoiceBridge::IsVoiceDetected() {
	// NOTE: ゼロクロス率と音量から人の声かどうかを判定する

	// NOTE: ゼロクロス率を計算する
	float zeroCrossingRate = CalculateZeroCrossingRate();

	// NOTE: 音量（dB）を取得する
	float volumeDB = GetCurrentVolumeDB();

	// NOTE: ノイズフロアより大きいかを確認する
	if (volumeDB < noiseFloorDB_) {
		return false;
	}

	// NOTE: 人の声判定 - 両方の条件を満たすときのみ音声と判定する
	// NOTE: 条件 1: ゼロクロス率が閾値未満（高周波成分が少ない）
	// NOTE: 条件 2: 音量が閾値以上（無音や小さすぎる音を除外）
	bool isVoice = (zeroCrossingRate < zcRateThreshold_) &&
				   (volumeDB > volumeThresholdDB_);

	return isVoice;
}

float MagVoiceBridge::GetPeakVolume() {
	// NOTE: ピークレベルを 0.0～1.0 で返す
	return peakVolume_.load();
}

float MagVoiceBridge::GetPeakVolumeDB() {
	// NOTE: ピークレベルを dB 単位で返す
	return peakVolumeDB_.load();
}

float MagVoiceBridge::GetSmoothedVolume() {
	// NOTE: スムージング済み音量を 0.0～1.0 で返す
	return smoothedVolume_.load();
}

float MagVoiceBridge::GetSmoothedVolumeDB() {
	// NOTE: スムージング済み音量を dB 単位で返す
	return smoothedVolumeDB_.load();
}

float MagVoiceBridge::GetVolumePercentage() {
	// NOTE: 現在の音量をパーセンテージ（0～100）で返す
	float volumeDB = currentVolumeDB_.load();

	// NOTE: dB値を 0～100% の範囲に正規化する
	float normalized = (volumeDB - volumeMinDB_) / (volumeMaxDB_ - volumeMinDB_);
	normalized = std::clamp(normalized, 0.0f, 1.0f);

	return normalized * 100.0f;
}

float MagVoiceBridge::GetVoiceCharacteristicScore() {
	// NOTE: 音声特性スコアを返す（0.0～1.0）
	// NOTE: 1.0に近いほど人の声、0.0に近いほどノイズ
	return voiceCharacteristicScore_.load();
}

void MagVoiceBridge::SetSmoothingFactor(float factor) {
	// NOTE: スムージング係数を設定する（0.0～1.0）
	// NOTE: デフォルト: 0.4。0.1=より反応的、0.9=より安定
	smoothingFactor_ = std::clamp(factor, 0.0f, 1.0f);
}

void MagVoiceBridge::SetVolumeRange(float minDb, float maxDb) {
	// NOTE: 音量の正規化範囲を設定する
	// NOTE: GetVolumePercentage() の計算に使用されます
	if (minDb < maxDb) {
		volumeMinDB_ = minDb;
		volumeMaxDB_ = maxDb;
	}
}

void MagVoiceBridge::SetNoiseFloor(float noiseFloorDB) {
	// NOTE: ノイズフロアを設定する
	// NOTE: この値より小さい音量は音声判定から除外されます
	noiseFloorDB_ = noiseFloorDB;
}

void MagVoiceBridge::SetVoiceDetectionThresholds(float zcRateThreshold, float volumeThresholdDB) {
	// NOTE: 音声検出の閾値パラメータを設定する
	// NOTE: デフォルト: zcRate=0.25, volume=-40dB
	zcRateThreshold_ = std::clamp(zcRateThreshold, 0.0f, 1.0f);
	volumeThresholdDB_ = volumeThresholdDB;
}

MagVoiceBridge::VolumeStats MagVoiceBridge::GetVolumeStats() {
	// NOTE: 全ての音量統計情報を一括取得する
	// NOTE: ImGui など UI 表示で複数の値を効率的に取得できます
	VolumeStats stats{};
	stats.currentRMS = currentVolume_.load();
	stats.currentRMSDB = currentVolumeDB_.load();
	stats.peakValue = peakVolume_.load();
	stats.peakDB = peakVolumeDB_.load();
	stats.smoothedRMS = smoothedVolume_.load();
	stats.smoothedRMSDB = smoothedVolumeDB_.load();
	stats.voiceScore = voiceCharacteristicScore_.load();
	stats.isVoiceDetected = isVoiceDetected_.load();

	// NOTE: パーセンテージを計算して追加する
	stats.percentage = GetVolumePercentage();

	return stats;
}
