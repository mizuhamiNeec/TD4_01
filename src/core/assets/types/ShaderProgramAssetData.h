#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Unnamed {
	struct ShaderProgramStage {
		std::string                                      sourcePath;
		std::string                                      entry;
		std::string                                      profile;
		std::vector<std::pair<std::string, std::string>> defines;
	};

	struct ShaderProgramAssetData {
		std::string                       name;
		std::optional<ShaderProgramStage> vs;
		std::optional<ShaderProgramStage> ps;
		std::optional<ShaderProgramStage> cs;
		std::vector<std::string>          includeDirectories;
	};
}
