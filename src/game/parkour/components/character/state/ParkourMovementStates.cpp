#include "ParkourMovementStates.h"

#include "ParkourAirMove.h"
#include "ParkourBlinkMove.h"
#include "ParkourDuckMove.h"
#include "ParkourGroundMove.h"
#include "ParkourSlideMove.h"
#include "ParkourSpeedVaultMove.h"
#include "ParkourWallRunMove.h"

#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/components/character/state/NoclipMove.h"

namespace Unnamed {
	void RegisterParkourMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddState(std::make_shared<NoclipMove>());
		stateMachine.AddState(std::make_shared<ParkourGroundMove>());
		stateMachine.AddState(std::make_shared<ParkourDuckMove>());
		stateMachine.AddState(std::make_shared<ParkourAirMove>());
		stateMachine.AddState(std::make_shared<ParkourWallRunMove>());
		stateMachine.AddState(std::make_shared<ParkourSlideMove>());
		stateMachine.AddState(std::make_shared<ParkourSpeedVaultMove>());
		stateMachine.AddState(std::make_shared<ParkourBlinkMove>());
	}
}
