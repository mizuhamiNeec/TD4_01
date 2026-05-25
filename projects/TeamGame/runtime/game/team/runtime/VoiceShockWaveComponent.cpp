/*********************************************************************
 * \file   VoiceShockWaveComponent.cpp
 * \brief  声の大きさを受け取り、衝撃波として TrashObj や GolfBall に力を伝える
 *
 * \author Harukichimaru
 *********************************************************************/

#include "VoiceShockWaveComponent.h"
#include "MagVoiceBridge.h"
#include "TrashObjMoverComponent.h"
#include "GolfBallComponent.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"
#include "engine/scene/Scene.h"
#include "./core/ComponentRegistry.h"

#include <core/math/Vec3.h>
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// =========================================================================
	// 静的メンバ初期化
	// =========================================================================

	MagVoiceBridge* VoiceShockWaveComponent::_voiceBridgeInstance = nullptr;

	// =========================================================================
	// ライフサイクル
	// =========================================================================

	void VoiceShockWaveComponent::OnAttached() {
		// NOTE: MagVoiceBridge の初期化（初回のみ）
		if (!_voiceBridgeInstance) {
			_voiceBridgeInstance = new MagVoiceBridge();
			if (_voiceBridgeInstance && _voiceBridgeInstance->Initialize()) {
				_voiceBridgeInstance->Start();
			}
		}

		_coolTimeCounter = 0.0f;
		_lastVolume = 0.0f;
		_previousVolume = 0.0f;
		_bIsShockWaveActive = false;
	}

	void VoiceShockWaveComponent::OnTick(float deltaTime) {
		// NOTE: MagVoiceBridge を更新
		auto* voiceBridge = GetVoiceBridge();
		if (voiceBridge) {
			voiceBridge->Update();
		}

		// NOTE: クールタイムのカウントダウン
		if (_coolTimeCounter > 0.0f) {
			_coolTimeCounter -= deltaTime;
		}

		// NOTE: 音量チェックと衝撃波発火判定
		CheckAndFireShockWave();

		// NOTE: 衝撃波のアクティブ状態をリセット（毎フレーム）
		_bIsShockWaveActive = false;
	}

	void VoiceShockWaveComponent::OnDetached() {
		// NOTE: MagVoiceBridge はシングルトンなので解放しない
		// （複数のコンポーネントが共有している可能性があるため）
	}

	// =========================================================================
	// 衝撃波パラメータ設定
	// =========================================================================

	void VoiceShockWaveComponent::SetShockWaveRadius(float radius) {
		_shockWaveRadius = std::max(0.1f, radius);
	}

	void VoiceShockWaveComponent::SetForceMultiplier(float forceMultiplier) {
		_forceMultiplier = std::max(0.1f, forceMultiplier);
	}

	void VoiceShockWaveComponent::SetVolumeThreshold(float threshold) {
		_volumeThreshold = std::clamp(threshold, 0.0f, 1.0f);
	}

	void VoiceShockWaveComponent::SetCoolTime(float coolTime) {
		_coolTime = std::max(0.0f, coolTime);
	}

	// =========================================================================
	// 状態取得
	// =========================================================================

	float VoiceShockWaveComponent::GetLastVolume() const {
		return _lastVolume;
	}

	bool VoiceShockWaveComponent::IsShockWaveActive() const {
		return _bIsShockWaveActive;
	}

	float VoiceShockWaveComponent::GetShockWaveRadius() const {
		return _shockWaveRadius;
	}

	// =========================================================================
	// BaseComponent override
	// =========================================================================

	std::string_view VoiceShockWaveComponent::GetStableName() const {
		return "mygame.gameplay.VoiceShockWave";
	}

	std::string_view VoiceShockWaveComponent::GetComponentName() const {
		return "Voice Shock Wave";
	}

#ifdef _DEBUG
	void VoiceShockWaveComponent::DrawInspectorImGui() {
		ImGui::Text("=== 音声衝撃波パラメータ ===");

		// NOTE: 衝撃波パラメータの表示・編集
		ImGui::SliderFloat("最小衝撃波半径##vsw_min_radius", &_minShockWaveRadius, 1.0f, 30.0f, "%.2f");
		ImGui::SliderFloat("最大衝撃波半径##vsw_max_radius", &_maxShockWaveRadius, 5.0f, 50.0f, "%.2f");
		ImGui::SliderFloat("水平方向の力倍率##vsw_force", &_forceMultiplier, 0.1f, 20.0f, "%.2f");
		ImGui::SliderFloat("音量変動の力倍率##vsw_delta", &_volumeDeltaMultiplier, 0.1f, 50.0f, "%.2f");
		ImGui::SliderFloat("打ち上げ力倍率##vsw_upward", &_upwardForceMultiplier, 1.0f, 50.0f, "%.2f");
		ImGui::SliderFloat("音量閾値##vsw_threshold", &_volumeThreshold, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("クールタイム##vsw_cooltime", &_coolTime, 0.0f, 5.0f, "%.2f");

		ImGui::Separator();

		// NOTE: 現在の音量情報を表示
		ImGui::Text("=== 音声情報 ===");
		ImGui::Text("現在の音量: %.3f", _lastVolume);
		ImGui::Text("前フレーム音量: %.3f", _previousVolume);
		ImGui::Text("音量の変動量: %.3f", std::abs(_lastVolume - _previousVolume));
		ImGui::Text("音量閾値: %.3f", _volumeThreshold);
		ImGui::Text("衝撃波アクティブ: %s", _bIsShockWaveActive ? "有効" : "無効");
		ImGui::Text("クールタイム: %.2f / %.2f秒", _coolTimeCounter, _coolTime);

		ImGui::Separator();

		// NOTE: テスト用：声発生ボタン
		ImGui::Text("=== テスト用：声発生 ===");
		ImGui::SliderFloat("テスト音量##test_volume", &_testVolume, 0.0f, 1.0f, "%.3f");
		
		if (ImGui::Button("弱い声を出す##test_weak", ImVec2(150, 30))) {
			FireTestShockWave(0.3f);
		}
		ImGui::SameLine();
		if (ImGui::Button("普通の声##test_normal", ImVec2(150, 30))) {
			FireTestShockWave(0.6f);
		}
		ImGui::SameLine();
		if (ImGui::Button("大きい声##test_loud", ImVec2(150, 30))) {
			FireTestShockWave(1.0f);
		}
		
		if (ImGui::Button("カスタム音量で発火##test_custom", ImVec2(300, 30))) {
			FireTestShockWave(_testVolume);
		}

		// NOTE: MagVoiceBridge の情報を表示
		auto* voiceBridge = GetVoiceBridge();
		if (voiceBridge) {
			auto stats = voiceBridge->GetVolumeStats();
			ImGui::Separator();
			ImGui::Text("=== 音声統計情報 ===");
			ImGui::Text("現在のRMS: %.3f (%.2f dB)", stats.currentRMS, stats.currentRMSDB);
			ImGui::Text("ピーク音量: %.3f (%.2f dB)", stats.peakValue, stats.peakDB);
			ImGui::Text("スムージング済み音量: %.3f (%.2f dB)", stats.smoothedRMS, stats.smoothedRMSDB);
			ImGui::Text("音声スコア: %.3f", stats.voiceScore);
			ImGui::Text("音声検出: %s", stats.isVoiceDetected ? "検出中" : "未検出");
		}
	}
#endif

	void VoiceShockWaveComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSON から パラメータを読み込む
		// Read() は std::optional<T> を返すため、value() で値を取得
		if (auto val = reader.Read<float>("minShockWaveRadius")) {
			_minShockWaveRadius = val.value();
		}
		if (auto val = reader.Read<float>("maxShockWaveRadius")) {
			_maxShockWaveRadius = val.value();
		}
		if (auto val = reader.Read<float>("forceMultiplier")) {
			_forceMultiplier = val.value();
		}
		if (auto val = reader.Read<float>("volumeDeltaMultiplier")) {
			_volumeDeltaMultiplier = val.value();
		}
		if (auto val = reader.Read<float>("upwardForceMultiplier")) {
			_upwardForceMultiplier = val.value();
		}
		if (auto val = reader.Read<float>("volumeThreshold")) {
			_volumeThreshold = val.value();
		}
		if (auto val = reader.Read<float>("coolTime")) {
			_coolTime = val.value();
		}
	}

	void VoiceShockWaveComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: パラメータを JSON に書き込む
		// Key(), Write() の順序で呼び出す
		writer.Key("minShockWaveRadius");
		writer.Write(_minShockWaveRadius);
		writer.Key("maxShockWaveRadius");
		writer.Write(_maxShockWaveRadius);
		writer.Key("forceMultiplier");
		writer.Write(_forceMultiplier);
		writer.Key("volumeDeltaMultiplier");
		writer.Write(_volumeDeltaMultiplier);
		writer.Key("upwardForceMultiplier");
		writer.Write(_upwardForceMultiplier);
		writer.Key("volumeThreshold");
		writer.Write(_volumeThreshold);
		writer.Key("coolTime");
		writer.Write(_coolTime);
	}

	// =========================================================================
	// 衝撃波処理（内部メソッド）
	// =========================================================================

	void VoiceShockWaveComponent::CheckAndFireShockWave() {
		// NOTE: MagVoiceBridge から音量を取得
		auto* voiceBridge = GetVoiceBridge();
		if (!voiceBridge) {
			// NOTE: 初期化がまだ完了していない
			return;
		}

		// NOTE: スムージング済み音量を取得（ノイズ対策）
		try {
			_lastVolume = voiceBridge->GetSmoothedVolume();
		} catch (...) {
			// NOTE: MagVoiceBridge のメソッド呼び出しが失敗した場合
			return;
		}

		// NOTE: 音量が閾値を超えており、クールタイムが終了している場合
		if (_lastVolume >= _volumeThreshold && _coolTimeCounter <= 0.0f) {
			// NOTE: 衝撃波を発火
			FireShockWave(_lastVolume);

			// NOTE: クールタイムをリセット
			_coolTimeCounter = _coolTime;
		}

		// NOTE: 次フレーム用に現在の音量を保存
		_previousVolume = _lastVolume;
	}

	void VoiceShockWaveComponent::FireShockWave(float volume) {
		// NOTE: 音量の変動率を計算（0-0.2 vs 0-0.8 のような急激な変化を検出）
		float volumeDelta = std::abs(volume - _previousVolume);
		
		// NOTE: 基本的な力を計算（音量ベース）
		float baseForce = volume * _forceMultiplier;
		
		// NOTE: 変動率による力の増幅（急激な変化ほど強い）
		float deltaForce = volumeDelta * _volumeDeltaMultiplier;
		
		// NOTE: 最終的な力 = ベース力 + 変動による増幅
		float forceStrength = baseForce + deltaForce;

		// NOTE: 衝撃波の半径を音量に応じて動的に変更
		// volume が 0.0 なら _minShockWaveRadius、1.0 なら _maxShockWaveRadius
		float dynamicRadius = _minShockWaveRadius + (volume * (_maxShockWaveRadius - _minShockWaveRadius));

		// NOTE: このコンポーネントの所有エンティティから位置を取得
		auto* ownerEntity = GetOwner();
		if (!ownerEntity) {
			return;
		}

		auto* transform = ownerEntity->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return;
		}

		Vec3 shockWaveCenter = transform->GetPosition();

		// NOTE: 衝撃波を発火（動的半径を使用）
		ApplyForceToEntitiesInRange(shockWaveCenter, forceStrength, dynamicRadius);

		// NOTE: アクティブフラグを立てる（次フレームでリセット）
		_bIsShockWaveActive = true;
	}

	void VoiceShockWaveComponent::ApplyForceToEntitiesInRange(const Vec3& shockWaveCenter, float forceStrength, float radius) {
		// NOTE: ワールドから全エンティティを取得
		auto* world = GetWorld();
		if (!world) {
			return;
		}

		auto* scene = world->GetScenePtr();
		if (!scene) {
			return;
		}

		const auto& allEntities = scene->GetEntities();

		// NOTE: 各エンティティをチェック
		for (const auto& entity : allEntities) {
			if (!entity) {
				continue;
			}

			// NOTE: 自分自身は除外
			if (entity.get() == GetOwner()) {
				continue;
			}

			// NOTE: エンティティの位置を取得
			auto* targetTransform = entity->GetComponent<Unnamed::TransformComponent>();
			if (!targetTransform) {
				continue;
			}

			Vec3 targetPos = targetTransform->GetPosition();

			// NOTE: 衝撃波の距離判定（円形範囲内か？）
			// XZ平面での距離を計算（Y軸は含めない）
			float distanceXZ = std::sqrt(
				std::pow(targetPos.x - shockWaveCenter.x, 2.0f) +
				std::pow(targetPos.z - shockWaveCenter.z, 2.0f)
			);

			// NOTE: 動的半径を使用
			if (distanceXZ > radius) {
				// NOTE: 範囲外なので処理しない
				continue;
			}

			// NOTE: TrashObjMoverComponent を持つエンティティ？
			auto* trashMover = entity->GetComponent<TrashObjMoverComponent>();
			if (trashMover) {
				// NOTE: ゴミを反発させる（吸い込みではなく）
				// NOTE: 衝撃波中心からの方向を計算（反発方向）
				Vec3 direction = targetPos - shockWaveCenter;
				direction.y = 0.0f;
				direction = direction.Normalized();

				// NOTE: 水平方向の力（反発）
				Vec3 forceVector = direction * forceStrength;
				
				// NOTE: 上向き力を追加（アニメ的な打ち上げ効果）
				float upwardForce = forceStrength * _upwardForceMultiplier;
				forceVector.y += upwardForce;
				
				// NOTE: 現在の速度を取得して新しい速度を計算
				Vec3 currentVelocity = trashMover->GetCurrentVelocity();
				Vec3 newVelocity = currentVelocity + forceVector;
				
				// NOTE: 新しい速度を設定
				trashMover->SetVelocity(newVelocity);
				
				// NOTE: 吸い込みをクリア（念のため）
				trashMover->ClearHoleSuckPosition();
				
				// NOTE: 物理演算は常に実行されるため、SetFalling は不要
				// 地面衝突判定が自動的に行われるようになった
			}

			// NOTE: GolfBallComponent を持つエンティティ？
			auto* golfBall = entity->GetComponent<GolfBallComponent>();
			if (golfBall) {
				// NOTE: ボール位置から衝撃波中心への方向を計算（反発方向）
				Vec3 direction = targetPos - shockWaveCenter;
				direction = direction.Normalized();

				// NOTE: 水平方向の力をベクトルに変換
				Vec3 forceVector = direction * forceStrength;
				
				// NOTE: 上向き力を追加（アニメ的な打ち上げ効果）
				float upwardForce = forceStrength * _upwardForceMultiplier;
				forceVector.y += upwardForce;
				
				// NOTE: 力を加算
				golfBall->ApplyForce(forceVector);
			}
		}
	}

	MagVoiceBridge* VoiceShockWaveComponent::GetVoiceBridge() {
		return _voiceBridgeInstance;
	}

	void VoiceShockWaveComponent::FireTestShockWave(float testVolume) {
		// NOTE: テスト用の衝撃波を発火（クールタイムを無視）
		_lastVolume = testVolume;
		FireShockWave(testVolume);
	}

	// NOTE: コンポーネント登録マクロ
	REGISTER_COMPONENT(VoiceShockWaveComponent);

}


