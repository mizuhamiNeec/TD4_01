#pragma once
#include "engine/platform/Window.h"

#include "interface/IGameView.h"

namespace Unnamed {
	class StandaloneGameView : public IGameView {
	public:
		explicit StandaloneGameView(Window& window) : mWindow(window) {}

		void GetRenderSize(
			uint32_t& outWidth, uint32_t& outHeight
		) const override {
			outWidth  = mWindow.GetDesc().width;
			outHeight = mWindow.GetDesc().height;
		}

		void BeginFrame() override {}
		void EndFrame() override {}

		[[nodiscard]] bool CanToggleFullscreen() const override { return true; }
		void ToggleFullscreen() override { mWindow.ToggleFullscreen(); }

	private:
		Window& mWindow;
	};
}
