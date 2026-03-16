#pragma once

#include <string>

#include <json.hpp>

namespace Unnamed {
	struct UiDocumentAssetData {
		std::string    name;
		nlohmann::json rootJson;
	};
}
