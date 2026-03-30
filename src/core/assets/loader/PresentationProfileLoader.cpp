#include "PresentationProfileLoader.h"

#include <algorithm>
#include <filesystem>

#include "core/assets/types/PresentationProfileAssetData.h"
#include "core/json/JsonReader.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsPresentationProfilePath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".presentation.json"
			);
		}
	}

	bool PresentationProfileLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsPresentationProfilePath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::PRESENTATION_PROFILE : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult PresentationProfileLoader::Load(const std::string& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const std::filesystem::path full(path);
		PresentationProfileAssetData data = {};
		data.name = root.Read<std::string>("name").value_or(
			full.stem().stem().string()
		);

		const JsonReader rulesNode = root["rules"];
		if (rulesNode.Valid() && rulesNode.IsArray()) {
			for (size_t i = 0; i < rulesNode.Size(); ++i) {
				const JsonReader ruleNode = rulesNode[i];
				if (!ruleNode.Valid() || !ruleNode.IsObject()) {
					continue;
				}

				PresentationRuleAssetData rule = {};
				if (ruleNode.Has("cueId")) {
					rule.cueId = ruleNode["cueId"].GetString("");
				}
				if (rule.cueId.empty()) {
					continue;
				}
				if (ruleNode.Has("minValue")) {
					rule.minValue = ruleNode["minValue"].GetFloat(rule.minValue);
				}
				if (ruleNode.Has("maxValue")) {
					rule.maxValue = ruleNode["maxValue"].GetFloat(rule.maxValue);
				}
				if (rule.maxValue < rule.minValue) {
					std::swap(rule.minValue, rule.maxValue);
				}
				if (ruleNode.Has("cooldownSec")) {
					rule.cooldownSec = std::max(
						0.0f, ruleNode["cooldownSec"].GetFloat(0.0f)
					);
				}

				const JsonReader outputsNode = ruleNode["outputs"];
				if (outputsNode.Valid() && outputsNode.IsArray()) {
					for (size_t outputIndex = 0; outputIndex < outputsNode.Size();
					     ++outputIndex) {
						const JsonReader outputNode = outputsNode[outputIndex];
						if (!outputNode.Valid() || !outputNode.IsObject()) {
							continue;
						}

						PresentationOutputAssetData output = {};
						if (outputNode.Has("type")) {
							output.type = outputNode["type"].GetString("");
						}
						if (outputNode.Has("id")) {
							output.id = outputNode["id"].GetString("");
						}
						if (output.type.empty() || output.id.empty()) {
							continue;
						}
						if (outputNode.Has("scale")) {
							output.scale = outputNode["scale"].GetFloat(output.scale);
						}
						rule.outputs.emplace_back(std::move(output));
					}
				}

				if (!rule.outputs.empty()) {
					data.rules.emplace_back(std::move(rule));
				}
			}
		}

		result.payload     = std::move(data);
		result.resolveName = full.stem().stem().string();

		std::error_code ec;
		if (std::filesystem::exists(path, ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}

		return result;
	}
}
