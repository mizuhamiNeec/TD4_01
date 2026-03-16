#pragma once
#include "core/math/Vec3.h"

#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

namespace Unnamed {
	class BaseKinematicCollisionResolver;
	class GameMovementStateMachine;

	namespace {
		constexpr char kStateAirMove[]    = "AirMove";
		constexpr char kStateGroundMove[] = "GroundMove";

		constexpr float kGroundProbeDistanceHu = 2.0f;
		constexpr float kGroundNormalMinY      = 0.7f;
		constexpr float kWishSpeedHu           = 320.0f;
		constexpr float kStepHeightHu          = 18.0f;

		constexpr float kDefaultAccelerate = 10.0f;
		constexpr float kDefaultMaxSpeed   = 320.0f;
		constexpr float kDefaultStopSpeed  = 100.0f;
		constexpr float kDefaultFriction   = 4.0f;
	}

	void RegisterParkourMovementStates(
		GameMovementStateMachine& stateMachine

	);
}
