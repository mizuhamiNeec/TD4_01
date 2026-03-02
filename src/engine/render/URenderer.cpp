#include "URenderer.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

#include "RenderDevice.h"
#include "core/math/Mat4.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec4.h"
#include "frame/RenderFrameInputs.h"

#include "engine/profiler/UProfiler.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/interface/IRhiDevice.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/unnamed/uprimitive/UPrimitives.h"

namespace Unnamed::Render {
	namespace {
		Vec3 TransformPoint(const Mat4& transform, const Vec3& point) {
			const Vec4 transformed = transform * Vec4(
				point.x, point.y, point.z, 1.0f
			);
			const float w = std::abs(transformed.w) > 1e-6f ?
				                transformed.w :
				                1.0f;
			return Vec3(
				transformed.x / w, transformed.y / w, transformed.z / w
			);
		}

		Mat4 ToRigidTransform(const Mat4& world) {
			Mat4 copy = world;
			return Mat4::Affine(
				Vec3::one,
				copy.ToQuaternion(),
				copy.GetTranslate()
			);
		}

		bool IsPortalFrontFacing(const Mat4& portalWorld, const Vec3& cameraPos) {
			Mat4 rigidWorld = ToRigidTransform(portalWorld);
			Vec3 portalPos = rigidWorld.GetTranslate();
			Vec3 portalNormal = rigidWorld.GetForward().Normalized();
			Vec3 toCamera = (cameraPos - portalPos).Normalized();
			return portalNormal.Dot(toCamera) > 0.0f;
		}

		Mat4 BuildOffCenterPerspectiveD3D(
			const float               left,
			const float               right,
			const float               bottom,
			const float               top,
			const float               nearClip,
			const float               farClip,
			const ProjectionDepthMode depthMode
		) {
			Mat4 result = Mat4::zero;

			const float width  = std::max(right - left, 1e-6f);
			const float height = std::max(top - bottom, 1e-6f);
			const float nearZ  = std::max(nearClip, 0.001f);
			const float farZ   = farClip > nearZ ? farClip : nearZ + 1.0f;
			const float dist   = farZ - nearZ;

			result.m[0][0] = 2.0f * nearZ / width;
			result.m[1][1] = 2.0f * nearZ / height;
			result.m[2][0] = (left + right) / (left - right);
			result.m[2][1] = (top + bottom) / (bottom - top);
			result.m[2][3] = 1.0f;

			if (depthMode == ProjectionDepthMode::ReverseZ) {
				result.m[2][2] = nearZ / (nearZ - farZ);
				result.m[3][2] = nearZ * farZ / dist;
			} else {
				result.m[2][2] = farZ / dist;
				result.m[3][2] = -(nearZ * farZ) / dist;
			}

			return result;
		}

		bool BuildPortalProjection(
			const Mat4&            portalView,
			const PortalPairInput& portalPair,
			const float            nearClip,
			const float            farClip,
			const ProjectionDepthMode depthMode,
			Mat4&                  outProj
		) {
			Mat4 targetPortalWorld = ToRigidTransform(
				portalPair.toPortalWorld
			);
			const Vec3 center = targetPortalWorld.GetTranslate();
			const Vec3 right = targetPortalWorld.GetRight().Normalized();
			const Vec3 up = targetPortalWorld.GetUp().Normalized();
			const float halfWidth = std::max(
				portalPair.toPortalHalfExtents.x, 1e-4f
			);
			const float halfHeight = std::max(
				portalPair.toPortalHalfExtents.y, 1e-4f
			);

			const std::array<Vec3, 4> worldCorners = {
				center - right * halfWidth - up * halfHeight,
				center + right * halfWidth - up * halfHeight,
				center - right * halfWidth + up * halfHeight,
				center + right * halfWidth + up * halfHeight,
			};

			float left   = std::numeric_limits<float>::max();
			float rightN = std::numeric_limits<float>::lowest();
			float bottom = std::numeric_limits<float>::max();
			float top    = std::numeric_limits<float>::lowest();

			for (const Vec3& worldCorner : worldCorners) {
				const Vec3 viewCorner = TransformPoint(portalView, worldCorner);
				if (viewCorner.z <= 1e-4f) { return false; }

				const float projectedX = nearClip * viewCorner.x / viewCorner.z;
				const float projectedY = nearClip * viewCorner.y / viewCorner.z;
				left                   = std::min(left, projectedX);
				rightN                 = std::max(rightN, projectedX);
				bottom                 = std::min(bottom, projectedY);
				top                    = std::max(top, projectedY);
			}

			if (rightN - left <= 1e-6f || top - bottom <= 1e-6f) {
				return false;
			}

			outProj = BuildOffCenterPerspectiveD3D(
				left, rightN, bottom, top, nearClip, farClip, depthMode
			);
			return true;
		}

		Vec4 BuildPortalClipPlane(
			const Mat4& portalView, const PortalPairInput& portalPair
		) {
			Mat4 targetPortalWorld = ToRigidTransform(
				portalPair.toPortalWorld
			);
			const Vec3 center = targetPortalWorld.GetTranslate();
			Vec3       normal = targetPortalWorld.GetForward().Normalized();

			constexpr float sampleOffset = 0.01f;
			const float centerDepth = TransformPoint(portalView, center).z;
			const float forwardDepth =
				TransformPoint(portalView, center + normal * sampleOffset).z;
			if (forwardDepth < centerDepth) { normal = -normal; }

			const Vec3 planePoint = center + normal * sampleOffset;
			return Vec4(
				normal.x,
				normal.y,
				normal.z,
				-normal.Dot(planePoint)
			);
		}
	}

	SceneOutputView URenderer::GetSceneOutputView(
		const RenderDevice& renderDevice
	) const {
		SceneOutputView view = {};
		view.textureId       = mSceneOutputTextureId;
		if (mSceneOutputTextureId == 0) { return view; }

		const auto& registry = const_cast<RenderDevice&>(renderDevice).
			GetRegistry();
		view.srvCpu      = registry.GetSrvCpu(mSceneOutputTextureId);
		view.srvRevision = registry.GetSrvRevision(mSceneOutputTextureId);
		return view;
	}

	void URenderer::RenderFrame(
		RenderDevice& renderDevice, const RenderFrameInputs& inputs
	) {
		UProfiler*                  profiler = ServiceLocator::Get<UProfiler>();
		auto&                       rhi = renderDevice.GetRhiDevice();
		auto&                       dx = static_cast<Rhi::D3D12Device&>(rhi);
		const auto&                 swapChain = rhi.GetSwapChain();
		const uint32_t              backBufferWidth = swapChain.GetWidth();
		const uint32_t              backBufferHeight = swapChain.GetHeight();
		std::unordered_set<AssetID> dirtyMeshAssets;
		bool                        materialsDirty = false;
		bool                        postFxDirty    = false;
		renderDevice.ConsumeDirtyAssets(
			dirtyMeshAssets, materialsDirty, postFxDirty
		);
		if (!dirtyMeshAssets.empty()) {
			for (const AssetID dirtyMesh : dirtyMeshAssets) {
				mSceneMeshesByAsset.erase(dirtyMesh);
				if (mLoadedMeshAsset == dirtyMesh) {
					mLoadedMeshAsset = kInvalidAssetID;
				}
			}
		}
		if (materialsDirty) { LoadMaterialResources(renderDevice, dx); }
		if (postFxDirty) {
			LoadPostFxChain(renderDevice);
			mGraph.Reset();
			mGraphBuilt = false;
		}

		constexpr SceneRenderRequest defaultSceneRequest        = {};
		const bool                   hasInputSceneRenderRequest =
			inputs.sceneRenderRequest.viewportPanelWidth != 0 ||
			inputs.sceneRenderRequest.viewportPanelHeight != 0 ||
			inputs.sceneRenderRequest.mode != defaultSceneRequest.mode ||
			inputs.sceneRenderRequest.presentToSwapChain !=
			defaultSceneRequest.presentToSwapChain ||
			inputs.sceneRenderRequest.clearSwapChainWhenNotPresenting !=
			defaultSceneRequest.clearSwapChainWhenNotPresenting;
		if (!mHasExternalSceneRenderRequest && hasInputSceneRenderRequest) {
			mSceneRenderRequest = inputs.sceneRenderRequest;
		}
		mHasExternalSceneRenderRequest = false;

		const bool presentChanged =
			mPresentSceneToSwapChain != mSceneRenderRequest.presentToSwapChain
			||
			mClearSwapChainWhenNotPresenting !=
			mSceneRenderRequest.clearSwapChainWhenNotPresenting;
		if (presentChanged) {
			mPendingSceneRenderWidth        = 0;
			mPendingSceneRenderHeight       = 0;
			mPendingSceneExtentStableFrames = 0;
		}

		const auto [resolvedWidth, resolvedHeight] = ResolveSceneRenderExtent(
			backBufferWidth, backBufferHeight, mSceneRenderRequest
		);
		if (!mGraphBuilt) {
			mPresentSceneToSwapChain =
				mSceneRenderRequest.presentToSwapChain;
			mClearSwapChainWhenNotPresenting =
				mSceneRenderRequest.clearSwapChainWhenNotPresenting;
			mSceneRenderWidth  = resolvedWidth;
			mSceneRenderHeight = resolvedHeight;
			DevMsg(
				"URenderer",
				"Initial scene extent: {}x{}",
				mSceneRenderWidth,
				mSceneRenderHeight
			);
			BuildGraph(renderDevice);
		} else if (
			resolvedWidth != mSceneRenderWidth ||
			resolvedHeight != mSceneRenderHeight
		) {
			const bool sameAsPending =
				mPendingSceneRenderWidth == resolvedWidth &&
				mPendingSceneRenderHeight == resolvedHeight;
			if (!sameAsPending) {
				mPendingSceneRenderWidth        = resolvedWidth;
				mPendingSceneRenderHeight       = resolvedHeight;
				mPendingSceneExtentStableFrames = 1;
			} else { ++mPendingSceneExtentStableFrames; }

			if (
				mPendingSceneExtentStableFrames >=
				kSceneExtentConfirmFrames
			) {
				mSceneRenderWidth  = resolvedWidth;
				mSceneRenderHeight = resolvedHeight;
				DevMsg(
					"URenderer",
					"Scene extent changed: {}x{}",
					mSceneRenderWidth,
					mSceneRenderHeight
				);
				renderDevice.GetRegistry().OnResize(
					mSceneRenderWidth,
					mSceneRenderHeight,
					swapChain.GetCurrentBackBufferIndex()
				);
				// テクスチャ再生成で実リソース状態が初期化されるため、RDGの状態追跡を更新する
				mGraph.Invalidate();
				mAdvancedFoundation.OnResize(resolvedWidth, resolvedHeight);
				mPendingSceneExtentStableFrames = 0;
			}
		} else {
			mPendingSceneRenderWidth        = 0;
			mPendingSceneRenderHeight       = 0;
			mPendingSceneExtentStableFrames = 0;
		}

		if (mGraphBuilt && presentChanged) {
			mPresentSceneToSwapChain =
				mSceneRenderRequest.presentToSwapChain;
			mClearSwapChainWhenNotPresenting =
				mSceneRenderRequest.clearSwapChainWhenNotPresenting;
			mGraph.Reset();
			mGraphBuilt = false;
			BuildGraph(renderDevice);
		}

		mScreenSprites = inputs.screenSprites;
		std::sort(
			mScreenSprites.begin(),
			mScreenSprites.end(),
			[](const ScreenSpriteInput& a, const ScreenSpriteInput& b) {
				return a.sortKey < b.sortKey;
			}
		);
		if (!mScreenSprites.empty()) {
			EnsureSpriteFallbackTexture(renderDevice);
			for (const auto& sprite : mScreenSprites) {
				(void)EnsureSpriteTextureLoaded(renderDevice, sprite.textureAssetId);
			}
		}

		Mat4 currentView      = Mat4::identity;
		Mat4 currentProj      = Mat4::identity;
		Vec3 currentCameraPos = Vec3::zero;
		Mat4 viewProjection   = Mat4::identity;
		{
			UProfiler::ScopeTimer scope(profiler, "Render.ResolveView");
			const float           aspect = mSceneRenderHeight > 0 ?
				                               static_cast<float>(
					                               mSceneRenderWidth) /
				                               static_cast<float>(
					                               mSceneRenderHeight) :
				                               16.0f / 9.0f;

			const Mat4 fallbackView = Mat4::identity;
			const Mat4 fallbackProj = Mat4::PerspectiveFovD3D(
				90.0f * Math::deg2Rad,
				aspect,
				0.001f,
				10000.0f,
				ProjectionDepthMode::ReverseZ
			);
			currentView = inputs.camera.valid ?
				              inputs.camera.view :
				              fallbackView;
			currentProj = inputs.camera.valid ?
				              inputs.camera.proj :
				              fallbackProj;
			currentCameraPos =
				inputs.camera.valid ? inputs.camera.cameraPos : Vec3::zero;
			viewProjection = currentView * currentProj;
			mActivePortalViews.clear();
			mPortalEnabled = false;

			struct PairSelection {
				bool valid = false;
				float bestDistanceSq = std::numeric_limits<float>::max();
				PortalPairInput forward = {};
				PortalPairInput backward = {};
				bool hasForward = false;
				bool hasBackward = false;
			};
			std::map<std::pair<uint64_t, uint64_t>, PairSelection>
				pairSelections;
			for (const auto& portal : inputs.portalPairs) {
				if (!portal.enabled) { continue; }
				Mat4       portalWorld = portal.fromPortalWorld;
				const Vec3 portalPos   = portalWorld.GetTranslate();

				AABB portalBounds = {};
				portalBounds.Expand(
					Vec3(
						-portal.fromPortalHalfExtents.x,
						-portal.fromPortalHalfExtents.y,
						-0.05f
					)
				);
				portalBounds.Expand(
					Vec3(
						portal.fromPortalHalfExtents.x,
						portal.fromPortalHalfExtents.y,
						0.05f
					)
				);
				if (
					!IsVisiblePerspective(
						BuildBoundingSphere(
							TransformAABB(portalBounds, portal.fromPortalWorld)
						),
						currentView,
						currentProj,
						inputs.camera.nearZ,
						inputs.camera.farZ
					)
				) { continue; }

				const Vec3  delta      = portalPos - currentCameraPos;
				const float distanceSq = delta.Dot(delta);
				if (!IsPortalFrontFacing(portal.fromPortalWorld, currentCameraPos)) {
					continue;
				}
				const auto  pairKey    = std::minmax(
					portal.fromPortalGuid, portal.toPortalGuid
				);
				auto& selection          = pairSelections[pairKey];
				selection.valid          = true;
				selection.bestDistanceSq = std::min(
					selection.bestDistanceSq, distanceSq
				);
				if (portal.fromPortalGuid == pairKey.first) {
					selection.forward    = portal;
					selection.hasForward = true;
				} else {
					selection.backward    = portal;
					selection.hasBackward = true;
				}
			}

			PairSelection* activeSelection = nullptr;
			for (auto& [_, selection] : pairSelections) {
				if (!selection.valid) { continue; }
				if (
					!activeSelection ||
					selection.bestDistanceSq < activeSelection->bestDistanceSq
				) { activeSelection = &selection; }
			}
			if (activeSelection) {
				if (activeSelection->hasForward) {
					ActivePortalView active = {};
					active.pair             = activeSelection->forward;
					active.stencilRef       = mPortalPass.stencilRefBase;
					mActivePortalViews.emplace_back(active);
				}
				if (activeSelection->hasBackward) {
					ActivePortalView active = {};
					active.pair             = activeSelection->backward;
					active.stencilRef       = mPortalPass.stencilRefBase + 1;
					mActivePortalViews.emplace_back(active);
				}
			}

			for (uint32_t directionIndex = 0; directionIndex < kPortalDirections
			     ;
			     ++directionIndex) {
				for (uint32_t depth = 0; depth < kPortalRecursionDepth; ++
				     depth) {
					mPortalRecursionViews[directionIndex][depth] = {};
				}
			}
			for (uint32_t directionIndex = 0;
			     directionIndex < static_cast<uint32_t>(mActivePortalViews.
				     size()) &&
			     directionIndex < kPortalDirections;
			     ++directionIndex) {
				auto currentPair = mActivePortalViews[directionIndex].pair;
				currentPair.fromPortalWorld = ToRigidTransform(
					currentPair.fromPortalWorld
				);
				currentPair.toPortalWorld = ToRigidTransform(
					currentPair.toPortalWorld
				);
				Mat4 parentCameraWorld = currentView.Inverse();
				Mat4 currentCameraWorld =
					parentCameraWorld *
					currentPair.fromPortalWorld.Inverse() *
					Mat4::RotateY(Math::pi) *
					currentPair.toPortalWorld;

				for (uint32_t depth = 0; depth < kPortalRecursionDepth; ++
				     depth) {
					auto& recursiveView =
						mPortalRecursionViews[directionIndex][depth];
					recursiveView.pair        = currentPair;
					recursiveView.cameraWorld = currentCameraWorld;
					recursiveView.stencilRef  = mActivePortalViews[
							directionIndex].
						stencilRef;
					recursiveView.visibleFromPrevious =
						IsPortalFrontFacing(
							currentPair.fromPortalWorld,
							parentCameraWorld.GetTranslate()
						);

					const PortalPairInput nextPair = {
						.enabled               = currentPair.enabled,
						.fromPortalGuid        = currentPair.toPortalGuid,
						.toPortalGuid          = currentPair.fromPortalGuid,
						.fromPortalWorld       = ToRigidTransform(
							currentPair.toPortalWorld
						),
						.toPortalWorld         = ToRigidTransform(
							currentPair.fromPortalWorld
						),
						.fromPortalHalfExtents = currentPair.
						toPortalHalfExtents,
						.toPortalHalfExtents = currentPair.
						fromPortalHalfExtents,
					};
					parentCameraWorld = currentCameraWorld;
					currentCameraWorld =
						parentCameraWorld *
						nextPair.fromPortalWorld.Inverse() *
						Mat4::RotateY(Math::pi) *
						nextPair.toPortalWorld;
					currentPair = nextPair;
				}
			}
			mPortalEnabled = !mActivePortalViews.empty();
		}

		rhi.BeginFrame();
		mAdvancedFoundation.BeginFrame();
		renderDevice.GetRegistry().CollectGarbage(
			dx.GetCompletedFenceValue()
		);

		const uint32_t frameIndex = swapChain.GetCurrentBackBufferIndex();
		const uint32_t frameSlot  = frameIndex % swapChain.GetBufferCount();

		uint32_t allocatedObjectCount = 0;
		{
			UProfiler::ScopeTimer scope(profiler, "Render.BuildVisibleSet");

			Rhi::FrameConstants frame{};
			frame.view      = currentView;
			frame.proj      = currentProj;
			frame.viewProj  = viewProjection;
			frame.cameraPos = currentCameraPos;
			frame.time      = inputs.time;
			frame.portalClipPlane = Vec4::zero;
			frame.portalClipEnabled = 0.0f;
			mFrameCb.Write(frameSlot, frame);

			const uint32_t frameBase = frameSlot * kMaxDrawObjects;
			std::unordered_map<uint32_t, uint32_t> skinningCbByPalette;
			uint32_t skinningWriteCount = 1;

			{
				Rhi::SkinningPaletteConstants identityPalette = {};
				for (auto& bone : identityPalette.bones) {
					bone = Mat4::identity;
				}
				mSkinningCb.Write(frameBase, identityPalette);
			}

			for (uint32_t i = 0;
			     i < static_cast<uint32_t>(inputs.skinningPalettes.size()) &&
			     skinningWriteCount < kMaxDrawObjects;
			     ++i) {
				const auto& inPalette = inputs.skinningPalettes[i];
				if (inPalette.boneMatrices.empty()) { continue; }

				Rhi::SkinningPaletteConstants outPalette = {};
				for (auto& bone : outPalette.bones) { bone = Mat4::identity; }

				const uint32_t maxBones = std::min<uint32_t>(
					static_cast<uint32_t>(inPalette.boneMatrices.size()),
					Rhi::SkinningPaletteConstants::kMaxBones
				);
				for (uint32_t boneIndex = 0; boneIndex < maxBones; ++
				     boneIndex) {
					outPalette.bones[boneIndex] = inPalette.boneMatrices[
						boneIndex];
				}

				const uint32_t cbIndex = frameBase + skinningWriteCount;
				mSkinningCb.Write(cbIndex, outPalette);
				skinningCbByPalette.emplace(i, cbIndex);
				++skinningWriteCount;
			}

			mMainDrawList.clear();
			mPortalDrawList.clear();

			const MaterialBinding* fallbackMaterial = nullptr;
			if (const auto it = mMaterialBindings.find(mDefaultMaterialInstance)
				; it != mMaterialBindings.end()) {
				fallbackMaterial = &it->second;
			}

			const uint32_t portalObjectReserve = static_cast<uint32_t>(
				                                     mActivePortalViews.size()
			                                     ) * kPortalRecursionDepth;
			const auto addDraws = [&](
				const D3D12_VERTEX_BUFFER_VIEW& vbv,
				const D3D12_INDEX_BUFFER_VIEW&  ibv,
				const uint32_t                  indexCount,
				const AABB&                     localBounds,
				const uint32_t                  meshIndex,
				const Mat4&                     world,
				const uint64_t                  ownerEntityGuid,
				const bool                      isPortalSurface,
				const AssetID                   materialInstanceId,
				const bool                      isSkinned,
				const uint32_t                  skinningPaletteId
			) {
				if (allocatedObjectCount >= kMaxDrawObjects -
				    portalObjectReserve) { return; }
				const auto skinIt = skinningCbByPalette.find(skinningPaletteId);
				const bool hasSkinningPalette =
					isSkinned && skinIt != skinningCbByPalette.end();

				Rhi::ObjectConstants obj  = {};
				obj.world                 = world;
				obj.worldInverseTranspose = world.Inverse().Transpose();
				obj.skinningInfo          = Vec4(
					0.0f, hasSkinningPalette ? 1.0f : 0.0f, 0.0f,
					0.0f
				);

				const AABB worldAabb = TransformAABB(localBounds, obj.world);
				const Sphere worldBounds = BuildBoundingSphere(worldAabb);

				const uint32_t objectCbIndex = frameBase + allocatedObjectCount;
				mObjectCb.Write(objectCbIndex, obj);

				Rhi::MaterialConstants material        = {};
				uint32_t               textureId       = 0;
				const MaterialBinding* materialBinding = fallbackMaterial;
				if (const auto it = mMaterialBindings.find(materialInstanceId);
					it != mMaterialBindings.end()) {
					materialBinding = &it->second;
				}
				if (materialBinding) {
					material  = materialBinding->constants;
					textureId = materialBinding->albedoTextureId;
				}

				const uint32_t materialCbIndex = frameBase + allocatedObjectCount;
				mMaterialCb.Write(materialCbIndex, material);

				MeshDrawItem item{};
				item.vbv             = vbv;
				item.ibv             = ibv;
				item.indexCount      = indexCount;
				item.objectCbIndex   = objectCbIndex;
				item.materialCbIndex = materialCbIndex;
				item.albedoTextureId = textureId;
				item.skinningCbIndex = hasSkinningPalette ?
					                       skinIt->second :
					                       frameBase;
				item.useSkinning     = hasSkinningPalette ? 1u : 0u;
				item.ownerEntityGuid = ownerEntityGuid;
				item.isPortalSurface = isPortalSurface ? 1u : 0u;
				mPortalDrawList.emplace_back(item);

				if (
					IsVisiblePerspective(
						worldBounds,
						currentView,
						currentProj,
						inputs.camera.nearZ,
						inputs.camera.farZ
					)
				) {
					mMainDrawList.emplace_back(item);

					auto& rtState = mAdvancedFoundation.GetRtState();
					rtState.visibleInstances.emplace_back(
						RtInstanceDesc{
							.meshIndex    = meshIndex,
							.instanceMask = 0xFF,
							.world        = obj.world,
							.worldBounds  = worldAabb,
						}
					);
					rtState.needsTlasRebuild = true;
				}

				++allocatedObjectCount;
			};

			if (!inputs.visibleObjects.empty()) {
				for (const auto& obj : inputs.visibleObjects) {
					const MeshBuffer* mesh = nullptr;
					if (obj.meshAssetId != kInvalidAssetID &&
					    EnsureMeshResourceLoaded(
						    renderDevice, dx, obj.meshAssetId
					    )) {
						if (const auto it = mSceneMeshesByAsset.find(
							obj.meshAssetId
						); it != mSceneMeshesByAsset.end()) {
							mesh = &it->second;
						}
					}
					if (!mesh) { continue; }

					addDraws(
						mesh->vbv,
						mesh->ibv,
						mesh->indexCount,
						mesh->localAABB,
						0,
						obj.world,
						obj.ownerEntityGuid,
						obj.isPortalSurface,
						obj.materialInstanceId,
						obj.isSkinned,
						obj.skeletonPaletteId
					);
				}
			}

			uint32_t portalObjectCursor = frameBase + allocatedObjectCount;
			for (uint32_t directionIndex = 0;
			     directionIndex < static_cast<uint32_t>(mActivePortalViews.
				     size()) &&
			     directionIndex < kPortalDirections;
			     ++directionIndex) {
				mActivePortalViews[directionIndex].maskObjectCbIndex =
					portalObjectCursor;

				for (uint32_t depth = 0; depth < kPortalRecursionDepth; ++
				     depth) {
					auto& recursiveView =
						mPortalRecursionViews[directionIndex][depth];
					recursiveView.frameCbIndex =
						frameSlot * (kPortalDirections * kPortalRecursionDepth)
						+
						directionIndex * kPortalRecursionDepth +
						depth;
					recursiveView.compositeObjectCbIndex = portalObjectCursor++;

					Rhi::ObjectConstants portalObject = {};
					portalObject.world                = Mat4::Scale(
						Vec3(
							recursiveView.pair.
							              fromPortalHalfExtents.x,
							recursiveView.pair.
							              fromPortalHalfExtents.y,
							1.0f
						)
					) * ToRigidTransform(recursiveView.pair.fromPortalWorld);
					portalObject.worldInverseTranspose =
						portalObject.world.Inverse().Transpose();
					mObjectCb.Write(
						recursiveView.compositeObjectCbIndex, portalObject
					);

					const Mat4 portalView = recursiveView.cameraWorld.Inverse();
					const ProjectionDepthMode portalDepthMode =
						inputs.camera.depthMode ==
								PROJECTION_DEPTH_MODE::ReverseZ ?
							ProjectionDepthMode::ReverseZ :
							ProjectionDepthMode::ForwardZ;
					Rhi::FrameConstants portalFrame = {};
					portalFrame.view = portalView;
					portalFrame.proj = currentProj;
					portalFrame.portalClipPlane = Vec4::zero;
					portalFrame.portalClipEnabled = 0.0f;
					if (
						BuildPortalProjection(
							portalView,
							recursiveView.pair,
							inputs.camera.nearZ,
							inputs.camera.farZ,
							portalDepthMode,
							portalFrame.proj
						)
					) {
						portalFrame.portalClipPlane = BuildPortalClipPlane(
							portalView, recursiveView.pair
						);
						portalFrame.portalClipEnabled = 1.0f;
					}
					portalFrame.viewProj = portalView * portalFrame.proj;
					portalFrame.cameraPos = recursiveView.cameraWorld.
						GetTranslate();
					portalFrame.time = inputs.time;
					mPortalFrameCb.Write(
						recursiveView.frameCbIndex, portalFrame
					);
				}
			}

			mPortalPass.maskPassGeom.pso =
				renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
					mPortalPass.maskPassGeom.psoKey
				);
			mPortalPass.compositeGeom.pso =
				renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
					mPortalPass.compositeGeom.psoKey
				);
			mGeometryPass.pso =
				renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
					mGeometryPass.psoKey
				);
			mPortalPass.scenePso = renderDevice.GetPipelineCache().
			                                    GetOrCreateGraphicsPso(
				                                    mPortalPass.scenePsoKey
			                                    );
			mSpritePass.geom.pso =
				renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
					mSpritePass.geom.psoKey
				);

			BuildDrawBatches();
		}

		{
			UProfiler::ScopeTimer scope(profiler, "Render.GraphExecute");
			mGraph.Execute(rhi);
		}
		if (mUiPlatformRenderCallback) { mUiPlatformRenderCallback(); }

		rhi.EndFrame();
	}

	void URenderer::OnResize(const uint32_t width, const uint32_t height) {
		// メインウィンドウのリサイズでは SceneColor/Depth を必ずしも再生成しないため、
		// ここで状態追跡だけ初期化すると実リソース状態とズレて
		// RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH が発生する。
		// Scene描画解像度変更時の再生成経路(RenderFrame内)でのみ Invalidate する。
		mAdvancedFoundation.OnResize(width, height);
	}

	void URenderer::BuildDrawBatches() {
		DrawKey baseKey   = {};
		baseKey.pso       = mGeometryPass.pso;
		baseKey.rootSig   = mGeometryPass.rootSig;
		baseKey.rtvFormat = mGeometryPass.rtvFormat;
		baseKey.dsvFormat = mGeometryPass.dsvFormat;

		mMainBatches = BuildDrawBatchesFromItems(mMainDrawList, baseKey);
		mPortalBatches = BuildDrawBatchesFromItems(mPortalDrawList, baseKey);
	}

	void URenderer::SetUiCallbacks(
		UiMainRenderCallback     mainRenderCallback,
		UiPlatformRenderCallback platformRenderCallback
	) {
		mUiMainRenderCallback     = std::move(mainRenderCallback);
		mUiPlatformRenderCallback = std::move(platformRenderCallback);
	}

	void URenderer::SetSceneRenderRequest(const SceneRenderRequest& request) {
		const bool modeChanged =
			request.mode != mSceneRenderRequest.mode ||
			request.presentToSwapChain !=
			mSceneRenderRequest.presentToSwapChain ||
			request.clearSwapChainWhenNotPresenting !=
			mSceneRenderRequest.clearSwapChainWhenNotPresenting;
		if (modeChanged) {
			mPendingSceneRenderWidth        = 0;
			mPendingSceneRenderHeight       = 0;
			mPendingSceneExtentStableFrames = 0;
		}
		mSceneRenderRequest            = request;
		mHasExternalSceneRenderRequest = true;
	}

	std::pair<uint32_t, uint32_t> URenderer::ResolveSceneRenderExtent(
		const uint32_t            backBufferWidth,
		const uint32_t            backBufferHeight,
		const SceneRenderRequest& request
	) {
		uint32_t width  = backBufferWidth;
		uint32_t height = backBufferHeight;

		const uint32_t panelWidth = request.viewportPanelWidth != 0 ?
			                            request.viewportPanelWidth :
			                            std::max(1u, backBufferWidth);
		const uint32_t panelHeight = request.viewportPanelHeight != 0 ?
			                             request.viewportPanelHeight :
			                             std::max(1u, backBufferHeight);

		switch (request.mode) {
			case SCENE_RENDER_MODE::FIT_VIEWPORT: {
				width  = panelWidth;
				height = panelHeight;
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_16X9: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 9 > height * 16) { width = height * 16 / 9; } else {
					height = width * 9 / 16;
				}
				break;
			}
			case SCENE_RENDER_MODE::HD_720P: {
				width  = 1280;
				height = 720;
				break;
			}
			case SCENE_RENDER_MODE::FHD_1080P: {
				width  = 1920;
				height = 1080;
				break;
			}
			default: break;
		}

		width  = std::clamp(width, 2u, 8192u);
		height = std::clamp(height, 2u, 8192u);

		if ((width & 1u) != 0u) { --width; }
		if ((height & 1u) != 0u) { --height; }

		width  = std::max(2u, width);
		height = std::max(2u, height);

		return {width, height};
	}
}
