#pragma once
#include <cstdint>

#include <engine/Properties.h>

namespace Unnamed {
	class RendererContext;

	struct ViewportSurfaceDesc {
		uint32_t width    = kClientWidth;
		uint32_t height   = kClientHeight;
		bool     hasDepth = true;
	};

	class ViewportSurface {
	public:
		ViewportSurface();
		~ViewportSurface();

		ViewportSurface(const ViewportSurface&)            = delete;
		ViewportSurface& operator=(const ViewportSurface&) = delete;

		bool Initialize(
			const RendererContext& renderer, const ViewportSurfaceDesc& desc
		);
		void Shutdown();

		bool Resize(
			const RendererContext& renderer, uint32_t width, uint32_t height
		);

		[[nodiscard]] uint32_t GetWidth() const;
		[[nodiscard]] uint32_t GetHeight() const;

		[[nodiscard]] uint64_t GetSrvGpuPtr() const noexcept;

	private:
		bool CreateResources(
			const RendererContext& renderer, uint32_t width, uint32_t height
		);
		void DestroyResources(const RendererContext& renderer);

		uint64_t mSrvGpuPtr = 0;
		uint32_t mWidth     = 0;
		uint32_t mHeight    = 0;
		bool     mHasDepth  = true;
	};
}
