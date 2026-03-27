#include "ParkourMovementStates.h"

#include "ParkourAirMove.h"
#include "ParkourGroundMove.h"
#include "ParkourSlideMove.h"

#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/components/character/state/NoclipMove.h"

namespace Unnamed {
	void RegisterParkourMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddState(std::make_shared<NoclipMove>());
		stateMachine.AddState(std::make_shared<ParkourGroundMove>());
		stateMachine.AddState(std::make_shared<ParkourAirMove>());
		stateMachine.AddState(std::make_shared<ParkourSlideMove>());
	}
}
