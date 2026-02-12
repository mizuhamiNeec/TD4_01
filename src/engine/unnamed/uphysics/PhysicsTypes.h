#pragma once

#include <core/math/Math.h>

#include <engine/Entity/Entity.h>
#include <engine/unnamed/uprimitive/UPrimitives.h>

namespace UPhysics {
	// ヒット情報
	struct Hit {
		float    t          = FLT_MAX; // 0～1
		float    depth      = 0.0f;
		Vec3     pos        = Vec3::zero;
		Vec3     normal     = Vec3::zero;
		uint32_t triIndex   = UINT_FAST32_MAX;
		bool     startSolid = false; // 開始時に形状が重なっていたか
		bool     allsolid   = false; // トレース全域で固体内だったか

		Entity* hitEntity = nullptr; // ヒットしたエンティティ
	};

	// 形状情報
	struct TriInfo {
		Unnamed::AABB bounds;   // 境界
		Vec3          center;   // 中心
		uint32_t      triIndex; // 三角形のインデックス
	};
}
