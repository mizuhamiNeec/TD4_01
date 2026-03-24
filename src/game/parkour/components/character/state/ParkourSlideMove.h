#pragma once
#include "ParkourGroundMove.h"

#include "game/core/components/character/state/interface/IMovementState.h"

namespace Unnamed {
	/// @brief パルクール用のスライディング移動状態
	class ParkourSlideMove : public ParkourGroundMove {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;
		std::string_view GetStateName() override;
	};
}
