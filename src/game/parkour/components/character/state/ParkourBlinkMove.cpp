#include "ParkourBlinkMove.h"

#include "game/core/components/character/state/MovementStateIds.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	void ParkourBlinkMove::Enter(ConsoleSystem* console) {
		AirMove::Enter(console);
	}

	void ParkourBlinkMove::Tick(
		MovementContext& context,
		float            deltaTime
	) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = MovementStateIds::Noclip;
			return;
		}

		auto* parkour = dynamic_cast<ParkourMovementComponent*>(
			context.movementComponent
		);
		if (!parkour) {
			context.requestedState = MovementStateIds::ParkourAir;
			return;
		}

		parkour->TickParkourTimers(deltaTime);
		parkour->GetParkourRuntime().lastJumpHeld = context.input.jumpPressed;
		if (!parkour->UpdateBlink(context, deltaTime)) {
			context.requestedState = MovementStateIds::ParkourAir;
		}
	}

	void ParkourBlinkMove::Exit() {}

	std::string_view ParkourBlinkMove::GetStateName() {
		return MovementStateIds::ParkourBlink;
	}
}
