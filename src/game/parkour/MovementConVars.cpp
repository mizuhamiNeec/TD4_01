#include "MovementConVars.h"

#include "engine/unnamed/subsystem/console/concommand/UnnamedConVar.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// HU = Hammer Units, Sourceのワールド単位。1hu = 0.0254m (1-インチ)
	//-------------------------------------------------------------------------
	void RegisterMovementConVars() {
		// General
		//static UnnamedConVar 

		// Jump
		static UnnamedConVar mv_jumpvelocity(
			"mv_jumpvelocity", 400.0f,
			FCVAR::NONE, "プレイヤーのジャンプ初速 (hu/s)"
		);
		static UnnamedConVar mv_jumpsnapdisabletime(
			"mv_jumpsnapdisabletime", 0.25f,
			FCVAR::NONE, "ジャンプ後に地面へのスナップを無効にする時間 (秒)"
		);
	}
}
