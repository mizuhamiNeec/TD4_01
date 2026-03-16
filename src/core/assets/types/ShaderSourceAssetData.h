#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	/// @brief シェーダーソースアセットのデータ構造体
	struct ShaderSourceAssetData {
		std::string              path;
		std::vector<std::string> includePaths;
	};
}
