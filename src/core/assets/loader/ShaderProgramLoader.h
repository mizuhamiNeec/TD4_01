#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	class ShaderProgramLoader : public IAssetLoader {
	public:
		explicit ShaderProgramLoader(AssetManager* assetManager);

		bool CanLoad(std::string_view path, ASSET_TYPE* outType) const override;
		LoadResult Load(const std::string& path) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
