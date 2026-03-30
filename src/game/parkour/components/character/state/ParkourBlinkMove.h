#pragma once

#include "game/core/components/character/state/AirMove.h"

namespace Unnamed {
	class ParkourBlinkMove final : public AirMove {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;
		std::string_view GetStateName() override;
	};
}
