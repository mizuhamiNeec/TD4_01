/*********************************************************************
 * \file   VoiceShockWaveComponent.h
 * \brief  声の大きさを受け取り、衝撃波として TrashObj や GolfBall に力を伝える
 *
 * 責務分離設計：
 * - 音声入力部: MagVoiceBridge から音量を取得
 * - 衝撃波検出部: 一定の音量閾値を超えたら衝撃波を発火
 * - 力の伝達部: 衝撃波の円形範囲内のエンティティに力を加える
 * 
 * \author Harukichimaru
 *********************************************************************/

#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec3.h>
#include <string>
#include <memory>
#include <cstdint>

// 前方宣言
class MagVoiceBridge;

namespace Unnamed {
	class ParticleEmitterComponent;
}

namespace MyGame {

	/// @brief 声の衝撃波を発生させ、周辺のゴミやボールに力を加えるコンポーネント
	/// 
	/// 機能：
	/// - MagVoiceBridge から声の大きさ(0.0～1.0)を取得
	/// - 一定閾値を超えたら衝撃波を発火
	/// - 衝撃波は円形の範囲内のエンティティに力を加える
	/// - TrashObjMoverComponent: SetHoleSuckPosition() で吸い込み力を与える
	/// - GolfBallComponent: 速度に直接力を加える
	class VoiceShockWaveComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新 - 音量検出と衝撃波発火
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// 衝撃波パラメータ設定
		// -----------------------------------------------------------------------

		/// @brief 衝撃波の作用範囲（半径）を設定
		/// @param radius 半径（m）、デフォルト: 10.0
		void SetShockWaveRadius(float radius);

		/// @brief 衝撃波の力の倍率を設定（音量 → 力への変換係数）
		/// @param forceMultiplier 倍率、デフォルト: 5.0
		void SetForceMultiplier(float forceMultiplier);

		/// 衝撃波発火の最小検出音量閾値を設定
		/// @param threshold 0.0～1.0、デフォルト: 0.3
		void SetVolumeThreshold(float threshold);

		/// @brief 衝撃波発火の最小クールタイム（秒）を設定
		/// @param coolTime クールタイム秒数、デフォルト: 0.5
		void SetCoolTime(float coolTime);

		// -----------------------------------------------------------------------
		// 状態取得
		// -----------------------------------------------------------------------

		/// @brief 直近の音量を取得（0.0～1.0）
		[[nodiscard]] float GetLastVolume() const;

		/// @brief 衝撃波が発火済みかを取得
		[[nodiscard]] bool IsShockWaveActive() const;

		/// @brief 衝撃波の作用範囲を取得
		[[nodiscard]] float GetShockWaveRadius() const;

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// コンポーネントの値を読み込む際に使用されます
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// コンポーネントの値を書き込む際に使用されます
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		// -----------------------------------------------------------------------
		// 衝撃波処理（内部メソッド）
		// -----------------------------------------------------------------------

		/// @brief 音声統計を取得して衝撃波発火の判定を行う
		void CheckAndFireShockWave(float deltaTime);

		/// @brief 衝撃波を発火し、範囲内のエンティティに力を加える
		/// @param volume 音量（0.0～1.0）に基づいた力の大きさ
		void FireShockWave(float volume);

		/// @brief 衝撃波範囲内のエンティティを検索して力を加える
		/// @param shockWaveCenter 衝撃波の中心座標
		/// @param forceStrength 力の大きさ
		/// @param radius 衝撃波の作用半径
		void ApplyForceToEntitiesInRange(const Vec3& shockWaveCenter, float forceStrength, float radius = 10.0f);

		/// @brief MagVoiceBridge のインスタンスを取得
		MagVoiceBridge* GetVoiceBridge();

		/// @brief 衝撃波パーティクル用エミッタの参照を解決してキャッシュする
		void ResolveShockWaveEmitters();

		/// @brief 音量・半径に応じて衝撃波パーティクルを1バースト発生させる
		/// @param volume 声ゲートを通った音量(0.0～1.0)
		/// @param dynamicRadius 実際に力が及ぶ円の半径（見た目をこれに一致させる）
		void EmitShockWaveParticles(float volume, float dynamicRadius);

		/// @brief テスト用：指定した音量で衝撃波を発火
		/// @param testVolume テスト音量（0.0～1.0）
		void FireTestShockWave(float testVolume);

	public:
		/// @brief グローバルな MagVoiceBridge インスタンスを設定（TeamGameModule から呼ばれる）
		static void SetVoiceBridgeInstance(MagVoiceBridge* bridge) {
			_voiceBridgeInstance = bridge;
		}

		// -----------------------------------------------------------------------
		// メンバ変数
		// -----------------------------------------------------------------------

		/// 衝撃波の作用範囲（円形の半径）
		float _shockWaveRadius = 15.0f;

		/// 音量から力への変換倍率（水平方向）
		float _forceMultiplier = 20.0f;

		/// 衝撃波発火の音量閾値（0.0～1.0）
		float _volumeThreshold = 0.08f;

		/// 衝撃波発火に必要なスムージング済み音量（dB）
		float _voiceMinDB = -35.0f;

		/// 衝撃波発火に必要な声らしさスコア
		float _voiceScoreThreshold = 0.35f;

		/// 声判定がこの時間継続したら衝撃波発火を許可する
		float _voiceGateDuration = 0.08f;

		/// 衝撃波発火のクールタイム（秒）
		float _coolTime = 0.1f;

		/// クールタイム用のカウンター
		float _coolTimeCounter = 0.0f;

		/// 声判定が継続している時間
		float _voiceGateTimer = 0.0f;

		/// 直近の音量（デバッグ用）
		float _lastVolume = 0.0f;

		/// 前フレームの音量（変動率計算用）
		float _previousVolume = 0.0f;

		/// 衝撃波が現在アクティブかどうか
		bool _bIsShockWaveActive = false;

		/// 衝撃波の最大半径（音量最大時）
		float _maxShockWaveRadius = 25.0f;

		/// 衝撃波の最小半径（音量最小時）
		float _minShockWaveRadius = 8.0f;

		/// 音量の変動率による力の倍率
		float _volumeDeltaMultiplier = 5.0f;

		/// 打ち上げ力（Y軸方向）- 大幅に強化
		float _upwardForceMultiplier = 50.0f;

		/// テスト用：現在のテスト音量値
		float _testVolume = 0.5f;

		// --- 衝撃波パーティクル連動 ---

		/// リング（地面に広がる波）エミッタのエンティティGUID
		uint64_t _ringEmitterEntityGuid = 72;

		/// スパーク（弾ける粒）エミッタのエンティティGUID
		uint64_t _sparksEmitterEntityGuid = 73;

		/// 解決済みのリングエミッタ（キャッシュ）
		Unnamed::ParticleEmitterComponent* _ringEmitter = nullptr;

		/// 解決済みのスパークエミッタ（キャッシュ）
		Unnamed::ParticleEmitterComponent* _sparksEmitter = nullptr;

		/// MagVoiceBridge のシングルトンインスタンス
		static MagVoiceBridge* _voiceBridgeInstance;
	};

}
