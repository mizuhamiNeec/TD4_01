#pragma once
#include <cstdint>
#include <dxgiformat.h>

#include <core/math/Vec4.h>

#include <engine/Properties.h>

#include "D3D12Types.h"

struct DepthStencilTexture;
struct RenderTargetTexture;
struct RenderPassTargets;
struct Vec4;
class D3D12;
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

		RenderPassTargets GetOffscreenPassTargets() const {
			return mOffscreenRenderPassTargets;
		}

		RenderTargetTexture* GetPostProcessedRtv() {
			return mPostProcessedRtv.get();
		}

		RenderTargetTexture* GetPostProcessedRtv() const {
			return mPostProcessedRtv.get();
		}

		DepthStencilTexture* GetPostProcessedDsv() {
			return mPostProcessedDsv.get();
		}

		DepthStencilTexture* GetPostProcessedDsv() const {
			return mPostProcessedDsv.get();
		}

		RenderTargetTexture* GetOffscreenRtv() { return mOffscreenRtv.get(); }

		RenderTargetTexture* GetOffscreenRtv() const {
			return mOffscreenRtv.get();
		}

		DepthStencilTexture* GetOffscreenDsv() { return mOffscreenDsv.get(); }

		DepthStencilTexture* GetOffscreenDsv() const {
			return mOffscreenDsv.get();
		}

	private:
		void Create(uint32_t width, uint32_t height, bool keepSrvIndices);

		D3D12*      mRenderer = nullptr;
		uint32_t    mWidth    = 0;
		uint32_t    mHeight   = 0;
		Vec4        mClearColor{};
		DXGI_FORMAT mRtvFormat = kBufferFormat;
		DXGI_FORMAT mDsvFormat = DXGI_FORMAT_D32_FLOAT;

		std::unique_ptr<RenderTargetTexture> mOffscreenRtv;
		std::unique_ptr<DepthStencilTexture> mOffscreenDsv;
		RenderPassTargets                    mOffscreenRenderPassTargets;

		std::unique_ptr<RenderTargetTexture> mPostProcessedRtv;
		std::unique_ptr<DepthStencilTexture> mPostProcessedDsv;
		RenderPassTargets                    mPostProcessedRenderPassTargets;
	};
}
