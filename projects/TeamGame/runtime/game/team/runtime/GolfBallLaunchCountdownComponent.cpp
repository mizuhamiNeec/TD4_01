#include "GolfBallLaunchCountdownComponent.h"

#include "collision/base/BaseKinematicCollisionResolver.h"
#include "GolfBallComponent.h"

#include <algorithm>
#include <cmath>

#include <core/ComponentRegistry.h>

#include <engine/gui/UiRoot.h>
#include <engine/gui/UiWidget.h>
#include <engine/gui/components/UiDigitStripComponent.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/framework/components/ui/UiCanvasComponent.h>
#include <engine/unnamed/framework/entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

void MyGame::GolfBallLaunchCountdownComponent::OnAttached() {
	ResetCountdown();
}

void MyGame::GolfBallLaunchCountdownComponent::OnTick(float deltaTime) {
	if (!_launchOnStart || _bHasLaunched) {
		return;
	}

	_countdownTimer = std::max(0.0f, _countdownTimer - deltaTime);
	UpdateCountdownUi();

	if (_countdownTimer > 0.0f) {
		return;
	}

	auto* golfBall = ResolveGolfBall();
	if (!golfBall) {
		return;
	}

	_bHasLaunched = true;
	golfBall->Launch();
	UpdateUiVisibility();
}

void MyGame::GolfBallLaunchCountdownComponent::OnDetached() {}

std::string_view MyGame::GolfBallLaunchCountdownComponent::GetStableName() const {
	return "mygame.GolfBallLaunchCountdownComponent";
}

std::string_view MyGame::GolfBallLaunchCountdownComponent::GetComponentName()
const {
	return "Golf Ball Launch Countdown Component";
}

#ifdef _DEBUG
void MyGame::GolfBallLaunchCountdownComponent::DrawInspectorImGui() {
	ImGui::Checkbox("起動時に発射##golf_launch_countdown_on_start", &_launchOnStart);
	if (ImGui::SliderFloat("発射カウントダウン##golf_launch_countdown", &_countdownDuration, 0.0f, 60.0f, "%.2f")) {
		if (!_bHasLaunched) {
			ResetCountdown();
		}
	}

	uint64_t ballGuid = _ballEntityGuid;
	if (ImGui::InputScalar("Ball Entity GUID", ImGuiDataType_U64, &ballGuid)) {
		_ballEntityGuid = ballGuid;
	}

	ImGui::Text("残り時間: %.2f", _countdownTimer);
	ImGui::Text("発射済み: %s", _bHasLaunched ? "true" : "false");

	if (ImGui::Button("カウントダウンをリセット##golf_launch_countdown_reset")) {
		ResetCountdown();
	}

	ImGui::Separator();
	ImGui::Checkbox(
		"発射後に距離UIへ切り替え##golf_launch_countdown_ui_switch",
		&_switchToDistanceUiOnLaunch
	);
}
#endif

void MyGame::GolfBallLaunchCountdownComponent::Deserialize(
	const Unnamed::JsonReader& reader
) {
	if (auto val = reader.Read<bool>("launchOnStart")) {
		_launchOnStart = val.value();
	}
	if (auto val = reader.Read<float>("countdownDuration")) {
		_countdownDuration = std::max(0.0f, val.value());
		_countdownTimer = _countdownDuration;
	}
	if (reader.Has("ballEntityGuid")) {
		_ballEntityGuid = reader["ballEntityGuid"].GetUint64();
	}
	if (reader.Has("ballTag")) {
		_ballTag = reader["ballTag"].GetString(_ballTag);
	}
	if (reader.Has("ballName")) {
		_ballName = reader["ballName"].GetString(_ballName);
	}
	if (reader.Has("countdownUiAssetPath")) {
		_countdownUiAssetPath =
			reader["countdownUiAssetPath"].GetString(_countdownUiAssetPath);
	}
	if (reader.Has("countdownDigitWidgetName")) {
		_countdownDigitWidgetName = reader["countdownDigitWidgetName"].GetString(
			_countdownDigitWidgetName
		);
	}
	if (reader.Has("countdownUiEntityGuid")) {
		_countdownUiEntityGuid = reader["countdownUiEntityGuid"].GetUint64();
	}
	if (reader.Has("countdownUiEntityName")) {
		_countdownUiEntityName =
			reader["countdownUiEntityName"].GetString(_countdownUiEntityName);
	}
	if (reader.Has("distanceUiEntityGuid")) {
		_distanceUiEntityGuid = reader["distanceUiEntityGuid"].GetUint64();
	}
	if (reader.Has("distanceUiEntityName")) {
		_distanceUiEntityName =
			reader["distanceUiEntityName"].GetString(_distanceUiEntityName);
	}
	if (auto val = reader.Read<bool>("switchToDistanceUiOnLaunch")) {
		_switchToDistanceUiOnLaunch = val.value();
	}
}

void MyGame::GolfBallLaunchCountdownComponent::Serialize(
	Unnamed::JsonWriter& writer
) const {
	writer.Key("launchOnStart");
	writer.Write(_launchOnStart);

	writer.Key("countdownDuration");
	writer.Write(_countdownDuration);

	writer.Key("ballEntityGuid");
	writer.Write(_ballEntityGuid);

	writer.Key("ballTag");
	writer.Write(_ballTag);

	writer.Key("ballName");
	writer.Write(_ballName);

	writer.Key("countdownUiAssetPath");
	writer.Write(_countdownUiAssetPath);

	writer.Key("countdownDigitWidgetName");
	writer.Write(_countdownDigitWidgetName);

	writer.Key("countdownUiEntityGuid");
	writer.Write(_countdownUiEntityGuid);

	writer.Key("countdownUiEntityName");
	writer.Write(_countdownUiEntityName);

	writer.Key("distanceUiEntityGuid");
	writer.Write(_distanceUiEntityGuid);

	writer.Key("distanceUiEntityName");
	writer.Write(_distanceUiEntityName);

	writer.Key("switchToDistanceUiOnLaunch");
	writer.Write(_switchToDistanceUiOnLaunch);
}

MyGame::GolfBallComponent*
MyGame::GolfBallLaunchCountdownComponent::ResolveGolfBall() const {
	if (auto* owner = GetOwner()) {
		if (auto* golfBall = owner->GetComponent<GolfBallComponent>()) {
			return golfBall;
		}
	}

	auto* entity = ResolveEntity();
	if (!entity) {
		return nullptr;
	}
	return entity->GetComponent<GolfBallComponent>();
}

Unnamed::Entity* MyGame::GolfBallLaunchCountdownComponent::ResolveEntity()
const {
	auto* scene = GetScene();
	if (!scene) {
		return nullptr;
	}

	if (_ballEntityGuid != 0) {
		if (auto* entity = scene->FindEntity(_ballEntityGuid)) {
			return entity;
		}
	}

	if (!_ballTag.empty()) {
		if (auto* entity = scene->FindFirstEntityByTag(_ballTag)) {
			return entity;
		}
	}

	if (!_ballName.empty()) {
		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr || !entityPtr->IsActive()) {
				continue;
			}
			if (entityPtr->GetName() == _ballName) {
				return entityPtr.get();
			}
		}
	}

	return nullptr;
}

void MyGame::GolfBallLaunchCountdownComponent::UpdateCountdownUi() const {
	auto* digitStrip = ResolveCountdownDigitStrip();
	if (!digitStrip) {
		return;
	}

	const int32_t displaySeconds = std::clamp(
		static_cast<int32_t>(std::ceil(std::max(0.0f, _countdownTimer))),
		0,
		99
	);
	digitStrip->SetValue(displaySeconds);
}

void MyGame::GolfBallLaunchCountdownComponent::UpdateUiVisibility() const {
	if (!_switchToDistanceUiOnLaunch) {
		return;
	}

	const bool showDistanceUi = _bHasLaunched;
	if (auto* countdownUi = ResolveUiEntity(
		_countdownUiEntityGuid,
		_countdownUiEntityName
	)) {
		countdownUi->SetVisible(!showDistanceUi);
	}
	if (auto* distanceUi = ResolveUiEntity(
		_distanceUiEntityGuid,
		_distanceUiEntityName
	)) {
		// 距離UI側は非表示中も値を更新させたいので、ActiveではなくVisibleだけ切り替える。
		distanceUi->SetVisible(showDistanceUi);
	}
}

Unnamed::Entity* MyGame::GolfBallLaunchCountdownComponent::ResolveUiEntity(
	const uint64_t entityGuid,
	const std::string& entityName
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

	if (entityName.empty()) {
		return nullptr;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr || !entityPtr->IsActive()) {
			continue;
		}
		if (entityPtr->GetName() == entityName) {
			return entityPtr.get();
		}
	}
	return nullptr;
}

Unnamed::Gui::UiDigitStripComponent*
MyGame::GolfBallLaunchCountdownComponent::ResolveCountdownDigitStrip() const {
	auto* scene = GetScene();
	if (!scene || _countdownUiAssetPath.empty()) {
		return nullptr;
	}

	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr || !entityPtr->IsActive()) {
			continue;
		}

		auto* canvas = entityPtr->GetComponent<Unnamed::UiCanvasComponent>();
		if (!canvas || canvas->GetUiAssetPath() != _countdownUiAssetPath) {
			continue;
		}

		if (!canvas->EnsureRuntimeLoaded()) {
			return nullptr;
		}

		auto* root = canvas->GetRuntimeRoot();
		if (!root) {
			return nullptr;
		}

		auto* widget = FindWidgetByName(
			root->GetRootWidget(),
			_countdownDigitWidgetName
		);
		if (!widget) {
			return nullptr;
		}
		return widget->GetComponent<Unnamed::Gui::UiDigitStripComponent>();
	}

	return nullptr;
}

Unnamed::Gui::UiWidget* MyGame::GolfBallLaunchCountdownComponent::FindWidgetByName(
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

void MyGame::GolfBallLaunchCountdownComponent::ResetCountdown() {
	// NOTE: 初期時間へ戻して、次のインゲーム開始で再発射できる状態にする。
	_countdownTimer = std::max(0.0f, _countdownDuration);
	_bHasLaunched = false;
	UpdateCountdownUi();
	UpdateUiVisibility();
}

void MyGame::GolfBallLaunchCountdownComponent::StartCountdown(float seconds) {
	// NOTE: ルール管理側から指定秒数でカウントダウンを開始できるようにする。
	_countdownDuration = std::max(0.0f, seconds);
	_countdownTimer = _countdownDuration;
	_bHasLaunched = false;
	_launchOnStart = true;
	UpdateCountdownUi();
	UpdateUiVisibility();
}

void MyGame::GolfBallLaunchCountdownComponent::StartCountdown() {
	// NOTE: 停止していたカウントダウンを現在の残り時間から再開する。
	_bHasLaunched = false;
	_launchOnStart = true;
	UpdateCountdownUi();
	UpdateUiVisibility();
}

void MyGame::GolfBallLaunchCountdownComponent::StopCountdown() {
	// NOTE: 残り時間は保持したままカウントだけ止める。
	_launchOnStart = false;
	UpdateCountdownUi();
	UpdateUiVisibility();
}

bool MyGame::GolfBallLaunchCountdownComponent::IsCountingDown() const {
	// NOTE: 発射前かつ残り時間がある状態をカウント中として扱う。
	return _launchOnStart && !_bHasLaunched && _countdownTimer > 0.0f;
}

bool MyGame::GolfBallLaunchCountdownComponent::HasLaunched() const {
	return _bHasLaunched;
}

float MyGame::GolfBallLaunchCountdownComponent::GetRemainingTime() const {
	return _countdownTimer;
}

float MyGame::GolfBallLaunchCountdownComponent::GetProgress01() const {
	// NOTE: 初期時間が0の場合は即発射扱いで0を返す。
	if (_countdownDuration <= 0.0f) {
		return 0.0f;
	}

	return std::clamp(_countdownTimer / _countdownDuration, 0.0f, 1.0f);
}

namespace MyGame {
	REGISTER_COMPONENT(GolfBallLaunchCountdownComponent);
}
