#include "UWorld.h"

#include <algorithm>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/render/frame/RenderFrameContext.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"
#include "engine/scene/USceneSerializer.h"
#include "engine/unnamed/framework/components/GameplayCameraComponent.h"
#include "engine/unnamed/framework/components/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/PortalComponent.h"
#include "engine/unnamed/framework/components/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/UEntity.h"
#include "engine/unnamed/subsystem/console/Log.h"

#include <set>

namespace Unnamed {
	static constexpr std::string_view kChannel = "UWorld";
	namespace {
		thread_local UWorld* gTickingWorld = nullptr;

		class ScopedTickingWorld {
		public:
			explicit ScopedTickingWorld(UWorld* world) : mPrevious(gTickingWorld) {
				gTickingWorld = world;
			}

			~ScopedTickingWorld() { gTickingWorld = mPrevious; }

		private:
			UWorld* mPrevious = nullptr;
		};
	}

	UWorld::~UWorld() = default;

	UWorld* UWorld::GetTickingWorld() noexcept { return gTickingWorld; }

	void UWorld::Initialize() {
		if (!mScene) { mScene = std::make_unique<UScene>(); }
	}

	void UWorld::Shutdown() { UnloadScene(); }

	void UWorld::Tick(const float deltaTime) {
		mTime.deltaTime         = deltaTime;
		mTime.unscaledDeltaTime = deltaTime;
		mTime.timeSeconds       += deltaTime;

		if (!mScene) { return; }
		ScopedTickingWorld tickingWorld(this);

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
		inputs.worldBillboards.clear();
		inputs.screenSprites.clear();

		if (!mScene) { return; }

		std::set<std::pair<uint64_t, uint64_t>> emittedPortalPairs;

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) { continue; }

			if (!inputs.camera.valid) {
				const auto* gameplayCamera = entity->GetComponent<
					GameplayCameraComponent>();
				if (gameplayCamera && gameplayCamera->IsActive() &&
				    gameplayCamera->IsCameraActive()) {
					gameplayCamera->BuildCameraInput(inputs.camera);
				}
			}

			if (!inputs.camera.valid) {
				const auto* editorCamera = entity->GetComponent<
					EditorCameraComponent>();
				if (editorCamera && editorCamera->IsActive()) {
					editorCamera->BuildCameraInput(inputs.camera);
				}
			}

			if (entity->IsEditorOnly() || !entity->IsVisible()) { continue; }

			const auto* transform = entity->GetComponent<TransformComponent>();
			const auto* portal = entity->GetComponent<PortalComponent>();
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
					object.ownerEntityGuid = entity->GetGuid();
					object.isPortalSurface =
						portal && portal->IsActive() && portal->IsEnabled() &&
						portal->GetLinkedPortalGuid() != 0 &&
						portal->GetUseAsPortalSurface();
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
				object.ownerEntityGuid = entity->GetGuid();
				object.isPortalSurface =
					portal && portal->IsActive() && portal->IsEnabled() &&
					portal->GetLinkedPortalGuid() != 0 &&
					portal->GetUseAsPortalSurface();
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

			if (
				portal && portal->IsActive() && portal->IsEnabled() &&
				portal->GetLinkedPortalGuid() != 0
			) {
				if (portal->GetLinkedPortalGuid() == entity->GetGuid()) {
					continue;
				}

				UEntity* linkedEntity = mScene->FindEntity(
					portal->GetLinkedPortalGuid()
				);
				if (!linkedEntity || !linkedEntity->IsActive()) { continue; }

				const auto* linkedPortal = linkedEntity->GetComponent<
					PortalComponent>();
				const auto* linkedTransform = linkedEntity->GetComponent<
					TransformComponent>();
				if (!linkedPortal || !linkedTransform || !linkedPortal->
				    IsActive() ||
				    !linkedPortal->IsEnabled()) { continue; }
				if (linkedPortal->GetLinkedPortalGuid() != entity->GetGuid()) {
					continue;
				}

				const auto pairKey = std::minmax(
					entity->GetGuid(), linkedEntity->GetGuid()
				);
				if (!emittedPortalPairs.emplace(pairKey).second) { continue; }

				Render::PortalPairInput portalPair = {};
				portalPair.enabled = true;
				portalPair.fromPortalGuid = entity->GetGuid();
				portalPair.toPortalGuid = linkedEntity->GetGuid();
				portalPair.fromPortalWorld = transform->WorldMat();
				portalPair.toPortalWorld = linkedTransform->WorldMat();
				portalPair.fromPortalHalfExtents = portal->GetHalfExtents();
				portalPair.toPortalHalfExtents = linkedPortal->GetHalfExtents();
				inputs.portalPairs.emplace_back(portalPair);

				std::swap(portalPair.fromPortalGuid, portalPair.toPortalGuid);
				std::swap(portalPair.fromPortalWorld, portalPair.toPortalWorld);
				std::swap(
					portalPair.fromPortalHalfExtents,
					portalPair.toPortalHalfExtents
				);
				inputs.portalPairs.emplace_back(portalPair);
			}
		}
	}

	bool UWorld::IsGameSimulationEnabled() const noexcept {
		return true;
	}

	std::string_view UWorld::GetLoadedScenePath() const {
		return mLoadedScenePath;
	}

	void UWorld::SetLoadedScenePath(std::string path) {
		mLoadedScenePath = std::move(path);
	}

	void UWorld::SetScene(std::unique_ptr<UScene> scene) {
		mScene = std::move(scene);
	}

	void UWorld::OnSceneLoaded() {}
	void UWorld::OnSceneUnloaded() {}
}
