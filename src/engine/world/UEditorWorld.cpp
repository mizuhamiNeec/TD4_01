#include "UEditorWorld.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/unnamed/framework/components/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/world/UGameWorld.h"

namespace Unnamed {
	void UEditorWorld::Initialize() {
		UWorld::Initialize();

		if (!mEditorEntity) {
			mEditorEntity = std::make_unique<UEntity>(
				"__EditorCameraEntity", mGuidGenerator.Alloc(), true
			);
			auto* transform = mEditorEntity->AddComponent<TransformComponent>();
			transform->SetPosition(Vec3(0.0f, 1.0f, -3.0f));
			mEditorEntity->AddComponent<EditorCameraComponent>();
		}
	}

	void UEditorWorld::Tick(const float deltaTime) {
		if (mPlayWorld) {
			mPlayWorld->Tick(deltaTime);
			return;
		}
		if (mEditorEntity && mEditorEntity->IsActive()) {
			mEditorEntity->PrePhysicsTick(deltaTime);
			mEditorEntity->Tick(deltaTime);
			mEditorEntity->PostPhysicsTick(deltaTime);
		}
		UWorld::Tick(deltaTime);
	}

	void UEditorWorld::StartPlayInEditor() {
		if (mPlayWorld || !mScene) { return; }

		auto playWorld = std::make_unique<UGameWorld>();
		playWorld->Initialize();

		auto          playScene = std::make_unique<UScene>();
		GuidGenerator cloneGuid;
		if (!USceneSerializer::CloneScene(*mScene, *playScene, cloneGuid)) {
			return;
		}

		playWorld->SetScene(std::move(playScene));
		mPlayWorld = std::move(playWorld);
	}

	void UEditorWorld::StopPlayInEditor() {
		if (!mPlayWorld) { return; }
		mPlayWorld->Shutdown();
		mPlayWorld.reset();
	}

	void UEditorWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		if (mPlayWorld) {
			mPlayWorld->FillRenderFrameInputs(
				inputs, frameContext, assetManager
			);
			return;
		}
		UWorld::FillRenderFrameInputs(inputs, frameContext, assetManager);

		if (!inputs.camera.valid && mEditorEntity && mEditorEntity->
		    IsActive()) {
			if (
				const auto* camera = mEditorEntity->GetComponent<
					EditorCameraComponent>(); camera && camera->IsActive()
			) {
				float aspect = 16.0f / 9.0f;
				switch (inputs.sceneRenderRequest.mode) {
					case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9
					: aspect = 16.0f / 9.0f;
						break;
					case Render::SCENE_RENDER_MODE::HD_720P
					: aspect = 1280.0f / 720.0f;
						break;
					case Render::SCENE_RENDER_MODE::FHD_1080P
					: aspect = 1920.0f / 1080.0f;
						break;
					default: {
						const auto width = inputs.sceneRenderRequest.
						                          viewportPanelWidth;
						const auto height = inputs.sceneRenderRequest.
						                           viewportPanelHeight;
						if (width > 0 && height > 0) {
							aspect =
								static_cast<float>(width) / static_cast<float>(
									height);
						}
						break;
					}
				}
				camera->BuildCameraInput(aspect, inputs.camera);
			}
		}
	}

	UWorld* UEditorWorld::GetRuntimeSceneWorld() {
		return mPlayWorld ? static_cast<UWorld*>(mPlayWorld.get()) : this;
	}

	const UWorld* UEditorWorld::GetRuntimeSceneWorld() const {
		return mPlayWorld ? static_cast<const UWorld*>(mPlayWorld.get()) : this;
	}
}
