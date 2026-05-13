#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "MagVoiceBridge.h"
#include <cstdint>
#include <memory>
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

namespace MyGame {

	class MicComponent : public Unnamed::BaseComponent {
	public:
		// NOTE: 起動時に動作
		void OnAttached() override;

		// -----------------------------------------------------------------------
		// マイク制御
		// -----------------------------------------------------------------------

		/// マイク入力の初期化（WASAPI初期化、デバイス取得）
		bool InitializeMic();

		/// マイク入力を開始（オーディオストリーム開始）
		bool StartMic();

		/// マイク入力を停止（オーディオストリーム停止）
		void StopMic();

		/// マイク入力をシャットダウン（リソース解放）
		void ShutdownMic();

		// -----------------------------------------------------------------------
		// 音声データ取得インターフェース
		// -----------------------------------------------------------------------

		/// 現在のスムージング済み音量を取得（0.0～1.0）
		[[nodiscard]] float GetSmoothedVolume() const;

		/// 現在の音量（RMS）を取得（0.0～1.0）
		[[nodiscard]] float GetCurrentVolume() const;

		/// 現在の音量（dB）を取得
		[[nodiscard]] float GetCurrentVolumeDB() const;

		/// ピークレベルを取得（0.0～1.0）
		[[nodiscard]] float GetPeakVolume() const;

		/// ピークレベル（dB）を取得
		[[nodiscard]] float GetPeakVolumeDB() const;

		/// スムージング済み音量（dB）を取得
		[[nodiscard]] float GetSmoothedVolumeDB() const;

		/// 音量をパーセンテージ（0～100）で取得
		[[nodiscard]] float GetVolumePercentage() const;

		/// 人の声が検出されているか判定
		[[nodiscard]] bool IsVoiceDetected() const;

		/// ゼロクロス率を計算
		[[nodiscard]] float CalculateZeroCrossingRate() const;

		/// 音声特性スコアを取得（0.0=ノイズ、1.0=人の声）
		[[nodiscard]] float GetVoiceCharacteristicScore() const;

		/// 音量統計情報を一括取得
		[[nodiscard]] MagVoiceBridge::VolumeStats GetVolumeStats() const;

		/// 取得済みサンプルを取得
		[[nodiscard]] std::vector<float> GetSamples() const;

		// -----------------------------------------------------------------------
		// パラメータ調整
		// -----------------------------------------------------------------------

		/// スムージング係数を設定（0.0～1.0）
		void SetSmoothingFactor(float factor);

		/// 音量範囲を設定（dB）
		void SetVolumeRange(float minDb, float maxDb);

		/// ノイズフロアを設定（dB）
		void SetNoiseFloor(float noiseFloorDB);

		/// 音声検出の閾値を設定
		void SetVoiceDetectionThresholds(float zcRateThreshold, float volumeThresholdDB);

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// 毎フレーム更新（MagVoiceBridge::Update()を呼び出し）
		void OnTick(float deltaTime) override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		// -----------------------------------------------------------------------
		// 公式が忘れてたやつ
		// -----------------------------------------------------------------------

		/// @brief コンポーネントの値を読み込む際に使用されます。
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// @brief コンポーネントの値を書き込む際に使用されます。
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		/// マイク入力で拡縮する対象のエンティティGUID(初期はオーナー)
		uint64_t scaleTargetEntityGuid_ = 0u;

		/// スケール変化の感度（0.0fで変化なし、1.0fで完全に反応）
		Vec3 scaleMultiplier = Vec3::one;
		
		/// 音量変化に対するスケールの反応速度(1.0fで即座に反応)
		float scaleResponsiveness_ = 0.25f;
		
		/// 内部的なMagVoiceBridgeインスタンス
		std::unique_ptr<MagVoiceBridge> voiceBridge_;

		/// 初期化状態フラグ
		bool bIsInitialized_ = false;

		/// キャプチャ中フラグ
		bool bIsCapturing_ = false;
	};

}