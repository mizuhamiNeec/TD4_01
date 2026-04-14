#pragma once
#include "ShapeCast.h"

namespace Unnamed::Physics {
	/// @brief スフィアキャスト構造体
	struct SphereCast final : ShapeCast {
		[[nodiscard]] AABB ExpandNode(
			const AABB& nodeBounds
		) const override;

		bool TestTriangle(
			const Triangle& triangle,
			const Vec3&     dir,
			float           length,
			float&          outTOI,
			Vec3&           outNormal
		) const override;

		bool OverlapAtStart(
			const Triangle& triangle,
			float&          depth,
			Vec3&           normal
		) const override;

		[[nodiscard]] Vec3 ComputeImpactPoint(
			const Vec3& start,
			const Vec3& dirNormalized,
			float       length,
			float       toi,
			const Vec3& normal
		) const override;

		Vec3  center;
		float radius;
	};
}
