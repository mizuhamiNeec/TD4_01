#include "GroundMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundProbeDistanceHu = 2.0f;
		constexpr float kGroundNormalMinY      = 0.7f;
		constexpr float kJumpDetachBiasHu      = 1.0f;
	}

	void GroundMove::Enter(ConsoleSystem* console) {
		if (!console) {
			Error(
				"GroundMove",
				"ConsoleSystem is not available."
			);
			return;
		}
		mConsole = console;

		mRebaseVelocityToSupportOnFirstTick = true;
	}

	void GroundMove::Tick(
		MovementContext& context, const float deltaTime
	) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = "NoclipMove";
			return;
		}

		context.isGrounded = false;

		if (mRebaseVelocityToSupportOnFirstTick) {
			context.velocity -= context.supportLinearVelocity;
			mRebaseVelocityToSupportOnFirstTick = false;
		}

		// 移動方向
		Vec3 wishDir = GetWishDirHoriz(context);

		// デバッグ描画: 移動方向をレイで表示
		context.transform->GetWorld()->GetDebugDraw().DrawRay(
			context.transform->Position(),
			wishDir,
			Vec4::white
		);

		// 地上移動中は鉛直速度をリセット
		context.velocity.y = 0.0f;

		// ジャンプしていない場合は摩擦を適用(バニーホップ中か?)
		if (!context.input.jumpPressed) {
			ApplyFriction(
				context.velocity,
				mConsole->GetConVarValueOr("sv_friction", 5.2f), deltaTime
			);
		}

		// 移動状態によって最大速度を変える
		// しゃがみ入力優先
		float currentMaxSpeed = mConsole->GetConVarValueOr(
			"sv_walkspeed", 150.0f
		);
		if (context.input.sprintPressed) {
			currentMaxSpeed = mConsole->GetConVarValueOr(
				"sv_sprintspeed", 320.0f
			);
		}
		if (context.input.crouchPressed) {
			currentMaxSpeed = mConsole->GetConVarValueOr(
				"sv_duckspeed", 63.3f
			);
		}

		Accelerate(
			context.velocity,
			wishDir,
			currentMaxSpeed,
			mConsole->GetConVarValueOr("sv_accelerate", 10.0f),
			deltaTime
		);

		if (context.input.jumpPressed) {
			ExecuteJumpAndLeaveGround(context, deltaTime, "AirMove");
			return;
		}

		KinematicMoveQuery query = {
			.position    = context.transform->Position(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents,
			.deltaTime   = deltaTime
		};

		KinematicMoveResult result;

		// 移動と衝突の解決
		context.resolver->StepMove(
			query.position, query.velocity,
			mConsole->GetConVarValueOr("sv_stepheight", 18.0f), deltaTime
		);

		result.position = query.position;
		result.velocity = query.velocity;

		// 位置を更新
		context.transform->SetPosition(result.position);
		context.velocity = result.velocity;

		// 地面に接しているかを判定し、状態遷移を行う
		Physics::Hit groundHit{};
		if (!IsGrounded(context.resolver, result.position, &groundHit)) {
			context.supportEntityGuid     = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta      = Vec3::zero;
			context.requestedState        = "AirMove";
			return;
		}

		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;
	}

	void GroundMove::Exit() {}

	std::string_view GroundMove::GetStateName() {
		return "GroundMove";
	}

	void GroundMove::ExecuteJumpAndLeaveGround(
		MovementContext&       context,
		const float            deltaTime,
		const std::string_view airStateName
	) const {
		if (!context.transform || !context.resolver) {
			return;
		}

		const float detachBiasM = Math::HtoM(kJumpDetachBiasHu) +
		                          std::max(0.0f, context.supportStepDelta.y);
		context.transform->SetPosition(
			context.transform->Position() + Vec3::up * detachBiasM
		);

		context.velocity   += context.supportLinearVelocity;
		context.velocity.y += Math::HtoM(
			mConsole->GetConVarValueOr("sv_jumpvelocity", 420.0f)
		);

		context.jumpSnapDisableRemaining = std::max(
			context.jumpSnapDisableRemaining,
			mConsole->GetConVarValueOr("sv_jumpsnapdisabletime", 0.1f)
		);

		Vec3 jumpPos = context.transform->Position();
		Vec3 jumpVel = context.velocity;
		context.resolver->SlideMove(jumpPos, jumpVel, deltaTime);
		context.transform->SetPosition(jumpPos);
		context.velocity = jumpVel;

		context.isGrounded            = false;
		context.supportEntityGuid     = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta      = Vec3::zero;
		context.requestedState        = std::string(airStateName);
	}

	void GroundMove::ApplyFriction(
		Vec3& velocity, const float amount, const float deltaTime
	) const {
		Vec3 velHorz       = velocity;
		velHorz.y          = 0.0f;
		const float speedM = velHorz.Length();
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

	void GroundMove::Accelerate(
		Vec3&       currentVel, Vec3 wishDir, const float wishSpeed,
		const float accel,
		const float deltaTime
	) {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}

		const float maxGroundSpeed = mConsole->GetConVarValueOr(
			"sv_maxspeed", 320.0f
		);
		const float wishSpd = std::min(wishSpeed, maxGroundSpeed);

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

	bool GroundMove::IsGrounded(
		const BaseKinematicCollisionResolver* resolver,
		const Vec3&                           position,
		Physics::Hit*                         outHit
	) {
		if (!resolver) {
			return false;
		}

		Physics::Hit hit{};
		if (
			!resolver->ProbeGround(
				position, Math::HtoM(kGroundProbeDistanceHu), &hit
			)
		) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}
		return hit.startSolid || hit.allsolid || hit.normal.y >
		       kGroundNormalMinY;
	}
}
