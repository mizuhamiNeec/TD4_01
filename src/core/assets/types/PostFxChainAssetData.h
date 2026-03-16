#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"

namespace Unnamed {
	/// @brief ポストエフェクトパスのデータ構造体
	struct PostFxPassAssetData {
		std::string name;
		bool        enabled = true;

		AssetID     shaderProgramId = kInvalidAssetID;
		std::string shaderProgramPath;

		std::unordered_map<std::string, float> scalarParams;
	};

	/// @brief ポストエフェクトチェーンアセットのデータ構造体
	struct PostFxChainAssetData {
		std::string                      name;
		std::vector<PostFxPassAssetData> passes;
	};
}
