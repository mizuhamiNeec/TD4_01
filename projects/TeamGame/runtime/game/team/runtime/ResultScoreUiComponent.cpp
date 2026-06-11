#include "ResultScoreUiComponent.h"

#include "TeamGameResultScore.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <core/ComponentRegistry.h>

#include <engine/gui/UiRoot.h>
#include <engine/gui/UiWidget.h>
#include <engine/gui/components/UiDigitStripComponent.h>
#include <engine/unnamed/framework/components/ui/UiCanvasComponent.h>
#include <engine/unnamed/framework/entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

void MyGame::ResultScoreUiComponent::OnAttached() {
	_elapsed = 0.0f;
	EnsureUiCanvasAsset();
	UpdateScoreUi(0.0f);
}

void MyGame::ResultScoreUiComponent::OnRenderTick(
	float renderDeltaTime,
	float interpolationAlpha
) {
	(void)interpolationAlpha;

	_elapsed += renderDeltaTime;

	// Normalized linear progress, then cubic ease-out so the digits snap into
	// place near the end (fast start, gentle stop).
	const float t = _countUpDuration > 0.0f
		? std::clamp(_elapsed / _countUpDuration, 0.0f, 1.0f)
		: 1.0f;
	const float eased = 1.0f - std::pow(1.0f - t, 3.0f);

	EnsureUiCanvasAsset();
	UpdateScoreUi(eased);
}

void MyGame::ResultScoreUiComponent::OnDetached() {}

std::string_view MyGame::ResultScoreUiComponent::GetStableName() const {
	return "mygame.ResultScoreUiComponent";
}

std::string_view MyGame::ResultScoreUiComponent::GetComponentName() const {
	return "Result Score Ui Component";
}

#ifdef _DEBUG
void MyGame::ResultScoreUiComponent::DrawInspectorImGui() {
	ImGui::Text("=== Result Score UI Component ===");
	ImGui::Text("UI Asset: %s", _uiAssetPath.c_str());

	ImGui::SliderFloat("Count-Up Duration", &_countUpDuration, 0.0f, 5.0f, "%.2f s");
	if (ImGui::Button("Replay Count-Up")) {
		_elapsed = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Text("Elapsed: %.2f s", _elapsed);

	const TeamGameResultScoreSnapshot snapshot = LoadTeamGameResultScore();
	ImGui::Text("Total: %d", snapshot.total);
	ImGui::Text("Trash To Sea: %d", snapshot.trashToSea);
	ImGui::Text("Trash Into Hole: %d", snapshot.trashIntoHole);
	ImGui::Text("Ball Catch: %d", snapshot.ballCatch);
	ImGui::Text("Hole In One: %d", snapshot.holeInOne);
	ImGui::Text("Direct Hole In One: %d", snapshot.directHoleInOne);
	ImGui::Text("OB Penalty: %d", snapshot.outOfBoundsPenalty);
}
#endif

void MyGame::ResultScoreUiComponent::Deserialize(
	const Unnamed::JsonReader& reader
) {
	if (reader.Has("uiAssetPath")) {
		_uiAssetPath = reader["uiAssetPath"].GetString(_uiAssetPath);
	}
	if (reader.Has("totalWidgetName")) {
		_totalWidgetName = reader["totalWidgetName"].GetString(_totalWidgetName);
	}
	if (reader.Has("trashToSeaWidgetName")) {
		_trashToSeaWidgetName =
			reader["trashToSeaWidgetName"].GetString(_trashToSeaWidgetName);
	}
	if (reader.Has("trashIntoHoleWidgetName")) {
		_trashIntoHoleWidgetName =
			reader["trashIntoHoleWidgetName"].GetString(_trashIntoHoleWidgetName);
	}
	if (reader.Has("ballCatchWidgetName")) {
		_ballCatchWidgetName =
			reader["ballCatchWidgetName"].GetString(_ballCatchWidgetName);
	}
	if (reader.Has("holeInOneWidgetName")) {
		_holeInOneWidgetName =
			reader["holeInOneWidgetName"].GetString(_holeInOneWidgetName);
	}
	if (reader.Has("directHoleInOneWidgetName")) {
		_directHoleInOneWidgetName =
			reader["directHoleInOneWidgetName"].GetString(_directHoleInOneWidgetName);
	}
	if (reader.Has("outOfBoundsPenaltyWidgetName")) {
		_outOfBoundsPenaltyWidgetName = reader["outOfBoundsPenaltyWidgetName"]
			.GetString(_outOfBoundsPenaltyWidgetName);
	}
	if (reader.Has("countUpDuration")) {
		_countUpDuration = reader["countUpDuration"].GetFloat(_countUpDuration);
	}
}

void MyGame::ResultScoreUiComponent::Serialize(
	Unnamed::JsonWriter& writer
) const {
	writer.Key("uiAssetPath");
	writer.Write(_uiAssetPath);
	writer.Key("totalWidgetName");
	writer.Write(_totalWidgetName);
	writer.Key("trashToSeaWidgetName");
	writer.Write(_trashToSeaWidgetName);
	writer.Key("trashIntoHoleWidgetName");
	writer.Write(_trashIntoHoleWidgetName);
	writer.Key("ballCatchWidgetName");
	writer.Write(_ballCatchWidgetName);
	writer.Key("holeInOneWidgetName");
	writer.Write(_holeInOneWidgetName);
	writer.Key("directHoleInOneWidgetName");
	writer.Write(_directHoleInOneWidgetName);
	writer.Key("outOfBoundsPenaltyWidgetName");
	writer.Write(_outOfBoundsPenaltyWidgetName);
	writer.Key("countUpDuration");
	writer.Write(_countUpDuration);
}

void MyGame::ResultScoreUiComponent::UpdateScoreUi(const float progress) const {
	const TeamGameResultScoreSnapshot snapshot = LoadTeamGameResultScore();

	// Scales a final score by the count-up progress, rounding so the displayed
	// digits climb from 0 and land exactly on the target when progress reaches 1.
	const auto countUp = [progress](const int finalValue) {
		return static_cast<int>(
			std::lround(static_cast<float>(finalValue) * progress)
		);
	};

	if (auto* strip = ResolveDigitStrip(_totalWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.total)));
	}
	if (auto* strip = ResolveDigitStrip(_trashToSeaWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.trashToSea)));
	}
	if (auto* strip = ResolveDigitStrip(_trashIntoHoleWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.trashIntoHole)));
	}
	if (auto* strip = ResolveDigitStrip(_ballCatchWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.ballCatch)));
	}
	if (auto* strip = ResolveDigitStrip(_holeInOneWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.holeInOne)));
	}
	if (auto* strip = ResolveDigitStrip(_directHoleInOneWidgetName)) {
		strip->SetValue(countUp(std::max(0, snapshot.directHoleInOne)));
	}
	if (auto* strip = ResolveDigitStrip(_outOfBoundsPenaltyWidgetName)) {
		strip->SetValue(countUp(std::abs(snapshot.outOfBoundsPenalty)));
	}
}

Unnamed::Gui::UiDigitStripComponent*
MyGame::ResultScoreUiComponent::ResolveDigitStrip(
	const std::string_view widgetName
) const {
	auto* owner = GetOwner();
	if (!owner || widgetName.empty()) {
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

	auto* widget = FindWidgetByName(root->GetRootWidget(), widgetName);
	if (!widget) {
		return nullptr;
	}

	return widget->GetComponent<Unnamed::Gui::UiDigitStripComponent>();
}

Unnamed::Gui::UiWidget* MyGame::ResultScoreUiComponent::FindWidgetByName(
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

void MyGame::ResultScoreUiComponent::EnsureUiCanvasAsset() const {
	auto* owner = GetOwner();
	if (!owner || _uiAssetPath.empty()) {
		return;
	}

	auto* canvas = owner->GetComponent<Unnamed::UiCanvasComponent>();
	if (!canvas) {
		return;
	}

	if (canvas->GetUiAssetPath() != _uiAssetPath) {
		canvas->SetUiAssetPath(_uiAssetPath);
	}
}

namespace MyGame {
	REGISTER_COMPONENT(ResultScoreUiComponent);
}
