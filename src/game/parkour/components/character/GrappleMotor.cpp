#include "GrappleMotor.h"

#include "ParkourMovementComponent.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/interface/IMovementState.h"

namespace Unnamed {
	namespace {
		constexpr float kEpsilon                = 0.0001f;
		constexpr float kDefaultTraceDistanceHu = 8192.0f;
		constexpr float kSwingAcceleration      = 18.0f;
	}

	void GrappleMotor::UpdateActivation(
		MovementContext& context, const CharacterActionFrameInput& actionInput
	) {
		auto* mv =
			dynamic_cast<ParkourMovementComponent*>(context.movementComponent);

		GrappleState& grapple = mv->GetParkourRuntime().grapple;

		if (actionInput.grapple.grapplePressed) {
			if (!grapple.isActive) {
				TryStartGrapple(context);
			}
		}

		if (grapple.isActive && actionInput.grapple.grappleReleased) {
			StopGrapple(context);
		}
	}

	void GrappleMotor::ApplyPreMove(
		MovementContext& context, const CharacterActionFrameInput& actionInput,
		float            deltaTime
	) {
		auto* mv =
			dynamic_cast<ParkourMovementComponent*>(context.movementComponent);

		GrappleState& grapple = mv->GetParkourRuntime().grapple;

		if (!grapple.isActive) {
			return;
		}

		UpdateRopeLength(grapple, actionInput, deltaTime);
		ApplyPullAndSwingConstraint(context, grapple, deltaTime);
	}

	void GrappleMotor::ApplyPostMove(MovementContext& context) {
		auto* mv =
			dynamic_cast<ParkourMovementComponent*>(context.movementComponent);

		auto& grapple = mv->GetParkourRuntime().grapple;

		if (!grapple.isActive) {
			return;
		}

		Vec3 playerPos = context.transform->Position();

		Vec3        ropeVector   = playerPos - grapple.anchorPoint;
		const float ropeDistance = ropeVector.Length();

		// ロープの長さが極端に短くなったらグラップルを停止する
		if (ropeDistance <= kEpsilon) {
			return;
		}

		if (ropeDistance > grapple.ropeLength) {
			const Vec3 ropeDir = ropeVector / ropeDistance;
			// ロープの長さを超えている分だけ位置を補正する
			context.transform->SetPosition(
				grapple.anchorPoint + ropeDir * grapple.ropeLength
			);

			const float radialSpeed = context.velocity.Dot(ropeDir);
			if (radialSpeed > 0.0f) {
				context.velocity -= ropeDir * radialSpeed;
			}
		}
	}

	bool GrappleMotor::TryStartGrapple(MovementContext& context) {
		// リゾルバから物理エンジンをぶんどる
		auto* physics = context.resolver->GetPhysics();

		auto* mv =
			dynamic_cast<ParkourMovementComponent*>(context.movementComponent);

		auto& grapple = mv->GetParkourRuntime().grapple;

		// TODO: プレイヤーの視点高さを向こうで計算するようにする

		// プレイヤー座標(中心) + 上方向 * 半身の高さ - 8unit 
		Vec3 eyePos = context.transform->Position() +
		              Vec3::up * context.halfExtents.y - Math::HtoM(8.0f);

		Physics::Hit hit;
		Ray          ray{
			.origin = eyePos,
			.dir    = context.input.forward,
			.tMax   = Math::HtoM(kDefaultTraceDistanceHu)
		};

		// レイをふっとばす。
		if (!physics->RayCast(ray, &hit)) {
			return false; // 衝突しなかったら返す
		}

		grapple.isActive    = true;
		grapple.isLatched   = true;
		grapple.mode        = GRAPPLE_MODE::SWING;
		grapple.anchorPoint = hit.pos; // 衝突点をアンカーポイントにする

		const Vec3 ropeVector = context.transform->Position() - grapple.
		                        anchorPoint;
		const float ropeDistance = ropeVector.Length();

		grapple.ropeLength = std::clamp(
			ropeDistance,
			grapple.minRopeLength,
			Math::HtoM(65536.0f)
		);

		return true;
	}

	void GrappleMotor::StopGrapple(MovementContext& context) {
		auto* mv =
			dynamic_cast<ParkourMovementComponent*>(context.movementComponent);

		mv->GetParkourRuntime().grapple.Reset();
	}

	void GrappleMotor::UpdateRopeLength(
		GrappleState& grapple, const CharacterActionFrameInput& actionInput,
		float         deltaTime
	) {
		if (actionInput.grapple.reelInHeld) {
			grapple.ropeLength -= grapple.reelSpeed * deltaTime;
		}

		if (actionInput.grapple.reelOutHeld) {
			grapple.ropeLength += grapple.reelSpeed * deltaTime;
		}

		grapple.ropeLength = std::clamp(
			grapple.ropeLength,
			grapple.minRopeLength,
			grapple.maxRopeLength
		);
	}

	void GrappleMotor::ApplyPullAndSwingConstraint(
		MovementContext& context, GrappleState& grapple, float deltaTime
	) {
		Vec3 ropeVector = context.transform->Position() - grapple.anchorPoint;
		const float ropeDistance = ropeVector.Length();
		if (ropeDistance <= kEpsilon) {
			return;
		}

		const Vec3 ropeDir = ropeVector / ropeDistance;

		if (grapple.mode == GRAPPLE_MODE::PULL) {
			context.velocity += (-ropeDir) * grapple.pullAcceleration *
				deltaTime;
		}

		const float radialSpeed = context.velocity.Dot(ropeDir);

		if (ropeDistance >= grapple.ropeLength && radialSpeed > 0.0f) {
			context.velocity -= ropeDir * radialSpeed;
		}

		Vec3 wishMove = Vec3(
			context.input.moveAxis.x, 0.0f, context.input.moveAxis.z
		);

		if (wishMove.SqrLength() > kEpsilon) {
			wishMove = wishMove.Normalized();

			Vec3 tangentWish = wishMove - ropeDir * wishMove.Dot(ropeDir);

			if (tangentWish.SqrLength() > kEpsilon) {
				tangentWish      = tangentWish.Normalized();
				context.velocity += tangentWish * kSwingAcceleration *
					deltaTime;
			}
		}
	}
}
