#include "MaterialInstanceAssetLoader.h"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialInstanceAssetData.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsMaterialInstancePath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".matinst.json"
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

		Vec4 ParseVec4(const nlohmann::json& j, const Vec4 fallback) {
			if (!j.is_array() || j.size() < 4) {
				return fallback;
			}
			return Vec4(
				j[0].get<float>(),
				j[1].get<float>(),
				j[2].get<float>(),
				j[3].get<float>()
			);
		}
	}

	MaterialInstanceAssetLoader::MaterialInstanceAssetLoader(
		AssetManager& assetManager
	) : mAssetManager(assetManager) {}

	bool MaterialInstanceAssetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = IsMaterialInstancePath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL_INSTANCE : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialInstanceAssetLoader::Load(const std::string& path) {
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

		const std::filesystem::path full(path);
		const std::filesystem::path baseDir = full.parent_path();

		MaterialInstanceAssetData data = {};
		data.name                      = root.value(
			"name", full.filename().string()
		);

		if (root.contains("material")) {
			data.materialPath = ResolveRelativePath(
				baseDir, root["material"].get<std::string>()
			);
			data.materialId = mAssetManager.LoadFromFile(
				data.materialPath, ASSET_TYPE::MATERIAL
			);
			if (data.materialId != kInvalidAssetID) {
				result.dependencies.emplace_back(data.materialId);
			}
		}

		if (root.contains("textures") && root["textures"].is_object()) {
			for (const auto& [slot, p] : root["textures"].items()) {
				if (!p.is_string()) {
					continue;
				}
				std::string texturePath = ResolveRelativePath(
					baseDir, p.get<std::string>()
				);
				data.textureOverrides[slot] = texturePath;

				const AssetID textureDep = mAssetManager.LoadFromFile(
					texturePath, ASSET_TYPE::TEXTURE
				);
				if (textureDep != kInvalidAssetID) {
					result.dependencies.emplace_back(textureDep);
				}
			}
		}

		if (root.contains("scalars") && root["scalars"].is_object()) {
			for (const auto& [k, v] : root["scalars"].items()) {
				if (!v.is_number()) {
					continue;
				}
				data.scalarOverrides[k] = v.get<float>();
			}
		}

		if (root.contains("vectors") && root["vectors"].is_object()) {
			for (const auto& [k, v] : root["vectors"].items()) {
				data.vectorOverrides[k] = ParseVec4(v, Vec4(0, 0, 0, 0));
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
