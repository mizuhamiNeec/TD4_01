#include "PostFxChainLoader.h"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/assets/AssetManager.h"
#include "core/assets/types/PostFxChainAssetData.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsPostFxPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".postfx.json"
			);
		}

		std::string ResolveRelativePath(
			const std::filesystem::path& baseDir, std::string path
		) {
			if (path.empty()) { return path; }
			std::filesystem::path p(path);
			if (p.is_relative()) { p = baseDir / p; }
			return StrUtil::NormalizePath(p.lexically_normal().string());
		}
	}

	PostFxChainLoader::PostFxChainLoader(AssetManager& assetManager) :
		mAssetManager(assetManager) {}

	bool PostFxChainLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsPostFxPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::POST_FX_CHAIN : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult PostFxChainLoader::Load(const std::string& path) {
		LoadResult    result = {};
		std::ifstream ifs(path);
		if (!ifs) { return result; }

		nlohmann::json root;
		try { ifs >> root; } catch (...) { return result; }

		const std::filesystem::path full(path);
		const std::filesystem::path baseDir = full.parent_path();

		PostFxChainAssetData data = {};
		data.name = root.value("name", full.filename().string());

		if (root.contains("passes") && root["passes"].is_array()) {
			for (const auto& p : root["passes"]) {
				if (!p.is_object()) { continue; }

				PostFxPassAssetData pass = {};
				pass.name                = p.value("name", std::string("Pass"));
				pass.enabled             = p.value("enabled", true);

				if (p.contains("shader")) {
					pass.shaderProgramPath = ResolveRelativePath(
						baseDir, p["shader"].get<std::string>()
					);
					pass.shaderProgramId = mAssetManager.LoadFromFile(
						pass.shaderProgramPath, ASSET_TYPE::SHADER_PROGRAM
					);
					if (pass.shaderProgramId != kInvalidAssetID) {
						result.dependencies.emplace_back(pass.shaderProgramId);
					}
				}

				if (p.contains("scalars") && p["scalars"].is_object()) {
					for (const auto& [k, v] : p["scalars"].items()) {
						if (!v.is_number()) { continue; }
						pass.scalarParams[k] = v.get<float>();
					}
				}

				data.passes.emplace_back(std::move(pass));
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
