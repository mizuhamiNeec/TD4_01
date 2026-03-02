#pragma once

#include <memory>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include "engine/physics/core/UPhysics.h"

#include "game/parkour/ParkourReplay.h"

namespace Unnamed {
	class AssetManager;
	class UEntity;
	class UGameWorld;
	class UScene;
	class UInputSystem;
	class GameplayCameraComponent;
	class CameraRotatorComponent;
	class MovementComponent;
	class TransformComponent;
	namespace Render {
		struct SceneRenderRequest;
		struct RenderFrameInputs;
		struct RenderCameraInput;
	}

	class ParkourRuntime {
	public:
		explicit ParkourRuntime(UGameWorld& world);

		void Initialize();
		void OnSceneLoaded(UScene& scene);
		void OnSceneUnloaded();
		void PreWorldTick(float deltaTime);
		void PostWorldTick(float deltaTime);
		void PrepareRender(const Render::SceneRenderRequest& request);
		void BuildOverlaySprites(
			Render::RenderFrameInputs& inputs,
			AssetManager&              assetManager
		);

	private:
		struct CachedEntities {
			UEntity* player            = nullptr;
			UEntity* cameraRoot        = nullptr;
			UEntity* camera            = nullptr;
			UEntity* world             = nullptr;
			UEntity* fan               = nullptr;
			MovementComponent* movement = nullptr;
			CameraRotatorComponent* cameraRotator = nullptr;
			GameplayCameraComponent* gameplayCamera = nullptr;
			TransformComponent* fanTransform = nullptr;
		};

		void CacheScene(UScene& scene);
		void RegisterStaticMeshes(UScene& scene);
		void StartTitleDemo();
		void AdvanceTitleReplay(float deltaTime);
		void TickFan(float deltaTime);
		void TickGameplayTriggers();
		void TickTeleport();
		void ResetProgressionState();
		[[nodiscard]] bool OverlapsPlayerAabb(
			const Vec3& center, const Vec3& halfExtents
		) const;
		[[nodiscard]] Vec2 ResolveOverlayExtent(
			const Render::SceneRenderRequest& request
		) const;
		[[nodiscard]] bool ProjectWorldToScreen(
			const Render::RenderCameraInput& camera,
			const Vec3&                      worldPosition,
			const Vec2&                      screenSize,
			Vec2&                            outScreenPosition
		) const;

		UGameWorld& mWorld;
		UInputSystem* mInput = nullptr;
		UPhysics::Engine mPhysics = {};
		ParkourReplayManager mReplayManager = {};
		ParkourReplayManager::ReplayClip mTitleReplayClip = {};
		size_t mTitleReplayCursor = 0;
		float  mTitleReplayAccumulator = 0.0f;
		float  mFanPhase = 0.0f;
		Vec3   mFanBasePosition = Vec3::zero;
		CachedEntities mCached = {};
		int32_t mCurrentCheckpointIndex = 0;
		bool    mTeleportArmed = true;
		AssetID mPingTexture = kInvalidAssetID;
		AssetID mArrowTexture = kInvalidAssetID;
	};
}
