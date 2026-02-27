#pragma once
#include <string>

namespace Unnamed {
	enum class RUN_MODE {
		EDITOR,
		STANDALONE,
	};

	enum class WINDOW_MODE : uint8_t {
		WINDOWED,
		BORDERLESS,
		FULLSCREEN,
	};

	enum class BACKEND_TYPE : uint8_t {
		D3D12,
	};

	struct EngineConfig {
		RUN_MODE mode;

		struct Window {
			std::string title     = "Unnamed Engine";
			int32_t     width     = 1280;
			int32_t     height    = 720;
			WINDOW_MODE mode      = WINDOW_MODE::WINDOWED;
			bool        resizable = true;
		} window;
	};
}
