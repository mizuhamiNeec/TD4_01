#include "ParkourMovementStates.h"

#include "ParkourGroundMove.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

namespace Unnamed {
	void RegisterParkourMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		// "GroundMove" を上書きし、入力遷移をパルクール向けに拡張する。
		stateMachine.AddState(std::make_shared<ParkourGroundMove>());
	}
}
