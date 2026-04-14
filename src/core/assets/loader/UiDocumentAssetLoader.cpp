
#include "UiDocumentAssetLoader.h"

#include <fstream>

#include "core/string/StrUtil.h"

namespace Unnamed {
	bool UiDocumentAssetLoader::CanLoad(
		std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = StrUtil::ToLowerCase(std::string(path)).ends_with(
			".ui.json"
		);
		if (outType) {
			*outType = ok ? ASSET_TYPE::UI_DOCUMENT : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult UiDocumentAssetLoader::Load(const std::string& path) {
		LoadResult    result = {};
		std::ifstream ifs(path);
		if (!ifs) {
			return result;
		}

		nlohmann::json root;
		try {
			ifs >> root;
		} catch (...) {
			return result;
		}

		if (
			!root.is_object() ||
			!root.contains("version") ||
			!root["version"].is_number_integer() ||
			root["version"].get<int>() != 2 ||
			!root.contains("root")
		) {
			return result;
		}

		const std::filesystem::path full(path);
		UiDocumentAssetData         data = {};
		data.name = root.value("name", full.filename().string());
		data.rootJson = std::move(root);

		result.payload     = std::move(data);
		result.resolveName = full.stem().stem().string();

		std::error_code ec;
		if (std::filesystem::exists(path, ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}
		return result;
	}
}
