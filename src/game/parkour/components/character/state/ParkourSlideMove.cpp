#include "ParkourSlideMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

namespace Unnamed {
	void ParkourSlideMove::Enter(ConsoleSystem* console) {
		// ParkourGroundMoveに任せる
		ParkourGroundMove::Enter(console);
	}

	void ParkourSlideMove::Tick(
		MovementContext& context, float deltaTime
	) {
		context.isGrounded = false;

		// 最初のティックでサポートの線形速度を基準にする
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
			ApplyFriction(
				context.velocity,
				mConsole->GetConVarValueOr("park_duckfriction", 1.0f),
				deltaTime
			);
		}

		if (!context.input.crouchPressed) {
			context.requestedState = "ParkourGroundMove";
		}

		Accelerate(
			context.velocity,
			wishDir,
			mConsole->GetConVarValueOr("sv_duckspeed", 63.3f),
			mConsole->GetConVarValueOr("park_duckaccelerate", 4.0f),
			deltaTime
		);

		if (context.input.jumpPressed) {
			ExecuteJumpAndLeaveGround(context, deltaTime, "ParkourAirMove");
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
		result.velocity = context.velocity;

		// 位置を更新
		context.transform->SetPosition(result.position);
		context.velocity = result.velocity;

		// 地面に接しているかを判定し、状態遷移を行う
		Physics::Hit groundHit{};
		if (!IsGrounded(context.resolver, result.position, &groundHit)) {
			context.supportEntityGuid     = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta      = Vec3::zero;
			context.requestedState        = "ParkourAirMove";
		}
		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;
	}

	void ParkourSlideMove::Exit() {}

	std::string_view ParkourSlideMove::GetStateName() {
		return "ParkourSlideMove";
	}
}
