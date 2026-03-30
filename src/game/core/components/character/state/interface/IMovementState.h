#pragma once
#include <cstdint>
#include <string>

#include "../../base/BaseCharacterComponent.h"

namespace Unnamed {
	class ConsoleSystem;
	class GameMovementComponent;

	struct MovementContext {
		MovementFrameInput input; // 入力情報(移動入力・ジャンプ、視点方向など)
		TransformComponent* transform; // キャラクターのTransformComponentへのポインタ
		Vec3 velocity; // キャラクターの現在の速度
		BaseKinematicCollisionResolver* resolver; // リゾルバへのポインタ
		Vec3 halfExtents; // キャラクター衝突ハルの半径 [m]
		std::string requestedState; // 次フレームで遷移したい状態名（空なら遷移なし）
		bool isGrounded = false;
		uint64_t supportEntityGuid = 0;
		Vec3 supportLinearVelocity = Vec3::zero;
		Vec3 supportStepDelta = Vec3::zero;
		float jumpSnapDisableRemaining = 0.0f;
		GameMovementComponent* movementComponent = nullptr;
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
		virtual std::string_view GetStateName() = 0;

		/// @brief カメラ方向を考慮した移動入力の水平成分を取得します。
		/// @param context 入力やキャラクターの状態を含むコンテキスト
		/// @return 水平成分の移動方向ベクトル
		/// @details XZ平面での移動方向を計算します。地上や空中の移動状態で用います。
		virtual Vec3 GetWishDirHoriz(MovementContext& context);

		/// @brief カメラ方向を考慮した移動入力を取得します。
		/// @param context 入力やキャラクターの状態を含むコンテキスト
		/// @return 移動方向ベクトル
		/// @details 空中移動や水中など3次元的な移動が必要な状態で用います。
		virtual Vec3 GetWishDir(MovementContext& context);

	protected:
		ConsoleSystem* mConsole = nullptr;
	};
}
