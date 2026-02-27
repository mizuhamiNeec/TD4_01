#include <pch.h>

#include <engine/renderer/RenderTargets.h>

#include "D3D12.h"

namespace Unnamed {
	void RenderTargets::Init(
		D3D12*            renderer,
		const uint32_t    width,
		const uint32_t    height,
		const Vec4        clearColor,
		const DXGI_FORMAT rtvFormat,
		const DXGI_FORMAT dsvFormat
	) {
		mRenderer   = renderer;
		mWidth      = width;
		mHeight     = height;
		mClearColor = clearColor;
		mRtvFormat  = rtvFormat;
		mDsvFormat  = dsvFormat;

		Create(width, height, false);
	}

	void RenderTargets::OnResize(const uint32_t width, const uint32_t height) {
		if (!mRenderer || width == 0 || height == 0) { return; }

		mRenderer->Flush();

		mWidth  = width;
		mHeight = height;

		// Resize 用のAPIがあるので呼ぶ（swapchainなども含む）
		mRenderer->Resize(width, height);

		Create(width, height, true);
	}

	void RenderTargets::Create(
		const uint32_t width,
		const uint32_t height,
		const bool     keepSrvIndices
	) {
		if (!mRenderer) { return; }

		mPostProcessedRtv->rtv.Reset();
		mPostProcessedDsv->dsv.Reset();

		if (keepSrvIndices) {
			mOffscreenRtv = std::make_unique<RenderTargetTexture>(
				mRenderer->CreateRenderTargetTexture(
					width,
					height,
					mClearColor,
					mOffscreenRtv->srvIndex,
					mRtvFormat
				)
			);
			mOffscreenDsv = std::make_unique<DepthStencilTexture>(
				mRenderer->CreateDepthStencilTexture(
					width,
					height,
					mOffscreenDsv->srvIndex,
					mDsvFormat
				)
			);

			mPostProcessedRtv = std::make_unique<RenderTargetTexture>(
				mRenderer->CreateRenderTargetTexture(
					width,
					height,
					mClearColor,
					mPostProcessedRtv->srvIndex,
					mRtvFormat
				)
			);
			mPostProcessedDsv = std::make_unique<DepthStencilTexture>(
				mRenderer->CreateDepthStencilTexture(
					width,
					height,
					mPostProcessedDsv->srvIndex,
					mDsvFormat
				)
			);
		} else {
			mOffscreenRtv = std::make_unique<RenderTargetTexture>(
				mRenderer->CreateRenderTargetTexture(
					width,
					height,
					mClearColor,
					mRtvFormat
				)
			);
			mOffscreenDsv = std::make_unique<DepthStencilTexture>(
				mRenderer->CreateDepthStencilTexture(
					width,
					height,
					mDsvFormat
				)
			);

			mPostProcessedRtv = std::make_unique<RenderTargetTexture>(
				mRenderer->CreateRenderTargetTexture(
					width,
					height,
					mClearColor,
					mRtvFormat
				)
			);
			mPostProcessedDsv = std::make_unique<DepthStencilTexture>(
				mRenderer->CreateDepthStencilTexture(
					width,
					height,
					mDsvFormat
				)
			);
		}

		mOffscreenRenderPassTargets = {
			.pRTVs        = &mOffscreenRtv->rtvHandle,
			.numRTVs      = 1,
			.pDSV         = &mOffscreenDsv->dsvHandle,
			.clearColor   = mClearColor,
			.clearDepth   = 1.0f,
			.clearStencil = 0,
			.bClearColor  = true,
			.bClearDepth  = true,
		};

		mPostProcessedRenderPassTargets = {
			.pRTVs        = &mPostProcessedRtv->rtvHandle,
			.numRTVs      = 1,
			.pDSV         = &mPostProcessedDsv->dsvHandle,
			.clearColor   = mClearColor,
			.clearDepth   = 1.0f,
			.clearStencil = 0,
			.bClearColor  = true,
			.bClearDepth  = true,
		};
	}
}
