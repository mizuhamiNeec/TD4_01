#include "IMovementState.h"

namespace Unnamed {
	Vec3 IMovementState::GetWishDirHoriz(MovementContext& context) {
		// 入力の視点方向
		const Vec3 right   = context.input.right.Normalized();
		Vec3       forward = context.input.forward.Normalized();
		forward.y          = 0.0f; // 上下成分は無視
		forward.Normalize();

		// カメラの基底ベクトルに入力軸を掛け合わせて、ワールド空間での移動方向を計算
		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;

		wishDir.y = 0.0f; // 上下成分は無視

		return wishDir;
	}

	Vec3 IMovementState::GetWishDir(MovementContext& context) {
		// 入力の視点方向
		const Vec3 right   = context.input.right.Normalized();
		const Vec3 up      = context.input.up.Normalized();
		const Vec3 forward = context.input.forward.Normalized();

		// カメラの基底ベクトルに入力軸を掛け合わせて、ワールド空間での移動方向を計算
		const Vec3 wishDir =
			right * context.input.moveAxis.x +
			up * context.input.moveAxis.y +
			forward * context.input.moveAxis.z;

		return wishDir;
	}
}
