#pragma once

#include "World.h"

namespace Unnamed {
	class GameWorld final : public World {
	public:
		~GameWorld() override;

		void Initialize() override;
		void Shutdown() override;
		void Tick(float unscaledDeltaTime, float deltaTime) override;

		bool LoadSceneFromFile(const char* path) override;
		void UnloadScene() override;

		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;

		void SetScene(std::unique_ptr<Scene> scene) override;
	};
}
