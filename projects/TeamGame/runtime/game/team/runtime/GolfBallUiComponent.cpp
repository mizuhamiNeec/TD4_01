#include "GolfBallUiComponent.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <core/assets/AssetManager.h>
#include <core/assets/AssetType.h>

#include <engine/render/frame/RenderFrameInputs.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/framework/components/TransformComponent.h>
#include <engine/unnamed/framework/entity/Entity.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>
#include <engine/world/World.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace {
	constexpr float kProjectionEpsilon = 1.0e-5f;

	[[nodiscard]] bool IsInsideScreen(const Vec2& position, const Vec2& size) {
		return
			position.x >= 0.0f &&
			position.y >= 0.0f &&
			position.x <= size.x &&
			position.y <= size.y;
	}
}

void MyGame::GolfBallUiComponent::OnAttached() {}

void MyGame::GolfBallUiComponent::OnTick(float deltaTime) {}

void MyGame::GolfBallUiComponent::OnRenderTick(
	float renderDeltaTime,
	float interpolationAlpha
) {
	(void)renderDeltaTime;
	(void)interpolationAlpha;

	auto* world = GetWorld();
	if (!world) {
		return;
	}

	auto* inputSystem = GetInputSystem();
	if (!inputSystem) {
		return;
	}

	const Vec2 viewportSize = inputSystem->GetMouseClientViewportSize();
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
		return;
	}

	Unnamed::Entity* targetEntity = ResolveTargetEntity();
	if (!targetEntity || !targetEntity->IsActive()) {
		return;
	}

	auto* targetTransform =
		targetEntity->GetComponent<Unnamed::TransformComponent>();
	if (!targetTransform) {
		return;
	}

	Mat4 targetWorld = targetTransform->RenderWorldMat();
	ScreenProjection projection = {};
	if (
		!ProjectWorldToScreen(
			targetWorld.GetTranslate(),
			viewportSize,
			projection
		)
	) {
		return;
	}

	if (
		(projection.onScreen && !_drawWhenOnScreen) ||
		(!projection.onScreen && !_drawWhenOffScreen)
	) {
		return;
	}

	Vec2 drawPosition = projection.onScreen ?
		                    projection.position :
		                    ClampToScreenEdge(projection.position, viewportSize);
	drawPosition += _screenOffset;

	auto* assetManager = GetAssetManager();
	if (!assetManager || _texturePath.empty()) {
		return;
	}

	const Unnamed::AssetID textureAssetId =
		assetManager->LoadFromFile(_texturePath, Unnamed::ASSET_TYPE::TEXTURE);
	if (textureAssetId == Unnamed::kInvalidAssetID) {
		return;
	}

	Unnamed::Render::ScreenSpriteInput sprite = {};
	sprite.texture.source = Unnamed::Render::SPRITE_TEXTURE_SOURCE::ASSET;
	sprite.texture.textureAssetId = textureAssetId;
	sprite.positionPx = drawPosition;
	sprite.sizePx = _spriteSize;
	sprite.anchor = Vec2(0.5f, 0.5f);
	sprite.color = _color;
	sprite.sortKey = _sortKey;

	world->QueueDebugScreenSprite(std::move(sprite));
}

void MyGame::GolfBallUiComponent::OnDetached() {}

std::string_view MyGame::GolfBallUiComponent::GetStableName() const {
	return "mygame.GolfBallUiComponent";
}

std::string_view MyGame::GolfBallUiComponent::GetComponentName() const {
	return "Golf Ball Ui Component";
}

#ifdef _DEBUG
void MyGame::GolfBallUiComponent::DrawInspectorImGui() {
	uint64_t targetGuid = _targetEntityGuid;
	if (ImGui::InputScalar("Target Entity GUID", ImGuiDataType_U64, &targetGuid)) {
		_targetEntityGuid = targetGuid;
	}

	float spriteSize[2] = {_spriteSize.x, _spriteSize.y};
	if (ImGui::DragFloat2("Sprite Size", spriteSize, 1.0f, 1.0f, 1024.0f)) {
		_spriteSize = Vec2(spriteSize[0], spriteSize[1]);
	}

	float screenOffset[2] = {_screenOffset.x, _screenOffset.y};
	if (ImGui::DragFloat2("Screen Offset", screenOffset, 1.0f)) {
		_screenOffset = Vec2(screenOffset[0], screenOffset[1]);
	}

	ImGui::DragFloat("Edge Padding", &_edgePadding, 1.0f, 0.0f, 512.0f);
	ImGui::DragInt("Sort Key", &_sortKey, 1.0f);
	ImGui::Checkbox("Draw When On Screen", &_drawWhenOnScreen);
	ImGui::Checkbox("Draw When Off Screen", &_drawWhenOffScreen);
}
#endif

void MyGame::GolfBallUiComponent::Deserialize(const Unnamed::JsonReader& reader) {
	if (reader.Has("targetEntityGuid")) {
		_targetEntityGuid = reader["targetEntityGuid"].GetUint64();
	}
	if (reader.Has("targetTag")) {
		_targetTag = reader["targetTag"].GetString(_targetTag);
	}
	if (reader.Has("texturePath")) {
		_texturePath = reader["texturePath"].GetString(_texturePath);
	}
	if (reader.Has("spriteSize")) {
		_spriteSize = reader["spriteSize"].GetVec2(_spriteSize);
	}
	if (reader.Has("screenOffset")) {
		_screenOffset = reader["screenOffset"].GetVec2(_screenOffset);
	}
	if (reader.Has("edgePadding")) {
		_edgePadding = reader["edgePadding"].GetFloat(_edgePadding);
	}
	if (reader.Has("sortKey")) {
		_sortKey = reader["sortKey"].GetInt(_sortKey);
	}
	if (reader.Has("drawWhenOnScreen")) {
		_drawWhenOnScreen = reader["drawWhenOnScreen"].GetBool(_drawWhenOnScreen);
	}
	if (reader.Has("drawWhenOffScreen")) {
		_drawWhenOffScreen =
			reader["drawWhenOffScreen"].GetBool(_drawWhenOffScreen);
	}
	if (reader.Has("color")) {
		_color = reader["color"].GetVec4(_color);
	}
}

void MyGame::GolfBallUiComponent::Serialize(Unnamed::JsonWriter& writer) const {
	writer.Key("targetEntityGuid");
	writer.Write(_targetEntityGuid);

	writer.Key("targetTag");
	writer.Write(_targetTag);

	writer.Key("texturePath");
	writer.Write(_texturePath);

	writer.Key("spriteSize");
	writer.BeginArray();
	writer.Write(_spriteSize.x);
	writer.Write(_spriteSize.y);
	writer.EndArray();

	writer.Key("screenOffset");
	writer.BeginArray();
	writer.Write(_screenOffset.x);
	writer.Write(_screenOffset.y);
	writer.EndArray();

	writer.Key("edgePadding");
	writer.Write(_edgePadding);

	writer.Key("sortKey");
	writer.Write(_sortKey);

	writer.Key("drawWhenOnScreen");
	writer.Write(_drawWhenOnScreen);

	writer.Key("drawWhenOffScreen");
	writer.Write(_drawWhenOffScreen);

	writer.Key("color");
	writer.BeginArray();
	writer.Write(_color.x);
	writer.Write(_color.y);
	writer.Write(_color.z);
	writer.Write(_color.w);
	writer.EndArray();
}

Unnamed::Entity* MyGame::GolfBallUiComponent::ResolveTargetEntity() const {
	auto* scene = GetScene();
	if (!scene) {
		return nullptr;
	}

	auto* owner = GetOwner();
	if (
		owner &&
		owner->IsActive() &&
		!_targetTag.empty() &&
		owner->HasTag(_targetTag) &&
		owner->GetComponent<Unnamed::TransformComponent>()
	) {
		// ボール自身にこのコンポーネントが付いている場合は、自分を追従対象にする。
		return owner;
	}

	if (_targetEntityGuid != 0) {
		if (auto* entity = scene->FindEntity(_targetEntityGuid)) {
			return entity;
		}
	}

	if (!_targetTag.empty()) {
		// UI用エンティティなど別の場所に付いている場合は、タグからボールを探す。
		if (auto* entity = scene->FindFirstEntityByTag(_targetTag)) {
			return entity;
		}
	}

	return nullptr;
}

bool MyGame::GolfBallUiComponent::ProjectWorldToScreen(
	const Vec3& worldPosition,
	const Vec2& viewportSize,
	ScreenProjection& outProjection
) const {
	auto* world = GetWorld();
	if (!world || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
		return false;
	}

	Unnamed::Render::RenderCameraInput camera = {};
	const float aspect = viewportSize.y > 0.0f ?
		                     viewportSize.x / viewportSize.y :
		                     16.0f / 9.0f;
	if (!world->GetCameraManager().ResolveForRender(aspect, camera) || !camera.valid) {
		return false;
	}

	// カメラのローカル軸に対するボール位置を使い、見た目の上下左右と一致する投影を行う。
	const Mat4 cameraWorld = camera.view.Inverse();
	const Vec3 toTarget = worldPosition - camera.cameraPos;
	const float cameraX = toTarget.Dot(cameraWorld.GetRight().Normalized());
	const float cameraY = toTarget.Dot(cameraWorld.GetUp().Normalized());
	const float cameraZ = toTarget.Dot(cameraWorld.GetForward().Normalized());
	const float depth = std::abs(cameraZ);
	if (depth <= kProjectionEpsilon) {
		return false;
	}

	const float ndcX = cameraX * camera.proj.m[0][0] / depth;
	const float ndcY = cameraY * camera.proj.m[1][1] / depth;

	// NDC(-1..1)からスクリーン座標へ変換する。Yは画面上が0なので反転する。
	outProjection.position = Vec2(
		(ndcX * 0.5f + 0.5f) * viewportSize.x,
		(0.5f - ndcY * 0.5f) * viewportSize.y
	);
	outProjection.onScreen =
		ndcX >= -1.0f &&
		ndcX <= 1.0f &&
		ndcY >= -1.0f &&
		ndcY <= 1.0f &&
		IsInsideScreen(outProjection.position, viewportSize);

	return true;
}

Vec2 MyGame::GolfBallUiComponent::ClampToScreenEdge(
	const Vec2& screenPosition,
	const Vec2& viewportSize
) const {
	const float padding = std::max(0.0f, _edgePadding);
	const float minX = std::min(padding, viewportSize.x * 0.5f);
	const float minY = std::min(padding, viewportSize.y * 0.5f);
	const float maxX = std::max(minX, viewportSize.x - minX);
	const float maxY = std::max(minY, viewportSize.y - minY);

	// 画面外の投影位置からスクリーン矩形への最近点に置く。
	return Vec2(
		std::clamp(screenPosition.x, minX, maxX),
		std::clamp(screenPosition.y, minY, maxY)
	);
}
