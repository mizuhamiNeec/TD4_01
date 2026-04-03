#pragma once
#include <game/core/input/CharacterActionFrameInput.h>

#include "core/math/Math.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	struct MovementContext;

	enum class GRAPPLE_MODE {
		NONE,
		PULL,
		SWING,
	};

	struct GrappleState {
		bool isActive  = false;
		bool isLatched = false;

		GRAPPLE_MODE mode = GRAPPLE_MODE::NONE;

		Vec3  anchorPoint      = Vec3::zero;
		float ropeLength       = 0.0f;
		float pullAcceleration = 45.0f;
		float reelSpeed        = 12.0f;
		float minRopeLength    = Math::HtoM(32.0f);
		float maxRopeLength    = Math::HtoM(65536.0f);

		void Reset() {
			isActive    = false;
			isLatched   = false;
			mode        = GRAPPLE_MODE::NONE;
			anchorPoint = Vec3::zero;
			ropeLength  = 0.0f;
		}
	};


	class GrappleMotor {
	public:
		static void UpdateActivation(
			MovementContext&                 context,
			const CharacterActionFrameInput& actionInput
		);

		static void ApplyPreMove(
			MovementContext&                 context,
			const CharacterActionFrameInput& actionInput,
			float                            deltaTime
		);

		static void ApplyPostMove(
			MovementContext& context
		);

	private:
		static bool TryStartGrapple(MovementContext& context);

		static void StopGrapple(MovementContext& context);
		static void UpdateRopeLength(
			GrappleState&                    grapple,
			const CharacterActionFrameInput& actionInput,
			float                            deltaTime
		);

		static void ApplyPullAndSwingConstraint(
			MovementContext& context,
			GrappleState&    grapple,
			float            deltaTime
		);
	};
}
