#pragma once

#include <cstdint>

#include <engine/renderer/D3D12.h>

class SrvManager;

namespace Unnamed {
	class RenderTargets {
	public:
		RenderTargets()  = default;
		~RenderTargets() = default;

		RenderTargets(const RenderTargets&)            = delete;
		RenderTargets& operator=(const RenderTargets&) = delete;

		void Init(
			D3D12*      renderer,
			uint32_t    width,
			uint32_t    height,
			Vec4        clearColor,
			DXGI_FORMAT rtvFormat,
			DXGI_FORMAT dsvFormat
		);

		void OnResize(uint32_t width, uint32_t height);

		const RenderPassTargets& GetOffscreenPassTargets() const {
			return mOffscreenRenderPassTargets;
		}

		RenderTargetTexture& GetPostProcessedRtv() { return mPostProcessedRtv; }

		const RenderTargetTexture& GetPostProcessedRtv() const {
			return mPostProcessedRtv;
		}

		DepthStencilTexture& GetPostProcessedDsv() { return mPostProcessedDsv; }

		const DepthStencilTexture& GetPostProcessedDsv() const {
			return mPostProcessedDsv;
		}

		RenderTargetTexture& GetOffscreenRtv() { return mOffscreenRtv; }

		const RenderTargetTexture& GetOffscreenRtv() const {
			return mOffscreenRtv;
		}

		DepthStencilTexture& GetOffscreenDsv() { return mOffscreenDsv; }

		const DepthStencilTexture& GetOffscreenDsv() const {
			return mOffscreenDsv;
		}

	private:
		void Create(uint32_t width, uint32_t height, bool keepSrvIndices);

		D3D12*      mRenderer = nullptr;
		uint32_t    mWidth    = 0;
		uint32_t    mHeight   = 0;
		Vec4        mClearColor{};
		DXGI_FORMAT mRtvFormat = kBufferFormat;
		DXGI_FORMAT mDsvFormat = DXGI_FORMAT_D32_FLOAT;

		RenderTargetTexture mOffscreenRtv;
		DepthStencilTexture mOffscreenDsv;
		RenderPassTargets   mOffscreenRenderPassTargets;

		RenderTargetTexture mPostProcessedRtv;
		DepthStencilTexture mPostProcessedDsv;
		RenderPassTargets   mPostProcessedRenderPassTargets;
	};
}
