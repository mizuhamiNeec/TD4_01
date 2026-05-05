/*********************************************************************
 * \file   MagVoiceBridge.cpp
 * \brief
 *
 * ███╗   ███╗ █████╗  ██████╗ ██╗   ██╗██████═╗
 * ████╗ ████║██╔══██╗██╔════╝ ██║   ██║██╔══██╝	MagVoiceBridge.h
 * ██╔████╔██║███████║██║  ███╗██║   ██║█████║		Ver1.00
 * ██║╚██╔╝██║██╔══██║██║   ██║╚██╗ ██╔╝██╔══██╗	2026/04/23
 * ██║ ╚═╝ ██║██║  ██║╚██████╔╝ ╚████╔╝ ██████╔╝
 * ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝   ╚═══╝  ╚═════╝
 *
 * \author Harukichimaru
 * \note
 *********************************************************************/
// MagVoiceBridge.h
// NOTE: WASAPI を使用してデフォルトマイクから音声入力を取得し、
//       リアルタイム音声分析（音量・音声検出）を行う

#pragma once

#include <atomic>
#include <audioclient.h>
#include <cstdint>
#include <mmdeviceapi.h>
#include <mutex>
#include <vector>
#include <windows.h>

class MagVoiceBridge {
public:
	MagVoiceBridge();
	~MagVoiceBridge();

	// NOTE: WASAPI を初期化し、デフォルト録音デバイスを取得する
	bool Initialize();

	// NOTE: 音声キャプチャを開始する（shared mode）
	bool Start();

	// NOTE: 音声キャプチャを停止する
	void Stop();

	// NOTE: WASAPI リソースを解放し、システムをクリーンアップする
	void Shutdown();

	// NOTE: WASAPI から最新のオーディオデータを取得し、サンプルバッファに追加する
	void Update();

	// NOTE: 現在保持している音声サンプルを返す
	std::vector<float> GetSamples();

	// NOTE: 保持しているサンプルバッファをクリアする
	void ClearSamples();

	// NOTE: 現在の音量を RMS で計算し、0.0～1.0 で返す
	float GetCurrentVolume();

	// NOTE: 現在の音量を dB 単位で返す
	float GetCurrentVolumeDB();

	// NOTE: サンプリングレートを取得する
	uint32_t GetSampleRate() const;

	// NOTE: チャンネル数を取得する
	uint16_t GetChannelCount() const;

	// NOTE: ゼロクロス率と音量を用いて人の声らしさを簡易判定する
	// NOTE: true = 人の声の可能性あり, false = 音声未検出
	bool IsVoiceDetected();

	// NOTE: 最新のサンプルデータからゼロクロス率を計算する
	float CalculateZeroCrossingRate();

	// NOTE: ピークレベル（最大振幅）を 0.0～1.0 で返す
	float GetPeakVolume();

	// NOTE: ピークレベルを dB 単位で返す
	float GetPeakVolumeDB();

	// NOTE: スムージング（移動平均）された音量を取得する
	float GetSmoothedVolume();

	// NOTE: スムージングされた音量を dB 単位で返す
	float GetSmoothedVolumeDB();

	// NOTE: 音量をパーセンテージ（0～100）で返す
	float GetVolumePercentage();

	// NOTE: ゼロクロス率を利用した音声の周波数特性スコア（0.0～1.0）を返す
	float GetVoiceCharacteristicScore();

	// NOTE: スムージング係数を設定する（0.0～1.0）
	// NOTE: 高い値 = より安定、低い値 = より反応的
	void SetSmoothingFactor(float factor);

	// NOTE: 音量の正規化範囲を設定する
	// NOTE: minDb: 最小基準（例：-80dB = 無音）、maxDb: 最大基準（例：-6dB = 大声）
	void SetVolumeRange(float minDb, float maxDb);

	// NOTE: ノイズフロア（最小検出音量）を設定する
	void SetNoiseFloor(float noiseFloorDB);

	// NOTE: 音声検出の閾値パラメータを設定する
	// NOTE: zcRate=0.15～0.35、volume=-50～-30dB が一般的
	void SetVoiceDetectionThresholds(float zcRateThreshold, float volumeThresholdDB);

	// 目的: 現在の音量統計情報を取得する
	// なぜ必要か: ゲーム内で音量の統計情報を利用するため
	struct VolumeStats {
		float currentRMS{};		// 現在の RMS 値（0.0～1.0）
		float currentRMSDB{};	// 現在の RMS 値（dB）
		float peakValue{};		// ピーク値（0.0～1.0）
		float peakDB{};			// ピーク値（dB）
		float smoothedRMS{};	// スムージング済み RMS（0.0～1.0）
		float smoothedRMSDB{};	// スムージング済み RMS（dB）
		float percentage{};		// パーセンテージ（0～100）
		float voiceScore{};		// 音声特性スコア（0.0～1.0）
		bool isVoiceDetected{}; // 音声検出フラグ
	};

	// NOTE: 音量統計情報を一括取得する
	VolumeStats GetVolumeStats();

private:
	// NOTE: WASAPI からオーディオデータを取得し、サンプルバッファに追加する
	void CaptureAudioData();

	// NOTE: 最新のサンプルデータから RMS を計算する
	float CalculateRMSLevel();

	// NOTE: 最新のサンプルデータから最大振幅（ピークレベル）を計算する
	float CalculatePeakLevel();

private:
	// ===== COM インターフェース =====
	// NOTE: デバイスの列挙に使用
	IMMDeviceEnumerator *deviceEnumerator_ = nullptr;

	// NOTE: デフォルト録音デバイスを表す
	IMMDevice *captureDevice_ = nullptr;

	// NOTE: オーディオストリームを制御する
	IAudioClient *audioClient_ = nullptr;

	// NOTE: キャプチャされたデータを取得する
	IAudioCaptureClient *captureClient_ = nullptr;

	// NOTE: WASAPI が使用するオーディオフォーマット
	WAVEFORMATEX *waveFormat_ = nullptr;

	// ===== 制御フラグ =====
	// NOTE: Initialize() が完了したかを示す
	std::atomic<bool> isInitialized_ = false;

	// NOTE: Start() が完了し、キャプチャ中かを示す
	std::atomic<bool> isCapturing_ = false;

	// ===== サンプルバッファ管理 =====
	// NOTE: スレッド安全なアクセスのためのミューテックス
	std::mutex sampleMutex_;

	// NOTE: キャプチャされた音声サンプル（正規化済み float [-1.0f, 1.0f]）
	std::vector<float> sampleBuffer_;

	// ===== オーディオフォーマット情報 =====
	// NOTE: GetMixFormat() から取得したサンプリングレート（Hz）
	uint32_t sampleRate_ = 0;

	// NOTE: GetMixFormat() から取得したチャンネル数
	uint16_t channelCount_ = 0;

	// NOTE: GetMixFormat() から取得したビット深度
	uint16_t bitsPerSample_ = 0;

	// ===== リアルタイム音量情報 =====
	// NOTE: 現在の RMS レベル（0.0～1.0、スレッドセーフ）
	std::atomic<float> currentVolume_ = 0.0f;

	// NOTE: 現在の RMS レベル（dB、スレッドセーフ）
	std::atomic<float> currentVolumeDB_ = -80.0f;

	// NOTE: ピークレベル（最大振幅、スレッドセーフ）
	std::atomic<float> peakVolume_ = 0.0f;

	// NOTE: ピークレベル（dB、スレッドセーフ）
	std::atomic<float> peakVolumeDB_ = -80.0f;

	// NOTE: スムージング済み RMS レベル（0.0～1.0、スレッドセーフ）
	std::atomic<float> smoothedVolume_ = 0.0f;

	// NOTE: スムージング済み RMS レベル（dB、スレッドセーフ）
	std::atomic<float> smoothedVolumeDB_ = -80.0f;

	// NOTE: 音声特性スコア（0.0～1.0、スレッドセーフ）
	std::atomic<float> voiceCharacteristicScore_ = 0.0f;

	// ===== 音声検出パラメータ =====
	// NOTE: 人の声判定フラグ（スレッドセーフ）
	std::atomic<bool> isVoiceDetected_ = false;

	// ===== サンプルバッファ肥大化防止 =====
	// NOTE: バッファサイズの最大値（サンプル数）= 48kHz * 10秒 = 480,000サンプル
	static constexpr size_t MAX_SAMPLE_BUFFER_SIZE = 480000;

	// ===== 音声検出パラメータ =====
	// NOTE: 人の声判定のための閾値（ゼロクロス率）= 0.25
	// NOTE: 音声は 10～20%、ノイズは 30～50%
	static constexpr float ZERO_CROSSING_RATE_THRESHOLD = 0.25f;

	// NOTE: 人の声判定のための閾値（RMS 音量） = -40dB
	static constexpr float VOICE_VOLUME_THRESHOLD_DB = -40.0f;

	// ===== スムージング関連パラメータ =====
	// NOTE: 音量スムージングの係数（0.0～1.0）
	// NOTE: 0.1=より反応的、0.9=より安定
	float smoothingFactor_ = 0.4f;

	// NOTE: 音量正規化の最小値（dB）
	float volumeMinDB_ = -60.0f;

	// NOTE: 音量正規化の最大値（dB）
	float volumeMaxDB_ = -6.0f;

	// NOTE: ノイズフロア（最小検出音量）
	// NOTE: -60dB 以下は完全に無視
	float noiseFloorDB_ = -50.0f;

	// NOTE: ゼロクロス率の閾値（調整可能）
	// NOTE: デフォルト = 0.25
	float zcRateThreshold_ = 0.25f;

	// NOTE: 音量判定の閾値（調整可能）
	// NOTE: デフォルト = -40dB
	float volumeThresholdDB_ = -40.0f;
};
