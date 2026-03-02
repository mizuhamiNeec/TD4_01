#include "PipelineCache.h"

#include "d3dx12.h"
#include "ShaderLibrary.h"

namespace {
	const char* SemanticToString(const Unnamed::Rhi::VertexSemantic s) {
		using namespace Unnamed::Rhi;
		switch (s) {
			case VertexSemantic::POSITION: return "POSITION";
			case VertexSemantic::NORMAL: return "NORMAL";
			case VertexSemantic::TANGENT: return "TANGENT";
			case VertexSemantic::COLOR: return "COLOR";
			case VertexSemantic::TEXCOORD: return "TEXCOORD";
			default: return "UNKNOWN_SEMANTIC";
		}
	}

	DXGI_FORMAT ToDxgiFormat(const Unnamed::Rhi::VertexFormat format) {
		using Unnamed::Rhi::VertexFormat;
		switch (format) {
			case VertexFormat::FLOAT1: return DXGI_FORMAT_R32_FLOAT;
			case VertexFormat::FLOAT2: return DXGI_FORMAT_R32G32_FLOAT;
			case VertexFormat::FLOAT3: return DXGI_FORMAT_R32G32B32_FLOAT;
			case VertexFormat::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case VertexFormat::U_BYTE4_N: return DXGI_FORMAT_R8G8B8A8_UNORM;
			default: return DXGI_FORMAT_R32G32B32_FLOAT;
		}
	}
}

namespace Unnamed::Render {
	BuiltInputLayout BuildInputLayout(
		const Rhi::VertexLayoutDesc& layout
	) {
		BuiltInputLayout out;
		out.elements.reserve(layout.elements.size());

		for (const auto& e : layout.elements) {
			D3D12_INPUT_ELEMENT_DESC d = {};
			d.SemanticName             = SemanticToString(e.semantic);
			d.SemanticIndex            = e.semanticIndex;
			d.Format                   = ToDxgiFormat(e.format);
			d.InputSlot                = e.inputSlot;
			d.AlignedByteOffset        = e.offset;
			d.InputSlotClass           = e.perInstance ?
				                             D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA :
				                             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			d.InstanceDataStepRate = e.perInstance ? e.instanceStepRate : 0;

			out.elements.emplace_back(d);
		}

		return out;
	}

	PipelineCache::PipelineCache(
		ID3D12Device* device, ShaderLibrary& shaders
	) : mDevice(device),
	    mShaders(shaders) {}

	ID3D12PipelineState* PipelineCache::GetOrCreateGraphicsPso(
		const GraphicsPsoKey& key
	) {
		if (auto it = mGraphics.find(key); it != mGraphics.end()) {
			return it->second.Get();
		}

		const auto& vsDxil = mShaders.GetOrCreateDxil(key.vs);
		const auto& psDxil = mShaders.GetOrCreateDxil(key.ps);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature                     = key.rootSignature;

		desc.VS = {vsDxil.bytes.data(), vsDxil.bytes.size()};
		desc.PS = {psDxil.bytes.data(), psDxil.bytes.size()};

		desc.BlendState               = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.BlendState.RenderTarget[0].BlendEnable = key.blendEnable;
		desc.BlendState.RenderTarget[0].SrcBlend = key.srcBlend;
		desc.BlendState.RenderTarget[0].DestBlend = key.destBlend;
		desc.BlendState.RenderTarget[0].BlendOp = key.blendOp;
		desc.BlendState.RenderTarget[0].SrcBlendAlpha = key.srcBlendAlpha;
		desc.BlendState.RenderTarget[0].DestBlendAlpha = key.destBlendAlpha;
		desc.BlendState.RenderTarget[0].BlendOpAlpha = key.blendOpAlpha;
		desc.RasterizerState          = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.RasterizerState.CullMode = key.cullMode;

		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		desc.SampleMask = UINT_MAX;

		desc.NumRenderTargets = key.numRenderTargets;
		desc.RTVFormats[0]    = key.numRenderTargets > 0 ?
			                        key.rtvFormat :
			                        DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;

		if (key.depthEnable || key.stencilEnable) {
			desc.DSVFormat = key.dsvFormat;
		} else { desc.DSVFormat = DXGI_FORMAT_UNKNOWN; }

		if (key.depthEnable) {
			desc.DepthStencilState.DepthEnable    = TRUE;
			desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthStencilState.DepthFunc      = key.depthFunc;
		} else {
			desc.DepthStencilState.DepthEnable = FALSE;
			desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		}

		if (key.stencilEnable) {
			desc.DepthStencilState.StencilEnable    = TRUE;
			desc.DepthStencilState.StencilReadMask  = key.stencilReadMask;
			desc.DepthStencilState.StencilWriteMask = key.stencilWriteMask;

			desc.DepthStencilState.FrontFace.StencilFailOp =
				key.stencilFrontFailOp;
			desc.DepthStencilState.FrontFace.StencilDepthFailOp =
				key.stencilFrontDepthFailOp;
			desc.DepthStencilState.FrontFace.StencilPassOp =
				key.stencilFrontPassOp;
			desc.DepthStencilState.FrontFace.StencilFunc = key.stencilFrontFunc;

			desc.DepthStencilState.BackFace.StencilFailOp =
				key.stencilBackFailOp;
			desc.DepthStencilState.BackFace.StencilDepthFailOp =
				key.stencilBackDepthFailOp;
			desc.DepthStencilState.BackFace.StencilPassOp =
				key.stencilBackPassOp;
			desc.DepthStencilState.BackFace.StencilFunc = key.stencilBackFunc;
		} else { desc.DepthStencilState.StencilEnable = FALSE; }

		BuiltInputLayout builtInputLayout;
		if (key.vertexLayout.has_value()) {
			builtInputLayout = BuildInputLayout(*key.vertexLayout);
			desc.InputLayout = {
				builtInputLayout.elements.data(),
				static_cast<UINT>(builtInputLayout.elements.size())
			};
		} else { desc.InputLayout = {nullptr, 0}; }

		// 今のところ三角形リスト固定
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		const HRESULT hr = mDevice->CreateGraphicsPipelineState(
			&desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())
		);
		if (FAILED(hr)) { return nullptr; }

		auto* raw = pso.Get();
		mGraphics.emplace(key, std::move(pso));
		return raw;
	}

	ID3D12PipelineState* PipelineCache::GetOrCreateComputePso(
		const ComputePipelineKey& key
	) {
		if (const auto it = mCompute.find(key); it != mCompute.end()) {
			return it->second.Get();
		}

		const auto& csDxil = mShaders.GetOrCreateDxil(key.cs);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = key.rootSignature;
		desc.CS = {csDxil.bytes.data(), csDxil.bytes.size()};

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		const HRESULT hr = mDevice->CreateComputePipelineState(
			&desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())
		);
		if (FAILED(hr)) { return nullptr; }

		auto* raw = pso.Get();
		mCompute.emplace(key, std::move(pso));
		return raw;
	}

	void PipelineCache::InvalidateAll() {
		mGraphics.clear();
		mCompute.clear();
	}
}
