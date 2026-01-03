#include "RenderUtils.h"

namespace Unnamed {
	Vec3 TransformPointRowVec(const Mat4& world, const Vec3& p) {
		const float x = p.x * world.m[0][0] + p.y * world.m[1][0] + p.z * world.
			m[2][0] + 1.0f * world.m[3][0];
		const float y = p.x * world.m[0][1] + p.y * world.m[1][1] + p.z * world.
			m[2][1] + 1.0f * world.m[3][1];
		const float z = p.x * world.m[0][2] + p.y * world.m[1][2] + p.z * world.
			m[2][2] + 1.0f * world.m[3][2];
		return Vec3(x, y, z);
	}

	float MaxAbsScale(const Vec3& s) {
		const float ax = std::fabs(s.x);
		const float ay = std::fabs(s.y);
		const float az = std::fabs(s.z);
		return std::max(ax, std::max(ay, az));
	}

	float MaxScaleRowVec(const Mat4& m) {
		const Vec3  c0(m.m[0][0], m.m[1][0], m.m[2][0]);
		const Vec3  c1(m.m[0][1], m.m[1][1], m.m[2][1]);
		const Vec3  c2(m.m[0][2], m.m[1][2], m.m[2][2]);
		const float s0 = std::sqrt(c0.Dot(c0));
		const float s1 = std::sqrt(c1.Dot(c1));
		const float s2 = std::sqrt(c2.Dot(c2));
		return std::max(s0, std::max(s1, s2));
	}
}
