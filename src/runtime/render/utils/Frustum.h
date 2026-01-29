#pragma once
#include "runtime/core/math/Math.h"

namespace Unnamed {
	struct Plane {
		Vec3  n = Vec3::zero;
		float d = 0.0f;
	};

	struct Frustum {
		Plane planes[6];

		static Frustum FromViewProjRowVec(const Mat4& viewProj);

		[[nodiscard]] bool TestSphere(
			const Vec3& centerWS,
			float       radiusWS
		) const;
	};
}
