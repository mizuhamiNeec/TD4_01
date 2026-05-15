#include "PlayerHoleComponent.h"
#include "TrashObjMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include <engine/unnamed/framework/components/TransformComponent.h>
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/scene/Scene.h"
#include "./core/ComponentRegistry.h"
#include <core/math/Vec3.h>
#include <algorithm>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void PlayerHoleComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		_trashInHole.clear();
		_previousTrashInHole.clear();
		_isHoleActive = true;
	}

	void PlayerHoleComponent::OnTick(float deltaTime) {
		// NOTE: 毎フレーム穴の範囲内にいるゴミを検出・処理
		if (!_isHoleActive) {
			return;
		}

		// NOTE: 穴の範囲内にいるゴミを検出
		DetectTrashInHole();

		// NOTE: 穴に入ったゴミと前フレームのゴミを比較して、新規追加を処理
		UpdateTrashList();
	}

	void PlayerHoleComponent::OnDetached() {
		// NOTE: クリーンアップ処理
		_trashInHole.clear();
		_previousTrashInHole.clear();
	}

	// -----------------------------------------------------------------------
	// 穴設定
	// -----------------------------------------------------------------------

	/// @brief 穴の半径を設定
	void PlayerHoleComponent::SetHoleRadius(float radius) {
		_holeRadius = radius;
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

	// -----------------------------------------------------------------------
	// ゴミ検出・処理
	// -----------------------------------------------------------------------

	int PlayerHoleComponent::GetTrashInHoleCount() const {
		return static_cast<int>(_trashInHole.size());
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

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
		ImGui::SliderFloat("Hole Radius", &_holeRadius, 0.1f, 20.0f, "%.2f units");

		// 穴の中心位置（相対座標）
		ImGui::DragFloat3("Hole Offset", &_holeOffset.x, 0.1f, -10.0f, 10.0f, "%.2f");

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
					ImGui::BulletText("#%zu: %s (GUID: %s)", i, _trashInHole[i]->GetName().c_str(), _trashInHole[i]->GetGuid().c_str());
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
			_holeRadius = val.value();
		}
		if (auto val = reader.Read<Vec3>("holeOffset")) {
			_holeOffset = val.value();
		}
		if (auto val = reader.Read<bool>("isHoleActive")) {
			_isHoleActive = val.value();
		}
	}

	void PlayerHoleComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		writer.Key("holeRadius");
		writer.Write(_holeRadius);
		writer.Key("holeOffset");
		writer.Write(_holeOffset);
		writer.Key("isHoleActive");
		writer.Write(_isHoleActive);
	}

	// -----------------------------------------------------------------------
	// 内部処理
	// -----------------------------------------------------------------------

	Vec3 PlayerHoleComponent::GetHoleWorldPosition() const {
		// NOTE: 穴の世界座標を計算（プレイヤーの位置 + オフセット）
		if (!GetOwner()) {
			return _holeOffset;
		}

		auto* transform = GetOwner()->GetComponent<Unnamed::TransformComponent>();
		if (!transform) {
			return _holeOffset;
		}

		// NOTE: プレイヤーの位置にオフセットを加算
		Vec3 playerPos = transform->GetPosition();
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
		// GetOwner() はプレイヤーエンティティなので、そのシーンから取得
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

		// -----------------------------------------------------------------------
		// 落下処理を実行
		// -----------------------------------------------------------------------
		// 
		// TrashObjMoverComponent の落下フラグを設定することで、
		// 穴に入ったゴミが落下状態に移行します。
		// 
		// 落下フラグが立つと:
		// 1. 物理エンジンが重力を適用
		// 2. ゴミが穴に吸い込まれるように落下
		// 3. Y座標が一定以下になったら削除 or リセット
		// 

		// NOTE: ゴミを落下状態に設定
		trashComponent->SetFalling(true);

#ifdef _DEBUG
		// NOTE: デバッグ用ログ出力
		if (trashEntity) {
			// 将来的にゴミが穴に入ったときのデバッグ情報を出力
		}
#endif
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

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(PlayerHoleComponent);

} // namespace MyGame
