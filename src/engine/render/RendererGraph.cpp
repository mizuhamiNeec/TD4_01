#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "RenderDevice.h"

#include "core/math/Math.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"

#include "rendergraph/RenderGraphBuilder.h"
#include "rendergraph/RenderPassContext.h"

namespace Unnamed::Render {
	namespace {
		Rhi::FrameConstants BuildSceneFrameConstants(
			const RenderCameraInput& camera,
			const uint32_t           width,
			const uint32_t           height,
			const float              time
		) {
			Rhi::FrameConstants frame = {};
			const float         aspect = height > 0 ?
				                             static_cast<float>(width) /
				                             static_cast<float>(height) :
				                             16.0f / 9.0f;
			const Mat4 fallbackView = Mat4::identity;
			const Mat4 fallbackProj = Mat4::PerspectiveFovD3D(
				90.0f * Math::deg2Rad,
				aspect,
				0.001f,
				10000.0f,
				ProjectionDepthMode::ReverseZ
			);
			frame.view      = camera.valid ? camera.view : fallbackView;
			frame.proj      = camera.valid ? camera.proj : fallbackProj;
			frame.viewProj  = frame.view * frame.proj;
			frame.cameraPos = camera.valid ? camera.cameraPos : Vec3::zero;
			frame.time      = time;
			frame.portalClipPlane   = Vec4::zero;
			frame.portalClipEnabled = 0.0f;
			return frame;
		}

		Mat4 BuildOrthographic(
			const uint32_t width, const uint32_t height
		) {
			return Mat4::MakeOrthographicMat(
				0.0f,
				0.0f,
				static_cast<float>(width),
				static_cast<float>(height),
				0.0f,
				1.0f
			);
		}
	}

	void Renderer::BuildGraph(
		RenderDevice&                         renderDevice,
		const std::vector<RenderViewInput>& frameViews
	) {
		mGraphBuilt = true;
		mGraph.SetRenderDevice(renderDevice);

		for (const RenderViewInput& view : frameViews) {
			auto& state = mViewStates[view.viewKey];
			if (state.type == RENDER_VIEW_TYPE::SCENE) {
				if (state.colorTextureId == 0) {
					state.colorTextureId = mGraph.CreateTexture(
						{
							.width          = state.width,
							.height         = state.height,
							.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
							.allowUav       = false,
							.allowRtv       = true,
							.debugName      = "ViewColor_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.postFxTextureAId == 0) {
					state.postFxTextureAId = mGraph.CreateTexture(
						{
							.width          = state.width,
							.height         = state.height,
							.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
							.allowRtv       = true,
							.debugName      = "ViewPostFxA_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.postFxTextureBId == 0) {
					state.postFxTextureBId = mGraph.CreateTexture(
						{
							.width          = state.width,
							.height         = state.height,
							.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
							.allowRtv       = true,
							.debugName      = "ViewPostFxB_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.depthTextureId == 0) {
					state.depthTextureId = mGraph.CreateTexture(
						{
							.width = state.width,
							.height = state.height,
							.resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS,
							.allowDsv = true,
							.srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
							.dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
							.debugName = "ViewDepth_" + view.viewKey,
							.optimizedClearDepth = 0.0f,
							.optimizedClearStencil = 0,
							.extentMode = RG_EXTENT_MODE::FIXED,
						}
					);
				}
			} else if (state.outputTextureId == 0) {
				state.outputTextureId = mGraph.CreateTexture(
					{
						.width          = state.width,
						.height         = state.height,
						.resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
						.allowRtv       = true,
						.debugName      = "SpriteOnly_" + view.viewKey,
						.extentMode     = RG_EXTENT_MODE::FIXED,
					}
				);
			}
		}

		for (size_t viewIndex = 0; viewIndex < frameViews.size(); ++viewIndex) {
			const RenderViewInput& view  = frameViews[viewIndex];
			const auto             state = mViewStates[view.viewKey];
			const std::string      prefix = "View[" + view.viewKey + "] ";
			const auto collectTextureIds = [this, &renderDevice](
				                               const auto& sprites
			                               ) {
				std::vector<uint32_t> ids;
				ids.reserve(sprites.size());
				std::unordered_set<uint32_t> seen;
				seen.reserve(sprites.size());
				for (const auto& sprite : sprites) {
					const uint32_t textureId =
						ResolveSpriteTexture(renderDevice, sprite.texture);
					if (textureId == 0) {
						continue;
					}
					if (seen.insert(textureId).second) {
						ids.emplace_back(textureId);
					}
				}
				return ids;
			};
			const std::vector<uint32_t> worldBillboardTextureIds =
				collectTextureIds(view.worldBillboards);
			const std::vector<uint32_t> worldSpriteTextureIds = collectTextureIds(
				view.worldSprites
			);
			const std::vector<uint32_t> screenSpriteTextureIds = collectTextureIds(
				view.screenSprites
			);

			uint32_t colorId  = state.colorTextureId;
			uint32_t depthId  = state.depthTextureId;
			uint32_t outputId = state.outputTextureId;
			if (view.type == RENDER_VIEW_TYPE::SCENE) {
				mGraph.AddPass(
					prefix + "Clear",
					[colorId, depthId](RenderGraphBuilder& b) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
						b.ClearColor(colorId, 0.1f, 0.1f, 0.2f, 1.0f);
						b.ClearDepth(depthId, 0.0f, 0);
					},
					[](RenderPassContext&) {}
				);

				mGraph.AddPass(
					prefix + "Geometry",
					[colorId, depthId](RenderGraphBuilder& b) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
					},
					[this, viewIndex, state, &renderDevice](RenderPassContext& pass
					) {
						const RenderViewInput& view = mFrameViews[viewIndex];
						if (view.visibleObjects.empty()) {
							return;
						}

						auto& allocator = static_cast<Rhi::D3D12Device&>(
							renderDevice.GetRhiDevice()
						).GetFrameUploadAllocator();
						Rhi::FrameConstants frame = BuildSceneFrameConstants(
							view.camera, state.width, state.height, 0.0f
						);
						const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
							allocator.AllocateConstantBuffer(&frame, sizeof(frame));

						Rhi::SkinningPaletteConstants identityPalette = {};
						for (auto& bone : identityPalette.bones) {
							bone = Mat4::identity;
						}
						const D3D12_GPU_VIRTUAL_ADDRESS identitySkinCb =
							allocator.AllocateConstantBuffer(
								&identityPalette, sizeof(identityPalette)
							);

						pass.SetViewportAndScissor(
							0.0f,
							0.0f,
							static_cast<float>(state.width),
							static_cast<float>(state.height)
						);
						pass.SetSrvUavHeap();
						pass.SetRenderTargetAndDepth(
							std::span<const uint32_t>(&state.colorTextureId, 1),
							state.depthTextureId
						);
						pass.SetGraphicsPipeline(
							mGeometryPass.rootSig, mGeometryPass.pso
						);
						pass.BindGraphicsCbv(0, frameCb);

						const MaterialBinding* fallbackMaterial = nullptr;
						if (const auto it = mMaterialBindings.find(
							mDefaultMaterialInstance
						); it != mMaterialBindings.end()) {
							fallbackMaterial = &it->second;
						}

						for (const auto& objectInput : view.visibleObjects) {
							const auto meshIt = mSceneMeshesByAsset.find(
								objectInput.meshAssetId
							);
							if (meshIt == mSceneMeshesByAsset.end()) {
								continue;
							}
							const MeshBuffer& mesh = meshIt->second;

							Rhi::ObjectConstants object = {};
							object.world                = objectInput.world;
							object.worldInverseTranspose =
								object.world.Inverse().Transpose();
							object.skinningInfo = Vec4(
								0.0f, objectInput.isSkinned ? 1.0f : 0.0f, 0.0f, 0.0f
							);

							Rhi::MaterialConstants material = {};
							uint32_t               textureId = 0;
							const MaterialBinding* materialBinding = fallbackMaterial;
							if (const auto matIt = mMaterialBindings.find(
								objectInput.materialInstanceId
							); matIt != mMaterialBindings.end()) {
								materialBinding = &matIt->second;
							}
							if (materialBinding) {
								material  = materialBinding->constants;
								textureId = materialBinding->albedoTextureId;
							}
							if (textureId == 0) {
								EnsureSpriteFallbackTexture(renderDevice);
								textureId = mSpriteFallbackTextureId;
							}

							Rhi::SkinningPaletteConstants skinPalette =
								identityPalette;
							if (
								objectInput.isSkinned &&
								objectInput.skeletonPaletteId < view.skinningPalettes.size()
							) {
								const auto& sourcePalette = view.skinningPalettes[
									objectInput.skeletonPaletteId];
								const uint32_t maxBones = std::min<uint32_t>(
									static_cast<uint32_t>(sourcePalette.boneMatrices.size()),
									Rhi::SkinningPaletteConstants::kMaxBones
								);
								for (uint32_t i = 0; i < maxBones; ++i) {
									skinPalette.bones[i] = sourcePalette.boneMatrices[i];
								}
							}

							const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
								allocator.AllocateConstantBuffer(&object, sizeof(object));
							const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
								allocator.AllocateConstantBuffer(
									&material, sizeof(material)
								);
							const D3D12_GPU_VIRTUAL_ADDRESS skinningCb =
								objectInput.isSkinned ?
									allocator.AllocateConstantBuffer(
										&skinPalette, sizeof(skinPalette)
									) :
									identitySkinCb;

							pass.SetVertexBuffer(mesh.vbv);
							pass.SetIndexBuffer(mesh.ibv);
							pass.BindGraphicsCbv(1, objectCb);
							pass.BindGraphicsCbv(2, materialCb);
							pass.BindGraphicsCbv(3, skinningCb);
							pass.BindGraphicsSrvTable(4, textureId);
							pass.DrawIndexedTest(mesh.indexCount);
						}
					}
				);

				mGraph.AddPass(
					prefix + "WorldBillboard",
					[colorId, depthId, worldBillboardTextureIds](RenderGraphBuilder& b
					) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
						for (const uint32_t texId : worldBillboardTextureIds) {
							b.ReadSrvPs(texId);
						}
					},
					[this, viewIndex, state, &renderDevice](RenderPassContext& pass
					) {
						const RenderViewInput& view = mFrameViews[viewIndex];
						if (view.worldBillboards.empty()) {
							return;
						}

						auto& allocator = static_cast<Rhi::D3D12Device&>(
							renderDevice.GetRhiDevice()
						).GetFrameUploadAllocator();
						Rhi::FrameConstants frame = BuildSceneFrameConstants(
							view.camera, state.width, state.height, 0.0f
						);
						const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
							allocator.AllocateConstantBuffer(&frame, sizeof(frame));

						const Mat4 cameraWorld = frame.view.Inverse();
						const Vec3 cameraRight = cameraWorld.GetRight().Normalized();
						const Vec3 cameraUp = cameraWorld.GetUp().Normalized();
						const Vec3 cameraForward = cameraWorld.GetForward().
							Normalized();

						pass.SetViewportAndScissor(
							0.0f,
							0.0f,
							static_cast<float>(state.width),
							static_cast<float>(state.height)
						);
						pass.SetSrvUavHeap();
						pass.SetRenderTargetAndDepth(
							std::span<const uint32_t>(&state.colorTextureId, 1),
							state.depthTextureId
						);
						pass.SetGraphicsPipeline(
							mBillboardPass.geom.rootSig, mBillboardPass.geom.pso
						);
						pass.BindGraphicsCbv(0, frameCb);
						pass.SetVertexBuffer(mBillboardPass.geom.vbv);
						pass.SetIndexBuffer(mBillboardPass.geom.ibv);

						for (const auto& billboard : view.worldBillboards) {
							const float cosine       = std::cos(billboard.rotationRad);
							const float sine         = std::sin(billboard.rotationRad);
							const Vec3  rotatedRight =
								cameraRight * cosine + cameraUp * sine;
							const Vec3 rotatedUp =
								cameraRight * -sine + cameraUp * cosine;

							Rhi::ObjectConstants object = {};
							object.world                = Mat4::identity;
							object.world.m[0][0] =
								rotatedRight.x * billboard.sizeWorld.x * 0.5f;
							object.world.m[0][1] =
								rotatedRight.y * billboard.sizeWorld.x * 0.5f;
							object.world.m[0][2] =
								rotatedRight.z * billboard.sizeWorld.x * 0.5f;
							object.world.m[1][0] =
								rotatedUp.x * billboard.sizeWorld.y * 0.5f;
							object.world.m[1][1] =
								rotatedUp.y * billboard.sizeWorld.y * 0.5f;
							object.world.m[1][2] =
								rotatedUp.z * billboard.sizeWorld.y * 0.5f;
							object.world.m[2][0]         = cameraForward.x;
							object.world.m[2][1]         = cameraForward.y;
							object.world.m[2][2]         = cameraForward.z;
							object.world.m[3][0]         = billboard.worldPosition.x;
							object.world.m[3][1]         = billboard.worldPosition.y;
							object.world.m[3][2]         = billboard.worldPosition.z;
							object.worldInverseTranspose =
								object.world.Inverse().Transpose();
							object.skinningInfo = Vec4(
								0.0f,
								0.0f,
								billboard.uvFlipY ? 1.0f : 0.0f,
								0.0f
							);

							Rhi::MaterialConstants material = {};
							material.baseColor              = billboard.color;
							material.opacity                = billboard.color.w;
							material.domainMode             = 0.0f;

							const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
								allocator.AllocateConstantBuffer(
									&object, sizeof(object)
								);
							const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
								allocator.AllocateConstantBuffer(
									&material, sizeof(material)
								);
							pass.BindGraphicsCbv(1, objectCb);
							pass.BindGraphicsCbv(2, materialCb);
							pass.BindGraphicsCbv(3, objectCb);
							pass.BindGraphicsSrvTable(
								4,
								ResolveSpriteTexture(renderDevice, billboard.texture)
							);
							pass.DrawIndexedTest(mBillboardPass.geom.indexCount);
						}
					}
				);

				mGraph.AddPass(
					prefix + "WorldSprite",
					[colorId, depthId, worldSpriteTextureIds](RenderGraphBuilder& b
					) {
						b.WriteRt(colorId);
						b.WriteDepth(depthId);
						for (const uint32_t texId : worldSpriteTextureIds) {
							b.ReadSrvPs(texId);
						}
					},
					[this, viewIndex, state, &renderDevice](RenderPassContext& pass
					) {
						const RenderViewInput& view = mFrameViews[viewIndex];
						if (view.worldSprites.empty()) {
							return;
						}

						auto& allocator = static_cast<Rhi::D3D12Device&>(
							renderDevice.GetRhiDevice()
						).GetFrameUploadAllocator();
						Rhi::FrameConstants frame = BuildSceneFrameConstants(
							view.camera, state.width, state.height, 0.0f
						);
						const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
							allocator.AllocateConstantBuffer(&frame, sizeof(frame));

						pass.SetViewportAndScissor(
							0.0f,
							0.0f,
							static_cast<float>(state.width),
							static_cast<float>(state.height)
						);
						pass.SetSrvUavHeap();
						pass.SetRenderTargetAndDepth(
							std::span<const uint32_t>(&state.colorTextureId, 1),
							state.depthTextureId
						);
						pass.SetGraphicsPipeline(
							mBillboardPass.geom.rootSig, mBillboardPass.geom.pso
						);
						pass.BindGraphicsCbv(0, frameCb);
						pass.SetVertexBuffer(mBillboardPass.geom.vbv);
						pass.SetIndexBuffer(mBillboardPass.geom.ibv);

						for (const auto& sprite : view.worldSprites) {
							Vec3 right = sprite.worldRight;
							Vec3 up    = sprite.worldUp;
							if (right.SqrLength() < 1e-6f) {
								right = Vec3::right;
							}
							if (up.SqrLength() < 1e-6f) {
								up = Vec3::up;
							}
							right = right.Normalized();
							up    = up.Normalized();
							const Vec3 normal = right.Cross(up).Normalized();

							const float cosine = std::cos(sprite.rotationRad);
							const float sine   = std::sin(sprite.rotationRad);
							const Vec3  rotatedRight = right * cosine + up * sine;
							const Vec3  rotatedUp = right * -sine + up * cosine;

							Rhi::ObjectConstants object = {};
							object.world                = Mat4::identity;
							object.world.m[0][0] =
								rotatedRight.x * sprite.sizeWorld.x * 0.5f;
							object.world.m[0][1] =
								rotatedRight.y * sprite.sizeWorld.x * 0.5f;
							object.world.m[0][2] =
								rotatedRight.z * sprite.sizeWorld.x * 0.5f;
							object.world.m[1][0] =
								rotatedUp.x * sprite.sizeWorld.y * 0.5f;
							object.world.m[1][1] =
								rotatedUp.y * sprite.sizeWorld.y * 0.5f;
							object.world.m[1][2] =
								rotatedUp.z * sprite.sizeWorld.y * 0.5f;
							object.world.m[2][0] = normal.x;
							object.world.m[2][1] = normal.y;
							object.world.m[2][2] = normal.z;
							object.world.m[3][0] = sprite.worldPosition.x;
							object.world.m[3][1] = sprite.worldPosition.y;
							object.world.m[3][2] = sprite.worldPosition.z;
							object.worldInverseTranspose =
								object.world.Inverse().Transpose();
							object.skinningInfo = Vec4(
								0.0f,
								0.0f,
								sprite.uvFlipY ? 1.0f : 0.0f,
								0.0f
							);

							Rhi::MaterialConstants material = {};
							material.baseColor              = sprite.color;
							material.opacity                = sprite.color.w;
							material.domainMode             = 0.0f;

							const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
								allocator.AllocateConstantBuffer(
									&object, sizeof(object)
								);
							const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
								allocator.AllocateConstantBuffer(
									&material, sizeof(material)
								);

							pass.BindGraphicsCbv(1, objectCb);
							pass.BindGraphicsCbv(2, materialCb);
							pass.BindGraphicsCbv(3, objectCb);
							pass.BindGraphicsSrvTable(
								4, ResolveSpriteTexture(renderDevice, sprite.texture)
							);
							pass.DrawIndexedTest(mBillboardPass.geom.indexCount);
						}
					}
				);

				uint32_t postFxInputId  = state.colorTextureId;
				uint32_t postFxOutputId = state.postFxTextureAId;
				for (size_t i = 0; i < mPostFxPasses.size(); ++i) {
					const auto passRes = mPostFxPasses[i];
					const auto inId    = postFxInputId;
					const auto outId   = postFxOutputId;

					mGraph.AddPass(
						prefix + "PostFx_" + passRes.name,
						[inId, outId](RenderGraphBuilder& b) {
							b.ReadSrvPs(inId);
							b.WriteRt(outId);
						},
						[this, passRes, inId, outId, state, &renderDevice](
						RenderPassContext& pass
					) {
							pass.SetViewportAndScissor(
								0.0f,
								0.0f,
								static_cast<float>(state.width),
								static_cast<float>(state.height)
							);
							pass.SetSrvUavHeap();

							auto* pso = renderDevice.GetPipelineCache().
								GetOrCreateGraphicsPso(passRes.pass.psoKey);
							pass.SetGraphicsPipeline(passRes.pass.rootSig, pso);
							pass.SetRenderTarget(outId);
							pass.BindGraphicsSrvTable(0, inId);
							pass.DrawFullscreenTriangle();
						}
					);

					postFxInputId  = outId;
					postFxOutputId = postFxOutputId == state.postFxTextureAId ?
						                 state.postFxTextureBId :
						                 state.postFxTextureAId;
				}
				outputId = postFxInputId;
			}

			if (view.type == RENDER_VIEW_TYPE::SPRITE_ONLY) {
				mGraph.AddPass(
					prefix + "Clear",
					[outputId](RenderGraphBuilder& b) {
						b.WriteRt(outputId);
						b.ClearColor(outputId, 0.0f, 0.0f, 0.0f, 0.0f);
					},
					[](RenderPassContext&) {}
				);
			}

			mGraph.AddPass(
				prefix + "ScreenSprites",
				[outputId, screenSpriteTextureIds](RenderGraphBuilder& b) {
					b.WriteRt(outputId);
					for (const uint32_t texId : screenSpriteTextureIds) {
						b.ReadSrvPs(texId);
					}
				},
				[this, viewIndex, state, outputId, &renderDevice](
				RenderPassContext& pass
			) {
					const RenderViewInput& view = mFrameViews[viewIndex];
					if (view.screenSprites.empty()) {
						return;
					}

					auto& allocator = static_cast<Rhi::D3D12Device&>(
						renderDevice.GetRhiDevice()
					).GetFrameUploadAllocator();

					Rhi::FrameConstants frame = {};
					frame.view                = Mat4::identity;
					frame.proj                = BuildOrthographic(
						state.width, state.height
					);
					frame.viewProj = frame.view * frame.proj;
					const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
						allocator.AllocateConstantBuffer(&frame, sizeof(frame));

					pass.SetViewportAndScissor(
						0.0f,
						0.0f,
						static_cast<float>(state.width),
						static_cast<float>(state.height)
					);
					pass.SetSrvUavHeap();
					pass.SetRenderTarget(outputId);
					pass.SetGraphicsPipeline(
						mSpritePass.geom.rootSig,
						mSpritePass.geom.pso
					);
					pass.BindGraphicsCbv(0, frameCb);
					pass.SetVertexBuffer(mSpritePass.geom.vbv);
					pass.SetIndexBuffer(mSpritePass.geom.ibv);

					for (const auto& sprite : view.screenSprites) {
						const Vec2 center = Vec2(
							sprite.positionPx.x +
							(0.5f - sprite.anchor.x) * sprite.sizePx.x,
							sprite.positionPx.y +
							(0.5f - sprite.anchor.y) * sprite.sizePx.y
						);

						Rhi::ObjectConstants object = {};
						object.world                = Mat4::Scale(
							               Vec3(
								               sprite.sizePx.x * 0.5f,
								               sprite.sizePx.y * 0.5f,
								               1.0f
							               )
						               ) * Mat4::RotateZ(
							               sprite.rotationRad
						               ) * Mat4::Translate(
							               Vec3(center.x, center.y, 0.0f)
						               );
						object.worldInverseTranspose =
							object.world.Inverse().Transpose();
						object.skinningInfo = Vec4(
							0.0f,
							0.0f,
							sprite.uvFlipY ? 1.0f : 0.0f,
							0.0f
						);

						Rhi::MaterialConstants material = {};
						material.baseColor              = sprite.color;
						material.opacity                = sprite.color.w;
						material.domainMode             = 0.0f;

						const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
							allocator.AllocateConstantBuffer(&object, sizeof(object));
						const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
							allocator.AllocateConstantBuffer(
								&material, sizeof(material)
							);

						pass.BindGraphicsCbv(1, objectCb);
						pass.BindGraphicsCbv(2, materialCb);
						pass.BindGraphicsCbv(3, objectCb);
						pass.BindGraphicsSrvTable(
							4, ResolveSpriteTexture(renderDevice, sprite.texture)
						);
						pass.DrawIndexedTest(mSpritePass.geom.indexCount);
					}
				}
			);

			mViewStates[view.viewKey].outputTextureId = outputId;
		}

		std::vector<uint32_t> uiReadableOutputs;
		uiReadableOutputs.reserve(frameViews.size());
		for (const RenderViewInput& view : frameViews) {
			if (!view.output.exposeToUi) {
				continue;
			}
			const auto it = mViewStates.find(view.viewKey);
			if (it == mViewStates.end() || it->second.outputTextureId == 0) {
				continue;
			}
			uiReadableOutputs.emplace_back(it->second.outputTextureId);
		}
		if (!uiReadableOutputs.empty()) {
			mGraph.AddPass(
				"PrepareUiViewOutputs",
				[uiReadableOutputs](RenderGraphBuilder& b) {
					for (const uint32_t texId : uiReadableOutputs) {
						b.ReadSrvPs(texId);
					}
				},
				[](RenderPassContext&) {}
			);
		}

		if (!mPresentViewKey.empty()) {
			const auto presentIt = mViewStates.find(mPresentViewKey);
			if (
				presentIt != mViewStates.end() &&
				presentIt->second.outputTextureId != 0
			) {
				const uint32_t presentTexture = presentIt->second.outputTextureId;
				mGraph.AddPass(
					"FullscreenSampleSrv",
					[presentTexture](RenderGraphBuilder& b) {
						b.ReadSrvPs(presentTexture);
						b.WriteBackBufferRt();
					},
					[this, presentTexture, &renderDevice](RenderPassContext& pass) {
						pass.SetViewportToBackBuffer();
						pass.SetSrvUavHeap();

						auto* pso = renderDevice.GetPipelineCache().
							GetOrCreateGraphicsPso(mFullscreenPass.psoKey);
						pass.SetGraphicsPipeline(mFullscreenPass.rootSig, pso);

						pass.BindGraphicsSrvTable(0, presentTexture);
						pass.DrawFullscreenTriangle();
					}
				);
			}
		} else {
			bool clearBackBuffer = false;
			for (const RenderViewInput& view : frameViews) {
				if (view.output.clearSwapChainWhenNotPresenting) {
					clearBackBuffer = true;
					break;
				}
			}
			if (clearBackBuffer) {
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
		}

		mGraph.AddPass(
			"ImGuiMainPass",
			[](RenderGraphBuilder& b) {
				b.WriteBackBufferRt();
			},
			[this](RenderPassContext& pass) {
				if (mUiMainRenderCallback) {
					mUiMainRenderCallback(pass);
				}
			}
		);
	}
}
