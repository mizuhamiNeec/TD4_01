#pragma once
#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <string>
#include <unordered_map>

#include "RenderGraphBuilder.h"

#include "engine/render/RenderDevice.h"

namespace Unnamed::Rhi {
	class D3D12CommandContext;
	class IRhiDevice;
}

namespace Unnamed::Render {
	enum class RG_ACCESS : uint8_t;
	struct RgUse;
	class RenderGraphBuilder;
	class RenderPassContext;

	struct RgPass {
		std::string                  name;
		std::vector<RgUse>           uses;
		std::vector<RgClearCmd>      clearsColor;
		std::vector<RgDepthClearCmd> clearDepth;

		std::vector<uint32_t>   colorRts;
		std::optional<uint32_t> depthRt;

		std::function<void(RenderPassContext&)> execute;
	};

	struct CompiledTransition {
		uint32_t              textureId = 0;
		D3D12_RESOURCE_STATES before    = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_STATES after     = D3D12_RESOURCE_STATE_COMMON;
	};

	struct CompiledPass {
		std::vector<CompiledTransition> transitionsBefore;
		std::vector<uint32_t>           uavBarriersBefore;
		std::vector<uint32_t>           uavWritesInPass;

		std::vector<uint32_t>   colorRts;
		std::optional<uint32_t> depthRt;

		std::vector<RgClearCmd>      clearsBefore;
		std::vector<RgDepthClearCmd> clearDepthBefore;

		uint32_t passIndex = 0;
	};

	enum class GRAPH_RESOURCE_STATE {
		UNKNOWN,
		PRESENT,
		RENDER_TARGET,
	};

	class RenderGraph {
	public:
		static constexpr uint32_t kBackBufferId = 0xFFFF'FFFEu;

		void SetRenderDevice(RenderDevice& renderDevice);

		uint32_t CreateTexture(const RgTextureDesc& desc) const;

		void AddPass(
			std::string                                     name,
			const std::function<void(RenderGraphBuilder&)>& setup,
			std::function<void(RenderPassContext&)>         execute
		);

		void Compile(Rhi::IRhiDevice& device);

		void Execute(Rhi::IRhiDevice& device);

		void Invalidate();
		void Reset();

	private:
		void BeginPass(
			Rhi::IRhiDevice&           device,
			Rhi::D3D12CommandContext&  context,
			ID3D12GraphicsCommandList* commandList,
			RenderPassContext&         passContext,
			const CompiledPass&        cp
		);

		void EndPass(RenderPassContext& passContext, const CompiledPass& cp);

		static D3D12_RESOURCE_STATES RequiredState(RG_ACCESS access);
		static bool                  IsSrvRead(RG_ACCESS access);
		static bool                  IsUavWrite(RG_ACCESS access);

		ID3D12Resource* ResolveResource(
			Rhi::IRhiDevice& device, uint32_t id
		) const;

		void ExecutePasses(Rhi::IRhiDevice& device);

		RenderDevice* mRenderDevice = nullptr;

		std::vector<RgPass> mPasses;
		std::unordered_map<uint32_t, D3D12_RESOURCE_STATES>
		mGlobalStates;
		std::unordered_map<uint32_t, uint64_t> mKnownResourceRevisions;

		std::vector<CompiledPass> mCompiled;
		bool                      mIsDirty           = true;
		bool                      mStatesInitialized = false;
	};
}
