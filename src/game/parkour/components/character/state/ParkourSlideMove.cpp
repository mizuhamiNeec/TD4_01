#include "ParkourSlideMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	void ParkourSlideMove::Enter(ConsoleSystem* console) {
		ParkourGroundMove::Enter(console);
	}

	void ParkourSlideMove::Tick(
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
			context.requestedState = "ParkourGroundMove";
			return;
		}

		parkour->TickParkourTimers(deltaTime);
		if (parkour->TryStartBlink(context)) {
			return;
		}
		(void)parkour->SetDuckHullEnabled(context, true);

		auto& runtime        = parkour->GetParkourRuntime();
		runtime.lastJumpHeld = context.input.jumpPressed;
		if (!runtime.slide.active) {
			runtime.slide.active = true;
			runtime.slide.time   = 0.0f;

			Vec3 velHorz = context.velocity;
			velHorz.y    = 0.0f;
			if (velHorz.IsZero()) {
				velHorz   = GetWishDirHoriz(context);
				velHorz.y = 0.0f;
			}
			if (!velHorz.IsZero()) {
				velHorz.Normalize();
				runtime.slide.direction = velHorz;
			} else {
				runtime.slide.direction = Vec3::forward;
			}

			Vec3 horizontalVelocity = context.velocity;
			horizontalVelocity.y    = 0.0f;
			float boostedSpeed      = horizontalVelocity.Length() + Math::HtoM(
				                     mConsole->GetConVarValueOr(
					                     "park_slide_boostspeed", 50.0f
				                     )
			                     );
			boostedSpeed = std::min(
				boostedSpeed,
				Math::HtoM(
					mConsole->GetConVarValueOr(
						"park_slide_hopspeedcap", 5000.0f
					)
				)
			);
			const float originalY = context.velocity.y;
			context.velocity      = runtime.slide.direction * boostedSpeed;
			context.velocity.y    = originalY;
		}

		context.isGrounded = false;
		if (mRebaseVelocityToSupportOnFirstTick) {
			context.velocity -= context.supportLinearVelocity;
			mRebaseVelocityToSupportOnFirstTick = false;
		}

		Vec3 wishDir       = GetWishDirHoriz(context);
		context.velocity.y = 0.0f;

		ApplyFriction(
			context.velocity,
			mConsole->GetConVarValueOr("park_duckfriction", 0.75f),
			deltaTime
		);

		Accelerate(
			context.velocity,
			wishDir,
			mConsole->GetConVarValueOr("sv_duckspeed", 63.3f),
			mConsole->GetConVarValueOr("park_duckaccelerate", 4.0f),
			deltaTime
		);

		if (context.input.jumpPressed) {
			runtime.slide.active = false;
			ExecuteJumpAndLeaveGround(context, deltaTime, "ParkourAirMove");
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
			runtime.slide.active          = false;
			context.supportEntityGuid     = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta      = Vec3::zero;
			context.requestedState        = "ParkourAirMove";
			return;
		}

		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;

		const Vec3 groundNormal = groundHit.normal;
		if (groundNormal.y < 0.999f && groundNormal.y > 0.0f) {
			const float gravity = Math::HtoM(
				mConsole->GetConVarValueOr("sv_gravity", 800.0f)
			);
			const Vec3  gravityVec = Vec3::down * gravity;
			const float dotGN      = gravityVec.Dot(groundNormal);
			const Vec3  slopeForce =
				(gravityVec - groundNormal * dotGN) *
				mConsole->GetConVarValueOr("park_slide_gravityscale", 1.75f);
			context.velocity += slopeForce * deltaTime;
		}

		runtime.slide.time += std::max(0.0f, deltaTime);
		if (runtime.slide.time >= mConsole->GetConVarValueOr(
			    "park_slide_maxtime", 20.0f
		    )) {
			runtime.slide.active   = false;
			context.requestedState = context.input.crouchPressed ?
				                         "ParkourDuckMove" :
				                         (parkour->CanStandAt(context) ?
					                          "ParkourGroundMove" :
					                          "ParkourDuckMove");
			return;
		}

		const float speedHu = parkour->GetHorizontalSpeedHu(context.velocity);
		if (speedHu < mConsole->GetConVarValueOr(
			    "park_slide_stopspeed", 50.0f
		    )) {
			runtime.slide.active   = false;
			context.requestedState = context.input.crouchPressed ?
				                         "ParkourDuckMove" :
				                         (parkour->CanStandAt(context) ?
					                          "ParkourGroundMove" :
					                          "ParkourDuckMove");
			return;
		}

		if (!context.input.crouchPressed) {
			runtime.slide.active   = false;
			context.requestedState = parkour->CanStandAt(context) ?
				                         "ParkourGroundMove" :
				                         "ParkourDuckMove";
		}
	}

	void ParkourSlideMove::Exit() {}

	std::string_view ParkourSlideMove::GetStateName() {
		return "ParkourSlideMove";
	}
}
