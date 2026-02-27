#pragma once
#include <cstdint>

namespace Unnamed::Render {
	enum class RESOURCE_USAGE : uint8_t {
		NONE,
		RENDER_TARGET,
		PRESENT,
	};
}
