#pragma once
#include "rendergraph/RgResourceRegistry.h"

#include "shaders/PipelineCache.h"
#include "shaders/ShaderLibrary.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Rhi {
	class IRhiDevice;
}

namespace Unnamed::Render {
	class RenderDevice {
	public:
		RenderDevice(Rhi::IRhiDevice& rhiDevice, AssetManager& assetManager);
		~RenderDevice();

		[[nodiscard]] AssetManager&    GetAssetManager() const;
		[[nodiscard]] Rhi::IRhiDevice& GetRhiDevice() const;

		ShaderLibrary& GetShaderLibrary();
		PipelineCache& GetPipelineCache();

		void                OnResize(uint32_t width, uint32_t height);
		RgResourceRegistry& GetRegistry();

	private:
		void HookHotReload();

		Rhi::IRhiDevice& mRhiDevice;
		AssetManager&    mAssetManager;

		ShaderLibrary mShaderLibrary;
		PipelineCache mPipelineCache;

		RgResourceRegistry mRegistry;
	};
}
