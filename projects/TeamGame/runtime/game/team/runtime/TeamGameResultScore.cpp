#include "TeamGameResultScore.h"

#include <engine/unnamed/subsystem/console/ConsoleFlags.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>

namespace MyGame {
	namespace {
		Unnamed::ConVar<int>& ResultScoreTotal() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_total",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result total score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreTrashToSea() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_trash_to_sea",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result trash-to-sea score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreTrashIntoHole() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_trash_into_hole",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result trash-into-hole score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreBallCatch() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_ball_catch",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result ball-catch score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreHoleInOne() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_hole_in_one",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result hole-in-one score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreDirectHoleInOne() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_direct_hole_in_one",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result direct hole-in-one score."
			);
			return value;
		}

		Unnamed::ConVar<int>& ResultScoreOutOfBoundsPenalty() {
			static Unnamed::ConVar<int> value(
				"tg_result_score_ob_penalty",
				0,
				Unnamed::FCVAR::NONE,
				"Latest TeamGame result out-of-bounds penalty score."
			);
			return value;
		}
	}

	void StoreTeamGameResultScore(const TeamGameResultScoreSnapshot& snapshot) {
		ResultScoreTotal() = snapshot.total;
		ResultScoreTrashToSea() = snapshot.trashToSea;
		ResultScoreTrashIntoHole() = snapshot.trashIntoHole;
		ResultScoreBallCatch() = snapshot.ballCatch;
		ResultScoreHoleInOne() = snapshot.holeInOne;
		ResultScoreDirectHoleInOne() = snapshot.directHoleInOne;
		ResultScoreOutOfBoundsPenalty() = snapshot.outOfBoundsPenalty;
	}

	TeamGameResultScoreSnapshot LoadTeamGameResultScore() {
		return {
			.total = ResultScoreTotal().GetValue(),
			.trashToSea = ResultScoreTrashToSea().GetValue(),
			.trashIntoHole = ResultScoreTrashIntoHole().GetValue(),
			.ballCatch = ResultScoreBallCatch().GetValue(),
			.holeInOne = ResultScoreHoleInOne().GetValue(),
			.directHoleInOne = ResultScoreDirectHoleInOne().GetValue(),
			.outOfBoundsPenalty = ResultScoreOutOfBoundsPenalty().GetValue(),
		};
	}
}
