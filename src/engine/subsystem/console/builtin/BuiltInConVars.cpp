#include "BuiltInConVars.h"

#include <engine/subsystem/console/concommand/UnnamedConVar.h>

namespace Unnamed {
	void RegisterBuiltInConVars() {
		EngineConVar();
		ClientConVar();
	}

	void EngineConVar() {
		static UnnamedConVar cl_showpos(
			"cl_showpos", 0, FCVAR::NONE,
			"Draw current position at top of screen (1 = meter, 2 = hammer)"
		);

		static UnnamedConVar cl_showfps(
			"cl_showfps",
#ifdef _DEBUG
			2,
#else
			0,
#endif
			FCVAR::NONE,
			"Draw fps meter (1 = fps, 2 = smooth)"
		);
	}

	void ClientConVar() {
		static UnnamedConVar fps_max(
			"fps_max", 0, FCVAR::ARCHIVE,
			"Frame rate limiter (0 = unlimited)"
		);

		static UnnamedConVar sensitivity(
			"sensitivity", 1.0f, FCVAR::ARCHIVE,
			"Mouse sensitivity."
		);
	}

	void ServerConVar() {
		static UnnamedConVar sv_gravity(
			"sv_gravity", 800.0f, FCVAR::NONE,
			"World gravity."
		);

		static UnnamedConVar sv_maxvelocity(
			"sv_maxvelocity", 3500.0f, FCVAR::NONE,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		static UnnamedConVar sv_accelerate(
			"sv_accelerate", 10.0f, FCVAR::NONE,
			"Linear acceleration amount (old value is 5.6)"
		);

		static UnnamedConVar sv_airaccelerate("sv_airaccelerate", 12.0f);

		static UnnamedConVar sv_maxspeed(
			"sv_maxspeed", 320.0f, FCVAR::NONE,
			"Maximum speed a player can move."
		);

		static UnnamedConVar sv_stopspeed(
			"sv_stopspeed", 100.0f, FCVAR::NONE,
			"Minimum stopping speed when on ground."
		);

		static UnnamedConVar sv_friction(
			"sv_friction", 4.0f, FCVAR::NONE, "World friction."
		);

		static UnnamedConVar sv_stepsize(
			"sv_stepsize", 18.0f, FCVAR::NONE,
			"Maximum step height."
		);
	}
}
