#include "Renderer.h"

#include <algorithm>
#include <unordered_set>

#include "RenderDevice.h"

#include "engine/profiler/Profiler.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/interface/IRhiDevice.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::Render {
	namespace {
		static constexpr uint32_t kShrinkSettleFrames = 12;
		static constexpr uint32_t kMinShrinkPixels    = 64;
		static constexpr float    kMinShrinkRatio     = 0.9f;

		void ResetPendingShrink(auto& state) {
			state.pendingShrinkWidth        = 0;
			state.pendingShrinkHeight       = 0;
			state.pendingShrinkStableFrames = 0;
		}

		bool ApplyViewSizeImmediately(
			auto& state, const uint32_t width, const uint32_t height
		) {
			const bool changed = state.width != width || state.height != height;
			state.width        = width;
			state.height       = height;
			ResetPendingShrink(state);
			return changed;
		}

		bool IsSignificantShrink(
			const uint32_t appliedWidth, const uint32_t appliedHeight,
			const uint32_t requestedWidth, const uint32_t requestedHeight
		) {
			const uint32_t shrinkWidth = appliedWidth > requestedWidth ?
				                             appliedWidth - requestedWidth :
				                             0u;
			const uint32_t shrinkHeight = appliedHeight > requestedHeight ?
				                              appliedHeight - requestedHeight :
				                              0u;
			if (shrinkWidth == 0 && shrinkHeight == 0) {
				return false;
			}

			float ratioWidth = 1.0f;
			if (requestedWidth < appliedWidth && appliedWidth > 0) {
				ratioWidth = static_cast<float>(requestedWidth) /
				             static_cast<float>(appliedWidth);
			}
			float ratioHeight = 1.0f;
			if (requestedHeight < appliedHeight && appliedHeight > 0) {
				ratioHeight = static_cast<float>(requestedHeight) /
				              static_cast<float>(appliedHeight);
			}

			return shrinkWidth >= kMinShrinkPixels ||
			       shrinkHeight >= kMinShrinkPixels ||
			       ratioWidth <= kMinShrinkRatio ||
			       ratioHeight <= kMinShrinkRatio;
		}
	}

	void Renderer::RenderFrame(
		RenderDevice& renderDevice, const RenderFrameInputs& inputs
	) {
		Profiler*                   profiler = ServiceLocator::Get<Profiler>();
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
		if (materialsDirty) {
			LoadMaterialResources(renderDevice, dx);
		}
		if (postFxDirty) {
			LoadPostFxChain(renderDevice);
		}

		mFrameViews = inputs.views;
		if (mFrameViews.empty()) {
			RenderViewInput fallback = {};
			fallback.viewKey         = "default.main";
			fallback.type            = RENDER_VIEW_TYPE::SCENE;
			fallback.output.sizeMode = RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER;
			fallback.output.presentToSwapChain = true;
			fallback.sceneViewMode.mode = SCENE_RENDER_MODE::FIT_VIEWPORT;
			mFrameViews.emplace_back(std::move(fallback));
		}

		mViewExecutionOrder.clear();
		mPresentViewKey.clear();
		std::unordered_set<std::string> activeViewKeys;
		activeViewKeys.reserve(mFrameViews.size());

		for (RenderViewInput& view : mFrameViews) {
			if (view.viewKey.empty()) {
				view.viewKey = "unnamed.view";
			}
			mViewExecutionOrder.emplace_back(view.viewKey);
			activeViewKeys.emplace(view.viewKey);

			if (view.type == RENDER_VIEW_TYPE::SCENE) {
				const auto [sceneWidth, sceneHeight] = ResolveSceneRenderExtent(
					backBufferWidth, backBufferHeight, view.sceneViewMode
				);
				view.output.width  = sceneWidth;
				view.output.height = sceneHeight;
			} else if (
				view.output.sizeMode == RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER
			) {
				view.output.width  = std::max(1u, backBufferWidth);
				view.output.height = std::max(1u, backBufferHeight);
			} else {
				view.output.width  = std::max(1u, view.output.width);
				view.output.height = std::max(1u, view.output.height);
			}

			std::sort(
				view.worldBillboards.begin(),
				view.worldBillboards.end(),
				[](const WorldBillboardInput& a, const WorldBillboardInput& b) {
					return a.sortKey < b.sortKey;
				}
			);
			std::sort(
				view.worldSprites.begin(),
				view.worldSprites.end(),
				[](const WorldSpriteInput& a, const WorldSpriteInput& b) {
					return a.sortKey < b.sortKey;
				}
			);
			std::sort(
				view.screenSprites.begin(),
				view.screenSprites.end(),
				[](const ScreenSpriteInput& a, const ScreenSpriteInput& b) {
					return a.sortKey < b.sortKey;
				}
			);

			if (
				mPresentViewKey.empty() &&
				view.output.presentToSwapChain
			) {
				mPresentViewKey = view.viewKey;
			}

			auto& state = mViewStates[view.viewKey];
			const bool     typeChanged     = state.type != view.type;
			const uint32_t requestedWidth  = view.output.width;
			const uint32_t requestedHeight = view.output.height;

			state.type            = view.type;
			state.output          = view.output;
			state.requestedWidth  = requestedWidth;
			state.requestedHeight = requestedHeight;

			bool sizeChanged = false;
			if (typeChanged) {
				sizeChanged = ApplyViewSizeImmediately(
					state, requestedWidth, requestedHeight
				);
			} else {
				const bool growWidth  = requestedWidth > state.width;
				const bool growHeight = requestedHeight > state.height;
				if (growWidth || growHeight) {
					sizeChanged = ApplyViewSizeImmediately(
						state,
						std::max(state.width, requestedWidth),
						std::max(state.height, requestedHeight)
					);
				} else {
					const bool shrinkWidth  = requestedWidth < state.width;
					const bool shrinkHeight = requestedHeight < state.height;
					if (shrinkWidth || shrinkHeight) {
						if (!view.sceneViewMode.preferRealtimeResize) {
							sizeChanged = ApplyViewSizeImmediately(
								state, requestedWidth, requestedHeight
							);
						} else if (
							IsSignificantShrink(
								state.width,
								state.height,
								requestedWidth,
								requestedHeight
							)
						) {
							if (
								state.pendingShrinkWidth == requestedWidth &&
								state.pendingShrinkHeight == requestedHeight
							) {
								state.pendingShrinkStableFrames = std::min(
									state.pendingShrinkStableFrames + 1u,
									kShrinkSettleFrames
								);
							} else {
								state.pendingShrinkWidth        = requestedWidth;
								state.pendingShrinkHeight       = requestedHeight;
								state.pendingShrinkStableFrames = 1;
							}

							if (state.pendingShrinkStableFrames >= kShrinkSettleFrames) {
								sizeChanged = ApplyViewSizeImmediately(
									state, requestedWidth, requestedHeight
								);
							}
						} else {
							ResetPendingShrink(state);
						}
					} else {
						ResetPendingShrink(state);
					}
				}
			}

			if (sizeChanged || typeChanged) {
				ReleaseViewRuntimeTextures(renderDevice, state);
				state.colorTextureId  = 0;
				state.depthTextureId  = 0;
				state.postFxTextureAId = 0;
				state.postFxTextureBId = 0;
				state.outputTextureId = 0;
			}
		}

		for (
			auto it = mViewStates.begin();
			it != mViewStates.end();
		) {
			if (!activeViewKeys.contains(it->first)) {
				ReleaseViewRuntimeTextures(renderDevice, it->second);
				it = mViewStates.erase(it);
			} else {
				++it;
			}
		}

		bool requiresSpriteTextures = false;
		for (const RenderViewInput& view : mFrameViews) {
			if (
				!view.worldBillboards.empty() ||
				!view.worldSprites.empty() ||
				!view.screenSprites.empty()
			) {
				requiresSpriteTextures = true;
			}
			for (const auto& s : view.worldBillboards) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			for (const auto& s : view.worldSprites) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			for (const auto& s : view.screenSprites) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			if (view.type != RENDER_VIEW_TYPE::SCENE) {
				continue;
			}
			for (const auto& obj : view.visibleObjects) {
				if (obj.meshAssetId != kInvalidAssetID) {
					(void)EnsureMeshResourceLoaded(
						renderDevice, dx, obj.meshAssetId
					);
				}
			}
		}
		if (requiresSpriteTextures) {
			EnsureSpriteFallbackTexture(renderDevice);
		}

		mGeometryPass.pso =
			renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
				mGeometryPass.psoKey
			);
		mSpritePass.geom.pso =
			renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
				mSpritePass.geom.psoKey
			);
		mBillboardPass.geom.pso =
			renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
				mBillboardPass.geom.psoKey
			);

		rhi.BeginFrame();
		mAdvancedFoundation.BeginFrame();
		renderDevice.GetRegistry().CollectGarbage(dx.GetCompletedFenceValue());

		{
			Profiler::ScopeTimer scope(profiler, "Render.BuildGraph");
			mGraph.Reset();
			mGraphBuilt = false;
			BuildGraph(renderDevice, mFrameViews);
		}
		{
			Profiler::ScopeTimer scope(profiler, "Render.GraphExecute");
			mGraph.Execute(rhi);
		}
		if (mUiPlatformRenderCallback) {
			mUiPlatformRenderCallback();
		}

		rhi.EndFrame();
	}

	void Renderer::OnResize(const uint32_t width, const uint32_t height) {
		mAdvancedFoundation.OnResize(width, height);
	}

	void Renderer::SetUiCallbacks(
		UiMainRenderCallback     mainRenderCallback,
		UiPlatformRenderCallback platformRenderCallback
	) {
		mUiMainRenderCallback     = std::move(mainRenderCallback);
		mUiPlatformRenderCallback = std::move(platformRenderCallback);
	}

	SceneOutputView Renderer::GetViewOutputView(
		const RenderDevice& renderDevice, const std::string_view viewKey
	) const {
		SceneOutputView view = {};
		const auto      it   = mViewStates.find(std::string(viewKey));
		if (it == mViewStates.end()) {
			return view;
		}

		view.textureId = it->second.outputTextureId;
		if (view.textureId == 0) {
			return view;
		}

		const auto& registry =
			const_cast<RenderDevice&>(renderDevice).GetRegistry();
		view.srvCpu      = registry.GetSrvCpu(view.textureId);
		view.srvRevision = registry.GetSrvRevision(view.textureId);
		return view;
	}

	Vec2 Renderer::GetViewOutputSize(const std::string_view viewKey) const {
		const auto it = mViewStates.find(std::string(viewKey));
		if (it == mViewStates.end()) {
			return Vec2::zero;
		}
		return Vec2(
			static_cast<float>(it->second.width),
			static_cast<float>(it->second.height)
		);
	}

	std::pair<uint32_t, uint32_t> Renderer::ResolveSceneRenderExtent(
		const uint32_t            backBufferWidth,
		const uint32_t            backBufferHeight,
		const SceneViewRenderMode& request
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
				if (width * 9 > height * 16) {
					width = height * 16 / 9;
				} else {
					height = width * 9 / 16;
				}
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_4X3: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 3 > height * 4) {
					width = height * 4 / 3;
				} else {
					height = width * 3 / 4;
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

		if ((width & 1u) != 0u) {
			--width;
		}
		if ((height & 1u) != 0u) {
			--height;
		}

		width  = std::max(2u, width);
		height = std::max(2u, height);
		return {width, height};
	}

	uint32_t Renderer::ResolveSpriteTexture(
		RenderDevice& renderDevice, const SpriteTextureRef& textureRef
	) {
		if (textureRef.source == SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT) {
			if (const auto it = mViewStates.find(textureRef.viewKey);
				it != mViewStates.end() && it->second.outputTextureId != 0) {
				return it->second.outputTextureId;
			}
			EnsureSpriteFallbackTexture(renderDevice);
			return mSpriteFallbackTextureId;
		}
		return EnsureSpriteTextureLoaded(renderDevice, textureRef.textureAssetId);
	}

	void Renderer::ReleaseViewRuntimeTextures(
		RenderDevice& renderDevice, ViewRuntimeState& state
	) {
		auto& registry = renderDevice.GetRegistry();
		if (state.colorTextureId != 0) {
			registry.ReleaseTexture(state.colorTextureId);
		}
		if (state.depthTextureId != 0) {
			registry.ReleaseTexture(state.depthTextureId);
		}
		if (state.postFxTextureAId != 0) {
			registry.ReleaseTexture(state.postFxTextureAId);
		}
		if (state.postFxTextureBId != 0) {
			registry.ReleaseTexture(state.postFxTextureBId);
		}
		if (state.outputTextureId != 0) {
			registry.ReleaseTexture(state.outputTextureId);
		}
		state.colorTextureId   = 0;
		state.depthTextureId   = 0;
		state.postFxTextureAId = 0;
		state.postFxTextureBId = 0;
		state.outputTextureId  = 0;
	}
}
