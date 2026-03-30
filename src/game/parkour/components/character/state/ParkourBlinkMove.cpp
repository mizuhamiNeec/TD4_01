#include "ParkourBlinkMove.h"

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
			context.requestedState = "NoclipMove";
			return;
		}

		auto* parkour = dynamic_cast<ParkourMovementComponent*>(
			context.movementComponent
		);
		if (!parkour) {
			context.requestedState = "ParkourAirMove";
			return;
		}

		parkour->TickParkourTimers(deltaTime);
		parkour->GetParkourRuntime().lastJumpHeld = context.input.jumpPressed;
		if (!parkour->UpdateBlink(context, deltaTime)) {
			context.requestedState = "ParkourAirMove";
		}
	}

	void ParkourBlinkMove::Exit() {}

	std::string_view ParkourBlinkMove::GetStateName() {
		return "ParkourBlinkMove";
	}
}
