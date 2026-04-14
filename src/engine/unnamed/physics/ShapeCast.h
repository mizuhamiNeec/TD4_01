#pragma once

#include <engine/unnamed/primitive/Primitives.h>

namespace Unnamed::Physics {
	/// @brief 形状キャストインターフェース
	struct ShapeCast {
		virtual                    ~ShapeCast() = default;
		[[nodiscard]] virtual AABB ExpandNode(
			const AABB& nodeBounds
		) const = 0;

		virtual bool TestTriangle(
			const Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		) const = 0;

		// 開始時(t=0)オーバーラップ検出
		// 重なっていれば true。depth はめり込み量（不明なら 0）
		virtual bool OverlapAtStart(
			const Triangle& tri,
			float&          depth,
			Vec3&           normal
		) const = 0;

		virtual Vec3 ComputeImpactPoint(
			const Vec3& start,
			const Vec3& dirNormalized,
			const float length,
			const float toi,
			const Vec3& normal
		) const {
			(void)normal;
			const float travel = toi * length;
			return start + dirNormalized * travel;
		}
	};
}
