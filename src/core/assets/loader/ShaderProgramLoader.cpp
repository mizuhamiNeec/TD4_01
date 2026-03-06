#include "ShaderProgramLoader.h"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsShaderProgramPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".shader.json"
			);
		}

		std::string ResolveRelativePath(
			const std::filesystem::path& baseDir, std::string path
		) {
			if (path.empty()) {
				return path;
			}

			std::filesystem::path p(path);
			if (p.is_relative()) {
				p = baseDir / p;
			}
			return StrUtil::NormalizePath(p.lexically_normal().string());
		}

		void ParseDefines(
			const nlohmann::json&                             j,
			std::vector<std::pair<std::string, std::string>>& outDefines
		) {
			if (j.is_array()) {
				for (const auto& v : j) {
					if (!v.is_string()) {
						continue;
					}
					const std::string s  = v.get<std::string>();
					const size_t      eq = s.find('=');
					if (eq == std::string::npos) {
						outDefines.emplace_back(s, "1");
					} else {
						outDefines.emplace_back(
							s.substr(0, eq), s.substr(eq + 1)
						);
					}
				}
				return;
			}

			if (!j.is_object()) {
				return;
			}
			for (const auto& [k, v] : j.items()) {
				if (v.is_string()) {
					outDefines.emplace_back(k, v.get<std::string>());
				} else if (v.is_number_integer()) {
					outDefines.emplace_back(k, std::to_string(v.get<int>()));
				} else if (v.is_number_float()) {
					outDefines.emplace_back(
						k, std::to_string(v.get<float>())
					);
				} else if (v.is_boolean()) {
					outDefines.emplace_back(k, v.get<bool>() ? "1" : "0");
				}
			}
		}

		std::optional<ShaderProgramStage> ParseStage(
			const nlohmann::json& j, const std::filesystem::path& baseDir
		) {
			if (!j.is_object()) {
				return std::nullopt;
			}
			if (!j.contains("path")) {
				return std::nullopt;
			}

			ShaderProgramStage stage = {};
			stage.sourcePath         = ResolveRelativePath(
				baseDir, j["path"].get<std::string>()
			);
			stage.entry   = j.value("entry", std::string("Main"));
			stage.profile = j.value("profile", std::string());
			if (j.contains("defines")) {
				ParseDefines(j["defines"], stage.defines);
			}
			return stage;
		}
	}

	ShaderProgramLoader::ShaderProgramLoader(AssetManager& assetManager) :
		mAssetManager(assetManager) {}

	bool ShaderProgramLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsShaderProgramPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::SHADER_PROGRAM : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult ShaderProgramLoader::Load(const std::string& path) {
		LoadResult result = {};

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

		const std::filesystem::path full(path);
		const std::filesystem::path baseDir = full.parent_path();

		ShaderProgramAssetData data = {};
		data.name                   = root.value(
			"name", full.filename().string()
		);

		if (root.contains("vs")) {
			data.vs = ParseStage(root["vs"], baseDir);
		}
		if (root.contains("ps")) {
			data.ps = ParseStage(root["ps"], baseDir);
		}
		if (root.contains("cs")) {
			data.cs = ParseStage(root["cs"], baseDir);
		}

		if (root.contains("includeDirs") && root["includeDirs"].is_array()) {
			for (const auto& v : root["includeDirs"]) {
				if (!v.is_string()) {
					continue;
				}
				data.includeDirectories.emplace_back(
					ResolveRelativePath(baseDir, v.get<std::string>())
				);
			}
		}

		auto addStageDependency = [&](
			const std::optional<ShaderProgramStage>& stage
		) {
			if (!stage.has_value()) {
				return;
			}
			const AssetID dep = mAssetManager.LoadFromFile(
				stage->sourcePath, ASSET_TYPE::SHADER_SOURCE
			);
			if (dep != kInvalidAssetID) {
				result.dependencies.emplace_back(dep);
			}
		};

		addStageDependency(data.vs);
		addStageDependency(data.ps);
		addStageDependency(data.cs);

		result.payload     = std::move(data);
		result.resolveName = full.stem().stem().string();

		std::error_code ec;
		if (std::filesystem::exists(path, ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}

		return result;
	}
}
