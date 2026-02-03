#pragma once
#include <string>

namespace Unnamed {
	enum class ENGINE_MODE {
		EDITOR,
		GAME,
	};

	enum class WINDOW_MODE : uint8_t {
		WINDOWED,
		BORDERLESS,
		FULLSCREEN,
	};

	struct EngineConfig {
		ENGINE_MODE mode;

		struct Window {
			std::string title     = "Unnamed Engine";
			int32_t     width     = 1280;
			int32_t     height    = 720;
			WINDOW_MODE mode      = WINDOW_MODE::WINDOWED;
			bool        resizable = true;
		} window;
	};
}
