#pragma once

#include <memory>

#include "engine/physics/core/UPhysics.h"

#include "game/parkour/ParkourServices.h"

namespace Unnamed {
	class AssetManager;
	class UGameWorld;
	class UScene;
	class GameplayCameraComponent;
	namespace Render {
		struct SceneRenderRequest;
		struct RenderFrameInputs;
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
		void BuildRenderContributions(
			Render::RenderFrameInputs& inputs,
			AssetManager&              assetManager
		);
		bool TriggerDebugExplosion();

	private:
		void CacheScene(UScene& scene);
		void RegisterStaticMeshes(UScene& scene);

		UGameWorld&       mWorld;
		ParkourServices   mServices = {};
		UPhysics::Engine mPhysics = {};
		GameplayCameraComponent* mGameplayCamera = nullptr;
	};
}
