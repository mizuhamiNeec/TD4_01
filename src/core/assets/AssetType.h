#pragma once
#include <cstdint>
#include <string>

namespace Unnamed {
	enum class ASSET_TYPE : uint8_t {
		UNKNOWN  = 1 << 0, // 不明
		TEXTURE  = 1 << 1, // テクスチャ
		SHADER   = 1 << 2, // シェーダー
		MATERIAL = 1 << 3, // マテリアル
		MESH     = 1 << 4, // メッシュ
		SOUND    = 1 << 5, // サウンド
		RAW_FILE = 1 << 6, // 生ファイル
	};

	std::string ToString(ASSET_TYPE e);
}
