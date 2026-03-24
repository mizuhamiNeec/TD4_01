#include "AirMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundProbeDistanceHu = 2.0f;
		constexpr float kGroundNormalMinY      = 0.7f;
	}

	void AirMove::Enter(ConsoleSystem* console) {
		if (!console) {
			Error(
				"AirMove",
				"ConsoleSystem is not available."
			);
			return;
		}
		mConsole = console;
	}

	void AirMove::Tick(MovementContext& context, float deltaTime) {
		context.isGrounded               = false;
		context.supportEntityGuid        = 0;
		context.supportLinearVelocity    = Vec3::zero;
		context.supportStepDelta         = Vec3::zero;
		context.jumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining - deltaTime
		);

		// 入力の視点方向
		const Vec3 right   = context.input.right.Normalized();
		const Vec3 forward = context.input.forward.Normalized();

		// カメラの基底ベクトルに入力軸を掛け合わせて、ワールド空間での移動方向を計算
		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f; // 上下成分は無視
		wishDir.Normalize();

		ApplyHalfGravity(context.velocity, deltaTime);

		// 移動速度を計算
		AirAccelerate(
			context.velocity, wishDir, 320.0f,
			mConsole->GetConVarValueOr("sv_airaccelerate", 10.0f),
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

		KinematicMoveResult result;

		// 移動と衝突の解決
		context.resolver->SlideMove(
			query.position, query.velocity, deltaTime
		);

		result.position = query.position;
		result.velocity = query.velocity;

		// 位置を更新
		context.transform->SetPosition(result.position);
		context.velocity = result.velocity;

		Physics::Hit groundHit{};
		if (context.jumpSnapDisableRemaining <= 0.0f &&
		    context.velocity.y <= 0.0f &&
		    IsGrounded(context.resolver, result.position, &groundHit)) {
			context.velocity.y        = 0.0f;
			context.isGrounded        = true;
			context.supportEntityGuid = groundHit.hitEntityGuid;
			context.requestedState    = "GroundMove";
		}
	}

	void AirMove::Exit() {}

	std::string_view AirMove::GetStateName() {
		return "AirMove";
	}

	void AirMove::AirAccelerate(
		Vec3& currentVel, Vec3 wishDir, float wishSpeed, float accel,
		float deltaTime
	) {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}
		const float wishspd = std::min(
			wishSpeed,
			mConsole->GetConVarValueOr("sv_airspeedcap", 30.0f)
		);
		const float cur = Math::MtoH(currentVel).Dot(wishDir);
		const float add = wishspd - cur;
		if (add <= 0.0f) {
			return;
		}
		const float acc = std::min(accel * wishSpeed * deltaTime, add);
		currentVel      += Math::HtoM(acc) * wishDir;
	}

	void AirMove::ApplyHalfGravity(
		Vec3& target, const float deltaTime
	) {
		const float gravity = mConsole->GetConVarValueOr("sv_gravity", 800.0f);
		target.y            -= Math::HtoM(gravity) * 0.5f * deltaTime;
	}

	bool AirMove::IsGrounded(
		const BaseKinematicCollisionResolver* resolver,
		const Vec3&                           position,
		Physics::Hit*                         outHit
	) {
		if (!resolver) {
			return false;
		}

		Physics::Hit hit{};
		if (!resolver->ProbeGround(
			position, Math::HtoM(kGroundProbeDistanceHu), &hit
		)) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}
		return hit.startSolid || hit.allsolid || hit.normal.y >
		       kGroundNormalMinY;
	}
}
