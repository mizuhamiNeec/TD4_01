#pragma once
#include <d3d12.h>
#include <functional>
#include <string>

#include "engine/render/rendercore/RenderHandles.h"
#include "engine/rhi/interface/IRhiCommandContext.h"

namespace Unnamed::Rhi {
	class IRhiDevice;
}

namespace Unnamed::Render {
	enum class RG_ACCESS : uint8_t {
		RENDER_TARGET,
		UAV_WRITE,
		SRV_READ,
	};

	struct RgUse {
		uint32_t  textureId = 0;
		RG_ACCESS access    = RG_ACCESS::SRV_READ;
	};

	class RenderGraphBuilder {
	public:
		static constexpr uint32_t kBackBufferId = 0;

		void WriteBackBufferRt();

		void WriteUav(uint32_t texId);

		void ReadSrv(uint32_t texId);

		const std::vector<RgUse>& GetUses();

	private:
		std::vector<RgUse> mUses;
	};

	struct RenderPass {
		std::string                                   name;
		std::vector<RgUse>                            uses;
		std::function<void(Rhi::IRhiCommandContext&)> execute;
	};

	enum class GRAPH_RESOURCE_STATE {
		UNKNOWN,
		PRESENT,
		RENDER_TARGET,
	};

	class RenderGraph {
	public:
		void AddPass(
			const std::string&                              name,
			const std::function<void(RenderGraphBuilder&)>& setup,
			std::function<void(Rhi::IRhiCommandContext&)>   execute
		);

		void Execute(Rhi::IRhiDevice& device);

	private:
		std::vector<RenderPass> mPasses;
		std::unordered_map<uint32_t, D3D12_RESOURCE_STATES>
		mGlobalStates;
		bool mStatesInitialized = false;
	};
}
