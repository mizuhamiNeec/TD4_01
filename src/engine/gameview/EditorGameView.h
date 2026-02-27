#pragma once
#include "interface/IGameView.h"

#include <engine/Properties.h>

namespace Unnamed {
	class EditorGameView final : public IGameView {
	public:
		void SetDesiredSize(const uint32_t width, const uint32_t height) {
			mWidth  = width;
			mHeight = height;
		}

		void GetRenderSize(
			uint32_t& outWidth, uint32_t& outHeight
		) const override {
			outWidth  = mWidth;
			outHeight = mHeight;
		}

		void BeginFrame() override {}
		void EndFrame() override {}

		[[nodiscard]] uint64_t GetSrvGpuPtr() const { return mSrvGpuPtr; }

		// Fullscreen はサポートしない
		[[nodiscard]] bool CanToggleFullscreen() const override {
			return false;
		}

		void ToggleFullscreen() override {}

	private:
		uint32_t mWidth     = kClientWidth;
		uint32_t mHeight    = kClientHeight;
		uint64_t mSrvGpuPtr = 0;
	};
}
