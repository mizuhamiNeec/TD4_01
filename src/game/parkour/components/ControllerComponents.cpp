#include "ControllerComponents.h"

#include <algorithm>
#include <limits>

#include "CameraRotatorComponent.h"
#include "MovementComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/scene/UScene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UGameWorld.h"
#include "engine/world/UWorld.h"

namespace Unnamed {
	namespace {
		Vec3 ReadVec3Or(
			const JsonReader& reader, const char* key, const Vec3& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 3) { return fallback; }
			return Vec3(
				value[0].GetFloat(),
				value[1].GetFloat(),
				value[2].GetFloat()
			);
		}

		Vec2 ReadVec2Or(
			const JsonReader& reader, const char* key, const Vec2& fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 2) { return fallback; }
			return Vec2(value[0].GetFloat(), value[1].GetFloat());
		}

		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}

		bool ReadBoolOr(
			const JsonReader& reader, const char* key, const bool fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetBool() : fallback;
		}

		std::string ReadStringOr(
			const JsonReader& reader,
			const char*       key,
			const std::string& fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetString() : fallback;
		}

		void WriteVec2(JsonWriter& writer, const char* key, const Vec2& value) {
			writer.Key(key);
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.EndArray();
		}

		void WriteVec3(JsonWriter& writer, const char* key, const Vec3& value) {
			writer.Key(key);
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.Write(value.z);
			writer.EndArray();
		}

		UEntity* FindEntity(UWorld* world, const uint64_t guid) {
			if (!world || guid == 0) { return nullptr; }
			return world->GetScene().FindEntity(guid);
		}

		MovementComponent* ResolveMovement(UWorld* world, const uint64_t guid) {
			UEntity* entity = FindEntity(world, guid);
			return entity ? entity->GetComponent<MovementComponent>() : nullptr;
		}

		CameraRotatorComponent* ResolveCameraRotator(
			UWorld* world,
			const uint64_t guid
		) {
			UEntity* entity = FindEntity(world, guid);
			return entity ? entity->GetComponent<CameraRotatorComponent>() :
			                nullptr;
		}

		UGameWorld::GameSceneId ParseSceneId(const std::string_view sceneName) {
			if (sceneName == "Title") { return UGameWorld::GameSceneId::Title; }
			return UGameWorld::GameSceneId::Game;
		}

		bool OverlapsPlayerAabb(
			const MovementComponent& movement,
			const Vec3&              center,
			const Vec3&              halfExtents
		) {
			const PlayerAabb player = movement.BuildWorldAabb();
			return std::abs(player.center.x - center.x) <=
			       (player.halfSize.x + halfExtents.x) &&
			       std::abs(player.center.y - center.y) <=
			       (player.halfSize.y + halfExtents.y) &&
			       std::abs(player.center.z - center.z) <=
			       (player.halfSize.z + halfExtents.z);
		}

		Vec2 ResolveOverlayExtent(const Render::SceneRenderRequest& request) {
			const float width = request.viewportPanelWidth != 0 ?
				                    static_cast<float>(
					                    request.viewportPanelWidth
				                    ) :
				                    1280.0f;
			const float height = request.viewportPanelHeight != 0 ?
				                     static_cast<float>(
					                     request.viewportPanelHeight
				                     ) :
				                     720.0f;
			return Vec2(width, height);
		}

		bool ProjectWorldToScreen(
			const Render::RenderCameraInput& camera,
			const Vec3&                      worldPosition,
			const Vec2&                      screenSize,
			Vec2&                            outScreenPosition,
			bool&                            outOffscreen,
			float&                           outArrowRotation
		) {
			outOffscreen = false;
			const Vec4 clip = camera.viewProj * Vec4(
				                  worldPosition.x,
				                  worldPosition.y,
				                  worldPosition.z,
				                  1.0f
			                  );
			if (std::abs(clip.w) <= 1e-6f) { return false; }

			Vec2 direction = Vec2(clip.x / clip.w, clip.y / clip.w);
			if (clip.w < 0.0f) { direction = -direction; }

			const bool inBounds = clip.w > 0.0f &&
			                      direction.x >= -1.0f &&
			                      direction.x <= 1.0f &&
			                      direction.y >= -1.0f &&
			                      direction.y <= 1.0f;
			if (inBounds) {
				outScreenPosition = Vec2(
					(direction.x * 0.5f + 0.5f) * screenSize.x,
					(1.0f - (direction.y * 0.5f + 0.5f)) * screenSize.y
				);
				outArrowRotation = 0.0f;
				return true;
			}

			outOffscreen = true;
			if (direction.SqrLength() <= 1e-6f) { direction = Vec2::up; }
			direction.Normalize();

			const Vec2 screenCenter = screenSize * 0.5f;
			Vec2 edgePos = Vec2(
				screenCenter.x + direction.x * 10000.0f,
				screenCenter.y - direction.y * 10000.0f
			);
			const float margin = 72.0f;
			edgePos.Clamp(
				Vec2(margin, margin),
				Vec2(
					std::max(margin, screenSize.x - margin),
					std::max(margin, screenSize.y - margin)
				)
			);
			outScreenPosition = edgePos;
			outArrowRotation = std::atan2(direction.x, direction.y);
			return true;
		}
	}

	void TitleFlowComponent::OnAttached() {
		mInput = ServiceLocator::Get<UInputSystem>();
	}

	void TitleFlowComponent::OnTick(const float deltaTime) {
		UWorld* world = UWorld::GetTickingWorld();
		auto* gameWorld = dynamic_cast<UGameWorld*>(world);
		if (!gameWorld) { return; }

		EnsureReplayLoaded();
		if (mReplayClip.frames.empty()) { return; }

		auto* movement = ResolveMovement(world, mPlayerEntityGuid);
		auto* cameraRotator = ResolveCameraRotator(world, mCameraRootEntityGuid);
		if (!movement || !cameraRotator) { return; }

		mReplayAccumulator += deltaTime;
		const float tickInterval = 1.0f / std::max(
			                           1.0f,
			                           static_cast<float>(mReplayClip.tickRate)
		                           );
		while (mReplayAccumulator >= tickInterval) {
			mReplayAccumulator -= tickInterval;
			if (mLoop) {
				mReplayCursor = (mReplayCursor + 1) % mReplayClip.frames.size();
			} else if (mReplayCursor + 1 < mReplayClip.frames.size()) {
				++mReplayCursor;
			}
		}

		const ReplayUserCmdFrame& frame = mReplayClip.frames[
			std::min(mReplayCursor, mReplayClip.frames.size() - 1)
		];
		movement->SetReplayFrame(frame);
		cameraRotator->SetReplayLookOverride(
			frame.viewPitchDeg,
			frame.viewYawDeg
		);

		if (mInput && mInput->IsPressed(mStartAction)) {
			gameWorld->RequestSceneChange(ParseSceneId(mTargetScene));
		}
	}

	void TitleFlowComponent::Deserialize(const JsonReader& reader) {
		mPlayerEntityGuid = reader.ReadUint64("playerEntityGuid").value_or(
			mPlayerEntityGuid
		);
		mCameraRootEntityGuid = reader.ReadUint64("cameraRootEntityGuid").value_or(
			mCameraRootEntityGuid
		);
		mReplayPath = ReadStringOr(reader, "replayPath", mReplayPath);
		mLoop = ReadBoolOr(reader, "loop", mLoop);
		mStartAction = ReadStringOr(reader, "startAction", mStartAction);
		mTargetScene = ReadStringOr(reader, "targetScene", mTargetScene);
		mLogoTexturePath = ReadStringOr(
			reader, "logoTexturePath", mLogoTexturePath
		);
		mPromptTexturePath = ReadStringOr(
			reader, "promptTexturePath", mPromptTexturePath
		);
		mLogoAnchorNormalized = ReadVec2Or(
			reader, "logoAnchorNormalized", mLogoAnchorNormalized
		);
		mPromptAnchorNormalized = ReadVec2Or(
			reader, "promptAnchorNormalized", mPromptAnchorNormalized
		);
		mLogoSizePx = ReadVec2Or(reader, "logoSizePx", mLogoSizePx);
		mPromptSizePx = ReadVec2Or(reader, "promptSizePx", mPromptSizePx);
		mPromptBlinkSpeed = ReadFloatOr(
			reader, "promptBlinkSpeed", mPromptBlinkSpeed
		);
		mReplayLoaded = false;
		mReplayClip.frames.clear();
		mReplayCursor = 0;
		mReplayAccumulator = 0.0f;
	}

	void TitleFlowComponent::Serialize(JsonWriter& writer) const {
		writer.Key("playerEntityGuid");
		writer.Write(mPlayerEntityGuid);
		writer.Key("cameraRootEntityGuid");
		writer.Write(mCameraRootEntityGuid);
		writer.Key("replayPath");
		writer.Write(mReplayPath);
		writer.Key("loop");
		writer.Write(mLoop);
		writer.Key("startAction");
		writer.Write(mStartAction);
		writer.Key("targetScene");
		writer.Write(mTargetScene);
		writer.Key("logoTexturePath");
		writer.Write(mLogoTexturePath);
		writer.Key("promptTexturePath");
		writer.Write(mPromptTexturePath);
		WriteVec2(writer, "logoAnchorNormalized", mLogoAnchorNormalized);
		WriteVec2(writer, "promptAnchorNormalized", mPromptAnchorNormalized);
		WriteVec2(writer, "logoSizePx", mLogoSizePx);
		WriteVec2(writer, "promptSizePx", mPromptSizePx);
		writer.Key("promptBlinkSpeed");
		writer.Write(mPromptBlinkSpeed);
	}

	void TitleFlowComponent::AppendOverlaySprites(
		Render::RenderFrameInputs& inputs,
		AssetManager&              assetManager
	) {
		if (mLogoTexture == kInvalidAssetID) {
			mLogoTexture = assetManager.LoadFromFile(
				mLogoTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}
		if (mPromptTexture == kInvalidAssetID) {
			mPromptTexture = assetManager.LoadFromFile(
				mPromptTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}

		const Vec2 screenSize = ResolveOverlayExtent(inputs.sceneRenderRequest);

		Render::ScreenSpriteInput logo = {};
		logo.textureAssetId = mLogoTexture;
		logo.positionPx = Vec2(
			screenSize.x * mLogoAnchorNormalized.x,
			screenSize.y * mLogoAnchorNormalized.y
		);
		logo.sizePx = mLogoSizePx;
		logo.anchor = Vec2(0.5f, 0.5f);
		logo.color = Vec4(1.0f, 1.0f, 1.0f, 0.92f);
		logo.sortKey = 100;
		inputs.screenSprites.emplace_back(logo);

		const float blinkAlpha = 0.45f + 0.55f * (
			                         0.5f + 0.5f * std::sin(
				                         inputs.time * mPromptBlinkSpeed
			                         )
		                         );
		Render::ScreenSpriteInput prompt = {};
		prompt.textureAssetId = mPromptTexture;
		prompt.positionPx = Vec2(
			screenSize.x * mPromptAnchorNormalized.x,
			screenSize.y * mPromptAnchorNormalized.y
		);
		prompt.sizePx = mPromptSizePx;
		prompt.anchor = Vec2(0.5f, 0.5f);
		prompt.color = Vec4(1.0f, 1.0f, 1.0f, blinkAlpha);
		prompt.sortKey = 101;
		inputs.screenSprites.emplace_back(prompt);
	}

	void TitleFlowComponent::EnsureReplayLoaded() {
		if (mReplayLoaded) { return; }
		mReplayLoaded = true;
		if (!mReplayManager.LoadClipFromFile(mReplayPath, mReplayClip)) {
			mReplayClip = mReplayManager.BuildDefaultTitleDemoClip();
		}
		mReplayCursor = 0;
		mReplayAccumulator = 0.0f;
	}

	void CourseComponent::OnTick(const float) {
		UWorld* world = UWorld::GetTickingWorld();
		auto* gameWorld = dynamic_cast<UGameWorld*>(world);
		if (!gameWorld) { return; }

		auto* movement = ResolveMovement(world, mPlayerEntityGuid);
		if (!movement) { return; }

		UScene& scene = world->GetScene();
		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			if (const auto* jumpPad = entity->GetComponent<JumpPadComponent>();
				jumpPad && OverlapsPlayerAabb(
					*movement,
					jumpPad->GetWorldCenter(),
					jumpPad->GetWorldHalfExtentsMeters()
				)) {
				movement->ApplyJumpPadBoost(jumpPad->GetBoostVelocityHu());
			}

			if (const auto* boost = entity->GetComponent<SpeedBoostAreaComponent>();
				boost && OverlapsPlayerAabb(
					*movement,
					boost->GetWorldCenter(),
					boost->GetWorldHalfExtentsMeters()
				)) {
				movement->ApplySpeedBoost(
					boost->GetMultiplier(),
					boost->GetDurationSec()
				);
			}

			if (const auto* checkpoint = entity->GetComponent<CheckpointComponent>();
				checkpoint && checkpoint->GetIndex() > mCurrentCheckpointIndex &&
				OverlapsPlayerAabb(
					*movement,
					checkpoint->GetWorldCenter(),
					checkpoint->GetWorldHalfExtentsMeters()
				)) {
				mCurrentCheckpointIndex = checkpoint->GetIndex();
				movement->SetRespawnPosition(checkpoint->GetRespawnPosition());
			}

			if (const auto* goal = entity->GetComponent<GoalComponent>();
				goal && OverlapsPlayerAabb(
					*movement,
					goal->GetWorldCenter(),
					goal->GetWorldHalfExtentsMeters()
				)) {
				gameWorld->RequestSceneChange(ParseSceneId(mReturnScene));
			}
		}
	}

	void CourseComponent::Deserialize(const JsonReader& reader) {
		mPlayerEntityGuid = reader.ReadUint64("playerEntityGuid").value_or(
			mPlayerEntityGuid
		);
		mReturnScene = ReadStringOr(reader, "returnScene", mReturnScene);
		mPingTexturePath = ReadStringOr(reader, "pingTexturePath", mPingTexturePath);
		mArrowTexturePath = ReadStringOr(
			reader, "arrowTexturePath", mArrowTexturePath
		);
		mPingSizePx = ReadVec2Or(reader, "pingSizePx", mPingSizePx);
		mArrowSizePx = ReadVec2Or(reader, "arrowSizePx", mArrowSizePx);
	}

	void CourseComponent::Serialize(JsonWriter& writer) const {
		writer.Key("playerEntityGuid");
		writer.Write(mPlayerEntityGuid);
		writer.Key("returnScene");
		writer.Write(mReturnScene);
		writer.Key("pingTexturePath");
		writer.Write(mPingTexturePath);
		writer.Key("arrowTexturePath");
		writer.Write(mArrowTexturePath);
		WriteVec2(writer, "pingSizePx", mPingSizePx);
		WriteVec2(writer, "arrowSizePx", mArrowSizePx);
	}

	void CourseComponent::AppendOverlaySprites(
		Render::RenderFrameInputs& inputs,
		AssetManager&              assetManager
	) {
		if (!inputs.camera.valid) { return; }
		UWorld* world = UWorld::GetTickingWorld();
		if (!world) { return; }

		Vec3    nextTarget = Vec3::zero;
		bool    hasTarget = false;
		int32_t bestIndex = std::numeric_limits<int32_t>::max();
		for (const auto& entity : world->GetScene().GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }
			if (const auto* checkpoint = entity->GetComponent<CheckpointComponent>();
				checkpoint && checkpoint->GetIndex() > mCurrentCheckpointIndex &&
				checkpoint->GetIndex() < bestIndex) {
				bestIndex = checkpoint->GetIndex();
				nextTarget = checkpoint->GetWorldCenter();
				hasTarget = true;
			}
		}
		if (!hasTarget) {
			for (const auto& entity : world->GetScene().GetEntities()) {
				if (!entity || !entity->IsActive()) { continue; }
				if (const auto* goal = entity->GetComponent<GoalComponent>()) {
					nextTarget = goal->GetWorldCenter();
					hasTarget = true;
					break;
				}
			}
		}
		if (!hasTarget) { return; }

		if (mPingTexture == kInvalidAssetID) {
			mPingTexture = assetManager.LoadFromFile(
				mPingTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}
		if (mArrowTexture == kInvalidAssetID) {
			mArrowTexture = assetManager.LoadFromFile(
				mArrowTexturePath,
				ASSET_TYPE::TEXTURE
			);
		}

		Vec2  screenPosition = Vec2::zero;
		bool  offscreen = false;
		float arrowRotation = 0.0f;
		const Vec2 screenSize = ResolveOverlayExtent(inputs.sceneRenderRequest);
		if (!ProjectWorldToScreen(
			inputs.camera,
			nextTarget,
			screenSize,
			screenPosition,
			offscreen,
			arrowRotation
		)) {
			return;
		}

		Render::ScreenSpriteInput sprite = {};
		sprite.positionPx = screenPosition;
		sprite.anchor = Vec2(0.5f, 0.5f);
		sprite.sortKey = 110;
		if (offscreen) {
			sprite.textureAssetId = mArrowTexture;
			sprite.sizePx = mArrowSizePx;
			sprite.rotationRad = arrowRotation;
			sprite.color = Vec4(1.0f, 1.0f, 1.0f, 0.9f);
		} else {
			sprite.textureAssetId = mPingTexture;
			sprite.sizePx = mPingSizePx;
			sprite.color = Vec4::one;
		}
		inputs.screenSprites.emplace_back(sprite);
	}

	void TeleportTriggerComponent::OnTick(const float) {
		UWorld* world = UWorld::GetTickingWorld();
		if (!world) { return; }

		MovementComponent* movement = nullptr;
		for (const auto& entity : world->GetScene().GetEntities()) {
			if (!entity) { continue; }
			movement = entity->GetComponent<MovementComponent>();
			if (movement) { break; }
		}
		if (!movement) { return; }

		const Vec3 center = GetWorldCenter();
		const Vec3 halfExtents = GetWorldHalfExtentsMeters();
		const bool inside = OverlapsPlayerAabb(*movement, center, halfExtents);
		const bool insideExpanded = OverlapsPlayerAabb(
			*movement,
			center,
			halfExtents + Vec3::one * mRearmBuffer
		);

		if (inside && mArmed) {
			movement->TeleportTo(mDestination);
			if (mResetVelocity) { movement->SetVelocity(Vec3::zero); }
			mArmed = false;
		} else if (!insideExpanded) { mArmed = true; }
	}

	void TeleportTriggerComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		mDestination = ReadVec3Or(reader, "destination", mDestination);
		mResetVelocity = ReadBoolOr(reader, "resetVelocity", mResetVelocity);
		mRearmBuffer = ReadFloatOr(reader, "rearmBuffer", mRearmBuffer);
	}

	void TeleportTriggerComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		WriteVec3(writer, "destination", mDestination);
		writer.Key("resetVelocity");
		writer.Write(mResetVelocity);
		writer.Key("rearmBuffer");
		writer.Write(mRearmBuffer);
	}

	void SceneChangeOnActionComponent::OnAttached() {
		mInput = ServiceLocator::Get<UInputSystem>();
	}

	void SceneChangeOnActionComponent::OnTick(const float) {
		if (!mInput || !mInput->IsPressed(mActionName)) { return; }
		UWorld* world = UWorld::GetTickingWorld();
		auto* gameWorld = dynamic_cast<UGameWorld*>(world);
		if (!gameWorld) { return; }
		gameWorld->RequestSceneChange(ParseSceneId(mTargetScene));
	}

	void SceneChangeOnActionComponent::Deserialize(const JsonReader& reader) {
		mActionName = ReadStringOr(reader, "actionName", mActionName);
		mTargetScene = ReadStringOr(reader, "targetScene", mTargetScene);
	}

	void SceneChangeOnActionComponent::Serialize(JsonWriter& writer) const {
		writer.Key("actionName");
		writer.Write(mActionName);
		writer.Key("targetScene");
		writer.Write(mTargetScene);
	}

	void CursorModeComponent::OnAttached() {
		mInput = ServiceLocator::Get<UInputSystem>();
	}

	void CursorModeComponent::OnTick(const float) {
		if (!mInput) { return; }
		const UWorld* world = UWorld::GetTickingWorld();
		if (!world || !world->IsGameSimulationEnabled()) { return; }
		//mInput->SetMouseCursorLocked(mLockCursor);
		//mInput->SetMouseCursorVisible(mShowCursor);
	}

	void CursorModeComponent::Deserialize(const JsonReader& reader) {
		mLockCursor = ReadBoolOr(reader, "lockCursor", mLockCursor);
		mShowCursor = ReadBoolOr(reader, "showCursor", mShowCursor);
	}

	void CursorModeComponent::Serialize(JsonWriter& writer) const {
		writer.Key("lockCursor");
		writer.Write(mLockCursor);
		writer.Key("showCursor");
		writer.Write(mShowCursor);
	}

	void OscillateTransformComponent::OnAttached() {
		if (TransformComponent* transform = GetTransform()) {
			mBasePosition = transform->Position();
			mCapturedBase = true;
		}
	}

	void OscillateTransformComponent::PrePhysicsTick(const float deltaTime) {
		TransformComponent* transform = GetTransform();
		if (!transform) { return; }
		if (!mCapturedBase) {
			mBasePosition = transform->Position();
			mCapturedBase = true;
		}

		mTime += deltaTime;
		Vec3 axis = mAxis;
		if (axis.SqrLength() <= 1e-6f) { axis = Vec3::right; }
		axis.Normalize();
		transform->SetPosition(
			mBasePosition + axis * (
				std::sin(mTime * mFrequency + mPhaseOffset) * mAmplitude
			)
		);
	}

	void OscillateTransformComponent::Deserialize(const JsonReader& reader) {
		mAxis = ReadVec3Or(reader, "axis", mAxis);
		mAmplitude = ReadFloatOr(reader, "amplitude", mAmplitude);
		mFrequency = ReadFloatOr(reader, "frequency", mFrequency);
		mPhaseOffset = ReadFloatOr(reader, "phaseOffset", mPhaseOffset);
	}

	void OscillateTransformComponent::Serialize(JsonWriter& writer) const {
		WriteVec3(writer, "axis", mAxis);
		writer.Key("amplitude");
		writer.Write(mAmplitude);
		writer.Key("frequency");
		writer.Write(mFrequency);
		writer.Key("phaseOffset");
		writer.Write(mPhaseOffset);
	}

	TransformComponent* OscillateTransformComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(TitleFlowComponent);
	REGISTER_COMPONENT(CourseComponent);
	REGISTER_COMPONENT(TeleportTriggerComponent);
	REGISTER_COMPONENT(SceneChangeOnActionComponent);
	REGISTER_COMPONENT(CursorModeComponent);
	REGISTER_COMPONENT(OscillateTransformComponent);
}
