#include "ParkourWallRunMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/MovementStateIds.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	void ParkourWallRunMove::Enter(ConsoleSystem* console) {
		AirMove::Enter(console);
	}

	void ParkourWallRunMove::Tick(
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

		if (parkour->TryStartBlink(context)) {
			parkour->EndWallRun();
			return;
		}

		if (!parkour->UpdateWallRun(context, deltaTime)) {
			return;
		}

		const auto& runtime = parkour->GetParkourRuntime();
		context.isGrounded            = false;
		context.supportEntityGuid     = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta      = Vec3::zero;

		const float gravity = mConsole->GetConVarValueOr("sv_gravity", 800.0f);
		const float gravityScale = mConsole->GetConVarValueOr(
			"park_wallrun_gravityscale",
			0.1f
		);
		context.velocity.y -= Math::HtoM(gravity) * gravityScale * deltaTime;

		const float wishSpeed = mConsole->GetConVarValueOr("sv_sprintspeed", 320.0f);
		const Vec3 wishDir = runtime.wallRun.direction;

		if (context.input.moveAxis.z > 0.0f && !wishDir.IsZero()) {
			const float currentSpeed = Math::MtoH(context.velocity.Dot(wishDir));
			const float addSpeed     = wishSpeed * 1.2f - currentSpeed;
			if (addSpeed > 0.0f) {
				const float accel = mConsole->GetConVarValueOr("sv_airaccelerate", 10.0f) *
				                    1.5f;
				const float accelSpeed = std::min(
					accel * wishSpeed * deltaTime,
					addSpeed
				);
				context.velocity += Math::HtoM(accelSpeed) * wishDir;
			}
		} else {
			const float speedM = context.velocity.Length();
			const float speed = Math::MtoH(speedM);
			if (speed > 0.1f) {
				const float friction = mConsole->GetConVarValueOr("sv_friction", 5.2f);
				const float drop = speed * friction * deltaTime * 0.5f;
				const float nextSpeed = std::max(0.0f, speed - drop);
				if (nextSpeed != speed) {
					context.velocity *= (nextSpeed / speed);
				}
			}
		}

		const float intoWall = context.velocity.Dot(-runtime.wallRun.normal);
		if (intoWall > 0.0f) {
			context.velocity += runtime.wallRun.normal * intoWall;
		}
		const float pullForce = Math::HtoM(
			mConsole->GetConVarValueOr("park_wallrun_pullforce", 80.0f)
		);
		context.velocity += -runtime.wallRun.normal * pullForce * deltaTime;

		KinematicMoveQuery query = {
			.position    = context.transform->Position(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents,
			.deltaTime   = deltaTime
		};
		context.resolver->SlideMove(
			query.position,
			query.velocity,
			deltaTime
		);
		context.transform->SetPosition(query.position);
		context.velocity = query.velocity;

		Physics::Hit groundHit{};
		if (context.velocity.y <= 0.0f && IsGrounded(context.resolver, query.position, &groundHit)) {
			context.velocity.y        = 0.0f;
			context.isGrounded        = true;
			context.supportEntityGuid = groundHit.hitEntityGuid;
			parkour->EndWallRun();
			context.requestedState = parkour->ResolveGroundStateFromInput(context);
		}
	}

	void ParkourWallRunMove::Exit() {}

	std::string_view ParkourWallRunMove::GetStateName() {
		return MovementStateIds::ParkourWallRun;
	}
}
