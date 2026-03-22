#include "ParkourMovementStates.h"

#include "ParkourAirMove.h"
#include "ParkourGroundMove.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

namespace Unnamed {
	void RegisterParkourMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddState(std::make_shared<ParkourGroundMove>());
		stateMachine.AddState(std::make_shared<ParkourAirMove>());
	}
}
