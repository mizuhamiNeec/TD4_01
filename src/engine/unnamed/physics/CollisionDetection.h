#pragma once

struct Vec3;

namespace Unnamed {
	struct Box;
	struct Triangle;
	struct AABB;
	struct Ray;
}

namespace Unnamed::Physics {
	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	);

	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	);

	bool SweptAabbVsTriSAT(
		const Box&      box0,
		const Vec3&     delta, // 速度 * dt
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	);

	bool SweptSphereVsTriSAT(
		const Vec3&     center,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	bool SweptCylinderVsTriSAT(
		const Vec3&     base,
		float           halfHeight,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	bool SweptCapsuleVsTriSAT(
		const Vec3&     a,
		const Vec3&     b,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	bool BoxVsTriangleOverlap(
		const Box&      box,
		const Triangle& tri,
		Vec3&           outSeparationAxis,
		float&          outPenetrationDepth
	);
}
