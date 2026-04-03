#pragma once

#include <cstdint>

namespace Unnamed {
	struct GrappleInput {
		bool grapplePressed  = false;
		bool grappleHeld     = false;
		bool grappleReleased = false;

		bool reelInHeld  = false;
		bool reelOutHeld = false;
	};

	struct CharacterActionFrameInput {
		GrappleInput grapple = {};
	};

	/// @brief キャラクター固有アクション入力の受け口です。
	class ICharacterActionInputReceiver {
	public:
		virtual ~ICharacterActionInputReceiver() = default;

		virtual void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) = 0;

		virtual void ClearDeterministicActionInputQueue() = 0;
	};
}
