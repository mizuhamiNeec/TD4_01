#pragma once
#include "ShaderKey.h"

#include <d3d12.h>
#include <optional>
#include <unordered_map>

#include <wrl/client.h>

#include "engine/rhi/PipelineKey.h"

namespace Unnamed::Render {
	class ShaderLibrary;

	struct GraphicsPsoKey {
		ShaderKey vs;
		ShaderKey ps;

		ID3D12RootSignature*                 rootSignature = nullptr;
		std::optional<Rhi::VertexLayoutDesc> vertexLayout  = std::nullopt;

		uint8_t     numRenderTargets = 1;
		DXGI_FORMAT rtvFormat        = DXGI_FORMAT_R8G8B8A8_UNORM;

		bool                  depthEnable = false;
		DXGI_FORMAT           dsvFormat   = DXGI_FORMAT_UNKNOWN;
		D3D12_COMPARISON_FUNC depthFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		bool    stencilEnable    = false;
		uint8_t stencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
		uint8_t stencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		D3D12_STENCIL_OP      stencilFrontFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilFrontDepthFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilFrontPassOp = D3D12_STENCIL_OP_KEEP;
		D3D12_COMPARISON_FUNC stencilFrontFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		D3D12_STENCIL_OP      stencilBackFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilBackDepthFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilBackPassOp = D3D12_STENCIL_OP_KEEP;
		D3D12_COMPARISON_FUNC stencilBackFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		uint32_t        stencilRef     = 0;
		D3D12_CULL_MODE cullMode       = D3D12_CULL_MODE_BACK;
		bool            blendEnable    = false;
		D3D12_BLEND     srcBlend       = D3D12_BLEND_ONE;
		D3D12_BLEND     destBlend      = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  blendOp        = D3D12_BLEND_OP_ADD;
		D3D12_BLEND     srcBlendAlpha  = D3D12_BLEND_ONE;
		D3D12_BLEND     destBlendAlpha = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  blendOpAlpha   = D3D12_BLEND_OP_ADD;

		bool operator==(const GraphicsPsoKey& rhs) const {
			return vs == rhs.vs &&
			       ps == rhs.ps &&
			       rootSignature == rhs.rootSignature &&
			       numRenderTargets == rhs.numRenderTargets &&
			       rtvFormat == rhs.rtvFormat &&
			       depthEnable == rhs.depthEnable &&
			       dsvFormat == rhs.dsvFormat &&
			       depthFunc == rhs.depthFunc &&
			       stencilEnable == rhs.stencilEnable &&
			       stencilReadMask == rhs.stencilReadMask &&
			       stencilWriteMask == rhs.stencilWriteMask &&
			       stencilFrontFailOp == rhs.stencilFrontFailOp &&
			       stencilFrontDepthFailOp == rhs.stencilFrontDepthFailOp &&
			       stencilFrontPassOp == rhs.stencilFrontPassOp &&
			       stencilFrontFunc == rhs.stencilFrontFunc &&
			       stencilBackFailOp == rhs.stencilBackFailOp &&
			       stencilBackDepthFailOp == rhs.stencilBackDepthFailOp &&
			       stencilBackPassOp == rhs.stencilBackPassOp &&
			       stencilBackFunc == rhs.stencilBackFunc &&
			       stencilRef == rhs.stencilRef &&
			       cullMode == rhs.cullMode &&
			       blendEnable == rhs.blendEnable &&
			       srcBlend == rhs.srcBlend &&
			       destBlend == rhs.destBlend &&
			       blendOp == rhs.blendOp &&
			       srcBlendAlpha == rhs.srcBlendAlpha &&
			       destBlendAlpha == rhs.destBlendAlpha &&
			       blendOpAlpha == rhs.blendOpAlpha &&
			       vertexLayout == rhs.vertexLayout;
		}
	};

	struct ComputePipelineKey {
		ShaderKey            cs;
		ID3D12RootSignature* rootSignature = nullptr;

		bool operator==(const ComputePipelineKey& rhs) const {
			return cs == rhs.cs &&
			       rootSignature == rhs.rootSignature;
		}
	};

	inline void HashCombine(size_t& seed, size_t value) {
		seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
	}

	inline size_t HashVertexLayout(const Rhi::VertexLayoutDesc& layout) {
		size_t seed = 0;
		HashCombine(seed, std::hash<uint32_t>{}(layout.stride));
		HashCombine(seed, std::hash<size_t>{}(layout.elements.size()));

		for (const auto& e : layout.elements) {
			HashCombine(
				seed, std::hash<uint8_t>{}(static_cast<uint8_t>(e.semantic))
			);
			HashCombine(seed, std::hash<uint8_t>{}(e.semanticIndex));
			HashCombine(
				seed, std::hash<uint8_t>{}(static_cast<uint8_t>(e.format))
			);
			HashCombine(seed, std::hash<uint16_t>{}(e.offset));
			HashCombine(seed, std::hash<uint16_t>{}(e.inputSlot));
			HashCombine(seed, std::hash<bool>{}(e.perInstance));
			HashCombine(seed, std::hash<uint16_t>{}(e.instanceStepRate));
		}
		return seed;
	}

	struct GraphicsPipelineKeyHash {
		size_t operator()(const GraphicsPsoKey& k) const noexcept {
			size_t seed = 0;

			HashCombine(seed, ShaderKeyHash{}(k.vs));
			HashCombine(seed, ShaderKeyHash{}(k.ps));
			HashCombine(seed, std::hash<void*>{}(k.rootSignature));
			HashCombine(seed, std::hash<uint8_t>{}(k.numRenderTargets));
			HashCombine(seed, std::hash<int>{}(k.rtvFormat));

			HashCombine(seed, std::hash<bool>{}(k.depthEnable));
			HashCombine(seed, std::hash<int>{}(k.dsvFormat));
			HashCombine(seed, std::hash<int>{}(k.depthFunc));
			HashCombine(seed, std::hash<bool>{}(k.stencilEnable));
			HashCombine(seed, std::hash<uint8_t>{}(k.stencilReadMask));
			HashCombine(seed, std::hash<uint8_t>{}(k.stencilWriteMask));
			HashCombine(seed, std::hash<int>{}(k.stencilFrontFailOp));
			HashCombine(seed, std::hash<int>{}(k.stencilFrontDepthFailOp));
			HashCombine(seed, std::hash<int>{}(k.stencilFrontPassOp));
			HashCombine(seed, std::hash<int>{}(k.stencilFrontFunc));
			HashCombine(seed, std::hash<int>{}(k.stencilBackFailOp));
			HashCombine(seed, std::hash<int>{}(k.stencilBackDepthFailOp));
			HashCombine(seed, std::hash<int>{}(k.stencilBackPassOp));
			HashCombine(seed, std::hash<int>{}(k.stencilBackFunc));
			HashCombine(seed, std::hash<uint32_t>{}(k.stencilRef));
			HashCombine(seed, std::hash<int>{}(k.cullMode));
			HashCombine(seed, std::hash<bool>{}(k.blendEnable));
			HashCombine(seed, std::hash<int>{}(k.srcBlend));
			HashCombine(seed, std::hash<int>{}(k.destBlend));
			HashCombine(seed, std::hash<int>{}(k.blendOp));
			HashCombine(seed, std::hash<int>{}(k.srcBlendAlpha));
			HashCombine(seed, std::hash<int>{}(k.destBlendAlpha));
			HashCombine(seed, std::hash<int>{}(k.blendOpAlpha));

			HashCombine(seed, std::hash<bool>{}(k.vertexLayout.has_value()));
			if (k.vertexLayout.has_value()) {
				HashCombine(seed, HashVertexLayout(*k.vertexLayout));
			}

			return seed;
		}
	};

	struct GraphicsPipelineKeyEqual {
		bool operator()(
			const GraphicsPsoKey& a, const GraphicsPsoKey& b
		) const {
			if (a.vs != b.vs) {
				return false;
			}
			if (a.ps != b.ps) {
				return false;
			}
			if (a.rootSignature != b.rootSignature) {
				return false;
			}
			if (a.numRenderTargets != b.numRenderTargets) {
				return false;
			}
			if (a.rtvFormat != b.rtvFormat) {
				return false;
			}
			if (a.depthEnable != b.depthEnable) {
				return false;
			}
			if (a.dsvFormat != b.dsvFormat) {
				return false;
			}
			if (a.depthFunc != b.depthFunc) {
				return false;
			}
			if (a.stencilEnable != b.stencilEnable) {
				return false;
			}
			if (a.stencilReadMask != b.stencilReadMask) {
				return false;
			}
			if (a.stencilWriteMask != b.stencilWriteMask) {
				return false;
			}
			if (a.stencilFrontFailOp != b.stencilFrontFailOp) {
				return false;
			}
			if (a.stencilFrontDepthFailOp != b.stencilFrontDepthFailOp) {
				return false;
			}
			if (a.stencilFrontPassOp != b.stencilFrontPassOp) {
				return false;
			}
			if (a.stencilFrontFunc != b.stencilFrontFunc) {
				return false;
			}
			if (a.stencilBackFailOp != b.stencilBackFailOp) {
				return false;
			}
			if (a.stencilBackDepthFailOp != b.stencilBackDepthFailOp) {
				return false;
			}
			if (a.stencilBackPassOp != b.stencilBackPassOp) {
				return false;
			}
			if (a.stencilBackFunc != b.stencilBackFunc) {
				return false;
			}
			if (a.stencilRef != b.stencilRef) {
				return false;
			}
			if (a.cullMode != b.cullMode) {
				return false;
			}
			if (a.blendEnable != b.blendEnable) {
				return false;
			}
			if (a.srcBlend != b.srcBlend) {
				return false;
			}
			if (a.destBlend != b.destBlend) {
				return false;
			}
			if (a.blendOp != b.blendOp) {
				return false;
			}
			if (a.srcBlendAlpha != b.srcBlendAlpha) {
				return false;
			}
			if (a.destBlendAlpha != b.destBlendAlpha) {
				return false;
			}
			if (a.blendOpAlpha != b.blendOpAlpha) {
				return false;
			}
			if (a.vertexLayout.has_value() != b.vertexLayout.has_value()) {
				return false;
			}
			if (a.vertexLayout.has_value()) {
				if (*a.vertexLayout != *b.vertexLayout) {
					return false;
				}
			}

			return true;
		}
	};

	struct ComputePipelineKeyHash {
		size_t operator()(const ComputePipelineKey& k) const noexcept {
			size_t h  = 0;
			auto   Hc = [&](size_t x) {
				h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
			};

			Hc(ShaderKeyHash{}(k.cs));
			Hc(std::hash<void*>{}(k.rootSignature));

			return h;
		}
	};

	struct BuiltInputLayout {
		std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
	};

	BuiltInputLayout BuildInputLayout(
		const Rhi::VertexLayoutDesc& layout
	);

	class PipelineCache {
	public:
		PipelineCache(ID3D12Device* device, ShaderLibrary& shaders);

		ID3D12PipelineState* GetOrCreateGraphicsPso(
			const GraphicsPsoKey& key
		);
		ID3D12PipelineState* GetOrCreateComputePso(
			const ComputePipelineKey& key
		);

		void InvalidateAll();

	private:
		ID3D12Device*  mDevice = nullptr;
		ShaderLibrary& mShaders;

		std::unordered_map<
			GraphicsPsoKey,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>,
			GraphicsPipelineKeyHash
		> mGraphics;
		std::unordered_map<
			ComputePipelineKey,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>,
			ComputePipelineKeyHash
		> mCompute;
	};
}
