#include "ParkourAirMove.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	ParkourAirMove::~ParkourAirMove() {}

	void ParkourAirMove::Enter(ConsoleSystem* console) {
		AirMove::Enter(console);
	}

	void ParkourAirMove::Tick(MovementContext& context, float deltaTime) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.requestedState = "NoclipMove";
			return;
		}

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
			context.requestedState    = "ParkourGroundMove";
		}
	}

	void ParkourAirMove::Exit() {
		AirMove::Exit();
	}

	std::string_view ParkourAirMove::GetStateName() {
		return "ParkourAirMove";
	}
}
