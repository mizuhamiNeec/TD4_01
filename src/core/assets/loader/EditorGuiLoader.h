#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class EditorGuiLoader : public IAssetLoader {
	public:
		bool CanLoad(std::string_view path, ASSET_TYPE* outType) const override;
		LoadResult Load(const std::string& path) override;
	};
}
