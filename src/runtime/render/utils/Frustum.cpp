#include "Frustum.h"

namespace Unnamed {
	Plane NormalizePlane(const Vec4& p) {
		const Vec3  n(p.x, p.y, p.z);
		const float lenSq = n.SqrLength();
		if (lenSq > 1e-12f) {
			const float invLen = 1.0f / std::sqrt(lenSq);
			return Plane{
				.n = n * invLen,
				.d = p.w * invLen
			};
		}

		return Plane{
			.n = Vec3::zero,
			.d = p.w
		};
	}

	Frustum Frustum::FromViewProjRowVec(const Mat4& viewProj) {
		const Mat4 mT = viewProj.Transpose();

		auto Row = [&](const int i) -> Vec4 {
			return Vec4(
				mT.m[i][0],
				mT.m[i][1],
				mT.m[i][2],
				mT.m[i][3]
			);
		};

		const Vec4 r0 = Row(0);
		const Vec4 r1 = Row(1);
		const Vec4 r2 = Row(2);
		const Vec4 r3 = Row(3);

		Frustum f   = {};
		f.planes[0] = NormalizePlane(r3 + r0); // left
		f.planes[1] = NormalizePlane(r3 - r0); // right
		f.planes[2] = NormalizePlane(r3 + r1); // bottom
		f.planes[3] = NormalizePlane(r3 - r1); // top
		f.planes[4] = NormalizePlane(r3 + r2); // near
		f.planes[5] = NormalizePlane(r3 - r2); // far
		return f;
	}

	bool Frustum::TestSphere(const Vec3& centerWS, float radiusWS) const {
		if (
			!std::ranges::all_of(
				planes,
				[&](const Plane& p) {
					const float dist = p.n.Dot(centerWS) + p.d;
					return dist >= -radiusWS;
				}
			)
		) {
			return false;
		}
		return true;
	}
}
