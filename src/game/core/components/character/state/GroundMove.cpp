#include "GroundMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundProbeDistanceHu = 2.0f;
		constexpr float kGroundNormalMinY      = 0.7f;
		constexpr float kStepHeightHu          = 18.0f;
	}

	void GroundMove::Enter(ConsoleSystem* console) {
		if (!console) {
			Error(
				"GroundMove",
				"ConsoleSystem is not available."
			);
			return;
		}

		mAccelerate = GetConVarSafe<float>(console, "sv_accelerate");
		mMaxSpeed   = GetConVarSafe<float>(console, "sv_maxspeed");
		mStopSpeed  = GetConVarSafe<float>(console, "sv_stopspeed");
		mFriction   = GetConVarSafe<float>(console, "sv_friction");
	}

	void GroundMove::Tick(MovementContext& context, float deltaTime) {
		// 入力の視点方向
		const Vec3 right   = context.input.right.Normalized();
		const Vec3 forward = context.input.forward.Normalized();

		// カメラの基底ベクトルに入力軸を掛け合わせて、ワールド空間での移動方向を計算
		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f; // 上下成分は無視
		wishDir.Normalize();

		// 地上移動中は鉛直速度をリセット
		context.velocity.y = 0.0f;

		ApplyFriction(context.velocity, mFriction->GetValue(), deltaTime);

		Accelerate(
			context.velocity,
			wishDir,
			320.0f,
			mAccelerate->GetValue(),
			deltaTime
		);

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
		context.resolver->StepMove(
			query.position, query.velocity, kStepHeightHu, deltaTime
		);

		result.position = query.position;
		result.velocity = query.velocity;

		// 位置を更新
		context.transform->SetPosition(result.position);
		context.velocity = result.velocity;

		if (!IsGrounded(context.resolver, result.position)) {
			context.requestedState = "AirMove";
		} else {
			context.velocity.y = 0.0f;
		}
	}

	void GroundMove::Exit() {}

	std::string GroundMove::GetStateName() {
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
		const BaseKinematicCollisionResolver* resolver, const Vec3& position
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

		return
			hit.startSolid ||
			hit.allsolid ||
			hit.normal.y > kGroundNormalMinY; // 地面の法線がある程度上向きであれば地面とみなす
	}
}
