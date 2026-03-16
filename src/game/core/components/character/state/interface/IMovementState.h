#pragma once
#include <string>

#include "../../base/BaseCharacterComponent.h"

namespace Unnamed {
	class ConsoleSystem;

	struct MovementContext {
		MovementFrameInput input; // 入力情報(移動入力・ジャンプ、視点方向など)
		TransformComponent* transform; // キャラクターのTransformComponentへのポインタ
		Vec3 velocity; // キャラクターの現在の速度
		BaseKinematicCollisionResolver* resolver; // リゾルバへのポインタ
		Vec3 halfExtents; // キャラクター衝突ハルの半径 [m]
		std::string requestedState; // 次フレームで遷移したい状態名（空なら遷移なし）
	};

	/// @brief キャラクターの移動状態を表すインターフェースです。
	class IMovementState {
	public:
		virtual ~IMovementState() = default;

		/// @brief 状態の初期化
		virtual void Enter(ConsoleSystem* console) = 0;

		/// @brief 状態の更新
		virtual void Tick(MovementContext& context, float deltaTime) = 0;

		/// @brief 状態の終了
		virtual void Exit() = 0;

		/// @brief 状態名を取得
		virtual std::string GetStateName() = 0;
	};
}
