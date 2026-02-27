#include "ViewportSurface.h"

namespace Unnamed {
	ViewportSurface::ViewportSurface() = default;

	ViewportSurface::~ViewportSurface() = default;

	bool ViewportSurface::Initialize(
		const RendererContext& renderer, const ViewportSurfaceDesc& desc
	) {
		mHasDepth = desc.hasDepth;
		return CreateResources(renderer, desc.width, desc.height);
	}

	void ViewportSurface::Shutdown() {}

	bool ViewportSurface::Resize(
		const RendererContext& renderer, const uint32_t width,
		const uint32_t         height
	) {
		// サイズが0なら何もしない
		if (width == 0 || height == 0) { return false; }
		// サイズが同じなら何もしないで済ます
		if (width == mWidth && height == mHeight) { return true; }

		DestroyResources(renderer);
		return CreateResources(renderer, width, height);
	}

	uint32_t ViewportSurface::GetWidth() const { return mWidth; }

	uint32_t ViewportSurface::GetHeight() const { return mHeight; }

	uint64_t ViewportSurface::GetSrvGpuPtr() const noexcept {
		return mSrvGpuPtr;
	}

	bool ViewportSurface::CreateResources(
		const RendererContext& renderer, const uint32_t width,
		const uint32_t         height
	) {
		renderer;

		mWidth  = width;
		mHeight = height;

		return true;
	}

	void ViewportSurface::DestroyResources(const RendererContext& renderer) {
		renderer;
		mSrvGpuPtr = 0;
		mWidth     = 0;
		mHeight    = 0;
	}
}
