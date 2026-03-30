#pragma once

#include <cfloat>
#include <string>
#include <vector>

namespace Unnamed {
	struct PresentationOutputAssetData {
		std::string type;
		std::string id;
		float       scale = 1.0f;
	};

	struct PresentationRuleAssetData {
		std::string                              cueId;
		float                                    minValue    = -FLT_MAX;
		float                                    maxValue    = FLT_MAX;
		float                                    cooldownSec = 0.0f;
		std::vector<PresentationOutputAssetData> outputs;
	};

	struct PresentationProfileAssetData {
		std::string                         name;
		std::vector<PresentationRuleAssetData> rules;
	};
}
