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
	// グローバル初期化関数（TeamGameModule から呼ばれる）
	// =========================================================================

	void SetGlobalVoiceBridge(MagVoiceBridge* bridge) {
		VoiceShockWaveComponent::SetVoiceBridgeInstance(bridge);
	}

	// =========================================================================
	// ライフサイクル
	// =========================================================================

	void VoiceShockWaveComponent::OnAttached() {
		// NOTE: MagVoiceBridge のインスタンスが既に初期化・起動済みのはず
		// （TeamGameModule::Initialize() で行われている）
		// ここでは単に参照を保持して使用するだけ

		_coolTimeCounter = 0.0f;
		_lastVolume = 0.0f;
		_previousVolume = 0.0f;
		_bIsShockWaveActive = false;

		auto* voiceBridge = GetVoiceBridge();
		if (!voiceBridge) {
			#ifdef _DEBUG
			OutputDebugStringA("[VoiceShockWave] WARNING: MagVoiceBridge not initialized\n");
			#endif
		} else {
			#ifdef _DEBUG
			OutputDebugStringA("[VoiceShockWave] OnAttached - MagVoiceBridge ready\n");
			#endif
		}
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
		ImGui::SliderFloat("音量閾値##vsw_threshold", &_volumeThreshold, 0.0f, 0.5f, "%.4f");
		ImGui::SliderFloat("クールタイム##vsw_cooltime", &_coolTime, 0.0f, 5.0f, "%.2f");

		ImGui::Separator();

		// NOTE: 現在の音量情報を表示
		ImGui::Text("=== 音声情報 ===");
		ImGui::Text("現在の音量: %.4f (%.2f %%)", _lastVolume, _lastVolume * 100.0f);
		ImGui::ProgressBar(_lastVolume, ImVec2(-1.0f, 20.0f), "");
		ImGui::Text("前フレーム音量: %.4f", _previousVolume);
		ImGui::Text("音量の変動量: %.4f", std::abs(_lastVolume - _previousVolume));
		ImGui::Text("音量閾値: %.4f", _volumeThreshold);
		ImGui::Text("衝撃波アクティブ: %s", _bIsShockWaveActive ? "有効" : "無効");
		ImGui::Text("クールタイム: %.2f / %.2f秒", _coolTimeCounter, _coolTime);

		if (_coolTimeCounter > 0.0f) {
			ImGui::ProgressBar(_coolTimeCounter / _coolTime, ImVec2(-1.0f, 20.0f), "CoolTime");
		}

		ImGui::Separator();

		// NOTE: テスト用：声発生ボタン
		ImGui::Text("=== テスト用：声発生 ===");
		ImGui::SliderFloat("テスト音量##test_volume", &_testVolume, 0.0f, 1.0f, "%.3f");
		
		if (ImGui::Button("弱い声を出す##test_weak", ImVec2(150, 30))) {
			FireTestShockWave(0.1f);
		}
		ImGui::SameLine();
		if (ImGui::Button("普通の声##test_normal", ImVec2(150, 30))) {
			FireTestShockWave(0.2f);
		}
		ImGui::SameLine();
		if (ImGui::Button("大きい声##test_loud", ImVec2(150, 30))) {
			FireTestShockWave(0.5f);
		}
		
		if (ImGui::Button("カスタム音量で発火##test_custom", ImVec2(300, 30))) {
			FireTestShockWave(_testVolume);
		}

		// NOTE: MagVoiceBridge の情報を表示
		auto* voiceBridge = GetVoiceBridge();
		if (voiceBridge) {
			ImGui::Separator();
			ImGui::Text("=== 音声統計情報（リアルタイム） ===");
			
			auto stats = voiceBridge->GetVolumeStats();
			
			ImGui::Text("現在のRMS: %.4f (%.2f dB)", stats.currentRMS, stats.currentRMSDB);
			ImGui::ProgressBar(stats.currentRMS, ImVec2(-1.0f, 15.0f), "Current RMS");
			
			ImGui::Text("ピーク音量: %.4f (%.2f dB)", stats.peakValue, stats.peakDB);
			ImGui::ProgressBar(stats.peakValue, ImVec2(-1.0f, 15.0f), "Peak");
			
			ImGui::Text("スムージング済み音量: %.4f (%.2f dB)", stats.smoothedRMS, stats.smoothedRMSDB);
			ImGui::ProgressBar(stats.smoothedRMS, ImVec2(-1.0f, 15.0f), "Smoothed");
			
			ImGui::Text("音声スコア: %.3f", stats.voiceScore);
			ImGui::Text("音声検出: %s", stats.isVoiceDetected ? "✓ 検出中" : "✗ 未検出");
			ImGui::Text("ボリューム：%.1f %%", stats.percentage);
			
			if (ImGui::CollapsingHeader("詳細統計情報", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::BulletText("閾値: %.4f", _volumeThreshold);
				ImGui::BulletText("現在の状態: %s", 
					(stats.smoothedRMS >= _volumeThreshold) ? "✓ 閾値超過" : "✗ 閾値未満");
			}
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR: MagVoiceBridge not initialized");
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

		#ifdef _DEBUG
		char buffer[256];
		sprintf_s(buffer, sizeof(buffer),
			"[VoiceShockWave] Loaded - Threshold: %.4f, CoolTime: %.2f\n",
			_volumeThreshold, _coolTime);
		OutputDebugStringA(buffer);
		#endif
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
		float volume = 0.0f;
		try {
			volume = voiceBridge->GetSmoothedVolume();
		} catch (...) {
			// NOTE: MagVoiceBridge のメソッド呼び出しが失敗した場合
			return;
		}

		// NOTE: 音量を保存
		_lastVolume = volume;

		// NOTE: クールタイムが終了しており、音量が閾値を超えている場合
		bool coolTimeReady = (_coolTimeCounter <= 0.0f);
		bool volumeAboveThreshold = (_lastVolume >= _volumeThreshold);
		
		// NOTE: デバッグ出力（音量状態を確認）
		#ifdef _DEBUG
		static float debugTimer = 0.0f;
		debugTimer += 1.0f / 60.0f; // 仮定：60FPS
		if (debugTimer >= 0.5f) { // 0.5秒ごとに出力
			char buffer[256];
			sprintf_s(buffer, sizeof(buffer),
				"[Audio] Volume: %.3f, Threshold: %.3f, CoolTime: %.2f/%.2f, Ready: %s\n",
				_lastVolume, _volumeThreshold, _coolTimeCounter, _coolTime,
				coolTimeReady ? "YES" : "NO");
			OutputDebugStringA(buffer);
			debugTimer = 0.0f;
		}
		#endif
		
		if (coolTimeReady && volumeAboveThreshold) {
			// NOTE: 衝撃波を発火
			#ifdef _DEBUG
			char buffer[256];
			sprintf_s(buffer, sizeof(buffer),
				"[SHOCKWAVE] FIRED! Volume: %.3f, Force: calculated\n", _lastVolume);
			OutputDebugStringA(buffer);
			#endif
			
			FireShockWave(_lastVolume);

			// NOTE: クールタイムをリセット
			_coolTimeCounter = _coolTime;
		}

		// NOTE: 次フレーム用に現在の音量を保存
		_previousVolume = _lastVolume;
	}

	void VoiceShockWaveComponent::FireShockWave(float volume) {
		// NOTE: 音量ベースの力を計算（非常に感度良く）
		// 小さな音でも反応するように、べき乗で増幅
		float baseForce = volume * volume * _forceMultiplier;  // 音量の二乗で増幅
		
		// NOTE: 変動率による力の増幅（急激な変化ほど強い）
		float volumeDelta = std::abs(volume - _previousVolume);
		float deltaForce = volumeDelta * _volumeDeltaMultiplier;
		
		// NOTE: 最終的な力 = ベース力 + 変動による増幅
		float forceStrength = baseForce + deltaForce;
		
		// NOTE: 最小の力を大幅に下げる（より敏感に反応）
		forceStrength = std::max(0.5f, forceStrength);

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

		#ifdef _DEBUG
		char buffer[256];
		sprintf_s(buffer, sizeof(buffer),
			"[SHOCKWAVE] Volume: %.4f, Force: %.2f, Radius: %.2f, UpwardMul: %.2f\n",
			volume, forceStrength, dynamicRadius, _upwardForceMultiplier);
		OutputDebugStringA(buffer);
		#endif
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

				// NOTE: 水平方向の力（反発）- 弱めに調整（垂直方向を強調）
				Vec3 forceVector = direction * forceStrength * 0.6f;
				
				// NOTE: 上向き力を追加（打ち上げを強調）
				// forceStrength * _upwardForceMultiplier で大幅に強化
				float upwardForce = forceStrength * _upwardForceMultiplier;
				forceVector.y = upwardForce;
				
				// NOTE: 現在の速度を取得して新しい速度を計算
				// 衝撃波による力は既存の速度に加算される（衝撃波の効果を最大化）
				Vec3 currentVelocity = trashMover->GetCurrentVelocity();
				Vec3 newVelocity = currentVelocity + forceVector;
				
				// NOTE: 新しい速度を設定
				trashMover->SetVelocity(newVelocity);
				trashMover->StartShockWaveSpin(forceVector);
				
				#ifdef _DEBUG
				static int shockCount = 0;
				shockCount++;
				if (shockCount % 5 == 0) {
					char buffer[256];
					sprintf_s(buffer, sizeof(buffer),
						"[TRASH HIT] Force: (%.2f, %.2f, %.2f), NewVel: (%.2f, %.2f, %.2f)\n",
						forceVector.x, forceVector.y, forceVector.z,
						newVelocity.x, newVelocity.y, newVelocity.z);
					OutputDebugStringA(buffer);
				}
				#endif
			}

			// NOTE: GolfBallComponent を持つエンティティ？
			auto* golfBall = entity->GetComponent<GolfBallComponent>();
			if (golfBall) {
				// NOTE: ボール位置から衝撃波中心への方向を計算（反発方向）
				Vec3 direction = targetPos - shockWaveCenter;
				direction.y = 0.0f;
				direction = direction.Normalized();

				// NOTE: ゴミと同じ水平反発を使い、範囲外へ抜けやすくする
				Vec3 forceVector = direction * forceStrength * 0.6f;
				
				// NOTE: 上方向は GolfBallComponent 側の質量と上限で制御する
				float upwardForce = forceStrength * _upwardForceMultiplier;
				forceVector.y = upwardForce;
				
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


