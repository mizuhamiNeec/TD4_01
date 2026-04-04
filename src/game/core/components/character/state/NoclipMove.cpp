#include "NoclipMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "game/core/components/character/state/MovementStateIds.h"

namespace Unnamed {
	void NoclipMove::Enter(ConsoleSystem* console) {
		if (!console) {
			Error(
				"NoclipMove",
				"ConsoleSystem is not available."
			);
			return;
		}
		mConsole = console;
	}

	void NoclipMove::Tick(MovementContext& context, float deltaTime) {
#ifndef _DEBUG // Noclipはデバッグモードでのみ有効
		context;
		deltaTime;
		return;
#endif

		if (!mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = context.defaultAirStateName.empty() ?
				                         std::string(MovementStateIds::Air) :
				                         context.defaultAirStateName;
			return;
		}

		context.isGrounded = false; // Noclip中は常に空中にいるとみなす

		Vec3  wishDir  = GetWishDir(context); // Noclipは水平・垂直両方の入力を考慮する
		float maxSpeed =
			mConsole->GetConVarValueOr("sv_maxspeed", 320.0f) *
			mConsole->GetConVarValueOr("sv_noclipspeed", 5.0f);
		float wishSpeed = wishDir.Length() * maxSpeed;

		ApplyFriction(
			context.velocity,
			mConsole->GetConVarValueOr("sv_friction", 5.2f), deltaTime
		);

		Accelerate(
			context.velocity, wishDir, wishSpeed,
			mConsole->GetConVarValueOr("sv_noclipaccelerate", 5.0f),
			deltaTime
		);

		context.transform->SetPosition(
			context.transform->Position() + context.velocity * deltaTime
		);
	}

	void NoclipMove::Exit() {}

	std::string_view NoclipMove::GetStateName() {
		return MovementStateIds::Noclip;
	}

	void NoclipMove::ApplyFriction(
		Vec3& velocity, const float amount, const float deltaTime
	) {
		const float speedM = velocity.Length();
		const float speed  = Math::MtoH(speedM);
		if (speed < 0.1f) {
			return;
		}

		const float stop = mConsole->GetConVarValueOr("sv_stopspeed", speed);

		const float ctrl = speed < stop ? stop : speed;

		const float drop = ctrl * amount * deltaTime;

		float newSpeed = std::max(0.0f, speed - drop);

		if (newSpeed != speed) {
			newSpeed /= speed;
			velocity *= newSpeed;
		}
	}

	void NoclipMove::Accelerate(
		Vec3&       currentVel, const Vec3 wishDir, const float wishSpeed,
		const float accel,
		const float deltaTime
	) {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}

		const float maxNoclipSpeed =
			mConsole->GetConVarValueOr("sv_maxspeed", 320.0f) *
			mConsole->GetConVarValueOr("sv_noclipspeed", 5.0f);
		const float wishSpd = std::min(wishSpeed, maxNoclipSpeed);

		Vec3 currentHorizontal = Math::MtoH(currentVel);
		currentHorizontal.y    = 0.0f;

		const float cur = currentHorizontal.Dot(wishDir);
		const float add = wishSpd - cur;
		if (add <= 0.0f) {
			return;
		}

		const float acc = std::min(accel * wishSpd * deltaTime, add);
		currentVel      += Math::HtoM(acc) * wishDir;
	}
}
