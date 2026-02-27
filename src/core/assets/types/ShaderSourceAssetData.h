#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	struct ShaderSourceAssetData {
		std::string              path;
		std::vector<std::string> includePaths;
	};
}
