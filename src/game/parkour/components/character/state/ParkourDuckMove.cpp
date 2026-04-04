#include "ParkourDuckMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/MovementStateIds.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	void ParkourDuckMove::Enter(ConsoleSystem* console) {
		ParkourGroundMove::Enter(console);
	}

	void ParkourDuckMove::Tick(
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
			context.requestedState = MovementStateIds::ParkourGround;
			return;
		}

		parkour->TickParkourTimers(deltaTime);
		auto& runtime = parkour->GetParkourRuntime();
		runtime.lastJumpHeld = context.input.jumpPressed;
		runtime.slide.active = false;

		if (parkour->TryStartBlink(context)) {
			return;
		}

		(void)parkour->SetDuckHullEnabled(context, true);

		context.isGrounded = false;
		if (mRebaseVelocityToSupportOnFirstTick) {
			context.velocity -= context.supportLinearVelocity;
			mRebaseVelocityToSupportOnFirstTick = false;
		}

		Vec3 wishDir = GetWishDirHoriz(context);
		context.velocity.y = 0.0f;

		if (!context.input.jumpPressed) {
			ApplyFriction(
				context.velocity,
				mConsole->GetConVarValueOr("park_duckfriction", 1.0f),
				deltaTime
			);
		}

		Accelerate(
			context.velocity,
			wishDir,
			mConsole->GetConVarValueOr("sv_duckspeed", 63.3f),
			mConsole->GetConVarValueOr("park_duckaccelerate", 4.0f),
			deltaTime
		);

		if (context.input.jumpPressed) {
			runtime.hasDoubleJump = true;
			ExecuteJumpAndLeaveGround(
				context, deltaTime, MovementStateIds::ParkourAir
			);
			return;
		}

		KinematicMoveQuery query = {
			.position    = context.transform->Position(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents,
			.deltaTime   = deltaTime
		};

		context.resolver->StepMove(
			query.position,
			query.velocity,
			mConsole->GetConVarValueOr("sv_stepheight", 18.0f),
			deltaTime
		);

		context.transform->SetPosition(query.position);
		context.velocity = query.velocity;

		Physics::Hit groundHit{};
		if (!IsGrounded(context.resolver, query.position, &groundHit)) {
			context.supportEntityGuid     = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta      = Vec3::zero;
			context.requestedState        = MovementStateIds::ParkourAir;
			return;
		}

		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;

		const float speedHu = parkour->GetHorizontalSpeedHu(context.velocity);
		if (context.input.crouchPressed && parkour->ShouldSlideFromSpeed(speedHu)) {
			context.requestedState = MovementStateIds::ParkourSlide;
			return;
		}

		if (!context.input.crouchPressed) {
			if (parkour->SetDuckHullEnabled(context, false)) {
				context.requestedState = MovementStateIds::ParkourGround;
			}
		}
	}

	void ParkourDuckMove::Exit() {}

	std::string_view ParkourDuckMove::GetStateName() {
		return MovementStateIds::ParkourDuck;
	}
}
