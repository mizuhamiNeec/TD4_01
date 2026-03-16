#pragma once
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "interface/IMovementState.h"

namespace Unnamed {
	class AirMove : public IMovementState {
	public:
		void        Enter(ConsoleSystem* console) override;
		void        Tick(MovementContext& context, float deltaTime) override;
		void        Exit() override;
		std::string GetStateName() override;

	private:
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

		ConVar<float>* mGravity       = nullptr;
		ConVar<float>* mAirAccelerate = nullptr;
		ConVar<float>* mAirSpeedCap   = nullptr;
	};
}
