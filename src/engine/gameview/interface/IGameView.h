#pragma once
#include <cstdint>

namespace Unnamed {
	class IGameView {
	public:
		virtual ~IGameView() = default;

		virtual void GetRenderSize(
			uint32_t& outWidth, uint32_t& outHeight
		) const = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		[[nodiscard]] virtual bool CanToggleFullscreen() const = 0;
		virtual void               ToggleFullscreen() = 0;
	};
}
