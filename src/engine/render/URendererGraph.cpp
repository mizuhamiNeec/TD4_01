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

		if (mSceneColorTextureId == 0) {
			mSceneColorTextureId = mGraph.CreateTexture(
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
		}

		if (mPostFxTextureAId == 0) {
			mPostFxTextureAId = mGraph.CreateTexture(
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
		}

		if (mPostFxTextureBId == 0) {
			mPostFxTextureBId = mGraph.CreateTexture(
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
		}

		if (mDepthTextureId == 0) {
			mDepthTextureId = mGraph.CreateTexture(
				{
					.width = w,
					.height = h,
					.resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS,
					.allowDsv = true,
					.srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
					.dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
					.debugName = "Depth",
					.optimizedClearDepth = 0.0f,
					.optimizedClearStencil = 0,
					.extentMode = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
				}
			);
		}

		for (uint32_t directionIndex = 0; directionIndex < kPortalDirections;
		     ++directionIndex) {
			for (uint32_t depthIndex = 0; depthIndex < kPortalRecursionDepth;
			     ++depthIndex) {
				if (mPortalRecursionColorIds[directionIndex][depthIndex] == 0) {
					mPortalRecursionColorIds[directionIndex][depthIndex] =
						mGraph.CreateTexture(
							{
								.width = w,
								.height = h,
								.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
								.allowRtv = true,
								.debugName = "PortalRecursionColor",
								.extentMode = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
							}
						);
				}
				if (mPortalRecursionDepthIds[directionIndex][depthIndex] == 0) {
					mPortalRecursionDepthIds[directionIndex][depthIndex] =
						mGraph.CreateTexture(
							{
								.width          = w,
								.height         = h,
								.resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS,
								.allowDsv       = true,
								.srvFormat      =
								DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
								.dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
								.debugName = "PortalRecursionDepth",
								.optimizedClearDepth = 0.0f,
								.optimizedClearStencil = 0,
								.extentMode = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
							}
						);
				}
			}
		}

		mAdvancedFoundation.Initialize(mGraph, w, h);

		mGraph.AddPass(
			"ClearPass",
			[this](RenderGraphBuilder& b) {
				b.WriteRt(mSceneColorTextureId);
				b.WriteDepth(mDepthTextureId);
				b.ClearColor(mSceneColorTextureId, 0.1f, 0.1f, 0.2f, 1.0f);
				b.ClearDepth(mDepthTextureId, 0.0f, 0);
			},
			[](RenderPassContext&) {}
		);

		mGraph.AddPass(
			"GeometryPass",
			[this](RenderGraphBuilder& b) {
				b.WriteRt(mSceneColorTextureId);
				b.WriteDepth(mDepthTextureId);
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

				for (const auto& batch : mMainBatches) {
					pass.SetGraphicsPipeline(batch.key.rootSig, batch.key.pso);

					pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));

					pass.SetVertexBuffer(batch.items[0].vbv);
					pass.SetIndexBuffer(batch.items[0].ibv);

					for (const auto& item : batch.items) {
						if (item.isPortalSurface != 0) { continue; }
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

		for (uint32_t directionIndex = 0; directionIndex < kPortalDirections;
		     ++directionIndex) {
			for (int depthIndex = static_cast<int>(kPortalRecursionDepth) - 1;
			     depthIndex >= 0;
			     --depthIndex) {
				const uint32_t colorId =
					mPortalRecursionColorIds[directionIndex][depthIndex];
				const uint32_t depthId =
					mPortalRecursionDepthIds[directionIndex][depthIndex];
				const std::string suffix =
					"_" + std::to_string(directionIndex) + "_" +
					std::to_string(depthIndex);

				mGraph.AddPass(
					"PortalRecursionClear" + suffix,
					[colorId, depthId](RenderGraphBuilder& b) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
						b.ClearColor(colorId, 0.1f, 0.1f, 0.2f, 1.0f);
						b.ClearDepth(depthId, 0.0f, 0);
					},
					[](RenderPassContext&) {}
				);

				mGraph.AddPass(
					"PortalRecursionGeometry" + suffix,
					[colorId, depthId](RenderGraphBuilder& b) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
					},
					[this, directionIndex, depthIndex](
					RenderPassContext& pass
				) {
						if (
							directionIndex >= mActivePortalViews.size() ||
							directionIndex >= kPortalDirections
						) { return; }

						const auto& recursiveView =
							mPortalRecursionViews[directionIndex][depthIndex];
						if (
							depthIndex > 0 &&
							!recursiveView.visibleFromPrevious
						) { return; }
						pass.SetViewportAndScissor(
							0.0f,
							0.0f,
							static_cast<float>(mSceneRenderWidth),
							static_cast<float>(mSceneRenderHeight)
						);
						pass.SetSrvUavHeap();
						pass.SetRenderTargetAndDepth(
							std::span<const uint32_t>(
								&mPortalRecursionColorIds[directionIndex][
									depthIndex], 1
							),
							mPortalRecursionDepthIds[directionIndex][depthIndex]
						);

						for (const auto& batch : mPortalBatches) {
							pass.SetGraphicsPipeline(
								batch.key.rootSig, batch.key.pso
							);
							pass.BindGraphicsCbv(
								0,
								mPortalFrameCb.GetGpuAddress(
									recursiveView.frameCbIndex
								)
							);
							pass.SetVertexBuffer(batch.items[0].vbv);
							pass.SetIndexBuffer(batch.items[0].ibv);

							for (const auto& item : batch.items) {
								if (item.isPortalSurface != 0) { continue; }
								pass.BindGraphicsCbv(
									1, mObjectCb.GetGpuAddress(
										item.objectCbIndex
									)
								);
								pass.BindGraphicsCbv(
									2, mMaterialCb.GetGpuAddress(
										item.materialCbIndex
									)
								);
								pass.BindGraphicsCbv(
									3, mSkinningCb.GetGpuAddress(
										item.skinningCbIndex
									)
								);
								if (item.albedoTextureId != 0) {
									pass.BindGraphicsSrvTable(
										4, item.albedoTextureId
									);
								}
								pass.DrawIndexedTest(item.indexCount);
							}
						}
					}
				);

				if (depthIndex + 1 < static_cast<int>(kPortalRecursionDepth)) {
					const uint32_t deeperColorId =
						mPortalRecursionColorIds[directionIndex][
							depthIndex + 1];
					mGraph.AddPass(
						"PortalRecursionMask" + suffix,
						[depthId](RenderGraphBuilder& b) {
							b.WriteDepth(depthId);
						},
						[this, directionIndex, depthIndex](
						RenderPassContext& pass
					) {
							if (
								directionIndex >= mActivePortalViews.size() ||
								directionIndex >= kPortalDirections
							) { return; }

							const auto& maskView =
								mPortalRecursionViews[directionIndex][
									depthIndex + 1];
							if (!maskView.visibleFromPrevious) { return; }
							pass.SetViewportAndScissor(
								0.0f,
								0.0f,
								static_cast<float>(mSceneRenderWidth),
								static_cast<float>(mSceneRenderHeight)
							);
							pass.SetSrvUavHeap();
							pass.SetDepthStencilById(
								mPortalRecursionDepthIds[directionIndex][
									depthIndex]
							);
							pass.SetGraphicsPipeline(
								mPortalPass.maskPassGeom.rootSig,
								mPortalPass.maskPassGeom.pso
							);
							pass.SetStencilRef(maskView.stencilRef);
							pass.BindGraphicsCbv(
								0, mPortalFrameCb.GetGpuAddress(
									mPortalRecursionViews[directionIndex][
										depthIndex].
									frameCbIndex
								)
							);
							pass.BindGraphicsCbv(
								1,
								mObjectCb.GetGpuAddress(
									maskView.compositeObjectCbIndex
								)
							);
							pass.SetVertexBuffer(mPortalPass.maskPassGeom.vbv);
							pass.SetIndexBuffer(mPortalPass.maskPassGeom.ibv);
							pass.DrawIndexedTest(
								mPortalPass.maskPassGeom.indexCount
							);
						}
					);

					mGraph.AddPass(
						"PortalRecursionComposite" + suffix,
						[depthId, colorId, deeperColorId
						](RenderGraphBuilder& b) {
							b.ReadSrvPs(deeperColorId);
							b.WriteRt(colorId);
							b.WriteDepth(depthId);
						},
						[this, directionIndex, depthIndex, deeperColorId](
						RenderPassContext& pass
					) {
							if (
								directionIndex >= mActivePortalViews.size() ||
								directionIndex >= kPortalDirections
							) { return; }

							const auto& maskView =
								mPortalRecursionViews[directionIndex][
									depthIndex + 1];
							if (!maskView.visibleFromPrevious) { return; }
							pass.SetViewportAndScissor(
								0.0f,
								0.0f,
								static_cast<float>(mSceneRenderWidth),
								static_cast<float>(mSceneRenderHeight)
							);
							pass.SetSrvUavHeap();
							const uint32_t colorId =
								mPortalRecursionColorIds[directionIndex][
									depthIndex];
							const uint32_t depthId =
								mPortalRecursionDepthIds[directionIndex][
									depthIndex];
							pass.SetRenderTargetAndDepth(
								std::span<const uint32_t>(&colorId, 1), depthId
							);
							pass.SetGraphicsPipeline(
								mPortalPass.compositeGeom.rootSig,
								mPortalPass.compositeGeom.pso
							);
							pass.SetStencilRef(maskView.stencilRef);
							pass.BindGraphicsCbv(
								0,
								mPortalFrameCb.GetGpuAddress(
									mPortalRecursionViews[directionIndex][
										depthIndex].
									frameCbIndex
								)
							);
							pass.BindGraphicsCbv(
								1,
								mObjectCb.GetGpuAddress(
									maskView.compositeObjectCbIndex
								)
							);
							pass.BindGraphicsSrvTable(4, deeperColorId);
							pass.SetVertexBuffer(mPortalPass.compositeGeom.vbv);
							pass.SetIndexBuffer(mPortalPass.compositeGeom.ibv);
							pass.DrawIndexedTest(
								mPortalPass.compositeGeom.indexCount
							);
						}
					);
				}
			}
		}

		mGraph.AddPass(
			"PortalMaskPass",
			[this](RenderGraphBuilder& b) { b.WriteDepth(mDepthTextureId); },
			[this, &renderDevice](RenderPassContext& pass) {
				if (mActivePortalViews.empty()) { return; }

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();

				auto&          rhi        = renderDevice.GetRhiDevice();
				const uint32_t frameIndex =
					rhi.GetSwapChain().GetCurrentBackBufferIndex();
				const uint32_t frameSlot =
					frameIndex % rhi.GetSwapChain().GetBufferCount();

				pass.SetDepthStencilById(mDepthTextureId);
				pass.SetGraphicsPipeline(
					mPortalPass.maskPassGeom.rootSig,
					mPortalPass.maskPassGeom.pso
				);
				pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));
				pass.SetVertexBuffer(mPortalPass.maskPassGeom.vbv);
				pass.SetIndexBuffer(mPortalPass.maskPassGeom.ibv);
				for (uint32_t directionIndex = 0;
				     directionIndex < static_cast<uint32_t>(mActivePortalViews.
					     size());
				     ++directionIndex) {
					pass.SetStencilRef(
						mActivePortalViews[directionIndex].stencilRef
					);
					pass.BindGraphicsCbv(
						1,
						mObjectCb.GetGpuAddress(
							mPortalRecursionViews[directionIndex][0].
							compositeObjectCbIndex
						)
					);
					pass.DrawIndexedTest(mPortalPass.maskPassGeom.indexCount);
				}
			}
		);

		mGraph.AddPass(
			"PortalCompositePass",
			[this](RenderGraphBuilder& b) {
				for (uint32_t directionIndex = 0;
				     directionIndex < kPortalDirections;
				     ++directionIndex) {
					b.ReadSrvPs(mPortalRecursionColorIds[directionIndex][0]);
				}
				b.WriteRt(mSceneColorTextureId);
				b.WriteDepth(mDepthTextureId);
			},
			[this, &renderDevice](RenderPassContext& pass) {
				if (mActivePortalViews.empty()) { return; }

				auto&          rhi        = renderDevice.GetRhiDevice();
				const uint32_t frameIndex =
					rhi.GetSwapChain().GetCurrentBackBufferIndex();
				const uint32_t frameSlot =
					frameIndex % rhi.GetSwapChain().GetBufferCount();

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();
				const uint32_t sceneColorId = mSceneColorTextureId;
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&sceneColorId, 1), mDepthTextureId
				);
				pass.SetGraphicsPipeline(
					mPortalPass.compositeGeom.rootSig,
					mPortalPass.compositeGeom.pso
				);
				pass.BindGraphicsCbv(0, mFrameCb.GetGpuAddress(frameSlot));
				pass.SetVertexBuffer(mPortalPass.compositeGeom.vbv);
				pass.SetIndexBuffer(mPortalPass.compositeGeom.ibv);
				for (uint32_t directionIndex = 0;
				     directionIndex < static_cast<uint32_t>(mActivePortalViews.
					     size());
				     ++directionIndex) {
					pass.SetStencilRef(
						mActivePortalViews[directionIndex].stencilRef
					);
					pass.BindGraphicsCbv(
						1,
						mObjectCb.GetGpuAddress(
							mPortalRecursionViews[directionIndex][0].
							compositeObjectCbIndex
						)
					);
					pass.BindGraphicsSrvTable(
						4, mPortalRecursionColorIds[directionIndex][0]
					);
					pass.DrawIndexedTest(mPortalPass.compositeGeom.indexCount);
				}
			}
		);

		uint32_t postFxInputId  = mSceneColorTextureId;
		uint32_t postFxOutputId = mPostFxTextureAId;
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
			postFxOutputId = postFxOutputId == mPostFxTextureAId ?
				                 mPostFxTextureBId :
				                 mPostFxTextureAId;
		}
		mSceneOutputTextureId = postFxInputId;

		mGraph.AddPass(
			"SpriteOverlayPass",
			[postFxInputId](RenderGraphBuilder& b) {
				b.WriteRt(postFxInputId);
			},
			[this, postFxInputId, &renderDevice](RenderPassContext& pass) {
				if (mScreenSprites.empty()) { return; }

				auto& dx = static_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				);
				auto& allocator = dx.GetFrameUploadAllocator();

				Rhi::FrameConstants frame = {};
				frame.view = Mat4::identity;
				frame.proj = Mat4::MakeOrthographicMat(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight),
					0.0f,
					1.0f
				);
				frame.viewProj = frame.view * frame.proj;
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(&frame, sizeof(frame));

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(mSceneRenderWidth),
					static_cast<float>(mSceneRenderHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(postFxInputId);
				pass.SetGraphicsPipeline(
					mSpritePass.geom.rootSig,
					mSpritePass.geom.pso
				);
				pass.BindGraphicsCbv(0, frameCb);
				pass.SetVertexBuffer(mSpritePass.geom.vbv);
				pass.SetIndexBuffer(mSpritePass.geom.ibv);

				for (const auto& sprite : mScreenSprites) {
					const Vec2 center = Vec2(
						sprite.positionPx.x + (0.5f - sprite.anchor.x) * sprite.sizePx.x,
						sprite.positionPx.y + (0.5f - sprite.anchor.y) * sprite.sizePx.y
					);

					Rhi::ObjectConstants object = {};
					object.world = Mat4::Scale(
						Vec3(sprite.sizePx.x * 0.5f, sprite.sizePx.y * 0.5f, 1.0f)
					) * Mat4::RotateZ(sprite.rotationRad) * Mat4::Translate(
						Vec3(center.x, center.y, 0.0f)
					);
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();

					Rhi::MaterialConstants material = {};
					material.baseColor = sprite.color;
					material.opacity = sprite.color.w;
					material.domainMode = 0.0f;

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(&object, sizeof(object));
					const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
						allocator.AllocateConstantBuffer(&material, sizeof(material));

					pass.BindGraphicsCbv(1, objectCb);
					pass.BindGraphicsCbv(2, materialCb);
					pass.BindGraphicsCbv(3, objectCb);
					pass.BindGraphicsSrvTable(
						4,
						EnsureSpriteTextureLoaded(renderDevice, sprite.textureAssetId)
					);
					pass.DrawIndexedTest(mSpritePass.geom.indexCount);
				}
			}
		);

		if (!mPresentSceneToSwapChain) {
			mGraph.AddPass(
				"PrepareSceneOutputForUi",
				[postFxInputId](RenderGraphBuilder& b) {
					b.ReadSrvPs(postFxInputId);
				},
				[](RenderPassContext&) {}
			);
		}

		if (mPresentSceneToSwapChain) {
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
		} else if (mClearSwapChainWhenNotPresenting) {
			mGraph.AddPass(
				"EditorBackBufferClearPass",
				[](RenderGraphBuilder& b) {
					b.WriteBackBufferRt();
					b.ClearColor(
						RenderGraph::kBackBufferId,
						0.02f,
						0.02f,
						0.02f,
						1.0f
					);
				},
				[](RenderPassContext&) {}
			);
		}

		mGraph.AddPass(
			"ImGuiMainPass",
			[](RenderGraphBuilder& b) { b.WriteBackBufferRt(); },
			[this](RenderPassContext& pass) {
				if (mUiMainRenderCallback) { mUiMainRenderCallback(pass); }
			}
		);
	}
}
