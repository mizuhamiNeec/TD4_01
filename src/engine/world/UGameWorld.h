#pragma once
#include <cstdint>

#include "UWorld.h"
#include "game/parkour/ParkourRuntime.h"

namespace Unnamed {
	class UGameWorld final : public UWorld {
	public:
		enum class GameSceneId : uint8_t {
			Title,
			Game,
		};

		void Initialize() override;
		~UGameWorld() override;
		void Shutdown() override;
		void Tick(float deltaTime) override;
		bool LoadSceneFromFile(const char* path) override;
		void UnloadScene() override;
		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;

		void SetScene(std::unique_ptr<UScene> scene);
		void RequestSceneChange(GameSceneId target);
		[[nodiscard]] GameSceneId GetActiveSceneId() const noexcept {
			return mActiveSceneId;
		}
		void ApplyPendingSceneChange();

	private:
		void AttachRuntimeToCurrentScene();
		[[nodiscard]] const char* ResolveScenePath(GameSceneId sceneId) const;

		GameSceneId mActiveSceneId = GameSceneId::Title;
		bool        mHasPendingSceneChange = false;
		GameSceneId mPendingSceneId = GameSceneId::Title;
		bool        mSceneRuntimeAttached = false;
		std::string mTitleScenePath = "./content/parkour/scenes/title.json";
		std::string mGameplayScenePath = "./content/parkour/scenes/game.json";
		std::unique_ptr<ParkourRuntime> mRuntime;
	};
}
