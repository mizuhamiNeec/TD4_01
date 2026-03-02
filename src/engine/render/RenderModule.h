#pragma once
#include <cstdint>
#include <d3d12.h>
#include <memory>

#include "URenderer.h"
#include "frame/RenderFrameInputs.h"

#include "core/math/Vec2.h"

namespace Unnamed {
	namespace Rhi {
		class IRhiDevice;
	}

	class AssetManager;
}

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderDevice;

	class RenderModule {
	public:
		RenderModule(AssetManager& assetManager, Rhi::IRhiDevice& rhiDevice);

		void Init();
		void Tick(RenderFrameInputs& inputs) const;

		void OnResize(uint32_t width, uint32_t height) const;

		void SetUiCallbacks(
			URenderer::UiMainRenderCallback     mainRenderCallback,
			URenderer::UiPlatformRenderCallback platformRenderCallback
		) const;

		[[nodiscard]] SceneOutputView GetSceneOutputView() const;
		[[nodiscard]] uint32_t GetSceneOutputTextureId() const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSceneOutputSrvCpu() const;
		[[nodiscard]] Vec2 GetSceneOutputSize() const;
		void SetSceneRenderRequest(const SceneRenderRequest& request) const;

	private:
		AssetManager&    mAssetManager;
		Rhi::IRhiDevice& mRhiDevice;

		std::unique_ptr<RenderDevice> mRenderDevice;
		std::unique_ptr<URenderer>    mRenderer;
	};
}
