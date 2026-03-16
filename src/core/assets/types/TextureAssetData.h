#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	// @brief テクスチャのミップマップ情報
	struct TextureMip {
		std::vector<uint8_t> bytes;        // ピクセルデータ
		uint32_t             width    = 0; // ミップマップの幅
		uint32_t             height   = 0; // ミップマップの高さ
		size_t               rowPitch = 0; // 1行あたりのバイト数
	};

	/// @brief テクスチャアセットのデータ構造体
	struct TextureAssetData {
		std::vector<uint8_t>    bytes;          // 元のファイルデータ
		std::vector<TextureMip> mips;           // ミップマップ情報
		uint32_t                width  = 0;     // テクスチャの幅
		uint32_t                height = 0;     // テクスチャの高さ
		bool                    isSRGB = false; // sRGBかどうか
		std::string             sourcePath;     // ソースファイルパス
	};
}
