#include "ParkourAirMove.h"

#include <algorithm>
#include <cfloat>

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/MovementStateIds.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	ParkourAirMove::~ParkourAirMove() = default;

	void ParkourAirMove::Enter(ConsoleSystem* console) {
		AirMove::Enter(console);
	}

	void ParkourAirMove::Tick(MovementContext& context, float deltaTime) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = MovementStateIds::Noclip;
			return;
		}

		auto* parkour = dynamic_cast<ParkourMovementComponent*>(
			context.movementComponent
		);
		auto syncJumpHeld = [&context, parkour]() {
			if (parkour) {
				parkour->GetParkourRuntime().lastJumpHeld = context.input.
					jumpPressed;
			}
		};

		if (parkour) {
			parkour->TickParkourTimers(deltaTime);
			if (parkour->TryStartBlink(context)) {
				syncJumpHeld();
				return;
			}
			if (parkour->TryStartSpeedVault(context)) {
				syncJumpHeld();
				return;
			}
			if (parkour->TryStartWallRun(context)) {
				syncJumpHeld();
				return;
			}

			auto&      runtime         = parkour->GetParkourRuntime();
			const bool jumpPressed     = context.input.jumpPressed;
			const bool jumpPressedEdge = jumpPressed && !runtime.lastJumpHeld;
			if (jumpPressedEdge && runtime.hasDoubleJump) {
				context.velocity.y = Math::HtoM(
					mConsole->GetConVarValueOr(
						"park_doublejump_velocity", 300.0f
					)
				);
				runtime.hasDoubleJump            = false;
				runtime.pendingDoubleJumpCue     = true;
				context.jumpSnapDisableRemaining = std::max(
					context.jumpSnapDisableRemaining,
					mConsole->GetConVarValueOr("sv_jumpsnapdisabletime", 0.1f)
				);
			}

			(void)parkour->SetDuckHullEnabled(
				context, context.input.crouchPressed
			);
		}

		context.isGrounded               = false;
		context.supportEntityGuid        = 0;
		context.supportLinearVelocity    = Vec3::zero;
		context.supportStepDelta         = Vec3::zero;
		context.jumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining - deltaTime
		);

		const Vec3 right   = context.input.right.Normalized();
		const Vec3 forward = context.input.forward.Normalized();

		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f;
		wishDir.Normalize();

		ApplyHalfGravity(context.velocity, deltaTime);
		AirAccelerate(
			context.velocity,
			wishDir,
			320.0f,
			mConsole->GetConVarValueOr("sv_airaccelerate", FLT_MAX),
			deltaTime
		);
		ApplyHalfGravity(context.velocity, deltaTime);

		KinematicMoveQuery query = {
			.position    = context.transform->Position(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents.IsZero() ?
				               Math::HtoM(Vec3(32.0f, 73.0f, 32.0f)) * 0.5f :
				               context.halfExtents,
			.deltaTime = deltaTime
		};

		context.resolver->SlideMove(
			query.position,
			query.velocity,
			deltaTime
		);

		context.transform->SetPosition(query.position);
		context.velocity = query.velocity;

		Physics::Hit groundHit{};
		if (context.jumpSnapDisableRemaining <= 0.0f &&
		    context.velocity.y <= 0.0f &&
		    IsGrounded(
			    context.resolver,
			    context.transform->Position(),
			    &groundHit
		    )) {
			context.velocity.y        = 0.0f;
			context.isGrounded        = true;
			context.supportEntityGuid = groundHit.hitEntityGuid;
			context.requestedState    = parkour ?
				                            parkour->
				                            ResolveGroundStateFromInput(
					                            context
				                            ) :
				                            std::string(
					                            MovementStateIds::ParkourGround
				                            );
			if (parkour) {
				parkour->GetParkourRuntime().hasDoubleJump = true;
			}
		}

		syncJumpHeld();
	}

	void ParkourAirMove::Exit() {
		AirMove::Exit();
	}

	std::string_view ParkourAirMove::GetStateName() {
		return MovementStateIds::ParkourAir;
	}
}
