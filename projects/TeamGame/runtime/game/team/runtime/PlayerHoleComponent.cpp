#include "PlayerHoleComponent.h"
#include "TrashObjMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include <engine/unnamed/framework/components/TransformComponent.h>
#include "engine/scene/Scene.h"
#include "./core/ComponentRegistry.h"
#include <core/math/Vec3.h>
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// ===================================================================
	// ライフサイクル メソッド
	// ===================================================================

	void PlayerHoleComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		_trashInHole.clear();
		_previousTrashInHole.clear();
		_isHoleActive = true;

		// NOTE: 所有者のTransformComponentをキャッシュ
		if (GetOwner()) {
			_ownerTransform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		}
	}

	void PlayerHoleComponent::OnTick(float deltaTime) {
		// NOTE: 毎フレーム穴の範囲内にいるゴミを検出・処理
		if (!_isHoleActive) {
			return;
		}

		// NOTE: 穴のサイズ変更アニメーションを更新
		UpdateHoleSizeAnimation(deltaTime);

		// NOTE: 穴の範囲内にいるゴミを検出
		DetectTrashInHole();

		// NOTE: 穴に入ったゴミと前フレームのゴミを比較して、新規追加を処理
		UpdateTrashList();

		// NOTE: 穴に入っているゴミに吸い込み処理を適用
		ApplySuckForceToTrash(deltaTime);
	}

	void PlayerHoleComponent::OnDetached() {
		// NOTE: クリーンアップ処理
		_trashInHole.clear();
		_previousTrashInHole.clear();
		_ownerTransform = nullptr;
	}

	// ===================================================================
	// 公開インターフェース - 穴設定
	// ===================================================================

	void PlayerHoleComponent::SetHoleRadius(float radius) {
		_holeRadius = std::max(0.1f, radius);
	}

	float PlayerHoleComponent::GetHoleRadius() const {
		return _holeRadius;
	}

	void PlayerHoleComponent::SetHoleOffset(const Vec3& offset) {
		_holeOffset = offset;
	}

	Vec3 PlayerHoleComponent::GetHoleOffset() const {
		return _holeOffset;
	}

	void PlayerHoleComponent::SetHoleActive(bool active) {
		_isHoleActive = active;
	}

	bool PlayerHoleComponent::IsHoleActive() const {
		return _isHoleActive;
	}

	// ===================================================================
	// 公開インターフェース - 穴の操作
	// ===================================================================

	void PlayerHoleComponent::MovePole(const Vec3& direction, float speed) {
		// NOTE: 穴をプレイヤーの相対座標で移動
		if (!GetOwner() || !_ownerTransform) {
			return;
		}

		// NOTE: 世界座標での移動量を計算
		Vec3 moveAmount = direction * speed;

		// NOTE: 穴の世界座標を移動
		Vec3 currentHoleWorldPos = GetHoleWorldPosition();
		Vec3 newHoleWorldPos = currentHoleWorldPos + moveAmount;

		// NOTE: プレイヤーの現在位置を取得
		Vec3 playerPos = _ownerTransform->GetPosition();

		// NOTE: 穴の新しい相対位置を計算
		_holeOffset = newHoleWorldPos - playerPos;
	}

	void PlayerHoleComponent::SetPoleSize(float newSize) {
		// NOTE: 穴のサイズを即座に変更
		_holeRadius = std::max(0.1f, newSize);
		_targetHoleRadius = _holeRadius;
	}

	void PlayerHoleComponent::SetPoleSizeSmooth(float targetSize, float speed) {
		// NOTE: 穴のサイズをスムーズに変更（アニメーション付き）
		_targetHoleRadius = std::max(0.1f, targetSize);
		_holeSizeChangeSpeed = std::max(0.01f, speed);
	}

	// ===================================================================
	// 公開インターフェース - ゴミ検出・処理
	// ===================================================================

	int PlayerHoleComponent::GetTrashInHoleCount() const {
		return static_cast<int>(_trashInHole.size());
	}

	const std::vector<Unnamed::Entity*>& PlayerHoleComponent::GetTrashInHole() const {
		return _trashInHole;
	}

	// ===================================================================
	// BaseComponent の必須オーバーライド
	// ===================================================================

	std::string_view PlayerHoleComponent::GetStableName() const {
		return "mygame.PlayerHoleComponent";
	}

	std::string_view PlayerHoleComponent::GetComponentName() const {
		return "Player Hole Component";
	}

#ifdef _DEBUG
	void PlayerHoleComponent::DrawInspectorImGui() {
		ImGui::Text("=== Player Hole Component ===");

		ImGui::Separator();
		ImGui::Text("Hole Configuration");

		// 穴の半径
		if (ImGui::SliderFloat("Hole Radius", &_holeRadius, 0.1f, 20.0f, "%.2f units")) {
			_holeRadius = std::max(0.1f, _holeRadius);
		}

		// 穴のターゲットサイズ（スムーズ変更用）
		if (ImGui::SliderFloat("Target Hole Radius", &_targetHoleRadius, 0.1f, 20.0f, "%.2f units")) {
			_targetHoleRadius = std::max(0.1f, _targetHoleRadius);
		}

		// 穴の中心位置（相対座標）
		ImGui::DragFloat3("Hole Offset", &_holeOffset.x, 0.1f, -10.0f, 10.0f, "%.2f");

		// 穴のサイズ変更速度
		if (ImGui::SliderFloat("Size Change Speed", &_holeSizeChangeSpeed, 0.01f, 10.0f, "%.2f")) {
			_holeSizeChangeSpeed = std::max(0.01f, _holeSizeChangeSpeed);
		}

	ImGui::Separator();
	ImGui::Text("Suck Force Parameters");

	// Suck Power
	ImGui::SliderFloat("Suck Power (Grounded Trash)", &_suckPower, 0.0f, 1.0f, "%.3f");
	ImGui::TextWrapped("Suck power applied to grounded trash (0.0 to 1.0)");

	// Suck Effect Distance
	if (ImGui::SliderFloat("Suck Effect Distance", &_suckEffectDistance, 0.0f, 20.0f, "%.2f")) {
		_suckEffectDistance = std::max(0.0f, _suckEffectDistance);
	}
	ImGui::TextWrapped("Distance range for suck effect (from hole radius)");

	// Suck Intensity Curve
	ImGui::SliderFloat("Suck Intensity Curve", &_suckIntensityCurve, 0.1f, 3.0f, "%.2f");
	ImGui::TextWrapped("Adjusts how suck power increases with distance");
	ImGui::TextWrapped("  1.0 = Linear (even increase)");
	ImGui::TextWrapped("  2.0 = Squared (rapid increase)");
	ImGui::TextWrapped("  0.5 = Square root (slow increase)");

		ImGui::Separator();
		ImGui::Text("Hole Status");

		// 穴の有効状態
		ImGui::Checkbox("Hole Active", &_isHoleActive);

		// 穴の世界座標を表示
		Vec3 holeWorldPos = GetHoleWorldPosition();
		ImGui::Text("Hole World Position: (%.2f, %.2f, %.2f)", holeWorldPos.x, holeWorldPos.y, holeWorldPos.z);

		ImGui::Separator();
		ImGui::Text("Trash in Hole");

		// 穴に入っているゴミの数
		ImGui::Text("Count: %d", GetTrashInHoleCount());

		// 穴に入っているゴミのリスト
		if (!_trashInHole.empty()) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Trash List:");
			for (size_t i = 0; i < _trashInHole.size(); ++i) {
				if (_trashInHole[i]) {
					auto name = _trashInHole[i]->GetName();
					ImGui::BulletText("#%zu: %.*s", i, 
						static_cast<int>(name.size()), name.data());
				}
			}
		} else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No trash in hole");
		}
	}
#endif

	void PlayerHoleComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSONから値を読み込む
		if (auto val = reader.Read<float>("holeRadius")) {
			_holeRadius = std::max(0.1f, val.value());
		}

		// 穴のオフセット位置（Vec3）
		float offsetX = _holeOffset.x;
		float offsetY = _holeOffset.y;
		float offsetZ = _holeOffset.z;

		if (auto val = reader.Read<float>("holeOffsetX")) {
			offsetX = val.value();
		}
		if (auto val = reader.Read<float>("holeOffsetY")) {
			offsetY = val.value();
		}
		if (auto val = reader.Read<float>("holeOffsetZ")) {
			offsetZ = val.value();
		}
		_holeOffset = Vec3(offsetX, offsetY, offsetZ);

		if (auto val = reader.Read<bool>("isHoleActive")) {
			_isHoleActive = val.value();
		}
		if (auto val = reader.Read<float>("targetHoleRadius")) {
			_targetHoleRadius = std::max(0.1f, val.value());
		}
		if (auto val = reader.Read<float>("holeSizeChangeSpeed")) {
			_holeSizeChangeSpeed = std::max(0.01f, val.value());
		}

	// 吸い込み力関連
	if (auto val = reader.Read<float>("suckPower")) {
		_suckPower = val.value();
	}
	if (auto val = reader.Read<float>("suckEffectDistance")) {
		_suckEffectDistance = std::max(0.0f, val.value());
	}
	if (auto val = reader.Read<float>("suckIntensityCurve")) {
		_suckIntensityCurve = val.value();
	}
	}

	void PlayerHoleComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		writer.Key("holeRadius");
		writer.Write(_holeRadius);

		// 穴のオフセット位置をfloat x3で保存
		writer.Key("holeOffsetX");
		writer.Write(_holeOffset.x);
		writer.Key("holeOffsetY");
		writer.Write(_holeOffset.y);
		writer.Key("holeOffsetZ");
		writer.Write(_holeOffset.z);

		writer.Key("isHoleActive");
		writer.Write(_isHoleActive);
		writer.Key("targetHoleRadius");
		writer.Write(_targetHoleRadius);
		writer.Key("holeSizeChangeSpeed");
		writer.Write(_holeSizeChangeSpeed);

	// 吸い込み力関連
	writer.Key("suckPower");
	writer.Write(_suckPower);
	writer.Key("suckEffectDistance");
	writer.Write(_suckEffectDistance);
	writer.Key("suckIntensityCurve");
	writer.Write(_suckIntensityCurve);
	}

	// ===================================================================
	// プライベート ヘルパーメソッド
	// ===================================================================

	Vec3 PlayerHoleComponent::GetHoleWorldPosition() const {
		// NOTE: 穴の世界座標を計算（プレイヤーの位置 + オフセット）
		if (!GetOwner() || !_ownerTransform) {
			return _holeOffset;
		}

		// NOTE: プレイヤーの位置にオフセットを加算
		Vec3 playerPos = _ownerTransform->GetPosition();
		return playerPos + _holeOffset;
	}

	void PlayerHoleComponent::DetectTrashInHole() {
		// NOTE: 穴の範囲内にいるゴミをすべて検出
		_trashInHole.clear();

		// NOTE: 所有者（プレイヤー）のエンティティからシーンを取得
		if (!GetOwner()) {
			return;
		}

		// NOTE: シーン内のすべてのエンティティを走査
		auto* scene = GetOwner()->GetScene();
		if (!scene) {
			return;
		}

		// NOTE: シーン内のすべてのエンティティを取得
		const auto& entities = scene->GetEntities();

		// NOTE: 各エンティティをチェック
		for (const auto& entityPtr : entities) {
			if (!entityPtr) {
				continue;
			}

			// NOTE: 穴の範囲内にいるか判定
			if (IsEntityInHoleRange(entityPtr.get())) {
				_trashInHole.push_back(entityPtr.get());
			}
		}
	}

	bool PlayerHoleComponent::IsEntityInHoleRange(Unnamed::Entity* entity) const {
		// NOTE: エンティティが穴の範囲内にいるかを判定
		if (!entity) {
			return false;
		}

		// NOTE: TrashObjMoverComponent を持つかチェック
		auto* trashComponent = entity->GetComponent<TrashObjMoverComponent>();
		if (!trashComponent) {
			return false;
		}

		// NOTE: エンティティの Transform を取得
		auto* transform = entity->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return false;
		}

		// NOTE: 穴の世界座標を取得
		Vec3 holeWorldPos = GetHoleWorldPosition();

		// NOTE: エンティティの位置を取得
		Vec3 entityPos = transform->GetPosition();

		// NOTE: 穴の中心からの距離を計算
		Vec3 diff = entityPos - holeWorldPos;
		float distance = diff.Length();

		// NOTE: 距離が穴の半径より小さいか判定
		return distance < _holeRadius;
	}

	void PlayerHoleComponent::MakeTrashFall(Unnamed::Entity* trashEntity) {
		// NOTE: ゴミを落下状態に移行させる
		if (!trashEntity) {
			return;
		}

		// NOTE: ゴミの TrashObjMoverComponent を取得
		auto* trashComponent = trashEntity->GetComponent<TrashObjMoverComponent>();
		if (!trashComponent) {
			return;
		}

		// NOTE: ゴミを落下状態に設定
		trashComponent->SetFalling(true);
	}

	void PlayerHoleComponent::UpdateTrashList() {
		// NOTE: 穴に入ったゴミと前フレームのゴミを比較
		// 新規に入ったゴミに対して落下処理を実行

		for (auto* trashEntity : _trashInHole) {
			// NOTE: 前フレームのリストに無ければ、新規に穴に入ったゴミ
			auto it = std::find(_previousTrashInHole.begin(), _previousTrashInHole.end(), trashEntity);
			if (it == _previousTrashInHole.end()) {
				// NOTE: 新規に穴に入ったゴミ - 落下処理を実行
				MakeTrashFall(trashEntity);
			}
		}

		// NOTE: 今フレームの状態を保存
		_previousTrashInHole = _trashInHole;
	}

	void PlayerHoleComponent::UpdateHoleSizeAnimation(float deltaTime) {
		// NOTE: 穴のサイズをスムーズに変更（ターゲットサイズに向かって補間）
		if (std::abs(_holeRadius - _targetHoleRadius) < 0.01f) {
			_holeRadius = _targetHoleRadius;
			return;
		}

		// NOTE: 線形補間でサイズを変更
		float direction = _targetHoleRadius > _holeRadius ? 1.0f : -1.0f;
		float changeAmount = _holeSizeChangeSpeed * deltaTime;

		_holeRadius += changeAmount * direction;

		// NOTE: ターゲットを超えないようにクリップ
		if (direction > 0.0f) {
			_holeRadius = std::min(_holeRadius, _targetHoleRadius);
		} else {
			_holeRadius = std::max(_holeRadius, _targetHoleRadius);
		}
	}

	void PlayerHoleComponent::ApplySuckForceToTrash(float deltaTime) {
		// NOTE: 穴の周辺のゴミに吸い込み力を適用
		// シーン内のすべてのゴミをスキャンして、吸い込み範囲内のゴミに吸い込み力を適用

		// 所有者（プレイヤー）のエンティティからシーンを取得
		if (!GetOwner()) {
			return;
		}

		auto* scene = GetOwner()->GetScene();
		if (!scene) {
			return;
		}

		// シーン内のすべてのエンティティを取得
		const auto& entities = scene->GetEntities();

		Vec3 holeWorldPos = GetHoleWorldPosition();
		float suckRangeOuter = _holeRadius + _suckEffectDistance;

		// 各エンティティをチェック
		for (const auto& entityPtr : entities) {
			if (!entityPtr) {
				continue;
			}

			auto* entity = entityPtr.get();

			// ゴミのTrashObjMoverComponentを取得
			auto* trashComponent = entity->GetComponent<TrashObjMoverComponent>();
			if (!trashComponent) {
				continue;
			}

			// ゴミのTransformComponentを取得
			auto* transform = entity->GetComponent<Unnamed::TransformComponent>();
			if (!transform) {
				continue;
			}

			// 穴の中心からゴミまでの距離を計算
			Vec3 trashPos = transform->GetPosition();
			Vec3 diffToHole = holeWorldPos - trashPos;
			float distanceToHole = diffToHole.Length();

			// 吸い込み範囲外
			if (distanceToHole >= suckRangeOuter) {
				// 吸い込み範囲外 - ゴミに吸い込み力を適用しない
				trashComponent->ClearHoleSuckPosition();
				continue;
			}

			// 穴に既に落ちている場合（穴の半径より内側）
			if (distanceToHole < _holeRadius) {
				// 穴に落ちているゴミは吸い込み力を適用しない（そのまま落下）
				trashComponent->ClearHoleSuckPosition();
				continue;
			}

			// 吸い込み範囲内（穴より大きい円の中にいるが、穴には落ちていない）
			// 距離に基づいて吸い込み力を計算

			// ゴミが落下状態かどうかで処理を分ける
			bool isFalling = trashComponent->IsFalling();

			// 落下中のゴミは吸い込まない（そのまま落下させる）
			if (isFalling) {
				trashComponent->ClearHoleSuckPosition();
				continue;
			}

			// 地上にあるゴミのみ吸い込む
			float distanceFromHoleEdge = distanceToHole - _holeRadius;
			float suckPowerFalloff = distanceFromHoleEdge / _suckEffectDistance; // 0.0～1.0（逆順）
			suckPowerFalloff = 1.0f - suckPowerFalloff; // 反転：外側ほど弱い
			
			// 吸い込み強度曲線を適用（近づくほど強くなる効果）
			suckPowerFalloff = std::pow(suckPowerFalloff, _suckIntensityCurve);
			
			float suckPower = _suckPower * suckPowerFalloff; // 設定値に基づいた吸い込み力

			trashComponent->SetHoleSuckPosition(holeWorldPos, suckPower);
		}
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerHoleComponent);

} // namespace MyGame
