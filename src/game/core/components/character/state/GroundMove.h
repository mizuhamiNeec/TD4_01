#pragma once
#include <string_view>

#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "interface/IMovementState.h"

namespace Unnamed {
	class GroundMove : public IMovementState {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;
		std::string_view GetStateName() override;

	protected:
		void ExecuteJumpAndLeaveGround(
			MovementContext& context, float deltaTime, std::string_view airStateName
		) const;

		/// @brief 摩擦を適用する
		/// @param velocity 摩擦を適用する速度ベクトル
		/// @param amount 摩擦の強さ
		/// @param deltaTime フレームの経過時間
		void ApplyFriction(
			Vec3& velocity, float amount, float deltaTime
		) const;

		/// @brief 加速を適用する
		/// @param currentVel 現在の速度ベクトル
		/// @param wishDir プレイヤーが望む移動方向の単位ベクトル
		/// @param wishSpeed プレイヤーが望む移動速度
		/// @param accel 加速の強さ
		/// @param deltaTime フレームの経過時間
		void Accelerate(
			Vec3& currentVel, Vec3 wishDir, float wishSpeed,
			float accel, float     deltaTime
		);

		/// @brief プレイヤーが地面に接しているかを判定する
		/// @param resolver 衝突解決器
		/// @param position プレイヤーの現在位置
		/// @param outHit 地面ヒット情報の出力先
		/// @return 地面に接している場合はtrue、そうでない場合はfalse
		bool IsGrounded(
			const BaseKinematicCollisionResolver* resolver,
			const Vec3&                           position,
			Physics::Hit*                         outHit = nullptr
		);

		ConVar<float>* mAccelerate   = nullptr;
		ConVar<float>* mMaxSpeed     = nullptr;
		ConVar<float>* mStopSpeed    = nullptr;
		ConVar<float>* mFriction     = nullptr;
		ConVar<float>* mJumpVelocity = nullptr;
		ConVar<float>* mJumpSnapDisableTime = nullptr;
		ConVar<float>* mStepHeight   = nullptr;

		ConVar<float>* mSprintSpeed = nullptr;
		ConVar<float>* mWalkSpeed   = nullptr;
		ConVar<float>* mDuckSpeed   = nullptr;

		bool mRebaseVelocityToSupportOnFirstTick = false;
	};
}
