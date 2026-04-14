#pragma once

#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	class Engine;

	class EngineServices {
	public:
		static Engine* Get() {
			return ServiceLocator::Get<Engine>();
		}
	};
}
