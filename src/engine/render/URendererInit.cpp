#include "URenderer.h"

#include <string>

#include "RenderDevice.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"

namespace Unnamed::Render {
	namespace {
		AssetID LoadAsset(
			AssetManager&      assetManager,
			const std::string& path,
			const ASSET_TYPE   type
		) { return assetManager.LoadFromFile(path, type); }
	}

	bool URenderer::ResolveShaderProgramStageKey(
		RenderDevice& renderDevice, const AssetID shaderProgramId,
		const char*   stage, ShaderKey&           outKey
	) const {
		auto&       assetManager = renderDevice.GetAssetManager();
		const auto* program      = assetManager.Get<ShaderProgramAssetData>(
			shaderProgramId
		);
		if (!program) { return false; }

		const std::optional<ShaderProgramStage>* src = nullptr;
		if (std::string_view(stage) == "vs") { src = &program->vs; }
		if (std::string_view(stage) == "ps") { src = &program->ps; }
		if (std::string_view(stage) == "cs") { src = &program->cs; }
		if (!src || !src->has_value()) { return false; }

		const AssetID shaderSourceId = LoadAsset(
			assetManager, src->value().sourcePath, ASSET_TYPE::SHADER_SOURCE
		);
		if (shaderSourceId == kInvalidAssetID) { return false; }

		outKey.shaderSourceId = shaderSourceId;
		outKey.entry          = src->value().entry;
		outKey.profile        = src->value().profile;
		outKey.defines        = src->value().defines;
		return true;
	}

	void URenderer::Init(RenderDevice& renderDevice) {
		auto& assetManager = renderDevice.GetAssetManager();
		auto& dx = dynamic_cast<Rhi::D3D12Device&>(renderDevice.GetRhiDevice());

		const AssetID fullscreenProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/fullscreen_copy.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID depthVisProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/depth_vis.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID geomProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/pbr.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID csProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/cs_write_uav.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID portalMaskProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/portal_mask.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID portalCompositeProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/portal_composite.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID spriteOverlayProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/sprite_overlay.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);

		mFullscreenPass.rootSig = dx.GetFsRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, fullscreenProgramId, "vs", mFullscreenPass.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, fullscreenProgramId, "ps", mFullscreenPass.psoKey.ps
		);

		mFullscreenPass.psoKey.depthEnable = false;
		mFullscreenPass.psoKey.dsvFormat   = DXGI_FORMAT_UNKNOWN;
		mFullscreenPass.psoKey.depthFunc   = D3D12_COMPARISON_FUNC_ALWAYS;

		mFullscreenPass.psoKey.rootSignature = mFullscreenPass.rootSig;

		// フルスクリーンパスは頂点バッファ使わない
		mFullscreenPass.psoKey.vertexLayout = std::nullopt;

		mFullscreenPass.psoKey.rtvFormat = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);

		mDepthVisPass.rootSig = dx.GetFsRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, fullscreenProgramId, "vs", mDepthVisPass.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, depthVisProgramId, "ps", mDepthVisPass.psoKey.ps
		);
		mDepthVisPass.psoKey.rootSignature = mDepthVisPass.rootSig;
		mDepthVisPass.psoKey.vertexLayout  = std::nullopt;
		mDepthVisPass.psoKey.rtvFormat     = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);

		mComputePass.rootSig = dx.GetCsRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, csProgramId, "cs", mComputePass.psoKey.cs
		);
		mComputePass.psoKey.rootSignature = mComputePass.rootSig;

		mGeometryPass.rootSig = dx.GetGeomRootSignature();

		ResolveShaderProgramStageKey(
			renderDevice, geomProgramId, "vs", mGeometryPass.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, geomProgramId, "ps", mGeometryPass.psoKey.ps
		);

		mGeometryPass.psoKey.rootSignature = mGeometryPass.rootSig;

		mGeometryPass.psoKey.rtvFormat = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);
		mGeometryPass.psoKey.depthEnable = true;
		mGeometryPass.psoKey.dsvFormat   = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		mGeometryPass.psoKey.depthFunc   = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		mGeometryPass.psoKey.vertexLayout = Rhi::VertexLayoutDesc{
			.stride   = sizeof(float) * 16,
			.elements = {
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::POSITION,
					.semanticIndex    = 0,
					.format           = Rhi::VertexFormat::FLOAT3,
					.offset           = 0,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::NORMAL,
					.semanticIndex    = 0,
					.format           = Rhi::VertexFormat::FLOAT3,
					.offset           = sizeof(float) * 3,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::TEXCOORD,
					.semanticIndex    = 0,
					.format           = Rhi::VertexFormat::FLOAT2,
					.offset           = sizeof(float) * 6,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::TEXCOORD,
					.semanticIndex    = 1,
					.format           = Rhi::VertexFormat::FLOAT4,
					.offset           = sizeof(float) * 8,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::TEXCOORD,
					.semanticIndex    = 2,
					.format           = Rhi::VertexFormat::FLOAT4,
					.offset           = sizeof(float) * 12,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
			}
		};

		mPortalPass.maskPassGeom.rootSig = dx.GetGeomRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, portalMaskProgramId, "vs",
			mPortalPass.maskPassGeom.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, portalMaskProgramId, "ps",
			mPortalPass.maskPassGeom.psoKey.ps
		);
		mPortalPass.maskPassGeom.psoKey.rootSignature = mPortalPass.maskPassGeom
			.
			rootSig;
		mPortalPass.maskPassGeom.psoKey.vertexLayout = Rhi::VertexLayoutDesc{
			.stride   = sizeof(float) * 5,
			.elements = {
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::POSITION,
					.semanticIndex    = 0,
					.format           = Rhi::VertexFormat::FLOAT3,
					.offset           = 0,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
				Rhi::VertexElementDesc{
					.semantic         = Rhi::VertexSemantic::TEXCOORD,
					.semanticIndex    = 0,
					.format           = Rhi::VertexFormat::FLOAT2,
					.offset           = sizeof(float) * 3,
					.inputSlot        = 0,
					.perInstance      = false,
					.instanceStepRate = 0,
				},
			}
		};
		mPortalPass.maskPassGeom.psoKey.numRenderTargets = 0;
		mPortalPass.maskPassGeom.psoKey.rtvFormat        = DXGI_FORMAT_UNKNOWN;
		mPortalPass.maskPassGeom.psoKey.dsvFormat        =
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		mPortalPass.maskPassGeom.psoKey.depthEnable = true;
		mPortalPass.maskPassGeom.psoKey.depthFunc   =
			D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		mPortalPass.maskPassGeom.psoKey.stencilEnable      = true;
		mPortalPass.maskPassGeom.psoKey.stencilReadMask    = 0xFF;
		mPortalPass.maskPassGeom.psoKey.stencilWriteMask   = 0xFF;
		mPortalPass.maskPassGeom.psoKey.stencilFrontFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPassGeom.psoKey.stencilFrontDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPassGeom.psoKey.stencilFrontPassOp =
			D3D12_STENCIL_OP_REPLACE;
		mPortalPass.maskPassGeom.psoKey.stencilFrontFunc =
			D3D12_COMPARISON_FUNC_ALWAYS;
		mPortalPass.maskPassGeom.psoKey.stencilBackFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPassGeom.psoKey.stencilBackDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPassGeom.psoKey.stencilBackPassOp =
			D3D12_STENCIL_OP_REPLACE;
		mPortalPass.maskPassGeom.psoKey.stencilBackFunc =
			D3D12_COMPARISON_FUNC_ALWAYS;
		mPortalPass.maskPassGeom.psoKey.stencilRef = 1;
		mPortalPass.maskPassGeom.psoKey.cullMode   = D3D12_CULL_MODE_NONE;

		mPortalPass.compositeGeom.rootSig = dx.GetGeomRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, portalCompositeProgramId, "vs",
			mPortalPass.compositeGeom.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, portalCompositeProgramId, "ps",
			mPortalPass.compositeGeom.psoKey.ps
		);
		mPortalPass.compositeGeom.psoKey.rootSignature =
			mPortalPass.compositeGeom.rootSig;
		mPortalPass.compositeGeom.psoKey.vertexLayout =
			mPortalPass.maskPassGeom.psoKey.vertexLayout;
		mPortalPass.compositeGeom.psoKey.rtvFormat = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);
		mPortalPass.compositeGeom.psoKey.depthEnable = false;
		mPortalPass.compositeGeom.psoKey.dsvFormat   =
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		mPortalPass.compositeGeom.psoKey.stencilEnable      = true;
		mPortalPass.compositeGeom.psoKey.stencilReadMask    = 0xFF;
		mPortalPass.compositeGeom.psoKey.stencilWriteMask   = 0x00;
		mPortalPass.compositeGeom.psoKey.stencilFrontFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilFrontDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilFrontPassOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilFrontFunc =
			D3D12_COMPARISON_FUNC_EQUAL;
		mPortalPass.compositeGeom.psoKey.stencilBackFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilBackDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilBackPassOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.compositeGeom.psoKey.stencilBackFunc =
			D3D12_COMPARISON_FUNC_EQUAL;
		mPortalPass.compositeGeom.psoKey.cullMode = D3D12_CULL_MODE_NONE;

		mPortalPass.sceneRootSig = dx.GetGeomRootSignature();
		mPortalPass.scenePsoKey = mGeometryPass.psoKey;
		mPortalPass.scenePsoKey.depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		mPortalPass.scenePsoKey.stencilEnable = true;
		mPortalPass.scenePsoKey.stencilReadMask = 0xFF;
		mPortalPass.scenePsoKey.stencilWriteMask = 0x00;
		mPortalPass.scenePsoKey.stencilFrontFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilFrontDepthFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilFrontPassOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilFrontFunc = D3D12_COMPARISON_FUNC_EQUAL;
		mPortalPass.scenePsoKey.stencilBackFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilBackDepthFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilBackPassOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.scenePsoKey.stencilBackFunc = D3D12_COMPARISON_FUNC_EQUAL;
		mPortalPass.scenePsoKey.stencilRef = mPortalPass.stencilRefBase;

		mSpritePass.geom.rootSig = dx.GetGeomRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, spriteOverlayProgramId, "vs",
			mSpritePass.geom.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, spriteOverlayProgramId, "ps",
			mSpritePass.geom.psoKey.ps
		);
		mSpritePass.geom.psoKey.rootSignature = mSpritePass.geom.rootSig;
		mSpritePass.geom.psoKey.vertexLayout = mPortalPass.maskPassGeom.psoKey.vertexLayout;
		mSpritePass.geom.psoKey.rtvFormat = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);
		mSpritePass.geom.psoKey.depthEnable = false;
		mSpritePass.geom.psoKey.dsvFormat   = DXGI_FORMAT_UNKNOWN;
		mSpritePass.geom.psoKey.cullMode    = D3D12_CULL_MODE_NONE;
		mSpritePass.geom.psoKey.blendEnable = true;
		mSpritePass.geom.psoKey.srcBlend = D3D12_BLEND_SRC_ALPHA;
		mSpritePass.geom.psoKey.destBlend = D3D12_BLEND_INV_SRC_ALPHA;
		mSpritePass.geom.psoKey.srcBlendAlpha = D3D12_BLEND_ONE;
		mSpritePass.geom.psoKey.destBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;

		mBillboardPass.geom.rootSig = dx.GetGeomRootSignature();
		mBillboardPass.geom.psoKey = mSpritePass.geom.psoKey;
		mBillboardPass.geom.psoKey.rootSignature = mBillboardPass.geom.rootSig;
		mBillboardPass.geom.psoKey.depthEnable = true;
		mBillboardPass.geom.psoKey.dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		mBillboardPass.geom.psoKey.depthFunc =
			D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		mFrameCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight(), L"FrameConstants"
		);
		mPortalFrameCb.Init(
			dx.GetDevice(),
			dx.GetFramesInFlight() * kMaxPortalViews,
			L"PortalFrameConstants"
		);
		mObjectCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"ObjectCB"
		);
		mMaterialCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"MaterialCB"
		);
		mSkinningCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"SkinningPaletteCB"
		);

		CreateTriangleTestResources(dx);
		CreatePortalQuadResources(dx);
		mSpritePass.geom.vb         = mPortalPass.maskPassGeom.vb;
		mSpritePass.geom.ib         = mPortalPass.maskPassGeom.ib;
		mSpritePass.geom.vbv        = mPortalPass.maskPassGeom.vbv;
		mSpritePass.geom.ibv        = mPortalPass.maskPassGeom.ibv;
		mSpritePass.geom.indexCount = mPortalPass.maskPassGeom.indexCount;
		mBillboardPass.geom.vb         = mPortalPass.maskPassGeom.vb;
		mBillboardPass.geom.ib         = mPortalPass.maskPassGeom.ib;
		mBillboardPass.geom.vbv        = mPortalPass.maskPassGeom.vbv;
		mBillboardPass.geom.ibv        = mPortalPass.maskPassGeom.ibv;
		mBillboardPass.geom.indexCount = mPortalPass.maskPassGeom.indexCount;
		LoadSceneMeshResources(renderDevice, dx);
		LoadMaterialResources(renderDevice, dx);
		LoadPostFxChain(renderDevice);
		mSceneRenderWidth  = dx.GetSwapChain().GetWidth();
		mSceneRenderHeight = dx.GetSwapChain().GetHeight();
		renderDevice.GetRegistry().OnResize(
			mSceneRenderWidth,
			mSceneRenderHeight,
			dx.GetSwapChain().GetCurrentBackBufferIndex()
		);
		BuildGraph(renderDevice);
	}
}
