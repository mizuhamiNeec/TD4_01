#include "TrashObjMoverComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void TrashObjMoverComponent::OnAttached() {
		// NOTE: コンポーネントがアタッチされたときに初期化
		// 物理エンジンの設定などはここで行う場合がある
	}

	void TrashObjMoverComponent::OnTick(float deltaTime) {
		// NOTE: 基本的に何もしない - 物理エンジンが動きを制御
		// 必要に応じてゴミの状態をチェックする処理をここに追加
	}

	void TrashObjMoverComponent::OnDetached() {
		// NOTE: クリーンアップ処理
	}

	// -----------------------------------------------------------------------
	// ゴミオブジェクト設定
	// -----------------------------------------------------------------------

	void TrashObjMoverComponent::SetMass(float mass) {
		_mass = mass;
	}

	float TrashObjMoverComponent::GetMass() const {
		return _mass;
	}

	void TrashObjMoverComponent::SetTrashType(const std::string& type) {
		_trashType = type;
	}

	const std::string& TrashObjMoverComponent::GetTrashType() const {
		return _trashType;
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view TrashObjMoverComponent::GetStableName() const {
		return "mygame.TrashObjMoverComponent";
	}

	std::string_view TrashObjMoverComponent::GetComponentName() const {
		return "Trash Object Mover Component";
	}

#ifdef _DEBUG
	void TrashObjMoverComponent::DrawInspectorImGui() {
		ImGui::Text("=== Trash Object Mover Component ===");

		ImGui::Separator();
		ImGui::Text("Trash Object Properties");

		// ゴミの重量
		ImGui::SliderFloat("Mass", &_mass, 0.1f, 100.0f, "%.2f kg");

		// ゴミのタイプ
		ImGui::InputText("Trash Type", &_trashType);

		ImGui::Separator();
		ImGui::TextWrapped("Note: This object is controlled by physics engine when collided with player.");
	}
#endif

	void TrashObjMoverComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: JSONから値を読み込む
		if (auto val = reader.Read<float>("mass")) {
			_mass = val.value();
		}
		if (auto val = reader.Read<std::string>("trashType")) {
			_trashType = val.value();
		}
	}

	void TrashObjMoverComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: 値をJSONに書き込む
		writer.Key("mass");
		writer.Write(_mass);
		writer.Key("trashType");
		writer.Write(_trashType);
	}

	// NOTE: 忘れると死ぬやつ
	REGISTER_COMPONENT(TrashObjMoverComponent);

} // namespace MyGame
