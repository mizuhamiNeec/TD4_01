#include "RenderGraph.h"

#include <unordered_set>
#include <utility>

#include "engine/rhi/d3d12/D3D12CommandContext.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12SwapChain.h"

namespace Unnamed::Render {
	void RenderGraphBuilder::WriteBackBufferRt() {
		mUses.emplace_back(kBackBufferId, RG_ACCESS::RENDER_TARGET);
	}

	void RenderGraphBuilder::WriteUav(uint32_t texId) {
		mUses.emplace_back(texId, RG_ACCESS::UAV_WRITE);
	}

	void RenderGraphBuilder::ReadSrv(uint32_t texId) {
		mUses.emplace_back(texId, RG_ACCESS::SRV_READ);
	}

	const std::vector<RgUse>& RenderGraphBuilder::GetUses() { return mUses; }

	void RenderGraph::AddPass(
		const std::string&                              name,
		const std::function<void(RenderGraphBuilder&)>& setup,
		std::function<void(Rhi::IRhiCommandContext&)>   execute
	) {
		RenderGraphBuilder builder;
		setup(builder);

		RenderPass pass = {};
		pass.name       = name;
		pass.uses       = builder.GetUses();
		pass.execute    = std::move(execute);

		mPasses.emplace_back(pass);
	}

	void RenderGraph::Execute(Rhi::IRhiDevice& device) {
		const auto& dxDevice    = static_cast<Rhi::D3D12Device&>(device);
		auto*       commandList = dxDevice.GetCommandList();
		auto*       swapChain   = dxDevice.GetD3D12SwapChain();

		if (!mStatesInitialized) {
			mGlobalStates[RenderGraphBuilder::kBackBufferId] =
				D3D12_RESOURCE_STATE_PRESENT;

			// Intermediate は CreateCommittedResource の InitialState に合わせる
			mGlobalStates[1] = D3D12_RESOURCE_STATE_COMMON;

			mStatesInitialized = true;
		} else {
			// BackBuffer は Present から始まる、で固定
			mGlobalStates[RenderGraphBuilder::kBackBufferId] =
				D3D12_RESOURCE_STATE_PRESENT;
		}

		Rhi::D3D12CommandContext context(commandList, swapChain);
		context.Begin();

		// 状態をトラッキング
		mGlobalStates[RenderGraphBuilder::kBackBufferId] =
			D3D12_RESOURCE_STATE_PRESENT;

		auto ResolveResource = [&](const uint32_t id) -> ID3D12Resource* {
			if (id == RenderGraphBuilder::kBackBufferId) {
				const uint32_t bbIndex = swapChain->GetCurrentBackBufferIndex();
				return swapChain->GetBackBuffer(bbIndex);
			}
			// Intermediateリソースのみ対応
			return dxDevice.GetIntermediate();
		};

		auto RequiredState = [&
			](const RG_ACCESS access) -> D3D12_RESOURCE_STATES {
			switch (access) {
				case RG_ACCESS::RENDER_TARGET: {
					return D3D12_RESOURCE_STATE_RENDER_TARGET;
				}
				case RG_ACCESS::UAV_WRITE: {
					return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}
				case RG_ACCESS::SRV_READ: {
					return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}
				default: return D3D12_RESOURCE_STATE_COMMON;
			}
		};

		std::unordered_set<uint32_t> uavWritten;

		for (auto& pass : mPasses) {
			// 必要遷移を挿入
			for (const auto& [textureId, access] : pass.uses) {
				ID3D12Resource* res = ResolveResource(textureId);
				const auto      req = RequiredState(access);
				auto&           cur = mGlobalStates[textureId];

				if (cur != req) {
					context.TransitionResource(res, cur, req);
					cur = req;
				}

				if (access == RG_ACCESS::UAV_WRITE) {
					uavWritten.insert(textureId);
				}
			}

			// UAV-->次読み取りの可視性を確保
			for (const auto& use : pass.uses) {
				if (
					use.access == RG_ACCESS::SRV_READ &&
					uavWritten.contains(use.textureId)
				) {
					D3D12_RESOURCE_BARRIER uav{};
					uav.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
					uav.UAV.pResource = ResolveResource(use.textureId);
					commandList->ResourceBarrier(1, &uav);

					uavWritten.erase(use.textureId);
				}
			}

			pass.execute(context);
		}

		// 最後にバックバッファをPresent状態に
		{
			ID3D12Resource* bb = ResolveResource(
				RenderGraphBuilder::kBackBufferId
			);
			auto& cur = mGlobalStates[RenderGraphBuilder::kBackBufferId];
			if (cur != D3D12_RESOURCE_STATE_PRESENT) {
				context.TransitionResource(
					bb, cur, D3D12_RESOURCE_STATE_PRESENT
				);
				cur = D3D12_RESOURCE_STATE_PRESENT;
			}
		}

		context.End();
	}
}
