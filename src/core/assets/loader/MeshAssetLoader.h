#pragma once
#include "interface/IAssetLoader.h"

#include <filesystem>

namespace Unnamed {
	class MeshAssetLoader final : public IAssetLoader {
	public:
		bool CanLoad(std::string_view path, ASSET_TYPE* outType) const override;
		LoadResult Load(const std::string& path) override;

	private:
		std::filesystem::path GetDerivedCachePath(
			const std::string& sourcePath
		) const;
		bool TryLoadDerivedCache(const std::string& path, LoadResult& out);
		bool WriteDerivedCache(const std::string& path, const LoadResult& in);
	};
}
