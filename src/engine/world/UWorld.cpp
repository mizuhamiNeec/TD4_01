#include "UWorld.h"

#include <algorithm>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/render/frame/RenderFrameContext.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/unnamed/framework/components/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "UWorld";

	namespace {
		float ResolveRenderAspect(const Render::SceneRenderRequest& request) {
			uint32_t width  = request.viewportPanelWidth;
			uint32_t height = request.viewportPanelHeight;

			switch (request.mode) {
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9: width = 16;
					height = 9;
					break;
				case Render::SCENE_RENDER_MODE::HD_720P: width = 1280;
					height = 720;
					break;
				case Render::SCENE_RENDER_MODE::FHD_1080P: width = 1920;
					height = 1080;
					break;
				default: break;
			}

			if (width == 0 || height == 0) { return 16.0f / 9.0f; }
			return static_cast<float>(width) / static_cast<float>(height);
		}
	}

	UWorld::~UWorld() = default;

	void UWorld::Initialize() {
		if (!mScene) { mScene = std::make_unique<UScene>(); }
	}

	void UWorld::Shutdown() { UnloadScene(); }

	void UWorld::Tick(const float deltaTime) {
		mTime.deltaTime         = deltaTime;
		mTime.unscaledDeltaTime = deltaTime;
		mTime.timeSeconds       += deltaTime;

		if (!mScene) { return; }

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }
			entity->PrePhysicsTick(deltaTime);
			entity->Tick(deltaTime);
			entity->PostPhysicsTick(deltaTime);
		}
	}

	bool UWorld::LoadSceneFromFile(const char* path) {
		if (!path || std::string_view(path).empty()) { return false; }

		auto       newScene = std::make_unique<UScene>();
		const bool ok       = USceneSerializer::LoadFromFile(
			*newScene, path, mGuidGenerator
		);
		if (!ok) { return false; }

		UnloadScene();
		SetScene(std::move(newScene));
		mLoadedScenePath = StrUtil::NormalizePath(path);
		OnSceneLoaded();

		Msg(
			kChannel, "Scene loaded successfully from file: {}",
			std::string(path)
		);

		return true;
	}

	bool UWorld::SaveSceneToFile(const char* path) const {
		if (!mScene || !path || std::string_view(path).empty()) {
			return false;
		}
		return USceneSerializer::SaveToFile(*mScene, path);
	}

	void UWorld::UnloadScene() {
		if (!mScene) { return; }

		OnSceneUnloaded();
		mScene.reset();
		mLoadedScenePath.clear();
	}

	void UWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		frameContext.Reset();
		inputs.camera = {};
		inputs.visibleObjects.clear();
		inputs.skinningPalettes.clear();
		inputs.portalPairs.clear();

		if (!mScene) { return; }
		const float renderAspect = ResolveRenderAspect(
			inputs.sceneRenderRequest
		);

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			if (!inputs.camera.valid) {
				const auto* editorCamera = entity->GetComponent<
					EditorCameraComponent>();
				if (editorCamera && editorCamera->IsActive()) {
					editorCamera->BuildCameraInput(renderAspect, inputs.camera);
				}
			}

			if (entity->IsEditorOnly()) { continue; }

			const auto* transform = entity->GetComponent<TransformComponent>();
			auto*       meshRenderer = entity->GetComponent<
				StaticMeshRendererComponent>();
			auto* skelRenderer = entity->GetComponent<
				SkeletalMeshRendererComponent>();
			if (!transform) { continue; }

			if (meshRenderer && meshRenderer->IsActive()) {
				const AssetID meshAssetId = meshRenderer->ResolveMeshAsset(
					assetManager
				);
				if (meshAssetId != kInvalidAssetID) {
					Render::VisibleRenderObject object = {};
					object.meshAssetId                 = meshAssetId;
					object.materialInstanceId          = meshRenderer->
						ResolveMaterialInstanceAsset(
							assetManager
						);
					object.world     = transform->WorldMat();
					object.isSkinned = false;
					inputs.visibleObjects.emplace_back(object);
				}
			}

			if (skelRenderer && skelRenderer->IsActive()) {
				const AssetID meshAssetId = skelRenderer->ResolveMeshAsset(
					assetManager
				);
				if (meshAssetId == kInvalidAssetID) { continue; }

				Render::VisibleRenderObject object = {};
				object.meshAssetId                 = meshAssetId;
				object.materialInstanceId          = skelRenderer->
					ResolveMaterialInstanceAsset(
						assetManager
					);
				object.world             = transform->WorldMat();
				object.isSkinned         = false;
				object.skeletonPaletteId = 0;

				const auto* meshAsset = assetManager.Get<MeshAssetData>(
					meshAssetId
				);
				const bool hasSkinning =
					meshAsset && meshAsset->hasSkinning && !meshAsset->skeleton.
					empty();
				if (hasSkinning) {
					object.isSkinned = true;
					const auto found = frameContext.skinningPaletteIndexByMesh.
					                                find(
						                                meshAssetId
					                                );
					if (found != frameContext.skinningPaletteIndexByMesh.
					                          end()) {
						object.skeletonPaletteId = found->second;
					} else {
						Render::SkinningPaletteInput palette = {};
						palette.meshAssetId                  = meshAssetId;

						const uint32_t boneCount = std::min<uint32_t>(
							static_cast<uint32_t>(meshAsset->skeleton.size()),
							64u
						);
						palette.boneMatrices.resize(boneCount, Mat4::identity);

						const uint32_t paletteIndex = static_cast<uint32_t>(
							inputs.skinningPalettes.size()
						);
						inputs.skinningPalettes.emplace_back(
							std::move(palette)
						);
						frameContext.skinningPaletteIndexByMesh.emplace(
							meshAssetId, paletteIndex
						);
						object.skeletonPaletteId = paletteIndex;
					}
				}

				inputs.visibleObjects.emplace_back(object);
			}
		}
	}

	void UWorld::SetScene(std::unique_ptr<UScene> scene) {
		mScene = std::move(scene);
	}

	void UWorld::OnSceneLoaded() {}
	void UWorld::OnSceneUnloaded() {}
}
