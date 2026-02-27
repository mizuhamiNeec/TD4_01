#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"

namespace Unnamed {
	struct PostFxPassAssetData {
		std::string name;
		bool        enabled = true;

		AssetID     shaderProgramId = kInvalidAssetID;
		std::string shaderProgramPath;

		std::unordered_map<std::string, float> scalarParams;
	};

	struct PostFxChainAssetData {
		std::string                      name;
		std::vector<PostFxPassAssetData> passes;
	};
}
