#include "ParkourGroundMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/MovementStateIds.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	ParkourGroundMove::ParkourGroundMove() = default;

	void ParkourGroundMove::Enter(ConsoleSystem* console) {
		GroundMove::Enter(console);
	}

	void ParkourGroundMove::Tick(
		MovementContext& context,
		const float      deltaTime
	) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = MovementStateIds::Noclip;
			return;
		}

		auto* parkour = dynamic_cast<ParkourMovementComponent*>(
			context.movementComponent
		);
		if (parkour) {
			parkour->TickParkourTimers(deltaTime);
			parkour->EndWallRun();
			auto& runtime      = parkour->GetParkourRuntime();
			runtime.hasDoubleJump = true;
			runtime.slide.active  = false;
			runtime.lastJumpHeld  = context.input.jumpPressed;
		}

		context.isGrounded = false;

		if (mRebaseVelocityToSupportOnFirstTick) {
			context.velocity -= context.supportLinearVelocity;
			mRebaseVelocityToSupportOnFirstTick = false;
		}

		if (parkour && !parkour->SetDuckHullEnabled(context, false)) {
			context.requestedState = MovementStateIds::ParkourDuck;
			return;
		}

		if (parkour && parkour->TryStartBlink(context)) {
			return;
		}

		if (context.input.crouchPressed) {
			context.requestedState = parkour ?
				parkour->ResolveGroundStateFromInput(context) :
				std::string(MovementStateIds::ParkourSlide);
			return;
		}

		Vec3 wishDir = GetWishDirHoriz(context);
		context.velocity.y = 0.0f;

		if (!context.input.jumpPressed) {
			ApplyFriction(
				context.velocity,
				mConsole->GetConVarValueOr("sv_friction", 5.2f),
				deltaTime
			);
		}

		Accelerate(
			context.velocity,
			wishDir,
			mConsole->GetConVarValueOr("sv_sprintspeed", 320.0f),
			mConsole->GetConVarValueOr("sv_accelerate", 10.0f),
			deltaTime
		);

		if (context.input.jumpPressed) {
			ExecuteJumpAndLeaveGround(
				context, deltaTime, MovementStateIds::ParkourAir
			);
			if (parkour) {
				parkour->GetParkourRuntime().hasDoubleJump = true;
			}
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
	}

	void ParkourGroundMove::Exit() {}

	std::string_view ParkourGroundMove::GetStateName() {
		return MovementStateIds::ParkourGround;
	}
}
