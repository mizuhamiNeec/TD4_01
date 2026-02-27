#pragma once
#include <variant>
#include <vector>

#include "AssetID.h"
#include "FileStamp.h"

#include "types/MaterialAssetData.h"
#include "types/MaterialInstanceAssetData.h"
#include "types/MeshAssetData.h"
#include "types/PostFxChainAssetData.h"
#include "types/ShaderProgramAssetData.h"
#include "types/ShaderSourceAssetData.h"
#include "types/TextureAssetData.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// variant: 複数の型の一つを突っ込めるもの。 スッゲェェエ
	// monostate: 空の状態
	//-------------------------------------------------------------------------

	/// @brief アセットのペイロードデータ
	using AssetPayload = std::variant<
		std::monostate,
		TextureAssetData,
		ShaderSourceAssetData,
		MeshAssetData,
		ShaderProgramAssetData,
		MaterialAssetData,
		MaterialInstanceAssetData,
		PostFxChainAssetData
	>;

	struct LoadResult {
		AssetPayload         payload;      // アセットのペイロードデータ
		std::vector<AssetID> dependencies; // パースして見つかった依存
		FileStamp            stamp;        // ファイルのタイムスタンプ
		std::string          resolveName;  // 表示名
	};
}
