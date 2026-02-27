#pragma once
#include "core/assets/AssetType.h"
#include "core/assets/LoadResult.h"

namespace Unnamed {
	/// @brief アセットローダーインターフェース
	class IAssetLoader {
	public:
		virtual      ~IAssetLoader() = default;
		virtual bool CanLoad(
			std::string_view path, ASSET_TYPE* outType
		) const = 0;

		virtual LoadResult Load(const std::string& path) = 0;
	};
}
