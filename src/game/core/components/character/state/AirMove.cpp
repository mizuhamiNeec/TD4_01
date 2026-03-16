#include "AirMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundProbeDistanceHu = 2.0f;
		constexpr float kGroundNormalMinY      = 0.7f;

		bool IsGrounded(
			BaseKinematicCollisionResolver* resolver, const Vec3& position
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

			return hit.startSolid || hit.allsolid || hit.normal.y > kGroundNormalMinY;
		}
	}

	void AirMove::Enter(ConsoleSystem* console) {
		if (!console) {
			Error(
				"AirMove",
				"ConsoleSystem is not available."
			);
			return;
		}

		mGravity       = GetConVarSafe<float>(console, "sv_gravity");
		mAirAccelerate = GetConVarSafe<float>(console, "sv_airaccelerate");
		mAirSpeedCap   = GetConVarSafe<float>(console, "sv_airspeedcap");
	}

	void AirMove::Tick(MovementContext& context, float deltaTime) {
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
			mAirAccelerate->GetValue(),
			deltaTime
		);

		ApplyHalfGravity(context.velocity, deltaTime);

		KinematicMoveQuery query = {
			.position    = context.transform->Position(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents.IsZero() ?
				               Math::HtoM(Vec3(32.0f, 73.0f, 32.0f)) * 0.5f :
				               context.halfExtents,
			.deltaTime   = deltaTime
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

		if (context.velocity.y <= 0.0f &&
		    IsGrounded(context.resolver, result.position)) {
			context.velocity.y    = 0.0f;
			context.requestedState = "GroundMove";
		}
	}

	void AirMove::Exit() {}

	std::string AirMove::GetStateName() {
		return "AirMove";
	}

	void AirMove::AirAccelerate(
		Vec3& currentVel, Vec3 wishDir, float wishSpeed, float accel,
		float deltaTime
	) {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}
		const float wishspd = std::min(wishSpeed, mAirSpeedCap->GetValue());
		const float cur     = Math::MtoH(currentVel).Dot(wishDir);
		const float add     = wishspd - cur;
		if (add <= 0.0f) {
			return;
		}
		const float acc = std::min(accel * wishSpeed * deltaTime, add);
		currentVel      += Math::HtoM(acc) * wishDir;
	}

	void AirMove::ApplyHalfGravity(
		Vec3& target, const float deltaTime
	) {
		target.y -= Math::HtoM(mGravity->GetValue()) * 0.5f * deltaTime;
	}
}
