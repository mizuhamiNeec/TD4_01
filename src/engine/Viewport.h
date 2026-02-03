#pragma once
#include <cstdint>

namespace Unnamed {
	/// @brief ビューポート構造体
	/// ビューポートの位置とサイズを表す
	struct Viewport {
		uint32_t x      = 0;
		uint32_t y      = 0;
		uint32_t width  = 800;
		uint32_t height = 600;
	};
}
