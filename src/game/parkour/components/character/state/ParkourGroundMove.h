#pragma once

#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/components/character/state/GroundMove.h"
#include "game/core/components/character/state/interface/IMovementState.h"

namespace Unnamed {
	/// @brief パルクール用の地上移動状態
	class ParkourGroundMove final : public GroundMove {
	public:
		explicit ParkourGroundMove();

		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;

		std::string_view GetStateName() override;

	private:
	};
}
