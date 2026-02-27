#include "MaterialAssetLoader.h"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialAssetData.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsMaterialPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".material.json"
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

		MATERIAL_DOMAIN ParseDomain(const std::string& s) {
			const auto v = StrUtil::ToLowerCase(s);
			if (v == "unlit") { return MATERIAL_DOMAIN::UNLIT; }
			return MATERIAL_DOMAIN::PBR_METAL_ROUGH;
		}

		Vec4 ParseVec4(const nlohmann::json& j, const Vec4 fallback) {
			if (!j.is_array() || j.size() < 4) { return fallback; }
			return Vec4(
				j[0].get<float>(),
				j[1].get<float>(),
				j[2].get<float>(),
				j[3].get<float>()
			);
		}
	}

	MaterialAssetLoader::MaterialAssetLoader(AssetManager& assetManager) :
		mAssetManager(assetManager) {}

	bool MaterialAssetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsMaterialPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialAssetLoader::Load(const std::string& path) {
		LoadResult    result = {};
		std::ifstream ifs(path);
		if (!ifs) { return result; }

		nlohmann::json root;
		try { ifs >> root; } catch (...) { return result; }

		const std::filesystem::path full(path);
		const std::filesystem::path baseDir = full.parent_path();

		MaterialAssetData data = {};
		data.name              = root.value("name", full.filename().string());
		data.domain            = ParseDomain(root.value("domain", "pbr"));

		if (root.contains("shader")) {
			data.shaderProgramPath = ResolveRelativePath(
				baseDir, root["shader"].get<std::string>()
			);
			data.shaderProgramId = mAssetManager.LoadFromFile(
				data.shaderProgramPath, ASSET_TYPE::SHADER_PROGRAM
			);
			if (data.shaderProgramId != kInvalidAssetID) {
				result.dependencies.emplace_back(data.shaderProgramId);
			}
		}

		if (root.contains("renderState") && root["renderState"].is_object()) {
			const auto& rs                   = root["renderState"];
			data.renderState.depthEnable     = rs.value("depthEnable", true);
			data.renderState.depthWrite      = rs.value("depthWrite", true);
			data.renderState.cullBackFace    = rs.value("cullBackFace", true);
			data.renderState.blendEnable     = rs.value("blendEnable", false);
			data.renderState.stencilEnable   = rs.value("stencilEnable", false);
			data.renderState.stencilReadMask = static_cast<uint8_t>(
				rs.value("stencilReadMask", 255)
			);
			data.renderState.stencilWriteMask = static_cast<uint8_t>(
				rs.value("stencilWriteMask", 255)
			);
		}

		if (root.contains("scalars") && root["scalars"].is_object()) {
			for (const auto& [k, v] : root["scalars"].items()) {
				if (!v.is_number()) { continue; }
				data.scalarParams[k] = v.get<float>();
			}
		}

		if (root.contains("vectors") && root["vectors"].is_object()) {
			for (const auto& [k, v] : root["vectors"].items()) {
				data.vectorParams[k] = ParseVec4(v, Vec4(0, 0, 0, 0));
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
