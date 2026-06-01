#include "Player2BallRangeUIComponent.h"

#include "PlayerMoveComponent.h"

#include <algorithm>
#include <cmath>

#include <core/ComponentRegistry.h>

#include <engine/gui/UiRoot.h>
#include <engine/gui/UiWidget.h>
#include <engine/gui/components/UiDigitStripComponent.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/framework/components/TransformComponent.h>
#include <engine/unnamed/framework/components/ui/UiCanvasComponent.h>
#include <engine/unnamed/framework/entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

void MyGame::Player2BallRangeUIComponent::OnAttached() {
	EnsureUiCanvasAsset();
}

void MyGame::Player2BallRangeUIComponent::OnRenderTick(
	float renderDeltaTime,
	float interpolationAlpha
) {
	(void)renderDeltaTime;
	(void)interpolationAlpha;

	EnsureUiCanvasAsset();

	auto* playerEntity = ResolvePlayerEntity();
	auto* ballEntity = ResolveBallEntity();
	if (!playerEntity || !ballEntity) {
		return;
	}

	auto* playerTransform =
		playerEntity->GetComponent<Unnamed::TransformComponent>();
	auto* ballTransform = ballEntity->GetComponent<Unnamed::TransformComponent>();
	if (!playerTransform || !ballTransform) {
		return;
	}

	auto* digitStrip = ResolveDigitStrip();
	if (!digitStrip) {
		return;
	}

	// 描画補間後の位置を使い、画面表示と距離表示のズレを抑える。
	Mat4 playerWorld = playerTransform->RenderWorldMat();
	Mat4 ballWorld = ballTransform->RenderWorldMat();
	const Vec3 playerPosition = playerWorld.GetTranslate();
	const Vec3 ballPosition = ballWorld.GetTranslate();
	const float distance = (ballPosition - playerPosition).Length();
	const int32_t displayValue = std::clamp(
		static_cast<int32_t>(std::lround(distance * std::max(0.0f, _distanceScale))),
		0,
		std::max(0, _maxDisplayValue)
	);
	digitStrip->SetValue(displayValue);
}

void MyGame::Player2BallRangeUIComponent::OnDetached() {}

std::string_view MyGame::Player2BallRangeUIComponent::GetStableName() const {
	return "mygame.Player2BallRangeUIComponent";
}

std::string_view MyGame::Player2BallRangeUIComponent::GetComponentName() const {
	return "Player To Ball Range UI Component";
}

#ifdef _DEBUG
void MyGame::Player2BallRangeUIComponent::DrawInspectorImGui() {
	uint64_t playerGuid = _playerEntityGuid;
	if (ImGui::InputScalar("Player Entity GUID", ImGuiDataType_U64, &playerGuid)) {
		_playerEntityGuid = playerGuid;
	}

	uint64_t ballGuid = _ballEntityGuid;
	if (ImGui::InputScalar("Ball Entity GUID", ImGuiDataType_U64, &ballGuid)) {
		_ballEntityGuid = ballGuid;
	}

	ImGui::DragFloat("Distance Scale", &_distanceScale, 0.01f, 0.0f, 100.0f);
	ImGui::DragInt("Max Display Value", &_maxDisplayValue, 1.0f, 0, 9999);
}
#endif

void MyGame::Player2BallRangeUIComponent::Deserialize(
	const Unnamed::JsonReader& reader
) {
	if (reader.Has("playerEntityGuid")) {
		_playerEntityGuid = reader["playerEntityGuid"].GetUint64();
	}
	if (reader.Has("ballEntityGuid")) {
		_ballEntityGuid = reader["ballEntityGuid"].GetUint64();
	}
	if (reader.Has("playerTag")) {
		_playerTag = reader["playerTag"].GetString(_playerTag);
	}
	if (reader.Has("ballTag")) {
		_ballTag = reader["ballTag"].GetString(_ballTag);
	}
	if (reader.Has("playerName")) {
		_playerName = reader["playerName"].GetString(_playerName);
	}
	if (reader.Has("ballName")) {
		_ballName = reader["ballName"].GetString(_ballName);
	}
	if (reader.Has("uiAssetPath")) {
		_uiAssetPath = reader["uiAssetPath"].GetString(_uiAssetPath);
	}
	if (reader.Has("digitWidgetName")) {
		_digitWidgetName = reader["digitWidgetName"].GetString(_digitWidgetName);
	}
	if (reader.Has("distanceScale")) {
		_distanceScale = reader["distanceScale"].GetFloat(_distanceScale);
	}
	if (reader.Has("maxDisplayValue")) {
		_maxDisplayValue = reader["maxDisplayValue"].GetInt(_maxDisplayValue);
	}
}

void MyGame::Player2BallRangeUIComponent::Serialize(
	Unnamed::JsonWriter& writer
) const {
	writer.Key("playerEntityGuid");
	writer.Write(_playerEntityGuid);

	writer.Key("ballEntityGuid");
	writer.Write(_ballEntityGuid);

	writer.Key("playerTag");
	writer.Write(_playerTag);

	writer.Key("ballTag");
	writer.Write(_ballTag);

	writer.Key("playerName");
	writer.Write(_playerName);

	writer.Key("ballName");
	writer.Write(_ballName);

	writer.Key("uiAssetPath");
	writer.Write(_uiAssetPath);

	writer.Key("digitWidgetName");
	writer.Write(_digitWidgetName);

	writer.Key("distanceScale");
	writer.Write(_distanceScale);

	writer.Key("maxDisplayValue");
	writer.Write(_maxDisplayValue);
}

Unnamed::Entity* MyGame::Player2BallRangeUIComponent::ResolvePlayerEntity()
const {
	auto* owner = GetOwner();
	if (owner && owner->IsActive()) {
		if (
			owner->GetComponent<PlayerMoveComponent>() ||
			(!_playerTag.empty() && owner->HasTag(_playerTag)) ||
			(!_playerName.empty() && owner->GetName() == _playerName)
		) {
			// プレイヤーに直接付けた場合も動くようにして、シーン側の参照設定を省けるようにする。
			return owner;
		}
	}

	return ResolveEntity(_playerEntityGuid, _playerTag, _playerName);
}

Unnamed::Entity* MyGame::Player2BallRangeUIComponent::ResolveBallEntity()
const {
	return ResolveEntity(_ballEntityGuid, _ballTag, _ballName);
}

Unnamed::Entity* MyGame::Player2BallRangeUIComponent::ResolveEntity(
	const uint64_t entityGuid,
	const std::string& tag,
	const std::string& name
) const {
	auto* scene = GetScene();
	if (!scene) {
		return nullptr;
	}

	if (entityGuid != 0) {
		if (auto* entity = scene->FindEntity(entityGuid)) {
			return entity;
		}
	}

	if (!tag.empty()) {
		// GUIDはシーン編集で変わる可能性があるため、タグ指定を次の安定した参照として使う。
		if (auto* entity = scene->FindFirstEntityByTag(tag)) {
			return entity;
		}
	}

	if (!name.empty()) {
		// 既存シーンのPlayerにはタグが無いため、名前検索を最後のフォールバックにする。
		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr || !entityPtr->IsActive()) {
				continue;
			}
			if (entityPtr->GetName() == name) {
				return entityPtr.get();
			}
		}
	}

	return nullptr;
}

Unnamed::Gui::UiDigitStripComponent*
MyGame::Player2BallRangeUIComponent::ResolveDigitStrip() const {
	auto* owner = GetOwner();
	if (!owner) {
		return nullptr;
	}

	auto* canvas = owner->GetComponent<Unnamed::UiCanvasComponent>();
	if (!canvas || !canvas->EnsureRuntimeLoaded()) {
		return nullptr;
	}

	auto* root = canvas->GetRuntimeRoot();
	if (!root) {
		return nullptr;
	}

	auto* widget = FindWidgetByName(root->GetRootWidget(), _digitWidgetName);
	if (!widget) {
		return nullptr;
	}

	return widget->GetComponent<Unnamed::Gui::UiDigitStripComponent>();
}

Unnamed::Gui::UiWidget* MyGame::Player2BallRangeUIComponent::FindWidgetByName(
	Unnamed::Gui::UiWidget* widget,
	const std::string_view name
) const {
	if (!widget) {
		return nullptr;
	}

	if (widget->GetName() == name) {
		return widget;
	}

	for (const auto& child : widget->GetChildren()) {
		if (auto* found = FindWidgetByName(child.get(), name)) {
			return found;
		}
	}

	return nullptr;
}

void MyGame::Player2BallRangeUIComponent::EnsureUiCanvasAsset() const {
	auto* owner = GetOwner();
	if (!owner || _uiAssetPath.empty()) {
		return;
	}

	auto* canvas = owner->GetComponent<Unnamed::UiCanvasComponent>();
	if (!canvas) {
		return;
	}

	if (canvas->GetUiAssetPath() != _uiAssetPath) {
		// 距離UI専用のCanvasとして使う前提。別UIと共存する場合は別Entityに分ける。
		canvas->SetUiAssetPath(_uiAssetPath);
	}
}

namespace MyGame {
	REGISTER_COMPONENT(Player2BallRangeUIComponent);
}
