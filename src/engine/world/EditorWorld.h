#pragma once
#include "World.h"

#include "engine/physics/core/Physics.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/GameWorld.h"

namespace Unnamed {
	namespace Render {
		struct SceneViewRenderMode;
	}

	class EditorCameraComponent;

	class EditorWorld final : public World {
	public:
		~EditorWorld() override;

		void Initialize() override;
		void FixedTick(float fixedDeltaTime) override;
		void FrameInputTick(float frameDeltaTime) override;
		void RenderTick(float renderDeltaTime, float interpolationAlpha) override;

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

		[[nodiscard]] Scene* GetEditableScene() {
			return mScene.get();
		}

		[[nodiscard]] const Scene* GetEditableScene() const {
			return mScene.get();
		}

		[[nodiscard]] World*                       GetRuntimeSceneWorld();
		[[nodiscard]] const World*                 GetRuntimeSceneWorld() const;
		[[nodiscard]] Scene*                       GetActiveScene();
		[[nodiscard]] const Scene*                 GetActiveScene() const;
		[[nodiscard]] EditorCameraComponent*       GetEditorCamera();
		[[nodiscard]] const EditorCameraComponent* GetEditorCamera() const;
		bool                                       BuildEditorCameraMatrices(
			const Render::SceneViewRenderMode& request,
			Mat4&                              outView,
			Mat4&                              outProj
		);
		void SetEditorCameraLookEnabled(bool enabled);

	private:
		[[nodiscard]] bool BuildUiInputCamera(
			Render::RenderCameraInput& outCamera
		) const override;

		void UpdateEditorCameraAspectIfNeeded(
			const Render::SceneViewRenderMode& request
		);

		std::unique_ptr<Entity>    mEditorEntity;
		std::unique_ptr<GameWorld> mPlayWorld;
		Render::SCENE_RENDER_MODE  mLastAspectMode =
			Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
		uint32_t mLastAspectViewportWidth  = 0;
		uint32_t mLastAspectViewportHeight = 0;
	};
}
