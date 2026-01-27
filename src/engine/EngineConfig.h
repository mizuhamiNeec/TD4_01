#pragma once

namespace Unnamed {
	enum class ENGINE_MODE {
		EDITOR,
		GAME,
	};

	struct EngineConfig {
		ENGINE_MODE mode;
	};
}
