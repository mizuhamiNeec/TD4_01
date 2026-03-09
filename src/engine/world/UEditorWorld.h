#pragma once
#include "UWorld.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	namespace Render {
		struct SceneRenderRequest;
	}

	class UGameWorld;
	class EditorCameraComponent;
	class Entity;

	class UEditorWorld final : public UWorld {
	public:
		void Initialize() override;
		void Tick(float deltaTime) override;

		void StartPlayInEditor();
		void StopPlayInEditor();

		[[nodiscard]] bool IsPlaying() const {
			return mPlayWorld != nullptr;
		}

		void FillRenderFrameInputs(
			Render::RenderFrameInputs&  inputs,
			Render::RenderFrameContext& frameContext,
			AssetManager&               assetManager
		) override;
		[[nodiscard]] bool IsGameSimulationEnabled() const noexcept override;

		[[nodiscard]] UScene* GetEditableScene() {
			return mScene.get();
		}

		[[nodiscard]] const UScene* GetEditableScene() const {
			return mScene.get();
		}

		[[nodiscard]] UWorld*                      GetRuntimeSceneWorld();
		[[nodiscard]] const UWorld*                GetRuntimeSceneWorld() const;
		[[nodiscard]] UScene*                      GetActiveScene();
		[[nodiscard]] const UScene*                GetActiveScene() const;
		[[nodiscard]] EditorCameraComponent*       GetEditorCamera();
		[[nodiscard]] const EditorCameraComponent* GetEditorCamera() const;
		bool                                       BuildEditorCameraMatrices(
			const Render::SceneRenderRequest& request,
			Mat4&                             outView,
			Mat4&                             outProj
		);
		void SetEditorCameraLookEnabled(bool enabled);

	private:
		void UpdateEditorCameraAspectIfNeeded(
			const Render::SceneRenderRequest& request
		);

		std::unique_ptr<Entity>    mEditorEntity;
		std::unique_ptr<UGameWorld> mPlayWorld;
		Render::SCENE_RENDER_MODE   mLastAspectMode =
			Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
		uint32_t mLastAspectViewportWidth  = 0;
		uint32_t mLastAspectViewportHeight = 0;
	};
}
