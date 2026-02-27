#include "URenderer.h"

#include <algorithm>

#include "RenderDevice.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/PostFxChainAssetData.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/Log.h"

#include "rendergraph/RenderGraphBuilder.h"
#include "rendergraph/RenderPassContext.h"

namespace Unnamed::Render {
	void URenderer::LoadPostFxChain(RenderDevice& renderDevice) {
		auto& assetManager = renderDevice.GetAssetManager();
		mPostFxChainAsset  = assetManager.LoadFromFile(
			"./content/core/postfx/default.postfx.json",
			ASSET_TYPE::POST_FX_CHAIN
		);
		mPostFxPasses.clear();

		const auto* chain = assetManager.Get<PostFxChainAssetData>(
			mPostFxChainAsset
		);
		if (!chain) { return; }

		auto& dx = static_cast<Rhi::D3D12Device&>(renderDevice.GetRhiDevice());
		for (const auto& p : chain->passes) {
			if (!p.enabled || p.shaderProgramId == kInvalidAssetID) {
				continue;
			}

			PostFxRuntimePass runtime         = {};
			runtime.name                      = p.name;
			runtime.enabled                   = p.enabled;
			runtime.pass.rootSig              = dx.GetFsRootSignature();
			runtime.pass.psoKey.rootSignature = runtime.pass.rootSig;
			runtime.pass.psoKey.vertexLayout  = std::nullopt;
			runtime.pass.psoKey.rtvFormat     = Rhi::ToDxgiFormat(
				dx.GetSwapChain().GetFormat()
			);
			runtime.pass.psoKey.depthEnable = false;
			runtime.pass.psoKey.dsvFormat   = DXGI_FORMAT_UNKNOWN;
			runtime.pass.psoKey.depthFunc   = D3D12_COMPARISON_FUNC_ALWAYS;

			if (!ResolveShaderProgramStageKey(
				renderDevice, p.shaderProgramId, "vs", runtime.pass.psoKey.vs
			)) { continue; }
			if (!ResolveShaderProgramStageKey(
				renderDevice, p.shaderProgramId, "ps", runtime.pass.psoKey.ps
			)) { continue; }

			mPostFxPasses.emplace_back(std::move(runtime));
		}
	}

	void URenderer::BuildGraph(RenderDevice& renderDevice) {
		if (mGraphBuilt) { return; }
		mGraphBuilt = true;

		mGraph.SetRenderDevice(renderDevice);

		auto& dx = static_cast<Rhi::D3D12Device&>(renderDevice.GetRhiDevice());
		const uint32_t w = std::max(
			1u,
			mSceneRenderWidth != 0 ?
				mSceneRenderWidth :
				dx.GetSwapChain().GetWidth()
		);
		const uint32_t h = std::max(
			1u,
			mSceneRenderHeight != 0 ?
				mSceneRenderHeight :
				dx.GetSwapChain().GetHeight()
		);
		DevMsg("URenderer", "BuildGraph with scene extent: {}x{}", w, h);

		const uint32_t sceneColorId = mGraph.CreateTexture(
			{
				.width          = w,
				.height         = h,
				.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
				.allowUav       = true,
				.allowRtv       = true,
				.debugName      = "SceneColor",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);

		const uint32_t postFxAId = mGraph.CreateTexture(
			{
				.width          = w,
				.height         = h,
				.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
				.allowUav       = false,
				.allowRtv       = true,
				.debugName      = "PostFxA",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);

		const uint32_t postFxBId = mGraph.CreateTexture(
			{
				.width          = w,
				.height         = h,
				.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
				.allowUav       = false,
				.allowRtv       = true,
				.debugName      = "PostFxB",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);

		const uint32_t depthId = mGraph.CreateTexture(
			{
				.width                 = w,
				.height                = h,
				.resourceFormat        = DXGI_FORMAT_R24G8_TYPELESS,
				.allowDsv              = true,
				.srvFormat             = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
				.dsvFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT,
				.debugName             = "Depth",
				.optimizedClearDepth   = 1.0f,
				.optimizedClearStencil = 0,
				.extentMode            = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);

		mAdvancedFoundation.Initialize(mGraph, w, h);

		mGraph.AddPass(
			"ClearPass",
			[=](RenderGraphBuilder& b) {
				b.WriteRt(sceneColorId);
				b.WriteDepth(depthId);
				b.ClearColor(sceneColorId, 0.1f, 0.1f, 0.2f, 1.0f);
				b.ClearDepth(depthId, 1.0f, 0);
			},
			[](RenderPassContext&) {}
		);

		mGraph.AddPass(
			"GeometryPass",
			[sceneColorId, depthId](RenderGraphBuilder& b) {
				b.WriteRt(sceneColorId);
				b.WriteDepth(depthId);
			},
			[this, &renderDevice](RenderPassContext& pass) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();

				auto& rhi = renderDevice.GetRhiDevice();

				const uint32_t frameIndex =
					rhi.GetSwapChain().GetCurrentBackBufferIndex();
				const uint32_t frameSlot =
					frameIndex % rhi.GetSwapChain().GetBufferCount();

				pass.SetGraphicsPipeline(
					mGeometryPass.rootSig, mGeometryPass.pso
				);

				pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));

				for (const auto& batch : mBatches) {
					pass.SetGraphicsPipeline(batch.key.rootSig, batch.key.pso);

					pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));

					pass.SetVertexBuffer(batch.items[0].vbv);
					pass.SetIndexBuffer(batch.items[0].ibv);

					for (const auto& item : batch.items) {
						pass.BindGraphicsCbv(
							1, mObjectCb.GetGpuAddress(item.objectCbIndex)
						);
						pass.BindGraphicsCbv(
							2, mMaterialCb.GetGpuAddress(item.materialCbIndex)
						);
						pass.BindGraphicsCbv(
							3, mSkinningCb.GetGpuAddress(item.skinningCbIndex)
						);
						if (item.albedoTextureId != 0) {
							pass.BindGraphicsSrvTable(4, item.albedoTextureId);
						}
						pass.DrawIndexedTest(item.indexCount);
					}
				}
			}
		);

		mGraph.AddPass(
			"PortalMaskPass",
			[sceneColorId, depthId](RenderGraphBuilder& b) {
				b.WriteRt(sceneColorId);
				b.WriteDepth(depthId);
			},
			[this, &renderDevice](RenderPassContext& pass) {
				if (!mPortalEnabled) { return; }

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();

				auto* pso =
					renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
						mPortalPass.maskPass.psoKey
					);
				pass.SetGraphicsPipeline(mPortalPass.maskPass.rootSig, pso);
				pass.SetStencilRef(mPortalPass.stencilRef);
				pass.DrawFullscreenTriangle();
			}
		);

		mGraph.AddPass(
			"PortalScenePass",
			[sceneColorId, depthId](RenderGraphBuilder& b) {
				b.WriteRt(sceneColorId);
				b.WriteDepth(depthId);
			},
			[this, &renderDevice](RenderPassContext& pass) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();

				auto& rhi = renderDevice.GetRhiDevice();

				const uint32_t frameIndex =
					rhi.GetSwapChain().GetCurrentBackBufferIndex();
				const uint32_t frameSlot =
					frameIndex % rhi.GetSwapChain().GetBufferCount();

				pass.SetStencilRef(mPortalPass.stencilRef);

				for (const auto& batch : mBatches) {
					pass.SetGraphicsPipeline(
						batch.key.rootSig, mPortalPass.scenePso
					);

					pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));
					pass.SetVertexBuffer(batch.items[0].vbv);
					pass.SetIndexBuffer(batch.items[0].ibv);

					for (const auto& item : batch.items) {
						pass.BindGraphicsCbv(
							1, mObjectCb.GetGpuAddress(item.objectCbIndex)
						);
						pass.BindGraphicsCbv(
							2, mMaterialCb.GetGpuAddress(item.materialCbIndex)
						);
						pass.BindGraphicsCbv(
							3, mSkinningCb.GetGpuAddress(item.skinningCbIndex)
						);
						if (item.albedoTextureId != 0) {
							pass.BindGraphicsSrvTable(4, item.albedoTextureId);
						}
						pass.DrawIndexedTest(item.indexCount);
					}
				}
			}
		);

		uint32_t postFxInputId  = sceneColorId;
		uint32_t postFxOutputId = postFxAId;
		for (size_t i = 0; i < mPostFxPasses.size(); ++i) {
			const auto passRes = mPostFxPasses[i];
			const auto inId    = postFxInputId;
			const auto outId   = postFxOutputId;

			mGraph.AddPass(
				"PostFx_" + passRes.name,
				[inId, outId](RenderGraphBuilder& b) {
					b.ReadSrvPs(inId);
					b.WriteRt(outId);
				},
				[this, passRes, inId, &renderDevice](RenderPassContext& pass) {
					pass.SetViewportAndScissor(
						0.0f,
						0.0f,
						static_cast<float>(mSceneRenderWidth),
						static_cast<float>(mSceneRenderHeight)
					);
					pass.SetSrvUavHeap();

					auto* pso =
						renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
							passRes.pass.psoKey
						);
					pass.SetGraphicsPipeline(passRes.pass.rootSig, pso);
					pass.BindGraphicsSrvTable(0, inId);
					pass.DrawFullscreenTriangle();
				}
			);

			postFxInputId  = outId;
			postFxOutputId = postFxOutputId == postFxAId ?
				                 postFxBId :
				                 postFxAId;
		}
		mSceneOutputTextureId = postFxInputId;

		mGraph.AddPass(
			"FullscreenSampleSrv",
			[=](RenderGraphBuilder& b) {
				b.ReadSrvPs(postFxInputId);
				b.WriteBackBufferRt();
			},
			[this, postFxInputId, &renderDevice](RenderPassContext& pass) {
				pass.SetViewportToBackBuffer();
				pass.SetSrvUavHeap();

				auto* pso =
					renderDevice.GetPipelineCache().GetOrCreateGraphicsPso(
						mFullscreenPass.psoKey
					);
				pass.SetGraphicsPipeline(mFullscreenPass.rootSig, pso);

				pass.BindGraphicsSrvTable(0, postFxInputId);
				pass.DrawFullscreenTriangle();
			}
		);

		mGraph.AddPass(
			"ImGuiMainPass",
			[](RenderGraphBuilder& b) { b.WriteBackBufferRt(); },
			[this](RenderPassContext& pass) {
				if (mUiMainRenderCallback) { mUiMainRenderCallback(pass); }
			}
		);
	}
}
