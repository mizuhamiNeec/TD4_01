#pragma once
#include <string>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	struct MaterialInstanceAssetData {
		std::string name;

		AssetID     materialId = kInvalidAssetID;
		std::string materialPath;

		std::unordered_map<std::string, std::string> textureOverrides;
		std::unordered_map<std::string, float>       scalarOverrides;
		std::unordered_map<std::string, Vec4>        vectorOverrides;
	};
}
