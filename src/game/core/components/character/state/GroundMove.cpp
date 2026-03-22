#include "GroundMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"

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

		mAccelerate   = GetConVarSafe<float>(console, "sv_accelerate");
		mMaxSpeed     = GetConVarSafe<float>(console, "sv_maxspeed");
		mStopSpeed    = GetConVarSafe<float>(console, "sv_stopspeed");
		mFriction     = GetConVarSafe<float>(console, "sv_friction");
		mJumpVelocity = GetConVarSafe<float>(console, "sv_jumpvelocity");
		mJumpSnapDisableTime = GetConVarSafe<float>(
			console, "sv_jumpsnapdisabletime"
		);
		mStepHeight   = GetConVarSafe<float>(console, "sv_stepheight");

		mDuckSpeed   = GetConVarSafe<float>(console, "sv_duckspeed");
		mWalkSpeed   = GetConVarSafe<float>(console, "sv_walkspeed");
		mSprintSpeed = GetConVarSafe<float>(console, "sv_sprintspeed");

		mRebaseVelocityToSupportOnFirstTick = true;
	}

	void GroundMove::Tick(
		MovementContext& context, float deltaTime
	) {
		context.isGrounded = false;

		if (mRebaseVelocityToSupportOnFirstTick) {
			context.velocity -= context.supportLinearVelocity;
			mRebaseVelocityToSupportOnFirstTick = false;
		}

		// 移動方向
		Vec3 wishDir = GetWishDirHoriz(context);

		// 地上移動中は鉛直速度をリセット
		context.velocity.y = 0.0f;

		// ジャンプしていない場合は摩擦を適用(バニーホップ中か?)
		if (!context.input.jumpPressed) {
			ApplyFriction(context.velocity, mFriction->GetValue(), deltaTime);
		}

		// 移動状態によって最大速度を変える
		// しゃがみ入力優先
		float currentMaxSpeed = mWalkSpeed->GetValue();
		if (context.input.sprintPressed) {
			currentMaxSpeed = mSprintSpeed->GetValue();
		}
		if (context.input.crouchPressed) {
			currentMaxSpeed = mDuckSpeed->GetValue();
		}

		Accelerate(
			context.velocity,
			wishDir,
			currentMaxSpeed,
			mAccelerate->GetValue(),
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
			query.position, query.velocity, mStepHeight->GetValue(), deltaTime
		);

		result.position = query.position;
		result.velocity = query.velocity;

		// 位置を更新
		context.transform->SetPosition(result.position);
		context.velocity = result.velocity;

		// 地面に接しているかを判定し、状態遷移を行う
		Physics::Hit groundHit{};
		if (!IsGrounded(context.resolver, result.position, &groundHit)) {
			context.supportEntityGuid = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta = Vec3::zero;
			context.requestedState = "AirMove";
			return;
		}

		context.isGrounded = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y = 0.0f;
	}

	void GroundMove::Exit() {}

	std::string_view GroundMove::GetStateName() {
		return "GroundMove";
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

		const float stop = mStopSpeed->GetValue();

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

		const float maxGroundSpeed = mMaxSpeed->GetValue();
		const float wishSpd        = std::min(wishSpeed, maxGroundSpeed);

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
		return hit.startSolid || hit.allsolid || hit.normal.y > kGroundNormalMinY;
	}

	void GroundMove::ExecuteJumpAndLeaveGround(
		MovementContext& context,
		const float      deltaTime,
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

		context.velocity += context.supportLinearVelocity;
		context.velocity.y += Math::HtoM(mJumpVelocity->GetValue());

		if (mJumpSnapDisableTime) {
			context.jumpSnapDisableRemaining = std::max(
				context.jumpSnapDisableRemaining,
				mJumpSnapDisableTime->GetValue()
			);
		}

		Vec3 jumpPos = context.transform->Position();
		Vec3 jumpVel = context.velocity;
		context.resolver->SlideMove(jumpPos, jumpVel, deltaTime);
		context.transform->SetPosition(jumpPos);
		context.velocity = jumpVel;

		context.isGrounded              = false;
		context.supportEntityGuid       = 0;
		context.supportLinearVelocity   = Vec3::zero;
		context.supportStepDelta        = Vec3::zero;
		context.requestedState          = std::string(airStateName);
	}
}
