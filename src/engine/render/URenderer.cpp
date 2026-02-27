#include "URenderer.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "RenderDevice.h"
#include "core/math/Mat4.h"
#include "core/math/Math.h"
#include "frame/RenderFrameInputs.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/interface/IRhiDevice.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/uprimitive/UPrimitives.h"

namespace Unnamed::Render {
	void URenderer::RenderFrame(
		RenderDevice& renderDevice, const RenderFrameInputs& inputs
	) {
		auto&          rhi              = renderDevice.GetRhiDevice();
		const auto&    swapChain        = rhi.GetSwapChain();
		const uint32_t backBufferWidth  = swapChain.GetWidth();
		const uint32_t backBufferHeight = swapChain.GetHeight();

		if (inputs.sceneRenderRequest.viewportPanelWidth != 0 ||
		    inputs.sceneRenderRequest.viewportPanelHeight != 0) {
			mSceneRenderRequest = inputs.sceneRenderRequest;
		}

		const auto [resolvedWidth, resolvedHeight] = ResolveSceneRenderExtent(
			backBufferWidth, backBufferHeight, mSceneRenderRequest
		);
		if (!mGraphBuilt) {
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
			const uint32_t deltaW = resolvedWidth > mSceneRenderWidth ?
				                        resolvedWidth - mSceneRenderWidth :
				                        mSceneRenderWidth - resolvedWidth;
			const uint32_t deltaH = resolvedHeight > mSceneRenderHeight ?
				                        resolvedHeight - mSceneRenderHeight :
				                        mSceneRenderHeight - resolvedHeight;
			const bool dynamicMode =
				mSceneRenderRequest.mode == SCENE_RENDER_MODE::FIT_VIEWPORT ||
				mSceneRenderRequest.mode ==
				SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
			const bool tinyDelta = deltaW < 8u && deltaH < 8u;
			if (
				dynamicMode &&
				(!mSceneRenderRequest.preferRealtimeResize || tinyDelta)
			) {
				// 微小なサイズ変化や非リアルタイム設定時は再生成を抑制する
			} else {
				mSceneRenderWidth  = resolvedWidth;
				mSceneRenderHeight = resolvedHeight;
				DevMsg(
					"URenderer",
					"Scene extent changed: {}x{}",
					mSceneRenderWidth,
					mSceneRenderHeight
				);
				// 旧リソースをGPUが参照中のまま破棄しないように同期する
				rhi.OnResize(backBufferWidth, backBufferHeight);
				renderDevice.GetRegistry().OnResize(
					mSceneRenderWidth,
					mSceneRenderHeight,
					swapChain.GetCurrentBackBufferIndex()
				);
				// テクスチャ再生成で実リソース状態が初期化されるため、RDGの状態追跡を更新する
				mGraph.Invalidate();
				mAdvancedFoundation.OnResize(resolvedWidth, resolvedHeight);
			}
		}

		const float aspect = mSceneRenderHeight > 0 ?
			                     static_cast<float>(mSceneRenderWidth) /
			                     static_cast<float>(mSceneRenderHeight) :
			                     16.0f / 9.0f;

		const Mat4 fallbackProj = Mat4::PerspectiveFovMat(
			90.0f * Math::deg2Rad, aspect, 0.001f, 10000.0f
		);
		const Mat4 viewProjection = inputs.camera.valid ?
			                            inputs.camera.viewProj :
			                            fallbackProj;
		const Frustum frustum = BuildFrustum(viewProjection);
		mPortalEnabled        = std::ranges::any_of(
			inputs.portalPairs, [](const PortalPairInput& portal) {
				return portal.enabled;
			}
		);

		rhi.BeginFrame();
		mAdvancedFoundation.BeginFrame();
		auto& dx = static_cast<Rhi::D3D12Device&>(rhi);

		const uint32_t frameIndex = swapChain.GetCurrentBackBufferIndex();
		const uint32_t frameSlot  = frameIndex % swapChain.GetBufferCount();

		Rhi::FrameConstants frame{};
		frame.viewProj = viewProjection;
		frame.time     = inputs.time;
		mFrameCb.Write(frameSlot, frame);

		const uint32_t frameBase = frameSlot * kMaxDrawObjects;
		uint32_t visibleObjectCount = 0;
		std::unordered_map<uint32_t, uint32_t> skinningCbByPalette;
		uint32_t skinningWriteCount = 1;

		{
			Rhi::SkinningPaletteConstants identityPalette = {};
			for (auto& bone : identityPalette.bones) { bone = Mat4::identity; }
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
			for (uint32_t boneIndex = 0; boneIndex < maxBones; ++boneIndex) {
				outPalette.bones[boneIndex] = inPalette.boneMatrices[boneIndex];
			}

			const uint32_t cbIndex = frameBase + skinningWriteCount;
			mSkinningCb.Write(cbIndex, outPalette);
			skinningCbByPalette.emplace(i, cbIndex);
			++skinningWriteCount;
		}

		mDrawList.clear();

		const MaterialBinding* fallbackMaterial = nullptr;
		if (const auto it = mMaterialBindings.find(mDefaultMaterialInstance);
			it != mMaterialBindings.end()) { fallbackMaterial = &it->second; }

		const auto addDraws = [&](
			const D3D12_VERTEX_BUFFER_VIEW& vbv,
			const D3D12_INDEX_BUFFER_VIEW&  ibv,
			const uint32_t                  indexCount,
			const AABB&                     localBounds,
			const uint32_t                  meshIndex,
			const Mat4&                     world,
			const AssetID                   materialInstanceId,
			const bool                      isSkinned,
			const uint32_t                  skinningPaletteId
		) {
			if (visibleObjectCount >= kMaxDrawObjects) { return; }
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
			if (!IsVisible(frustum, worldAabb)) { return; }

			const uint32_t objectCbIndex = frameBase + visibleObjectCount;
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

			const uint32_t materialCbIndex = frameBase + visibleObjectCount;
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
			item.useSkinning = hasSkinningPalette ? 1u : 0u;
			mDrawList.emplace_back(item);

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

			++visibleObjectCount;
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
					); it != mSceneMeshesByAsset.end()) { mesh = &it->second; }
				}
				if (!mesh) { continue; }

				addDraws(
					mesh->vbv,
					mesh->ibv,
					mesh->indexCount,
					mesh->localAABB,
					0,
					obj.world,
					obj.materialInstanceId,
					obj.isSkinned,
					obj.skeletonPaletteId
				);
			}
		}

		mGeometryPass.pso =
			renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
				mGeometryPass.psoKey
			);
		mPortalPass.scenePso = renderDevice.GetPipelineCache().
		                                    GetOrCreateGraphicsPso(
			                                    mPortalPass.scenePsoKey
		                                    );

		BuildDrawBatches();

		mGraph.Execute(rhi);
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

		mBatches = BuildDrawBatchesFromItems(mDrawList, baseKey);
	}

	void URenderer::SetUiCallbacks(
		UiMainRenderCallback     mainRenderCallback,
		UiPlatformRenderCallback platformRenderCallback
	) {
		mUiMainRenderCallback     = std::move(mainRenderCallback);
		mUiPlatformRenderCallback = std::move(platformRenderCallback);
	}

	void URenderer::SetSceneRenderRequest(const SceneRenderRequest& request) {
		mSceneRenderRequest = request;
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
