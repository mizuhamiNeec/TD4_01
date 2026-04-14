#pragma once
#include <cstdint>
#include <string_view>

namespace Unnamed {
	using TypeId = uint64_t;

	// FNV-1a 64ビットハッシュ定数
	static constexpr uint64_t kFnv1A64OffsetBasis = 14695981039346656037ull;

	// FNV-1a 64ビットプライム定数
	static constexpr TypeId kFnv1A64Prime = 1099511628211ull;

	/// @brief 文字列のFNV-1aハッシュを計算します。
	/// @param s ハッシュを計算する文字列ビュー
	/// @return 計算されたハッシュ値
	constexpr TypeId HashTypeName(const std::string_view s) {
		TypeId h = kFnv1A64OffsetBasis;
		for (const char c : s) {
			h ^= static_cast<unsigned char>(c);
			h *= kFnv1A64Prime;
		}
		return h;
	}
}
