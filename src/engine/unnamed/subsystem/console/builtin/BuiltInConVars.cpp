#include "BuiltInConVars.h"

#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>

namespace Unnamed {
	void RegisterBuiltInConVars() {
		EngineConVar();
		ClientConVar();
		ServerConVar();

#ifdef _DEBUG
		EditorConVar();
#endif
	}

	void EngineConVar() {
		static UnnamedConVar<std::string> im_configpath(
			"im_configpath",
			"./content/core/settings/imconfig.json",
			FCVAR::NONE, "Path to ImGui config file."
		);

		static UnnamedConVar<std::string> im_guizmoconfigpath(
			"im_guizmoconfigpath",
			"./content/core/settings/imguizmo.json",
			FCVAR::NONE, "Path to ImGuizmo config file."
		);

		static UnnamedConVar host_timescale(
			"host_timescale", 1.0f, FCVAR::NONE,
			"Prescale the clock by this amount."
		);

		//---------------------------------------------------------------------
		// Asset
		//---------------------------------------------------------------------
		static UnnamedConVar asset_hotreloadpollinterval(
			"asset_hotreloadpollinterval",
			0.25f, // まあリアルタイムといえるでしょう
			FCVAR::ARCHIVE,
			"アセットのホットリロード確認の間隔（秒）",
			true,
			0.125f, // 0.125s = 8回/s
			true,
			5.0f // 適当に上限を設ける
		);
	}

	void EditorConVar() {
		static UnnamedConVar ed_gridcolor(
			"ed_gridcolor", Vec3(0.28f, 0.28f, 0.28f), FCVAR::ARCHIVE,
			"Editor grid color."
		);

		static UnnamedConVar ed_gridmajorcolor_r(
			"ed_gridmajorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid major line r color."
		);
		static UnnamedConVar ed_gridmajorcolor_g(
			"ed_gridmajorcolor_g", 0.2f, FCVAR::ARCHIVE,
			"Editor grid major line g color."
		);
		static UnnamedConVar ed_gridmajorcolor_b(
			"ed_gridmajorcolor_b", 0.02f, FCVAR::ARCHIVE,
			"Editor grid major line b color."
		);

		static UnnamedConVar ed_gridaxiscolor_r(
			"ed_gridaxiscolor_r", 0.0f, FCVAR::ARCHIVE,
			"Editor grid axis line r color."
		);
		static UnnamedConVar ed_gridaxiscolor_g(
			"ed_gridaxiscolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line g color."
		);
		static UnnamedConVar ed_gridaxiscolor_b(
			"ed_gridaxiscolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line b color."
		);

		static UnnamedConVar ed_gridminorcolor_r(
			"ed_gridminorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line r color."
		);
		static UnnamedConVar ed_gridminorcolor_g(
			"ed_gridminorcolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line g color."
		);
		static UnnamedConVar ed_gridminorcolor_b(
			"ed_gridminorcolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line b color."
		);
	}

	void ClientConVar() {
		static UnnamedConVar<std::string> name(
			"name", "unnamed", FCVAR::NONE,
			"Current user name."
		);

		static UnnamedConVar cl_showpos(
			"cl_showpos",
#ifdef _DEBUG
			1,
#else
			0,
#endif
			FCVAR::CHEAT,
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

		//---------------------------------------------------------------------
		// Camera 
		//---------------------------------------------------------------------
		static UnnamedConVar sensitivity(
			"sensitivity", 1.0f, FCVAR::ARCHIVE,
			"Mouse sensitivity."
		);
		static UnnamedConVar m_pitch(
			"m_pitch", 0.022f, FCVAR::ARCHIVE,
			"Mouse pitch factor."
		);
		static UnnamedConVar m_yaw(
			"m_yaw", 0.022f, FCVAR::ARCHIVE,
			"Mouse yaw factor."
		);
		static UnnamedConVar cl_pitchdown(
			"cl_pitchdown", -89.0f, FCVAR::CHEAT,
			"Maximum downward pitch angle."
		);
		static UnnamedConVar cl_pitchup(
			"cl_pitchup", 89.0f, FCVAR::CHEAT,
			"Maximum upward pitch angle."
		);
		static UnnamedConVar cl_fov(
			"cl_fov", 90.0f, FCVAR::ARCHIVE,
			"Player Camera field of view."
		);
		static UnnamedConVar cl_maxfov(
			"cl_maxfov", 179.9f, FCVAR::CHEAT,
			"Maximum allowed field of view."
		);
		static UnnamedConVar cl_minfov(
			"cl_minfov", 0.1f, FCVAR::CHEAT,
			"Minimum allowed field of view."
		);

		//---------------------------------------------------------------------
		// Map 
		//---------------------------------------------------------------------
		static UnnamedConVar r_mapextents(
			"r_mapextents", 16384.0f, FCVAR::CHEAT,
			"Set the max dimension for the map."
		);

		//---------------------------------------------------------------------
		// Graphics
		//---------------------------------------------------------------------
		static UnnamedConVar fps_max(
			"fps_max", 360.0, FCVAR::ARCHIVE,
			"Frame rate limiter. 0 = unlimited."
		);

		static UnnamedConVar r_vsync(
			"r_vsync", false, FCVAR::ARCHIVE,
			"Vertical sync"
		);

		//---------------------------------------------------------------------
		// Debug
		//---------------------------------------------------------------------
		static UnnamedConVar ent_axis(
			"ent_axis", false, FCVAR::CHEAT,
			"Draw axis for entities."
		);

		static UnnamedConVar ent_bbox(
			"ent_bbox", false, FCVAR::CHEAT,
			"Draw bounding box for entities."
		);
	}

	void ServerConVar() {
		static UnnamedConVar sv_gravity(
			"sv_gravity", 1200.0f, FCVAR::NONE,
			"World gravity."
		);

		static UnnamedConVar sv_maxvelocity(
			"sv_maxvelocity", 3500.0f, FCVAR::NONE,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		static UnnamedConVar sv_stopspeed(
			"sv_stopspeed", 100.0f, FCVAR::NONE,
			"Minimum stopping speed when on ground."
		);
	}
}
