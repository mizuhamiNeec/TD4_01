#include "GolfBallEndPosComponent.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void GolfBallEndPosComponent::OnAttached() {
		// NOTE: 特別な初期化処理は不要
		// このコンポーネントは位置マーカーとしての役割のみ
	}

	void GolfBallEndPosComponent::OnDetached() {
		// NOTE: クリーンアップ処理は不要
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view GolfBallEndPosComponent::GetStableName() const {
		return "mygame.GolfBallEndPos";
	}

	std::string_view GolfBallEndPosComponent::GetComponentName() const {
		return "Golf Ball End Position";
	}

#ifdef _DEBUG
	void GolfBallEndPosComponent::DrawInspectorImGui() {
		ImGui::Text("ゴルフボール着弾位置マーカー");
		ImGui::TextDisabled("このエンティティの位置がボール着弾地点になります");
		ImGui::Separator();
		ImGui::BulletText("GolfBallComponent::SetTargetPosEntity() で参照");
	}
#endif

	void GolfBallEndPosComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: このコンポーネントは位置情報のみを使用するため、
		//      特別なデシリアライズは不要
	}

	void GolfBallEndPosComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: このコンポーネントは位置情報のみを使用するため、
		//      特別なシリアライズは不要
	}

	// NOTE: コンポーネント登録マクロ
	REGISTER_COMPONENT(GolfBallEndPosComponent);
}
