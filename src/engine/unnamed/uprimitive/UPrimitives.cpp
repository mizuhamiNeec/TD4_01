#include "UPrimitives.h"

#include "core/math/Mat4.h"

namespace Unnamed {
	Plane NormalizePlane(const Plane& plane) {
		const float len = plane.normal.Length();
		if (len <= 0.0f) {
			// 法線ベクトルの長さがゼロ以下の場合は正規化できないので、そのまま返す
			return plane;
		}
		Plane o  = plane;
		o.normal /= len;
		o.d      /= len;
		return o;
	}

	Frustum BuildFrustum(const Mat4& viewProjRowVector) {
		const Mat4 m = viewProjRowVector.Transpose();

		auto Row = [&](const int r) {
			return Vec4(m.m[r][0], m.m[r][1], m.m[r][2], m.m[r][3]);
		};

		const Vec4 r0 = Row(0);
		const Vec4 r1 = Row(1);
		const Vec4 r2 = Row(2);
		const Vec4 r3 = Row(3);

		Frustum f = {};
		// D3D NDC: x,y in [-w,+w], z in [0,+w]
		// 行ベクトル系なので、転置して列を行として扱う
		// 左
		f.planes[0] = NormalizePlane(
			{.normal = Vec3(r3 + r0), .d = r3.w + r0.w}
		);
		// 右
		f.planes[1] = NormalizePlane(
			{.normal = Vec3(r3 - r0), .d = r3.w - r0.w}
		);
		// 下
		f.planes[2] = NormalizePlane(
			{.normal = Vec3(r3 + r1), .d = r3.w + r1.w}
		);
		// 上
		f.planes[3] = NormalizePlane(
			{.normal = Vec3(r3 - r1), .d = r3.w - r1.w}
		);
		// ニア (z >= 0)
		f.planes[4] = NormalizePlane(
			{.normal = Vec3(r2), .d = r2.w}
		);
		// ファー (z <= w)
		f.planes[5] = NormalizePlane(
			{.normal = Vec3(r3 - r2), .d = r3.w - r2.w}
		);

		return f;
	}

	/// @brief 点でAABBを拡張します
	/// @param point 拡張する点
	void AABB::Expand(const Vec3& point) {
		min = Vec3::Min(min, point);
		max = Vec3::Max(max, point);
	}

	/// @brief AABBでAABBを拡張します
	/// @param aabb 拡張するAABB
	void AABB::Expand(const AABB& aabb) {
		Expand(aabb.min);
		Expand(aabb.max);
	}

	/// @brief AABBの中心を取得します
	/// @return AABBの中心座標
	Vec3 AABB::Center() const { return (min + max) * 0.5f; }

	/// @brief AABBの表面積を取得します
	/// @return AABBの表面積
	float AABB::SurfaceArea() const {
		const Vec3 d = max - min;
		return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	/// @brief AABBの最も長い軸を取得します
	/// @return 最も長い軸のインデックス(0=x, 1=y, 2=z)
	int AABB::LongestAxis() const {
		const Vec3 d = max - min;
		if (d.x >= d.y && d.x >= d.z) { return 0; }
		if (d.y >= d.x && d.y >= d.z) { return 1; }
		return 2;
	}

	/// @brief AABBのサイズを取得します
	/// @return AABBのサイズベクトル
	Vec3 AABB::Size() const { return max - min; }

	AABB TransformAABB(const AABB& local, const Mat4& world) {
		const Vec3 c[8]{
			{local.min.x, local.min.y, local.min.z},
			{local.max.x, local.min.y, local.min.z},
			{local.min.x, local.max.y, local.min.z},
			{local.max.x, local.max.y, local.min.z},
			{local.min.x, local.min.y, local.max.z},
			{local.max.x, local.min.y, local.max.z},
			{local.min.x, local.max.y, local.max.z},
			{local.max.x, local.max.y, local.max.z},
		};

		AABB aabb;

		for (const auto i : c) {
			const auto p4 = Vec4(i.x, i.y, i.z, 1.0f);
			const Vec4 tp = world * p4;
			const auto p  = Vec3(tp);

			aabb.Expand(p);
		}

		return aabb;
	}

	bool IsAABBOutsidePlane(const AABB& aabb, const Plane& p) {
		Vec3 v = aabb.min;
		if (p.normal.x >= 0) { v.x = aabb.max.x; }
		if (p.normal.y >= 0) { v.y = aabb.max.y; }
		if (p.normal.z >= 0) { v.z = aabb.max.z; }
		return p.normal.Dot(v) + p.d < 0.0f;
	}

	bool IsVisible(const Frustum& f, const AABB& worldAABB) {
		for (const Plane& p : f.planes) {
			if (IsAABBOutsidePlane(worldAABB, p)) { return false; }
		}
		return true;
	}
}
