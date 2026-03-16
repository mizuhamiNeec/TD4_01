#include "ParkourGroundMove.h"

#include "engine/unnamed/framework/components/TransformComponent.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] Vec3 ComputeWishDir(const MovementFrameInput& input) {
			const Vec3 right   = input.right.Normalized();
			const Vec3 forward = input.forward.Normalized();

			Vec3 wishDir = right * input.moveAxis.x + forward * input.moveAxis.
			               z;
			wishDir.y = 0.0f;
			wishDir.Normalize();
			return wishDir;
		}
	}

	ParkourGroundMove::ParkourGroundMove() = default;

	void ParkourGroundMove::Enter(ConsoleSystem* console) {
		// GroundMoveに任せる
		GroundMove::Enter(console);

		mJumpVelocity = GetConVarSafe<float>(console, "sv_jumpvelocity");
	}

	void ParkourGroundMove::Tick(
		MovementContext& context, const float deltaTime
	) {
		if (!context.transform || !context.resolver) {
			return;
		}

		const Vec3 wishDir = ComputeWishDir(context.input);
		context.velocity.y = 0.0f;

		// ジャンプ入力がない場合のみ摩擦を適用する(バニーホップ用)
		if (!context.input.jumpPressed) {
			ApplyFriction(
				context.velocity,
				mFriction->GetValue(),
				deltaTime
			);
		}

		Accelerate(
			context.velocity,
			wishDir,
			kWishSpeedHu,
			mAccelerate->GetValue(),
			deltaTime
		);

		Vec3 position = context.transform->Position();
		context.resolver->StepMove(
			position, context.velocity, kStepHeightHu, deltaTime
		);
		context.transform->SetPosition(position);

		if (!IsGrounded(context.resolver, position)) {
			context.requestedState = kStateAirMove;
			return;
		}
		context.velocity.y = 0.0f;

		if (context.input.jumpPressed) {
			context.velocity.y     = Math::HtoM(mJumpVelocity->GetValue());
			context.requestedState = kStateAirMove;
		}
	}

	void ParkourGroundMove::Exit() {}

	std::string ParkourGroundMove::GetStateName() {
		return kStateGroundMove;
	}
}
