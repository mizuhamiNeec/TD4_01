#include "ParkourRuntime.h"

#include <algorithm>

#include "components/CameraRotatorComponent.h"
#include "components/MovementComponent.h"
#include "components/TriggerComponents.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"
#include "engine/unnamed/framework/components/GameplayCameraComponent.h"
#include "engine/unnamed/framework/components/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UGameWorld.h"

namespace Unnamed {
	namespace {
		constexpr float kTitleTickRate = 66.0f;
		constexpr Vec3  kTeleportCenter = Vec3(19.5072f, -29.2608f, 260.096f);
		constexpr Vec3  kTeleportHalfExtents = Vec3(13.0048f, 13.0048f, 13.0048f);
		constexpr float kTeleportReararmBuffer = 1.0f;

		UEntity* FindEntityByName(UScene& scene, const std::string_view name) {
			for (const auto& entity : scene.GetEntities()) {
				if (entity && entity->GetName() == name) { return entity.get(); }
			}
			return nullptr;
		}
	}

	ParkourRuntime::ParkourRuntime(UGameWorld& world) : mWorld(world) {}

	void ParkourRuntime::Initialize() {
		mInput = ServiceLocator::Get<UInputSystem>();
		mPhysics.Init();
		StartTitleDemo();
	}

	void ParkourRuntime::OnSceneLoaded(UScene& scene) {
		CacheScene(scene);
		RegisterStaticMeshes(scene);
		ResetProgressionState();
		if (mInput) {
			const bool isTitle = mWorld.GetActiveSceneId() ==
				UGameWorld::GameSceneId::Title;
			mInput->SetMouseCursorLocked(!isTitle);
		}
		StartTitleDemo();
	}

	void ParkourRuntime::OnSceneUnloaded() {
		mCached = {};
		mPhysics.ClearStaticMeshes();
		if (mInput) { mInput->SetMouseCursorLocked(false); }
	}

	void ParkourRuntime::PreWorldTick(const float deltaTime) {
		if (mWorld.GetActiveSceneId() == UGameWorld::GameSceneId::Title) {
			AdvanceTitleReplay(deltaTime);
			if (mInput && mInput->IsPressed("jump")) {
				mWorld.RequestSceneChange(UGameWorld::GameSceneId::Game);
			}
		} else if (mInput && mInput->IsPressed("returntotitle")) {
			mWorld.RequestSceneChange(UGameWorld::GameSceneId::Title);
		}
	}

	void ParkourRuntime::PostWorldTick(const float deltaTime) {
		TickFan(deltaTime);
		if (mWorld.GetActiveSceneId() == UGameWorld::GameSceneId::Game) {
			TickGameplayTriggers();
			TickTeleport();
		}
	}

	void ParkourRuntime::PrepareRender(
		const Render::SceneRenderRequest& request
	) {
		if (mCached.gameplayCamera) {
			const Vec2 size = ResolveOverlayExtent(request);
			mCached.gameplayCamera->SetAspectRatio(size.x / std::max(1.0f, size.y));
		}
	}

	void ParkourRuntime::BuildOverlaySprites(
		Render::RenderFrameInputs& inputs,
		AssetManager&              assetManager
	) {
		if (mPingTexture == kInvalidAssetID) {
			mPingTexture = assetManager.LoadFromFile(
				"./content/parkour/textures/ping.png",
				ASSET_TYPE::TEXTURE
			);
		}
		if (mArrowTexture == kInvalidAssetID) {
			mArrowTexture = assetManager.LoadFromFile(
				"./content/parkour/textures/arrow.png",
				ASSET_TYPE::TEXTURE
			);
		}

		const Vec2 screenSize = ResolveOverlayExtent(inputs.sceneRenderRequest);
		if (mWorld.GetActiveSceneId() == UGameWorld::GameSceneId::Title) {
			inputs.screenSprites.emplace_back(
				Render::ScreenSpriteInput{
					.textureAssetId = mPingTexture,
					.positionPx     = Vec2(screenSize.x * 0.5f, screenSize.y * 0.32f),
					.sizePx         = Vec2(320.0f, 320.0f),
					.anchor         = Vec2(0.5f, 0.5f),
					.rotationRad    = 0.0f,
					.color          = Vec4(1.0f, 1.0f, 1.0f, 0.92f),
					.sortKey        = 100,
				}
			);
			const float blinkAlpha = 0.45f + 0.55f * (
				0.5f + 0.5f * std::sin(inputs.time * 4.0f)
			);
			inputs.screenSprites.emplace_back(
				Render::ScreenSpriteInput{
					.textureAssetId = mArrowTexture,
					.positionPx     = Vec2(screenSize.x * 0.5f, screenSize.y * 0.82f),
					.sizePx         = Vec2(180.0f, 90.0f),
					.anchor         = Vec2(0.5f, 0.5f),
					.rotationRad    = 0.0f,
					.color          = Vec4(1.0f, 1.0f, 1.0f, blinkAlpha),
					.sortKey        = 101,
				}
			);
			return;
		}

		if (!mCached.movement) { return; }
		Vec3 nextTarget = Vec3::zero;
		bool hasTarget  = false;
		if (auto* scene = &mWorld.GetScene()) {
			for (const auto& entity : scene->GetEntities()) {
				if (!entity) { continue; }
				const auto* checkpoint = entity->GetComponent<CheckpointComponent>();
				if (!checkpoint || checkpoint->GetIndex() <= mCurrentCheckpointIndex) {
					continue;
				}
				nextTarget = checkpoint->GetWorldCenter();
				hasTarget  = true;
				break;
			}
			if (!hasTarget) {
				for (const auto& entity : scene->GetEntities()) {
					if (!entity) { continue; }
					const auto* goal = entity->GetComponent<GoalComponent>();
					if (!goal) { continue; }
					nextTarget = goal->GetWorldCenter();
					hasTarget = true;
					break;
				}
			}
		}
		if (!hasTarget || !inputs.camera.valid) { return; }

		Vec2 screenPos = Vec2::zero;
		if (ProjectWorldToScreen(inputs.camera, nextTarget, screenSize, screenPos)) {
			inputs.screenSprites.emplace_back(
				Render::ScreenSpriteInput{
					.textureAssetId = mPingTexture,
					.positionPx     = screenPos,
					.sizePx         = Vec2(96.0f, 96.0f),
					.anchor         = Vec2(0.5f, 0.5f),
					.rotationRad    = 0.0f,
					.color          = Vec4::one,
					.sortKey        = 100,
				}
			);
		} else {
			inputs.screenSprites.emplace_back(
				Render::ScreenSpriteInput{
					.textureAssetId = mArrowTexture,
					.positionPx     = Vec2(screenSize.x * 0.5f, screenSize.y * 0.84f),
					.sizePx         = Vec2(160.0f, 80.0f),
					.anchor         = Vec2(0.5f, 0.5f),
					.rotationRad    = 0.0f,
					.color          = Vec4(1.0f, 1.0f, 1.0f, 0.85f),
					.sortKey        = 101,
				}
			);
		}
	}

	void ParkourRuntime::CacheScene(UScene& scene) {
		mCached = {};
		mCached.player = FindEntityByName(scene, "Player");
		mCached.cameraRoot = FindEntityByName(scene, "CameraRoot");
		mCached.camera     = FindEntityByName(scene, "Camera");
		mCached.world      = FindEntityByName(scene, "World");
		mCached.fan        = FindEntityByName(scene, "Fan");

		if (mCached.player) {
			mCached.movement = mCached.player->GetComponent<MovementComponent>();
			if (mCached.movement) {
				mCached.movement->Initialize(&mPhysics, nullptr);
			}
		}
		if (mCached.cameraRoot) {
			mCached.cameraRotator = mCached.cameraRoot->GetComponent<
				CameraRotatorComponent>();
			if (mCached.movement) {
				mCached.movement->Initialize(&mPhysics, mCached.cameraRotator);
			}
		}
		if (mCached.camera) {
			mCached.gameplayCamera = mCached.camera->GetComponent<
				GameplayCameraComponent>();
		}
		if (mCached.fan) {
			mCached.fanTransform = mCached.fan->GetComponent<TransformComponent>();
			if (mCached.fanTransform) {
				mFanBasePosition = mCached.fanTransform->Position();
			}
		}
	}

	void ParkourRuntime::RegisterStaticMeshes(UScene& scene) {
		mPhysics.ClearStaticMeshes();

		auto* assetManager = ServiceLocator::Get<AssetManager>();
		if (!assetManager) { return; }

		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }
			if (auto* transform = entity->GetComponent<TransformComponent>()) {
				transform->OnTick(0.0f);
			}
		}

		for (const auto& entity : scene.GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			const auto* collider = entity->GetComponent<StaticMeshColliderComponent>();
			const auto* transform = entity->GetComponent<TransformComponent>();
			auto* renderer = entity->GetComponent<StaticMeshRendererComponent>();
			if (!collider || !collider->IsCollisionEnabled() || !transform || !renderer) {
				continue;
			}

			const AssetID meshAssetId = renderer->ResolveMeshAsset(*assetManager);
			const auto* mesh = assetManager->Get<MeshAssetData>(meshAssetId);
			if (!mesh) { continue; }

			mPhysics.RegisterStaticMesh(
				entity->GetGuid(),
				mesh->vertices,
				mesh->indices,
				transform->WorldMat()
			);
		}
	}

	void ParkourRuntime::StartTitleDemo() {
		if (mWorld.GetActiveSceneId() != UGameWorld::GameSceneId::Title) {
			return;
		}

		if (!mReplayManager.LoadClipFromFile(
			std::string(ParkourReplayManager::kDefaultTitleDemoPath),
			mTitleReplayClip
		)) {
			mTitleReplayClip = mReplayManager.BuildDefaultTitleDemoClip();
		}
		mTitleReplayCursor      = 0;
		mTitleReplayAccumulator = 0.0f;
	}

	void ParkourRuntime::AdvanceTitleReplay(const float deltaTime) {
		if (!mCached.movement || !mCached.cameraRotator || mTitleReplayClip.frames.empty()) {
			return;
		}

		mTitleReplayAccumulator += deltaTime;
		const float tickInterval = 1.0f / std::max(
			1.0f,
			static_cast<float>(mTitleReplayClip.tickRate)
		);
		while (mTitleReplayAccumulator >= tickInterval) {
			mTitleReplayAccumulator -= tickInterval;
			const ReplayUserCmdFrame& frame = mTitleReplayClip.frames[
				mTitleReplayCursor % mTitleReplayClip.frames.size()
			];
			mCached.movement->SetReplayFrame(frame);
			mCached.cameraRotator->SetReplayLookOverride(
				frame.viewPitchDeg,
				frame.viewYawDeg
			);
			++mTitleReplayCursor;
		}
	}

	void ParkourRuntime::TickFan(const float deltaTime) {
		if (!mCached.fanTransform) { return; }
		mFanPhase += deltaTime;
		Vec3 position = mFanBasePosition;
		position.x += std::sin(mFanPhase) * 20.0f;
		mCached.fanTransform->SetPosition(position);
	}

	void ParkourRuntime::TickGameplayTriggers() {
		auto* scene = &mWorld.GetScene();
		if (!mCached.movement || !scene) { return; }

		for (const auto& entity : scene->GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			if (const auto* jumpPad = entity->GetComponent<JumpPadComponent>();
				jumpPad && OverlapsPlayerAabb(
					jumpPad->GetWorldCenter(),
					jumpPad->GetWorldHalfExtentsMeters()
				)) {
				mCached.movement->ApplyJumpPadBoost(jumpPad->GetBoostVelocityHu());
			}

			if (const auto* boost = entity->GetComponent<SpeedBoostAreaComponent>();
				boost && OverlapsPlayerAabb(
					boost->GetWorldCenter(),
					boost->GetWorldHalfExtentsMeters()
				)) {
				mCached.movement->ApplySpeedBoost(
					boost->GetMultiplier(),
					boost->GetDurationSec()
				);
			}

			if (const auto* checkpoint = entity->GetComponent<CheckpointComponent>();
				checkpoint && checkpoint->GetIndex() > mCurrentCheckpointIndex &&
				OverlapsPlayerAabb(
					checkpoint->GetWorldCenter(),
					checkpoint->GetWorldHalfExtentsMeters()
				)) {
				mCurrentCheckpointIndex = checkpoint->GetIndex();
				mCached.movement->SetRespawnPosition(
					checkpoint->GetRespawnPosition()
				);
			}

			if (const auto* goal = entity->GetComponent<GoalComponent>();
				goal && OverlapsPlayerAabb(
					goal->GetWorldCenter(),
					goal->GetWorldHalfExtentsMeters()
				)) {
				mWorld.RequestSceneChange(UGameWorld::GameSceneId::Title);
			}
		}
	}

	void ParkourRuntime::TickTeleport() {
		if (!mCached.movement) { return; }
		const Vec3 expanded = kTeleportHalfExtents + Vec3::one * kTeleportReararmBuffer;
		const bool inside = OverlapsPlayerAabb(kTeleportCenter, kTeleportHalfExtents);
		const bool insideExpanded = OverlapsPlayerAabb(kTeleportCenter, expanded);

		if (inside && mTeleportArmed) {
			mCached.movement->TeleportTo(Vec3::zero);
			mCached.movement->SetVelocity(Vec3::zero);
			mTeleportArmed = false;
		} else if (!insideExpanded) {
			mTeleportArmed = true;
		}
	}

	void ParkourRuntime::ResetProgressionState() {
		mCurrentCheckpointIndex = 0;
		mTeleportArmed = true;
		if (mCached.movement) {
			mCached.movement->SetRespawnPosition(mCached.movement->GetFeetPosition());
		}
	}

	bool ParkourRuntime::OverlapsPlayerAabb(
		const Vec3& center, const Vec3& halfExtents
	) const {
		if (!mCached.movement) { return false; }
		const PlayerAabb player = mCached.movement->BuildWorldAabb();
		return std::abs(player.center.x - center.x) <=
		           (player.halfSize.x + halfExtents.x) &&
		       std::abs(player.center.y - center.y) <=
		           (player.halfSize.y + halfExtents.y) &&
		       std::abs(player.center.z - center.z) <=
		           (player.halfSize.z + halfExtents.z);
	}

	Vec2 ParkourRuntime::ResolveOverlayExtent(
		const Render::SceneRenderRequest& request
	) const {
		const float width = request.viewportPanelWidth != 0 ?
			request.viewportPanelWidth :
			1280.0f;
		const float height = request.viewportPanelHeight != 0 ?
			request.viewportPanelHeight :
			720.0f;
		return Vec2(width, height);
	}

	bool ParkourRuntime::ProjectWorldToScreen(
		const Render::RenderCameraInput& camera,
		const Vec3&                      worldPosition,
		const Vec2&                      screenSize,
		Vec2&                            outScreenPosition
	) const {
		const Vec4 clip = camera.viewProj * Vec4(
			worldPosition.x,
			worldPosition.y,
			worldPosition.z,
			1.0f
		);
		if (std::abs(clip.w) <= 1e-6f || clip.w <= 0.0f) { return false; }

		const Vec3 ndc = Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
		if (ndc.x < -1.1f || ndc.x > 1.1f || ndc.y < -1.1f || ndc.y > 1.1f) {
			return false;
		}

		outScreenPosition.x = (ndc.x * 0.5f + 0.5f) * screenSize.x;
		outScreenPosition.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenSize.y;
		return true;
	}
}
