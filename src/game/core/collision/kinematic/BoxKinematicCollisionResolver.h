#pragma once
#include "base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	class BoxKinematicCollisionResolver final : public
		BaseKinematicCollisionResolver {
	public:
		using BaseKinematicCollisionResolver::BaseKinematicCollisionResolver;

		void UpdateHull(
			const Vec3& pos, const Vec3& halfExtents
		);

		void SlideMove(
			Vec3& position, Vec3& velocity, float timeTotal
		) const override;

		void StepMove(
			Vec3& position, Vec3& velocity, float stepHeight, float timeTotal
		) const override;

		bool ProbeGround(
			const Vec3& position, float maxDistance, Physics::Hit* outHit
		) const override;

		int CollectOverlaps(
			const Vec3& position,
			const Vec3& halfExtents,
			Physics::Hit* outHits,
			int           maxHits
		) const override;

	private:
		static float CastSkinM();
		static float SkinM();

		Box mHull;
	};
}
