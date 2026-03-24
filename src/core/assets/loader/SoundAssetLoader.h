#pragma once

#include "core/assets/loader/interface/IAssetLoader.h"

namespace Unnamed {
	class SoundAssetLoader final : public IAssetLoader {
	public:
		bool CanLoad(
			std::string_view path, ASSET_TYPE* outType
		) const override;

		LoadResult Load(const std::string& path) override;
	};
}
