#pragma once
#include <cstdint>

namespace Unnamed::Rhi {
	struct DeviceDesc {
		bool enableDebugLayer         = false; // デバッグレイヤーを有効にするか?
		bool enableGpuBasedValidation = false; // バリデーションレイヤーを有効にするか?
	};

	enum class TEXTURE_FORMAT : uint8_t {
		R8G8B8A8_UNORM,   // 32-bit RGBA
		R10G10B10A2_UNORM // 10-10-10-2 RGBA
	};

	struct SwapChainDesc {
		uint32_t width       = 1280; // デフォルト幅
		uint32_t height      = 720;  // デフォルト高さ
		uint32_t bufferCount = 2;    // ダブルバッファリング

		// バックバッファのフォーマット
		TEXTURE_FORMAT format = TEXTURE_FORMAT::R8G8B8A8_UNORM;

		bool vSync = false; // 垂直同期を行うか?
	};

	struct ClearColor {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;
	};
}
