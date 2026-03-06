#pragma once
#include <cstdint>
#include <vector>

#include <engine/unnamed/uphysics/BVHBuilder.h>

/// @brief BVH構造体
struct BVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t>           triIndices;
};

/// @brief 登録されたBVH構造体
struct RegisteredBVH {
	std::vector<UPhysics::FlatNode> nodes;
	std::vector<uint32_t>           triIndices;

	size_t   triStart  = 0;
	size_t   triCount  = 0;
	uint64_t ownerGuid = 0;
};
