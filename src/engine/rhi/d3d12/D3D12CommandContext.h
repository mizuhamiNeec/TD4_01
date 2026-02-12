#pragma once

#include "D3D12Device.h"

#include "engine/rhi/interface/IRhiCommandContext.h"

namespace Unnamed::Rhi {
	class D3D12CommandContext final : public IRhiCommandContext {
	public:
		D3D12CommandContext(
			ID3D12GraphicsCommandList* commandList, D3D12SwapChain* swapChain
		);

		void Begin() override;
		void End() override;

		void TransitionBackBufferToRenderTarget() override;
		void TransitionBackBufferToPresent() override;
		void ClearBackBuffer(const ClearColor& color) override;

		void SetSrvUavHeap(ID3D12DescriptorHeap* heap);

		void TransitionResource(
			ID3D12Resource*       resource, D3D12_RESOURCE_STATES before,
			D3D12_RESOURCE_STATES after
		);

		void SetComputePipeline(
			ID3D12RootSignature* rs, ID3D12PipelineState* pso
		);
		void SetGraphicsPipeline(
			ID3D12RootSignature* rs, ID3D12PipelineState* pso
		);

		void SetComputeRootUavTable(
			uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle
		);
		void SetGraphicsRootSrvTable(
			uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle
		);

		void Dispatch(uint32_t x, uint32_t y, uint32_t z);
		void DrawFullScreenTriangle();

	private:
		ID3D12GraphicsCommandList* mCommandList = nullptr;
		D3D12SwapChain*            mSwapChain   = nullptr;
	};
}
