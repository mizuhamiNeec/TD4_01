#pragma once
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "interface/IMovementState.h"

namespace Unnamed {
	class NoclipMove : public IMovementState {
	public:
		void             Enter(ConsoleSystem* console) override;
		void             Tick(MovementContext& context, float deltaTime) override;
		void             Exit() override;
		std::string_view GetStateName() override;

	protected:
		void ApplyFriction(Vec3& velocity, float amount, float deltaTime);
		void Accelerate(
			Vec3& currentVel, Vec3 wishDir, float wishSpeed, float accel,
			float deltaTime
		);

		ConVar<float>* mNoclipAccelerate = nullptr;
		ConVar<float>* mNoclipSpeed      = nullptr;
		ConVar<float>* mMaxSpeed         = nullptr;
	};
}
