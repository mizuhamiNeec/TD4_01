#pragma once
#include "UWorld.h"

namespace Unnamed {
	class UGameWorld;
	class UEntity;

	class UEditorWorld final : public UWorld {
	public:
		void Initialize() override;
		void Tick(float deltaTime) override;

		void               StartPlayInEditor();
		void               StopPlayInEditor();
		[[nodiscard]] bool IsPlaying() const { return mPlayWorld != nullptr; }

		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;

		[[nodiscard]] UScene* GetEditableScene() { return mScene.get(); }

		[[nodiscard]] const UScene* GetEditableScene() const {
			return mScene.get();
		}

		[[nodiscard]] UWorld*       GetRuntimeSceneWorld();
		[[nodiscard]] const UWorld* GetRuntimeSceneWorld() const;

	private:
		std::unique_ptr<UEntity>    mEditorEntity;
		std::unique_ptr<UGameWorld> mPlayWorld;
	};
}
