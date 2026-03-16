#include "World.h"

#include <algorithm>
#include <cfloat>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/gui/UiDrawCommand.h"
#include "engine/gui/UiRoot.h"
#include "engine/render/frame/RenderFrameContext.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/portal/PortalComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/unnamed/physics/CollisionDetection.h"

#include <set>

namespace Unnamed {
	static constexpr std::string_view kChannel = "World";
	namespace {
		struct UiCanvasRuntimeEntry {
			Entity*            entity    = nullptr;
			TransformComponent* transform = nullptr;
			UiCanvasComponent* canvas    = nullptr;
		};

		Vec4 ToVec4(const Gui::Color& color) {
			return Vec4(color.r, color.g, color.b, color.a);
		}

		bool BuildRayFromMouse(
			const Render::RenderCameraInput& camera,
			const Vec2&                      mousePos,
			const Vec2&                      viewportSize,
			Ray&                             outRay
		) {
			if (
				!camera.valid ||
				viewportSize.x <= 0.0f ||
				viewportSize.y <= 0.0f
			) {
				return false;
			}
			if (
				mousePos.x < 0.0f ||
				mousePos.y < 0.0f ||
				mousePos.x > viewportSize.x ||
				mousePos.y > viewportSize.y
			) {
				return false;
			}

			const float ndcX = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
			const float ndcY = 1.0f - (mousePos.y / viewportSize.y) * 2.0f;

			const Mat4 invViewProj = (camera.view * camera.proj).Inverse();
			Vec4 nearClip(ndcX, ndcY, 0.0f, 1.0f);
			Vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

			Vec4 nearWorld4 = invViewProj * nearClip;
			Vec4 farWorld4  = invViewProj * farClip;
			const float nearW =
				std::abs(nearWorld4.w) > 1e-6f ? nearWorld4.w : 1.0f;
			const float farW =
				std::abs(farWorld4.w) > 1e-6f ? farWorld4.w : 1.0f;

			const Vec3 nearWorld(
				nearWorld4.x / nearW,
				nearWorld4.y / nearW,
				nearWorld4.z / nearW
			);
			const Vec3 farWorld(
				farWorld4.x / farW,
				farWorld4.y / farW,
				farWorld4.z / farW
			);

			Vec3 direction = farWorld - nearWorld;
			if (direction.SqrLength() < 1e-8f) {
				return false;
			}
			direction = direction.Normalized();

			const Vec3 invDir(
				std::abs(direction.x) > 1e-6f ? 1.0f / direction.x : FLT_MAX,
				std::abs(direction.y) > 1e-6f ? 1.0f / direction.y : FLT_MAX,
				std::abs(direction.z) > 1e-6f ? 1.0f / direction.z : FLT_MAX
			);

			outRay = {
				.origin = nearWorld,
				.dir    = direction,
				.invDir = invDir,
				.tMin   = 0.0f,
				.tMax   = FLT_MAX,
			};
			return true;
		}

		bool RayIntersectsUiPlane(
			const Ray&  ray,
			const Vec3& center,
			const Vec3& axisRight,
			const Vec3& axisUp,
			const Vec2& worldSize,
			float&      outDistance,
			Vec2&       outLocalWorld
		) {
			Vec3 right = axisRight;
			Vec3 up    = axisUp;
			if (right.SqrLength() < 1e-8f || up.SqrLength() < 1e-8f) {
				return false;
			}
			right = right.Normalized();
			up    = up.Normalized();
			Vec3 normal = right.Cross(up);
			if (normal.SqrLength() < 1e-8f) {
				return false;
			}
			normal = normal.Normalized();

			const float denom = ray.dir.Dot(normal);
			if (std::abs(denom) <= 1e-6f) {
				return false;
			}

			const float t = (center - ray.origin).Dot(normal) / denom;
			if (t < 0.0f || t > ray.tMax) {
				return false;
			}

			const Vec3 hitPoint = ray.origin + ray.dir * t;
			const Vec3 localVec = hitPoint - center;
			const float localX = localVec.Dot(right);
			const float localY = localVec.Dot(up);
			const float halfW = worldSize.x * 0.5f;
			const float halfH = worldSize.y * 0.5f;
			if (
				localX < -halfW || localX > halfW ||
				localY < -halfH || localY > halfH
			) {
				return false;
			}

			outDistance   = t;
			outLocalWorld = Vec2(localX, localY);
			return true;
		}

		Vec2 WorldToUiPixels(
			const Vec2& localWorld,
			const Vec2& worldSize,
			const Vec2& pixelSize
		) {
			const float u = (localWorld.x / worldSize.x) + 0.5f;
			const float v = 0.5f - (localWorld.y / worldSize.y);
			return Vec2(u * pixelSize.x, v * pixelSize.y);
		}

		Vec3 UiPixelsToWorldOffset(
			const Gui::Rect& rect,
			const Vec2& pixelSize,
			const Vec2& worldSize
		) {
			const float centerX = rect.x + rect.width * 0.5f;
			const float centerY = rect.y + rect.height * 0.5f;
			const float worldX = (centerX / pixelSize.x - 0.5f) * worldSize.x;
			const float worldY = (0.5f - centerY / pixelSize.y) * worldSize.y;
			return Vec3(worldX, worldY, 0.0f);
		}

		Vec2 UiPixelsToWorldSize(const Gui::Rect& rect, const Vec2& pixelSize,
		                         const Vec2& worldSize) {
			return Vec2(
				rect.width / pixelSize.x * worldSize.x,
				rect.height / pixelSize.y * worldSize.y
			);
		}

		bool RayHitsSceneGeometryBefore(
			Scene&                    scene,
			AssetManager&             assetManager,
			const Ray&                ray,
			const float               maxDistance,
			const uint64_t            ignoredEntityGuid
		) {
			if (maxDistance <= 0.0f) {
				return false;
			}

			Ray limitedRay = ray;
			limitedRay.tMax = maxDistance;

			float nearestHit = FLT_MAX;
			for (const auto& entityPtr : scene.GetEntities()) {
				Entity* entity = entityPtr.get();
				if (
					!entity ||
					!entity->IsActive() ||
					!entity->IsVisible() ||
					entity->IsEditorOnly() ||
					entity->GetGuid() == ignoredEntityGuid
				) {
					continue;
				}

				auto* transform = entity->GetComponent<TransformComponent>();
				if (!transform) {
					continue;
				}

				AssetID meshAssetId = kInvalidAssetID;
				if (
					auto* staticMesh = entity->GetComponent<
						StaticMeshRendererComponent>();
					staticMesh && staticMesh->IsActive()
				) {
					meshAssetId = staticMesh->ResolveMeshAsset(assetManager);
				} else if (
					auto* skeletalMesh = entity->GetComponent<
						SkeletalMeshRendererComponent>();
					skeletalMesh && skeletalMesh->IsActive()
				) {
					meshAssetId = skeletalMesh->ResolveMeshAsset(assetManager);
				}

				if (meshAssetId == kInvalidAssetID) {
					continue;
				}

				const auto* meshAsset = assetManager.Get<MeshAssetData>(meshAssetId);
				if (!meshAsset || meshAsset->indices.size() < 3) {
					continue;
				}

				AABB localAabb = {};
				localAabb.min = meshAsset->localBoundsMin;
				localAabb.max = meshAsset->localBoundsMax;
				const AABB worldAabb = TransformAABB(
					localAabb,
					transform->WorldMat()
				);
				float aabbDistance = limitedRay.tMax;
				if (!Physics::RayVsAABB(limitedRay, worldAabb, aabbDistance)) {
					continue;
				}

				const Mat4& world = transform->WorldMat();
				for (size_t i = 0; i + 2 < meshAsset->indices.size(); i += 3) {
					const uint32_t i0 = meshAsset->indices[i + 0];
					const uint32_t i1 = meshAsset->indices[i + 1];
					const uint32_t i2 = meshAsset->indices[i + 2];
					if (
						i0 >= meshAsset->vertices.size() ||
						i1 >= meshAsset->vertices.size() ||
						i2 >= meshAsset->vertices.size()
					) {
						continue;
					}

					Triangle tri = {};
					tri.v0 = world.TransformPoint(meshAsset->vertices[i0].position);
					tri.v1 = world.TransformPoint(meshAsset->vertices[i1].position);
					tri.v2 = world.TransformPoint(meshAsset->vertices[i2].position);

					float triangleDistance = limitedRay.tMax;
					Vec3  normal           = Vec3::zero;
					if (
						Physics::TriangleVsRay(
							tri,
							limitedRay,
							triangleDistance,
							normal
						) &&
						triangleDistance > 0.0f &&
						triangleDistance < nearestHit
					) {
						nearestHit = triangleDistance;
					}
				}
			}

			return nearestHit < maxDistance - 1e-4f;
		}

		Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandRect& rect,
			const int32_t                 sortKey
		) {
			Render::ScreenSpriteInput sprite = {};
			sprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			sprite.texture.textureAssetId = kInvalidAssetID;
			sprite.positionPx     = Vec2(rect.rect.x, rect.rect.y);
			sprite.sizePx         = Vec2(rect.rect.width, rect.rect.height);
			sprite.anchor         = Vec2(0.0f, 0.0f);
			sprite.rotationRad    = 0.0f;
			sprite.color          = ToVec4(rect.fillColor);
			sprite.sortKey        = sortKey;
			return sprite;
		}
	}

	World::~World() = default;

	void World::Initialize() {
		mCameraManager.SetWorld(this);
		if (!mScene) {
			mScene = std::make_unique<Scene>();
		}
		mScene->SetWorld(this);
		for (const auto& entity : mScene->GetEntities()) {
			if (!entity) {
				continue;
			}
			entity->SetScene(mScene.get());
		}
	}

	void World::Shutdown() {
		UnloadScene();
	}

	void World::Tick(const float unscaledDeltaTime, const float deltaTime) {
		// まずは削除予定のエンティティを削除
		mScene->ProcessPendingEntityDestruction();

		mTime.deltaTime         = deltaTime;
		mTime.unscaledDeltaTime = unscaledDeltaTime;
		mTime.timeSeconds       += deltaTime;

		if (!mScene) {
			return;
		}

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) {
				continue;
			}
			entity->PrePhysicsTick(deltaTime);
			entity->Tick(deltaTime);
			entity->PostPhysicsTick(deltaTime);
		}
	}

	void World::EditorTick(const float unscaledDeltaTime) {
		// まずは削除予定のエンティティを削除
		mScene->ProcessPendingEntityDestruction();

		mTime.unscaledDeltaTime = unscaledDeltaTime;

		if (!mScene) {
			return;
		}

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) {
				continue;
			}
			entity->OnEditorTick(unscaledDeltaTime);
		}
	}

	bool World::LoadSceneFromFile(const char* path) {
		if (!path || std::string_view(path).empty()) {
			return false;
		}

		auto       newScene = std::make_unique<Scene>();
		const bool ok       = SceneSerializer::LoadFromFile(
			*newScene, path, mGuidGenerator
		);
		if (!ok) {
			return false;
		}

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

	bool World::SaveSceneToFile(const char* path) const {
		if (!mScene || !path || std::string_view(path).empty()) {
			return false;
		}
		return SceneSerializer::SaveToFile(*mScene, path);
	}

	void World::UnloadScene() {
		if (!mScene) {
			return;
		}

		OnSceneUnloaded();
		mCameraManager.ClearCurrentCamera();
		mScene.reset();
		mLoadedScenePath.clear();
	}

	void World::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		frameContext.Reset();
		inputs.views.clear();

		if (!mScene) {
			return;
		}

		Render::RenderViewInput sceneView = {};
		sceneView.viewKey = "world.main";
		sceneView.type    = Render::RENDER_VIEW_TYPE::SCENE;
		sceneView.output.sizeMode = Render::RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER;
		sceneView.output.presentToSwapChain = true;
		sceneView.output.clearSwapChainWhenNotPresenting = false;
		sceneView.sceneViewMode.mode = Render::SCENE_RENDER_MODE::FIT_VIEWPORT;

		std::set<std::pair<uint64_t, uint64_t>> emittedPortalPairs;
		std::vector<UiCanvasRuntimeEntry>       uiCanvasEntries;
		uiCanvasEntries.reserve(mScene->GetEntities().size());
		InputSystem* inputSystem = ServiceLocator::Get<InputSystem>();
		static bool  sTextWarningLogged = false;
		const Vec2 aspectViewportSize = inputSystem ?
			                                inputSystem->GetMouseClientViewportSize() :
			                                Vec2::zero;
		const float runtimeAspect = aspectViewportSize.y > 0.0f ?
			                            aspectViewportSize.x / aspectViewportSize.y :
			                            16.0f / 9.0f;
		mCameraManager.ResolveForRender(runtimeAspect, sceneView.camera);

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) {
				continue;
			}

			if (!sceneView.camera.valid) {
				const auto* editorCamera = entity->GetComponent<
					EditorCameraComponent>();
				if (editorCamera && editorCamera->IsActive()) {
					editorCamera->BuildCameraInput(sceneView.camera);
				}
			}

			if (entity->IsEditorOnly() || !entity->IsVisible()) {
				continue;
			}

			auto*       transform = entity->GetComponent<TransformComponent>();
			const auto* portal = entity->GetComponent<PortalComponent>();
			auto*       meshRenderer = entity->GetComponent<
				StaticMeshRendererComponent>();
			auto* skelRenderer = entity->GetComponent<
				SkeletalMeshRendererComponent>();
			auto* uiCanvas = entity->GetComponent<UiCanvasComponent>();
			if (!transform) {
				continue;
			}

			if (uiCanvas && uiCanvas->IsActive()) {
				UiCanvasRuntimeEntry entry = {};
				entry.entity    = entity.get();
				entry.transform = transform;
				entry.canvas    = uiCanvas;
				uiCanvasEntries.emplace_back(entry);
			}

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
					sceneView.visibleObjects.emplace_back(object);
				}
			}

			if (skelRenderer && skelRenderer->IsActive()) {
				const AssetID meshAssetId = skelRenderer->ResolveMeshAsset(
					assetManager
				);
				if (meshAssetId == kInvalidAssetID) {
					continue;
				}

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
							sceneView.skinningPalettes.size()
						);
						sceneView.skinningPalettes.emplace_back(
							std::move(palette)
						);
						frameContext.skinningPaletteIndexByMesh.emplace(
							meshAssetId, paletteIndex
						);
						object.skeletonPaletteId = paletteIndex;
					}
				}

				sceneView.visibleObjects.emplace_back(object);
			}

			if (
				portal && portal->IsActive() && portal->IsEnabled() &&
				portal->GetLinkedPortalGuid() != 0
			) {
				if (portal->GetLinkedPortalGuid() == entity->GetGuid()) {
					continue;
				}

				Entity* linkedEntity = mScene->FindEntity(
					portal->GetLinkedPortalGuid()
				);
				if (!linkedEntity || !linkedEntity->IsActive()) {
					continue;
				}

				const auto* linkedPortal = linkedEntity->GetComponent<
					PortalComponent>();
				const auto* linkedTransform = linkedEntity->GetComponent<
					TransformComponent>();
				if (!linkedPortal || !linkedTransform || !linkedPortal->
				    IsActive() ||
				    !linkedPortal->IsEnabled()) {
					continue;
				}
				if (linkedPortal->GetLinkedPortalGuid() != entity->GetGuid()) {
					continue;
				}

				const auto pairKey = std::minmax(
					entity->GetGuid(), linkedEntity->GetGuid()
				);
				if (!emittedPortalPairs.emplace(pairKey).second) {
					continue;
				}

				Render::PortalPairInput portalPair = {};
				portalPair.enabled = true;
				portalPair.fromPortalGuid = entity->GetGuid();
				portalPair.toPortalGuid = linkedEntity->GetGuid();
				portalPair.fromPortalWorld = transform->WorldMat();
				portalPair.toPortalWorld = linkedTransform->WorldMat();
				portalPair.fromPortalHalfExtents = portal->GetHalfExtents();
				portalPair.toPortalHalfExtents = linkedPortal->GetHalfExtents();
				sceneView.portalPairs.emplace_back(portalPair);

				std::swap(portalPair.fromPortalGuid, portalPair.toPortalGuid);
				std::swap(portalPair.fromPortalWorld, portalPair.toPortalWorld);
				std::swap(
					portalPair.fromPortalHalfExtents,
					portalPair.toPortalHalfExtents
				);
				sceneView.portalPairs.emplace_back(portalPair);
			}
		}

		uint32_t uiCanvasViewCounter = 0;
		for (UiCanvasRuntimeEntry& entry : uiCanvasEntries) {
			if (!entry.entity || !entry.transform || !entry.canvas) {
				continue;
			}
			if (!entry.canvas->EnsureRuntimeLoaded()) {
				continue;
			}

			Gui::UiRoot* runtimeRoot = entry.canvas->GetRuntimeRoot();
			if (!runtimeRoot) {
				continue;
			}

			const Vec2 pixelSize = entry.canvas->GetPixelSize();
			Vec2 runtimeCanvasSize = pixelSize;
			if (
				entry.canvas->GetSpaceMode() == UI_CANVAS_SPACE_MODE::SCREEN &&
				inputSystem
			) {
				const Vec2 viewportSize = inputSystem->GetMouseClientViewportSize();
				if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
					runtimeCanvasSize = viewportSize;
				}
			}
			runtimeRoot->SetRootSize(runtimeCanvasSize.x, runtimeCanvasSize.y);
			entry.canvas->TickRuntime(mTime.deltaTime);
			runtimeRoot->UpdateLayout();

			const bool canProcessInput =
				entry.canvas->GetReceiveInput() && inputSystem != nullptr;
			if (canProcessInput) {
				const Vec2 mousePos = inputSystem->GetMouseClientPosition();
				const Vec2 viewportSize = inputSystem->GetMouseClientViewportSize();
				const bool leftDown = inputSystem->IsMouseButtonDown(VM_1);
				const bool leftPressed = inputSystem->WasMouseButtonPressed(VM_1);
				const bool leftReleased = inputSystem->WasMouseButtonReleased(VM_1);

				if (entry.canvas->GetSpaceMode() == UI_CANVAS_SPACE_MODE::SCREEN) {
					runtimeRoot->ProcessMouse(
						mousePos.x,
						mousePos.y,
						leftDown,
						leftPressed,
						leftReleased
					);
				} else {
					Render::RenderCameraInput inputCamera = sceneView.camera;
					if (!inputCamera.valid) {
						Render::RenderCameraInput fallbackCamera = {};
						if (
							BuildUiInputCamera(fallbackCamera) &&
							fallbackCamera.valid
						) {
							inputCamera = fallbackCamera;
						}
					}

					bool processed = false;
					Ray  ray       = {};
					if (BuildRayFromMouse(inputCamera, mousePos, viewportSize, ray)) {
						Vec3 axisRight = Vec3::right;
						Vec3 axisUp    = Vec3::up;
						Mat4 transformWorld = entry.transform->WorldMat();
						Vec3 center         = transformWorld.GetTranslate();

						if (
							entry.canvas->GetSpaceMode() ==
							UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD
						) {
							const Mat4 cameraWorld = sceneView.camera.view.Inverse();
							axisRight = cameraWorld.GetRight().Normalized();
							axisUp    = cameraWorld.GetUp().Normalized();
						} else {
							axisRight = entry.transform->WorldMat().GetRight().
								Normalized();
							axisUp = entry.transform->WorldMat().GetUp().Normalized();
						}

						float hitDistance = 0.0f;
						Vec2  localWorld  = Vec2::zero;
						if (
							RayIntersectsUiPlane(
								ray,
								center,
								axisRight,
								axisUp,
								entry.canvas->GetWorldSize(),
								hitDistance,
								localWorld
							) &&
							!RayHitsSceneGeometryBefore(
								*mScene,
								assetManager,
								ray,
								hitDistance,
								entry.entity->GetGuid()
							)
						) {
							const Vec2 localPixels = WorldToUiPixels(
								localWorld,
								entry.canvas->GetWorldSize(),
								pixelSize
							);
							runtimeRoot->ProcessMouse(
								localPixels.x,
								localPixels.y,
								leftDown,
								leftPressed,
								leftReleased
							);
							processed = true;
						}
					}
					if (!processed) {
						runtimeRoot->ProcessMouse(
							-FLT_MAX,
							-FLT_MAX,
							leftDown,
							leftPressed,
							leftReleased
						);
					}
				}
			}

			std::vector<Gui::UiDrawCommand> drawCommands;
			runtimeRoot->BuildDrawCommands(drawCommands);

			const Vec2 worldSize = entry.canvas->GetWorldSize();
			const int32_t canvasSort = entry.canvas->GetSortKey();
			const bool worldSpaceCanvas =
				entry.canvas->GetSpaceMode() != UI_CANVAS_SPACE_MODE::SCREEN;

			Render::RenderViewInput canvasSpriteView = {};
			std::string             canvasViewKey;
			if (worldSpaceCanvas) {
				canvasViewKey = std::string("ui.canvas.") +
					std::to_string(entry.entity->GetGuid()) + "." +
					std::to_string(uiCanvasViewCounter++);
				canvasSpriteView.viewKey = canvasViewKey;
				canvasSpriteView.type = Render::RENDER_VIEW_TYPE::SPRITE_ONLY;
				canvasSpriteView.output.sizeMode = Render::RENDER_VIEW_SIZE_MODE::FIXED;
				canvasSpriteView.output.width = static_cast<uint32_t>(std::max(
					1.0f, pixelSize.x
				));
				canvasSpriteView.output.height = static_cast<uint32_t>(std::max(
					1.0f, pixelSize.y
				));
			}

			for (size_t i = 0; i < drawCommands.size(); ++i) {
				const auto& draw = drawCommands[i];
				const int32_t sort = canvasSort * 10000 + static_cast<int32_t>(i);

				if (draw.type == Gui::UiDrawCommandType::RECT) {
					if (
						entry.canvas->GetSpaceMode() ==
						UI_CANVAS_SPACE_MODE::SCREEN
					) {
						sceneView.screenSprites.emplace_back(
							BuildScreenSprite(draw.rect, sort)
						);
					} else {
						canvasSpriteView.screenSprites.emplace_back(
							BuildScreenSprite(draw.rect, sort)
						);
					}
				} else if (!sTextWarningLogged) {
					Warning(
						kChannel,
						"UiDrawCommand TEXT is not supported in runtime pass yet (Phase2)."
					);
					sTextWarningLogged = true;
				}
			}

			if (!worldSpaceCanvas) {
				continue;
			}

			inputs.views.emplace_back(std::move(canvasSpriteView));

			Mat4 transformWorld = entry.transform->WorldMat();
			const Vec3 center   = transformWorld.GetTranslate();
			if (
				entry.canvas->GetSpaceMode() ==
				UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD
			) {
				Render::WorldBillboardInput billboard = {};
				billboard.texture.source = Render::SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT;
				billboard.texture.viewKey = canvasViewKey;
				billboard.worldPosition = center;
				billboard.sizeWorld     = worldSize;
				billboard.color         = Vec4::one;
				billboard.rotationRad   = 0.0f;
				billboard.sortKey       = canvasSort;
				billboard.uvFlipY       = true;
				sceneView.worldBillboards.emplace_back(billboard);
			} else {
				Render::WorldSpriteInput sprite = {};
				sprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT;
				sprite.texture.viewKey = canvasViewKey;
				sprite.worldPosition = center;
				sprite.worldRight = transformWorld.GetRight().Normalized();
				sprite.worldUp    = transformWorld.GetUp().Normalized();
				sprite.sizeWorld  = worldSize;
				sprite.color      = Vec4::one;
				sprite.rotationRad = 0.0f;
				sprite.sortKey     = canvasSort;
				sprite.uvFlipY     = true;
				sceneView.worldSprites.emplace_back(sprite);
			}
		}

		inputs.views.emplace_back(std::move(sceneView));
	}
	bool World::IsGameSimulationEnabled() const noexcept {
		return true;
	}

	std::string_view World::GetLoadedScenePath() const {
		return mLoadedScenePath;
	}

	void World::SetLoadedScenePath(std::string path) {
		mLoadedScenePath = std::move(path);
	}

	void World::SetScene(std::unique_ptr<Scene> scene) {
		mCameraManager.SetWorld(this);
		if (scene) {
			scene->SetWorld(this);
			for (const auto& entity : scene->GetEntities()) {
				if (!entity) {
					continue;
				}
				entity->SetScene(scene.get());
			}
		}
		mScene = std::move(scene);
		mCameraManager.ClearCurrentCamera();
	}

	bool World::BuildUiInputCamera(Render::RenderCameraInput& outCamera) const {
		outCamera = {};
		return false;
	}

	void World::OnSceneLoaded() {}
	void World::OnSceneUnloaded() {}
}
