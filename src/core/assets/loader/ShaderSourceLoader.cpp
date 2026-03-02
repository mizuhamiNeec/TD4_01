#include "ShaderSourceLoader.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel            = "ShaderSrcLdr";
	static constexpr std::string_view kSupportedExtension = ".hlsl";

	ShaderSourceLoader::ShaderSourceLoader(AssetManager& assetManager) :
		mAssetManager(assetManager) {}

	bool ShaderSourceLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		// outTypeがnullptrならfalseを返す
		if (!outType) { return false; }
		// 拡張子がサポートされているかを確認
		if (StrUtil::HasExtension(path, kSupportedExtension)) {
			*outType = ASSET_TYPE::SHADER_SOURCE;
			return true;
		}
		return false;
	}

	LoadResult ShaderSourceLoader::Load(const std::string& path) {
		LoadResult r = {};

		ShaderSourceAssetData data = {};
		data.path                  = path;

		std::string text;
		if (!StrUtil::ReadFileToString(path, text)) {
			Error(kChannel, "シェーダーソースの読み込みに失敗しました: {}", path);
			return r;
		}
		data.includePaths = ParseIncludes(text);

		const std::filesystem::path baseDir = std::filesystem::path(path).
			parent_path();

		// 依存関係の解決
		for (const auto& include : data.includePaths) {
			std::filesystem::path includePath(include);
			if (includePath.is_relative()) {
				includePath = baseDir / includePath;
			}
			const std::string depPath = StrUtil::NormalizePath(
				includePath.lexically_normal().string()
			);
			const AssetID depId = mAssetManager.LoadFromFile(
				depPath, ASSET_TYPE::SHADER_SOURCE
			);
			if (depId != kInvalidAssetID) {
				r.dependencies.emplace_back(depId);
			}
		}

		r.payload     = std::move(data);
		r.resolveName = std::filesystem::path(path).filename().string();
		if (std::error_code ec; std::filesystem::exists(path, ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(path);
		}

		return r;
	}

	std::vector<std::string> ShaderSourceLoader::ParseIncludes(
		const std::string& text
	) {
		std::vector<std::string> result;
		size_t                   pos = 0;

		while (true) {
			pos = text.find("#include", pos);
			if (pos == std::string::npos) { break; }

			const auto quote0 = text.find('"', pos);
			if (quote0 == std::string::npos) {
				static constexpr size_t kIncludeLen = 8; // "#include"の文字列の長さ
				pos                                 += kIncludeLen;
				continue;
			}
			const auto quote1 = text.find('"', quote0 + 1);
			if (quote1 == std::string::npos) {
				pos = quote0 + 1;
				continue;
			}

			result.emplace_back(text.substr(quote0 + 1, quote1 - (quote0 + 1)));
			pos = quote1 + 1;
		}
		return result;
	}
}
