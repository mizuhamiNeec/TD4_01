#include "BuiltInConVars.h"

#include <engine/unnamed/subsystem/console/concommand/ConVar.h>

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
		static ConVar<std::string> im_configpath(
			"im_configpath",
			"./content/core/settings/imconfig.json",
			FCVAR::NONE, "Path to ImGui config file."
		);

		static ConVar<std::string> im_guizmoconfigpath(
			"im_guizmoconfigpath",
			"./content/core/settings/imguizmo.json",
			FCVAR::NONE, "Path to ImGuizmo config file."
		);

		//---------------------------------------------------------------------
		// Asset
		//---------------------------------------------------------------------
		static ConVar asset_hotreloadpollinterval(
			"asset_hotreloadpollinterval",
			0.25f, // まあリアルタイムといえるでしょう
			FCVAR::ARCHIVE,
			"アセットのホットリロード確認の間隔（秒）",
			true,
			0.125f, // 0.125s = 8回/s
			true,
			5.0f // 適当に上限を設ける
		);

		//---------------------------------------------------------------------
		// Renderer
		//---------------------------------------------------------------------
		static ConVar r_vsync(
			"r_vsync", false, FCVAR::ARCHIVE,
			"Vertical sync"
		);

		static ConVar r_clear(
			"r_clear", true, FCVAR::NONE,
			"Clear the backbuffer each frame."
		);

		static ConVar post_bloommipcount(
			"post_bloommipcount", 2, FCVAR::ARCHIVE,
			"Number of mip levels to use for bloom effect."
		);
	}

	void EditorConVar() {
		static ConVar ed_gridcolor(
			"ed_gridcolor", Vec3(0.28f, 0.28f, 0.28f), FCVAR::ARCHIVE,
			"Editor grid color."
		);

		static ConVar ed_gridmajorcolor_r(
			"ed_gridmajorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid major line r color."
		);
		static ConVar ed_gridmajorcolor_g(
			"ed_gridmajorcolor_g", 0.2f, FCVAR::ARCHIVE,
			"Editor grid major line g color."
		);
		static ConVar ed_gridmajorcolor_b(
			"ed_gridmajorcolor_b", 0.02f, FCVAR::ARCHIVE,
			"Editor grid major line b color."
		);

		static ConVar ed_gridaxiscolor_r(
			"ed_gridaxiscolor_r", 0.0f, FCVAR::ARCHIVE,
			"Editor grid axis line r color."
		);
		static ConVar ed_gridaxiscolor_g(
			"ed_gridaxiscolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line g color."
		);
		static ConVar ed_gridaxiscolor_b(
			"ed_gridaxiscolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line b color."
		);

		static ConVar ed_gridminorcolor_r(
			"ed_gridminorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line r color."
		);
		static ConVar ed_gridminorcolor_g(
			"ed_gridminorcolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line g color."
		);
		static ConVar ed_gridminorcolor_b(
			"ed_gridminorcolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line b color."
		);
	}

	void ClientConVar() {
		static ConVar<std::string> name(
			"name", "unnamed", FCVAR::NOTIFY,
			"Current user name."
		);

		static ConVar cl_showpos(
			"cl_showpos",
#ifdef _DEBUG
			1,
#else
			0,
#endif
			FCVAR::CHEAT,
			"Draw current position at top of screen (1 = meter, 2 = hammer)"
		);

		static ConVar cl_showfps(
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
		// Time
		//---------------------------------------------------------------------
		static ConVar host_timescale(
			"host_timescale", 1.0f, FCVAR::NONE,
			"Prescale the clock by this amount."
		);

		//---------------------------------------------------------------------
		// Mouse
		//---------------------------------------------------------------------
		static ConVar sensitivity(
			"sensitivity", 1.0f, FCVAR::ARCHIVE,
			"Mouse sensitivity."
		);

		static ConVar m_pitch(
			"m_pitch", 0.022f, FCVAR::ARCHIVE,
			"Mouse pitch factor."
		);
		static ConVar m_yaw(
			"m_yaw", 0.022f, FCVAR::ARCHIVE,
			"Mouse yaw factor."
		);

		//---------------------------------------------------------------------
		// Camera 
		//---------------------------------------------------------------------

		static ConVar cl_pitchdown(
			"cl_pitchdown", 89.0f, FCVAR::CHEAT,
			"Maximum downward pitch angle."
		);
		static ConVar cl_pitchup(
			"cl_pitchup", 89.0f, FCVAR::CHEAT,
			"Maximum upward pitch angle."
		);

		static ConVar cl_fov(
			"cl_fov", 90.0f, FCVAR::ARCHIVE,
			"Player Camera field of view."
		);
		static ConVar viewmodel_fov(
			"viewmodel_fov", 54.0f, FCVAR::CHEAT,
			"Viewmodel field of view."
		);

		static ConVar cl_maxfov(
			"cl_maxfov", 179.9f, FCVAR::CHEAT,
			"Maximum allowed field of view."
		);
		static ConVar cl_minfov(
			"cl_minfov", 0.1f, FCVAR::CHEAT,
			"Minimum allowed field of view."
		);

		//---------------------------------------------------------------------
		// Map 
		//---------------------------------------------------------------------
		static ConVar r_mapextents(
			"r_mapextents", 16384.0f, FCVAR::CHEAT,
			"Set the max dimension for the map."
		);

		//---------------------------------------------------------------------
		// Graphics
		//---------------------------------------------------------------------
		static ConVar fps_max(
			"fps_max", 360.0, FCVAR::ARCHIVE,
			"Frame rate limiter. 0 = unlimited."
		);

		//---------------------------------------------------------------------
		// Debug
		//---------------------------------------------------------------------
		static ConVar ent_axis(
			"ent_axis", false, FCVAR::CHEAT,
			"Draw axis for entities."
		);

		static ConVar ent_bbox(
			"ent_bbox", false, FCVAR::CHEAT,
			"Draw bounding box for entities."
		);
	}

	/// @brief サーバー側のConVarを登録します。
	/// @details このエンジンはネットワーク・マルチプレイ非対応です。ロマン。
	void ServerConVar() {
		static ConVar noclip(
			"noclip", false, FCVAR::CHEAT | FCVAR::NOTIFY,
			"Enable noclip mode."
		);

		// World
		static ConVar sv_gravity(
			"sv_gravity", 800.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"World gravity."
		);

		static ConVar sv_maxvelocity(
			"sv_maxvelocity", 3500.0f, FCVAR::REPLICATED,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		// Cheat
		static ConVar sv_cheats(
			"sv_cheats", false, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Allow cheats on server"
		);

		// Player movement
		static ConVar sv_accelerate(
			"sv_accelerate", 10.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_airaccelerate(
			"sv_airaccelerate", 10.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_maxspeed(
			"sv_maxspeed", 320.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_stopspeed(
			"sv_stopspeed", 100.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum stopping speed when on ground."
		);

		static ConVar sv_friction(
			"sv_friction", 5.2f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"World friction."
		);

		static ConVar sv_airspeedcap(
			"sv_airspeedcap", 30.0f, FCVAR::CHEAT | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_duckspeed(
			"sv_duckspeed", 63.3f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Duck speed in HU/s."
		);
		static ConVar sv_walkspeed(
			"sv_walkspeed", 150.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Walk speed in HU/s."
		);
		static ConVar sv_sprintspeed(
			"sv_sprintspeed", 320.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Sprint speed in HU/s."
		);

		static ConVar sv_jumpvelocity(
			"sv_jumpvelocity", 420.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Jump velocity in HU/s."
		);
		static ConVar sv_jumpsnapdisabletime(
			"sv_jumpsnapdisabletime", 0.10f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Disable ground snap duration immediately after jump (seconds)."
		);
		static ConVar sv_passivepush_contactskin(
			"sv_passivepush_contactskin", 4.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Passive push contact skin width in HU."
		);
		static ConVar sv_passivepush_maxdepenetrationiters(
			"sv_passivepush_maxdepenetrationiters", 4,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum depenetration iterations for passive push."
		);

		static ConVar sv_stepheight(
			"sv_stepheight", 18.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Step height in HU."
		);

		static ConVar park_duckaccelerate(
			"park_duckaccelerate", 4.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour duck accelerate."
		);

		static ConVar park_duckfriction(
			"park_duckfriction", 1.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour duck friction."
		);
	}
}
