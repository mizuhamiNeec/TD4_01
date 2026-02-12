#pragma once
#include <variant>
#include <vector>

#include "AssetID.h"
#include "FileStamp.h"

#include "types/TextureAsset.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// variant: 複数の型の一つを突っ込めるもの。 スッゲェェエ
	// monostate: 空の状態
	//-------------------------------------------------------------------------

	/// @brief アセットのペイロードデータ
	using AssetPayload = std::variant<
		std::monostate,
		TextureAssetData
	>;

	struct LoadResult {
		AssetPayload         payload;      // アセットのペイロードデータ
		std::vector<AssetID> dependencies; // パースして見つかった依存
		FileStamp            stamp;        // ファイルのタイムスタンプ
		std::string          resolveName;  // 表示名
	};
}
