#pragma once
#include <array>
#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

#include "core/math/Mat4.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	struct MeshVertex {
		Vec3                    position    = Vec3::zero;
		Vec3                    normal      = Vec3::up;
		Vec2                    uv          = Vec2::zero;
		std::array<uint16_t, 4> boneIndices = {0, 0, 0, 0};
		std::array<float, 4>    boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
	};

	struct SkeletonBoneAssetData {
		std::string name;
		int32_t     parentIndex     = -1;
		Mat4        inverseBindPose = Mat4::identity;
	};

	struct MeshAssetData {
		std::vector<MeshVertex>            vertices;
		std::vector<uint32_t>              indices;
		std::vector<SkeletonBoneAssetData> skeleton;
		bool                               hasSkinning = false;

		Vec3 localBoundsMin = Vec3(FLT_MAX);
		Vec3 localBoundsMax = Vec3(-FLT_MAX);

		std::string sourcePath;
	};
}
