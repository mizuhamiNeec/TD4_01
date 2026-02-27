#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <engine/scene/UScene.h>

#include "core/guidgenerator/GuidGenerator.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;

	namespace Render {
		struct RenderFrameContext;
		struct RenderFrameInputs;
	}

	struct WorldTime {
		float deltaTime         = 0.0f;
		float unscaledDeltaTime = 0.0f;
		float timeSeconds       = 0.0f;
	};

	class UWorld {
	public:
		virtual ~UWorld();

		[[nodiscard]] UScene&       GetScene() { return *mScene; }
		[[nodiscard]] const UScene& GetScene() const { return *mScene; }

		virtual void Initialize();
		virtual void Shutdown();
		virtual void Tick(float deltaTime);

		virtual bool LoadSceneFromFile(const char* path);
		virtual bool SaveSceneToFile(const char* path) const;
		virtual void UnloadScene();

		virtual void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		);

		[[nodiscard]] std::string_view GetLoadedScenePath() const {
			return mLoadedScenePath;
		}

		void SetLoadedScenePath(std::string path) {
			mLoadedScenePath = std::move(path);
		}

		void SetScene(std::unique_ptr<UScene> scene);

	protected:
		virtual void OnSceneLoaded();
		virtual void OnSceneUnloaded();

		std::unique_ptr<UScene> mScene;
		GuidGenerator           mGuidGenerator;
		WorldTime               mTime;
		std::string             mLoadedScenePath;
	};
}
