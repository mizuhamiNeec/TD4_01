#pragma once
#include "ParkourMovementStates.h"

#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/components/character/state/GroundMove.h"
#include "game/core/components/character/state/interface/IMovementState.h"

namespace Unnamed {
	class ParkourGroundMove final : public GroundMove {
	public:
		explicit ParkourGroundMove();

		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;

		std::string GetStateName() override;

	private:
		ConVar<float>* mJumpVelocity = nullptr;
	};
}
