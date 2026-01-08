#pragma once
#include <runtime/core/math/Math.h>

namespace Unnamed {
	Vec3  TransformPointRowVec(const Mat4& world, const Vec3& p);
	float MaxAbsScale(const Vec3& s);
	float MaxScaleRowVec(const Mat4& m);
}
