#pragma once

#include "ParkourGroundMove.h"

namespace Unnamed {
	class ParkourDuckMove final : public ParkourGroundMove {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;
		std::string_view GetStateName() override;
	};
}
