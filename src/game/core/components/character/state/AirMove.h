#pragma once
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

#include "interface/IMovementState.h"

namespace Unnamed {
	class AirMove : public IMovementState {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;
		std::string_view GetStateName() override;

	protected:
		/// @brief 空中加速を行う関数
		/// @param currentVel 現在の速度ベクトル
		/// @param wishDir 行きたい方向
		/// @param wishSpeed 到達したい速度
		/// @param accel 加速率
		/// @param deltaTime フレームの経過時間
		void AirAccelerate(
			Vec3& currentVel, Vec3 wishDir, float wishSpeed,
			float accel, float     deltaTime
		);

		/// @brief 重力を速度に適用する関数
		/// @param target 重力を適用する対象の速度ベクトル
		/// @param deltaTime フレームの経過時間
		void ApplyHalfGravity(
			Vec3& target, float deltaTime
		);

		/// @brief 地面に接地しているかどうかを判定する関数
		/// @param resolver 衝突解決器
		/// @param position 判定する位置
		/// @param outHit 接地している場合、接地情報を格納するためのポインタ（省略可能）
		/// @return 接地している場合は true、そうでない場合は false
		bool IsGrounded(
			const BaseKinematicCollisionResolver* resolver,
			const Vec3&                           position,
			Physics::Hit*                         outHit = nullptr
		);
	};
}
