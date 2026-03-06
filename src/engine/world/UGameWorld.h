#pragma once
#include <cstdint>

#include "UWorld.h"

namespace Unnamed {
	class UGameWorld final : public UWorld {
	public:
		~UGameWorld() override;

		void Initialize() override;
		void Shutdown() override;
		void Tick(float deltaTime) override;

		bool LoadSceneFromFile(const char* path) override;
		void UnloadScene() override;

		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;

		void SetScene(std::unique_ptr<UScene> scene) override;

	private:
	};
}
