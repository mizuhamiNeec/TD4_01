#pragma once

namespace MyGame {
	struct TeamGameResultScoreSnapshot {
		int total = 0;
		int trashToSea = 0;
		int trashIntoHole = 0;
		int ballCatch = 0;
		int holeInOne = 0;
		int directHoleInOne = 0;
		int outOfBoundsPenalty = 0;
	};

	/// @brief Stores the latest result score snapshot in TeamGame console variables.
	void StoreTeamGameResultScore(const TeamGameResultScoreSnapshot& snapshot);

	/// @brief Loads the latest result score snapshot from TeamGame console variables.
	[[nodiscard]] TeamGameResultScoreSnapshot LoadTeamGameResultScore();
}
