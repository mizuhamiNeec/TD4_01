#include "VoiceUiComponent.h"

#include "MicComponent.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <core/assets/AssetManager.h>
#include <core/assets/AssetType.h>

#include <engine/render/frame/RenderFrameInputs.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/framework/entity/Entity.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>
#include <engine/world/World.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

void MyGame::VoiceUiComponent::OnAttached() {
	EnsureMicStarted();
}

void MyGame::VoiceUiComponent::OnTick(float deltaTime) {
	(void)deltaTime;
	EnsureMicStarted();
}

void MyGame::VoiceUiComponent::OnRenderTick(
	float renderDeltaTime,
	float interpolationAlpha
) {
	(void)renderDeltaTime;
	(void)interpolationAlpha;

	auto* mic = ResolveMicComponent();
	if (!mic) {
		return;
	}

	auto* assetManager = GetAssetManager();
	auto* world = GetWorld();
	if (!assetManager || !world) {
		return;
	}

	// UI全体の基準位置を画面内に収める
	Vec2 basePosition = _position;
	if (auto* inputSystem = GetInputSystem()) {
		const Vec2 viewportSize = inputSystem->GetMouseClientViewportSize();
		if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
			basePosition.x = std::clamp(basePosition.x, 0.0f, viewportSize.x);
			basePosition.y = std::clamp(basePosition.y, 0.0f, viewportSize.y);
		}
	}

	const int32_t barCount = std::max(1, _barCount);
	const int32_t activeBarCount = GetActiveBarCount(mic->GetVolumePercentage());

	// 画像パスと位置を渡して、画面スプライトを1枚積む
	auto queueSprite = [&](const std::string& texturePath, const Vec2& position,
	                       const Vec2& size, int32_t sortKey) {
		if (texturePath.empty()) {
			return;
		}

		const Unnamed::AssetID textureAssetId =
			assetManager->LoadFromFile(texturePath, Unnamed::ASSET_TYPE::TEXTURE);
		if (textureAssetId == Unnamed::kInvalidAssetID) {
			return;
		}

		Unnamed::Render::ScreenSpriteInput sprite = {};
		sprite.texture.source = Unnamed::Render::SPRITE_TEXTURE_SOURCE::ASSET;
		sprite.texture.textureAssetId = textureAssetId;
		sprite.positionPx = position;
		sprite.sizePx = size;
		sprite.anchor = _anchor;
		sprite.color = _color;
		sprite.sortKey = sortKey;

		world->QueueDebugScreenSprite(std::move(sprite));
	};

	// まず全てのバーを灰色で描画する
	for (int32_t i = 0; i < barCount; ++i) {
		const Vec2 barPosition = basePosition + Vec2(0.0f, _barStepY * i);
		queueSprite(_levelTexturePaths[0], barPosition, _barSize, _sortKey);
	}

	// 音量に応じて下から順に色付きバーを重ねる
	for (int32_t i = 0; i < activeBarCount; ++i) {
		const int32_t barIndexFromTop = barCount - 1 - i;
		const Vec2 barPosition =
			basePosition + Vec2(0.0f, _barStepY * barIndexFromTop);
		queueSprite(GetActiveBarTexturePath(i), barPosition, _barSize, _sortKey + 1);
	}

	// メーター下部の穴画像を描画する
	queueSprite(
		_holeTexturePath,
		basePosition + Vec2(0.0f, _holeOffsetY),
		_holeSize,
		_sortKey + 2
	);
}

void MyGame::VoiceUiComponent::OnDetached() {
}

std::string_view MyGame::VoiceUiComponent::GetStableName() const {
	return "mygame.VoiceUiComponent";
}

std::string_view MyGame::VoiceUiComponent::GetComponentName() const {
	return "Voice Ui Component";
}

#ifdef _DEBUG
void MyGame::VoiceUiComponent::DrawInspectorImGui() {
	uint64_t micGuid = _micEntityGuid;
	if (ImGui::InputScalar("Mic Entity GUID", ImGuiDataType_U64, &micGuid)) {
		_micEntityGuid = micGuid;
		_micStartRequested = false;
	}

	float position[2] = {_position.x, _position.y};
	if (ImGui::DragFloat2("Position", position, 1.0f)) {
		_position = Vec2(position[0], position[1]);
	}

	float barSize[2] = {_barSize.x, _barSize.y};
	if (ImGui::DragFloat2("Bar Size", barSize, 1.0f, 1.0f, 1024.0f)) {
		_barSize = Vec2(barSize[0], barSize[1]);
	}

	float holeSize[2] = {_holeSize.x, _holeSize.y};
	if (ImGui::DragFloat2("Hole Size", holeSize, 1.0f, 1.0f, 1024.0f)) {
		_holeSize = Vec2(holeSize[0], holeSize[1]);
	}

	ImGui::DragInt("Bar Count", &_barCount, 1.0f, 1, 16);
	ImGui::DragFloat("Bar Step Y", &_barStepY, 1.0f, 1.0f, 128.0f);
	ImGui::DragFloat("Hole Offset Y", &_holeOffsetY, 1.0f, 0.0f, 1024.0f);
	ImGui::DragInt("Sort Key", &_sortKey, 1.0f);
	ImGui::Checkbox("Auto Start Mic", &_autoStartMic);

	if (auto* mic = ResolveMicComponent()) {
		const auto stats = mic->GetVolumeStats();
		ImGui::Separator();
		ImGui::Text("Volume: %.1f%%", stats.percentage);
		ImGui::Text("Smoothed: %.3f (%.1f dB)", stats.smoothedRMS, stats.smoothedRMSDB);
		ImGui::ProgressBar(stats.percentage / 100.0f, ImVec2(-1.0f, 0.0f));
	} else {
		ImGui::Text("MicComponent not found");
	}
}
#endif

void MyGame::VoiceUiComponent::Deserialize(const Unnamed::JsonReader& reader) {
	if (reader.Has("micEntityGuid")) {
		_micEntityGuid = reader["micEntityGuid"].GetUint64();
	}
	if (reader.Has("micEntityTag")) {
		_micEntityTag = reader["micEntityTag"].GetString(_micEntityTag);
	}
	if (reader.Has("position")) {
		_position = reader["position"].GetVec2(_position);
	}
	if (reader.Has("size")) {
		_barSize = reader["size"].GetVec2(_barSize);
	}
	if (reader.Has("barSize")) {
		_barSize = reader["barSize"].GetVec2(_barSize);
	}
	if (reader.Has("holeSize")) {
		_holeSize = reader["holeSize"].GetVec2(_holeSize);
	}
	if (reader.Has("anchor")) {
		_anchor = reader["anchor"].GetVec2(_anchor);
	}
	if (reader.Has("color")) {
		_color = reader["color"].GetVec4(_color);
	}
	if (reader.Has("barCount")) {
		_barCount = reader["barCount"].GetInt(_barCount);
	}
	if (reader.Has("barStepY")) {
		_barStepY = reader["barStepY"].GetFloat(_barStepY);
	}
	if (reader.Has("holeOffsetY")) {
		_holeOffsetY = reader["holeOffsetY"].GetFloat(_holeOffsetY);
	}
	if (reader.Has("sortKey")) {
		_sortKey = reader["sortKey"].GetInt(_sortKey);
	}
	if (reader.Has("autoStartMic")) {
		_autoStartMic = reader["autoStartMic"].GetBool(_autoStartMic);
	}
	for (size_t i = 0; i < _levelTexturePaths.size(); ++i) {
		const std::string key = "level" + std::to_string(i) + "TexturePath";
		if (reader.Has(key)) {
			_levelTexturePaths[i] = reader[key].GetString(_levelTexturePaths[i]);
		}
	}
	if (reader.Has("holeTexturePath")) {
		_holeTexturePath = reader["holeTexturePath"].GetString(_holeTexturePath);
	}
}

void MyGame::VoiceUiComponent::Serialize(Unnamed::JsonWriter& writer) const {
	writer.Key("micEntityGuid");
	writer.Write(_micEntityGuid);

	writer.Key("micEntityTag");
	writer.Write(_micEntityTag);

	writer.Key("position");
	writer.BeginArray();
	writer.Write(_position.x);
	writer.Write(_position.y);
	writer.EndArray();

	writer.Key("barSize");
	writer.BeginArray();
	writer.Write(_barSize.x);
	writer.Write(_barSize.y);
	writer.EndArray();

	writer.Key("holeSize");
	writer.BeginArray();
	writer.Write(_holeSize.x);
	writer.Write(_holeSize.y);
	writer.EndArray();

	writer.Key("anchor");
	writer.BeginArray();
	writer.Write(_anchor.x);
	writer.Write(_anchor.y);
	writer.EndArray();

	writer.Key("color");
	writer.BeginArray();
	writer.Write(_color.x);
	writer.Write(_color.y);
	writer.Write(_color.z);
	writer.Write(_color.w);
	writer.EndArray();

	writer.Key("barCount");
	writer.Write(_barCount);

	writer.Key("barStepY");
	writer.Write(_barStepY);

	writer.Key("holeOffsetY");
	writer.Write(_holeOffsetY);

	writer.Key("sortKey");
	writer.Write(_sortKey);

	writer.Key("autoStartMic");
	writer.Write(_autoStartMic);

	for (size_t i = 0; i < _levelTexturePaths.size(); ++i) {
		const std::string key = "level" + std::to_string(i) + "TexturePath";
		writer.Key(key);
		writer.Write(_levelTexturePaths[i]);
	}

	writer.Key("holeTexturePath");
	writer.Write(_holeTexturePath);
}

MyGame::MicComponent* MyGame::VoiceUiComponent::ResolveMicComponent() const {
	// 同じEntityにMicComponentがある場合はそれを優先する
	if (auto* owner = GetOwner()) {
		if (auto* mic = owner->GetComponent<MicComponent>()) {
			return mic;
		}
	}

	auto* scene = GetScene();
	if (!scene) {
		return nullptr;
	}

	// GUIDが指定されている場合は、そのEntityからMicComponentを探す
	if (_micEntityGuid != 0) {
		if (auto* entity = scene->FindEntity(_micEntityGuid)) {
			if (auto* mic = entity->GetComponent<MicComponent>()) {
				return mic;
			}
		}
	}

	// タグが指定されている場合は、最初に見つかったEntityからMicComponentを探す
	if (!_micEntityTag.empty()) {
		if (auto* entity = scene->FindFirstEntityByTag(_micEntityTag)) {
			if (auto* mic = entity->GetComponent<MicComponent>()) {
				return mic;
			}
		}
	}

	// 指定がない場合は、シーン内で最初に見つかったMicComponentを使う
	for (const auto& entityPtr : scene->GetEntities()) {
		if (!entityPtr || !entityPtr->IsActive()) {
			continue;
		}
		if (auto* mic = entityPtr->GetComponent<MicComponent>()) {
			return mic;
		}
	}

	return nullptr;
}

int32_t MyGame::VoiceUiComponent::GetActiveBarCount(float percentage) const {
	// 0～100% を 0～バー本数 に変換する
	const float clampedPercentage = std::clamp(percentage, 0.0f, 100.0f);
	const int32_t barCount = std::max(1, _barCount);
	return std::clamp(
		static_cast<int32_t>(std::ceil(clampedPercentage / 100.0f * barCount)),
		0,
		barCount
	);
}

const std::string& MyGame::VoiceUiComponent::GetActiveBarTexturePath(
	int32_t activeIndex
) const {
	// 下3本は緑、中3本はオレンジ、それより上は赤にする
	if (activeIndex >= 6) {
		return _levelTexturePaths[3];
	}
	if (activeIndex >= 3) {
		return _levelTexturePaths[2];
	}
	return _levelTexturePaths[1];
}

void MyGame::VoiceUiComponent::EnsureMicStarted() {
	// MicComponentをUI表示用に一度だけ開始する
	if (!_autoStartMic || _micStartRequested) {
		return;
	}

	if (auto* mic = ResolveMicComponent()) {
		mic->StartMic();
		_micStartRequested = true;
	}
}
