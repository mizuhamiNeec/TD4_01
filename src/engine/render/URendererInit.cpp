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
		mGeometryPass.psoKey.dsvFormat   = DXGI_FORMAT_D24_UNORM_S8_UINT;
		mGeometryPass.psoKey.depthFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL;

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

		mPortalPass.maskPass.rootSig = dx.GetFsRootSignature();
		ResolveShaderProgramStageKey(
			renderDevice, portalMaskProgramId, "vs",
			mPortalPass.maskPass.psoKey.vs
		);
		ResolveShaderProgramStageKey(
			renderDevice, portalMaskProgramId, "ps",
			mPortalPass.maskPass.psoKey.ps
		);
		mPortalPass.maskPass.psoKey.rootSignature = mPortalPass.maskPass.
			rootSig;
		mPortalPass.maskPass.psoKey.vertexLayout = std::nullopt;
		mPortalPass.maskPass.psoKey.rtvFormat = mGeometryPass.psoKey.rtvFormat;
		mPortalPass.maskPass.psoKey.dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		mPortalPass.maskPass.psoKey.depthEnable = false;
		mPortalPass.maskPass.psoKey.stencilEnable = true;
		mPortalPass.maskPass.psoKey.stencilReadMask = 0xFF;
		mPortalPass.maskPass.psoKey.stencilWriteMask = 0xFF;
		mPortalPass.maskPass.psoKey.stencilFrontFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPass.psoKey.stencilFrontDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPass.psoKey.stencilFrontPassOp =
			D3D12_STENCIL_OP_REPLACE;
		mPortalPass.maskPass.psoKey.stencilFrontFunc =
			D3D12_COMPARISON_FUNC_ALWAYS;
		mPortalPass.maskPass.psoKey.stencilBackFailOp = D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPass.psoKey.stencilBackDepthFailOp =
			D3D12_STENCIL_OP_KEEP;
		mPortalPass.maskPass.psoKey.stencilBackPassOp =
			D3D12_STENCIL_OP_REPLACE;
		mPortalPass.maskPass.psoKey.stencilBackFunc =
			D3D12_COMPARISON_FUNC_ALWAYS;
		mPortalPass.maskPass.psoKey.stencilRef = 1;

		mPortalPass.sceneRootSig = dx.GetGeomRootSignature();
		mPortalPass.scenePsoKey = mGeometryPass.psoKey;
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
		mPortalPass.scenePsoKey.stencilRef = mPortalPass.stencilRef;

		mFrameCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight(), L"FrameConstants"
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
