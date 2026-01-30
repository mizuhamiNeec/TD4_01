#include <pch.h>

#include <engine/renderer/RenderTargets.h>

namespace Unnamed {
	void RenderTargets::Init(
		D3D12* renderer,
		uint32_t width,
		uint32_t height,
		Vec4 clearColor,
		DXGI_FORMAT rtvFormat,
		DXGI_FORMAT dsvFormat
	) {
		mRenderer = renderer;
		mWidth = width;
		mHeight = height;
		mClearColor = clearColor;
		mRtvFormat = rtvFormat;
		mDsvFormat = dsvFormat;

		Create(width, height, false);
	}

	void RenderTargets::OnResize(uint32_t width, uint32_t height) {
		if (!mRenderer || width == 0 || height == 0) { return; }

		mRenderer->Flush();

		mWidth = width;
		mHeight = height;

		// Resize 用のAPIがあるので呼ぶ（swapchainなども含む）
		mRenderer->Resize(width, height);

		Create(width, height, true);
	}

	void RenderTargets::Create(
		uint32_t width,
		uint32_t height,
		bool keepSrvIndices
	) {
		if (!mRenderer) { return; }

		mPostProcessedRtv.rtv.Reset();
		mPostProcessedDsv.dsv.Reset();

		if (keepSrvIndices) {
			mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
				width,
				height,
				mClearColor,
				mOffscreenRtv.srvIndex,
				mRtvFormat
			);
			mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
				width,
				height,
				mOffscreenDsv.srvIndex,
				mDsvFormat
			);

			mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
				width,
				height,
				mClearColor,
				mPostProcessedRtv.srvIndex,
				mRtvFormat
			);
			mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
				width,
				height,
				mPostProcessedDsv.srvIndex,
				mDsvFormat
			);
		}
		else {
			mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
				width,
				height,
				mClearColor,
				mRtvFormat
			);
			mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
				width,
				height,
				mDsvFormat
			);

			mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
				width,
				height,
				mClearColor,
				mRtvFormat
			);
			mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
				width,
				height,
				mDsvFormat
			);
		}

		mOffscreenRenderPassTargets = {
			.pRTVs = &mOffscreenRtv.rtvHandle,
			.numRTVs = 1,
			.pDSV = &mOffscreenDsv.dsvHandle,
			.clearColor = mClearColor,
			.clearDepth = 1.0f,
			.clearStencil = 0,
			.bClearColor = true,
			.bClearDepth = true,
		};

		mPostProcessedRenderPassTargets = {
			.pRTVs = &mPostProcessedRtv.rtvHandle,
			.numRTVs = 1,
			.pDSV = &mPostProcessedDsv.dsvHandle,
			.clearColor = mClearColor,
			.clearDepth = 1.0f,
			.clearStencil = 0,
			.bClearColor = true,
			.bClearDepth = true,
		};
	}
}
