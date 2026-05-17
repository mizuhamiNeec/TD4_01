#include "ParticlePresetLoader.h"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/assets/types/ParticlePresetAssetData.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		using json = nlohmann::json;

		[[nodiscard]] bool IsCandidateJsonPath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(".json");
		}

		[[nodiscard]] bool LooksLikeParticlePresetText(
			const std::string& source
		) {
			return source.find("\"emitterSettings\"") != std::string::npos &&
			       source.find("\"emitterSpawn\"") != std::string::npos &&
			       source.find("\"particleUpdate\"") != std::string::npos &&
			       source.find("\"render\"") != std::string::npos;
		}
	}

	bool ParticlePresetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		if (outType) {
			*outType = ASSET_TYPE::UNKNOWN;
		}
		if (!IsCandidateJsonPath(path)) {
			return false;
		}

		std::string source;
		if (!StrUtil::ReadFileToString(std::string(path), source)) {
			return false;
		}
		if (!LooksLikeParticlePresetText(source)) {
			return false;
		}

		if (outType) {
			*outType = ASSET_TYPE::PARTICLE_PRESET;
		}
		return true;
	}

	LoadResult ParticlePresetLoader::Load(const std::string& path) {
		LoadResult result = {};

		std::ifstream ifs(path);
		if (!ifs) {
			return result;
		}

		json j;
		try {
			ifs >> j;
		} catch (...) {
			return result;
		}

		ParticlePresetAssetData data = {};
		data.sourcePath              = path;
		data.presetName              = j.value("name", "");

		if (data.presetName.empty()) {
			const std::filesystem::path p(path);
			data.presetName = p.stem().string();
		}

		result.payload     = std::move(data);
		result.resolveName = data.presetName;

		std::error_code ec;
		if (std::filesystem::exists(path, ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}
		if (const auto lastWrite = std::filesystem::last_write_time(path, ec);
			!ec) {
			result.stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
		}

		return result;
	}
}
