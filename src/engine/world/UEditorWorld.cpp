#include "UEditorWorld.h"

#include <algorithm>
#include <chrono>

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/UGameWorld.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "UEditorWorld";

	namespace {
		float ResolveEditorCameraAspect(
			const Render::SceneRenderRequest& request
		) {
			switch (request.mode) {
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9
				: return 16.0f / 9.0f;
				case Render::SCENE_RENDER_MODE::HD_720P
				: return 1280.0f / 720.0f;
				case Render::SCENE_RENDER_MODE::FHD_1080P
				: return 1920.0f / 1080.0f;
				case Render::SCENE_RENDER_MODE::FIT_VIEWPORT:
				default: {
					const uint32_t width = std::max(
						1u, request.viewportPanelWidth
					);
					const uint32_t height = std::max(
						1u, request.viewportPanelHeight
					);
					return static_cast<float>(width) / static_cast<float>(
						       height);
				}
			}
		}
	}

	void UEditorWorld::Initialize() {
		UWorld::Initialize();

		if (!mEditorEntity) {
			mEditorEntity = std::make_unique<Entity>(
				"__EditorCameraEntity", mGuidGenerator.Alloc(), true
			);
			auto* transform = mEditorEntity->AddComponent<TransformComponent>();
			transform->SetPosition(Vec3(0.0f, 5.0f, -3.0f));
			transform->SetRotation(
				Quaternion::EulerDegrees(Vec3::right * 15.0f)
			);
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
		if (mPlayWorld || !mScene) {
			return;
		}

		const auto totalStart     = std::chrono::steady_clock::now();
		auto       playWorld      = std::make_unique<UGameWorld>();
		const auto worldInitStart = std::chrono::steady_clock::now();
		playWorld->Initialize();
		const auto worldInitEnd = std::chrono::steady_clock::now();

		auto          playScene = std::make_unique<UScene>();
		GuidGenerator cloneGuid;
		const auto    cloneStart = std::chrono::steady_clock::now();
		if (!USceneSerializer::CloneScene(*mScene, *playScene, cloneGuid)) {
			return;
		}
		const auto cloneEnd = std::chrono::steady_clock::now();

		const auto setSceneStart = std::chrono::steady_clock::now();
		playWorld->SetScene(std::move(playScene));
		const auto setSceneEnd = std::chrono::steady_clock::now();
		mPlayWorld             = std::move(playWorld);

		DevMsg(
			kChannel,
			"StartPlayInEditor timing: worldInit={}ms clone={}ms setScene={}ms total={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				worldInitEnd - worldInitStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				cloneEnd - cloneStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setSceneEnd - setSceneStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setSceneEnd - totalStart
			).count()
		);
	}

	void UEditorWorld::StopPlayInEditor() {
		if (!mPlayWorld) {
			return;
		}
		mPlayWorld->Shutdown();
		mPlayWorld.reset();
		if (auto* input = ServiceLocator::Get<UInputSystem>()) {
			input->SetMouseCursorLocked(false);
			input->SetMouseCursorVisible(true);
			input->ClearMouseCursorLockAnchor();
		}
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
		UpdateEditorCameraAspectIfNeeded(inputs.sceneRenderRequest);
		UWorld::FillRenderFrameInputs(inputs, frameContext, assetManager);

		if (!inputs.camera.valid && mEditorEntity && mEditorEntity->
		    IsActive()) {
			if (
				const auto* camera = mEditorEntity->GetComponent<
					EditorCameraComponent>();
				camera && camera->IsActive()
			) {
				camera->BuildCameraInput(inputs.camera);
			}
		}
	}

	bool UEditorWorld::IsGameSimulationEnabled() const noexcept {
		return false;
	}

	UWorld* UEditorWorld::GetRuntimeSceneWorld() {
		return mPlayWorld ? static_cast<UWorld*>(mPlayWorld.get()) : this;
	}

	const UWorld* UEditorWorld::GetRuntimeSceneWorld() const {
		return mPlayWorld ? static_cast<const UWorld*>(mPlayWorld.get()) : this;
	}

	UScene* UEditorWorld::GetActiveScene() {
		UWorld* runtimeWorld = GetRuntimeSceneWorld();
		return runtimeWorld ? &runtimeWorld->GetScene() : nullptr;
	}

	const UScene* UEditorWorld::GetActiveScene() const {
		const UWorld* runtimeWorld = GetRuntimeSceneWorld();
		return runtimeWorld ? &runtimeWorld->GetScene() : nullptr;
	}

	EditorCameraComponent* UEditorWorld::GetEditorCamera() {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return nullptr;
		}
		auto* camera = mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return nullptr;
		}
		return camera;
	}

	const EditorCameraComponent* UEditorWorld::GetEditorCamera() const {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return nullptr;
		}
		const auto* camera =
			mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return nullptr;
		}
		return camera;
	}

	bool UEditorWorld::BuildEditorCameraMatrices(
		const Render::SceneRenderRequest& request,
		Mat4&                             outView,
		Mat4&                             outProj
	) {
		UpdateEditorCameraAspectIfNeeded(request);
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return false;
		}

		const auto* camera = mEditorEntity->GetComponent<
			EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return false;
		}
		return camera->BuildViewProjectionMatrices(outView, outProj);
	}

	void UEditorWorld::SetEditorCameraLookEnabled(const bool enabled) {
		auto* camera = GetEditorCamera();
		if (!camera) {
			return;
		}
		camera->SetLookEnabled(enabled);
	}

	void UEditorWorld::UpdateEditorCameraAspectIfNeeded(
		const Render::SceneRenderRequest& request
	) {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return;
		}

		auto* camera = mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return;
		}

		bool shouldUpdate = false;
		switch (request.mode) {
			case Render::SCENE_RENDER_MODE::FIT_VIEWPORT: {
				shouldUpdate =
					request.mode != mLastAspectMode ||
					request.viewportPanelWidth != mLastAspectViewportWidth ||
					request.viewportPanelHeight != mLastAspectViewportHeight;
				break;
			}
			case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9:
			case Render::SCENE_RENDER_MODE::HD_720P:
			case Render::SCENE_RENDER_MODE::FHD_1080P: {
				shouldUpdate = request.mode != mLastAspectMode;
				break;
			}
			default: break;
		}

		if (shouldUpdate) {
			camera->SetAspectRatio(ResolveEditorCameraAspect(request));
		}

		mLastAspectMode           = request.mode;
		mLastAspectViewportWidth  = request.viewportPanelWidth;
		mLastAspectViewportHeight = request.viewportPanelHeight;
	}
}
