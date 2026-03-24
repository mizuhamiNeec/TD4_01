#include "ParkourGroundMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	ParkourGroundMove::ParkourGroundMove() = default;

	void ParkourGroundMove::Enter(ConsoleSystem* console) {
		// GroundMoveに任せる
		GroundMove::Enter(console);
	}

	void ParkourGroundMove::Tick(
		MovementContext& context, const float deltaTime
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
				mConsole->GetConVarValueOr("sv_friction", 5.2f),
				deltaTime
			);
		} else {
			Msg(
				kChannelNone, "Friction skipped."
			);
		}

		if (context.input.crouchPressed) {
			context.requestedState = "ParkourSlideMove";
		}

		Accelerate(
			context.velocity,
			wishDir,
			mConsole->GetConVarValueOr("sv_sprintspeed", 320.0f),
			mConsole->GetConVarValueOr("sv_accelerate", 10.0f),
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
			mConsole->GetConVarValueOr("sv_stepheight", 18.0f),
			deltaTime
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
			context.requestedState        = "ParkourAirMove";
			return;
		}
		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;
	}

	void ParkourGroundMove::Exit() {}

	std::string_view ParkourGroundMove::GetStateName() {
		return "ParkourGroundMove";
	}
}
