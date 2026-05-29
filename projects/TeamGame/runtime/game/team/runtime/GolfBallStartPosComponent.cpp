#include "GolfBallStartPosComponent.h"
#include "./core/ComponentRegistry.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	void GolfBallStartPosComponent::OnAttached() {
		// NOTE: 特別な初期化処理は不要
		// このコンポーネントは位置マーカーとしての役割のみ
	}

	void GolfBallStartPosComponent::OnDetached() {
		// NOTE: クリーンアップ処理は不要
	}

	// -----------------------------------------------------------------------
	// BaseComponent override
	// -----------------------------------------------------------------------

	std::string_view GolfBallStartPosComponent::GetStableName() const {
		return "mygame.GolfBallStartPos";
	}

	std::string_view GolfBallStartPosComponent::GetComponentName() const {
		return "Golf Ball Start Position";
	}

#ifdef _DEBUG
	void GolfBallStartPosComponent::DrawInspectorImGui() {
		ImGui::Text("ゴルフボール発射位置マーカー");
		ImGui::TextDisabled("このエンティティの位置がボール発射地点になります");
		ImGui::Separator();
		ImGui::BulletText("GolfBallComponent::SetStartPosEntity() で参照");
	}
#endif

	void GolfBallStartPosComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// NOTE: このコンポーネントは位置情報のみを使用するため、
		//      特別なデシリアライズは不要
	}

	void GolfBallStartPosComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// NOTE: このコンポーネントは位置情報のみを使用するため、
		//      特別なシリアライズは不要
	}

	// NOTE: コンポーネント登録マクロ
	REGISTER_COMPONENT(GolfBallStartPosComponent);
}

