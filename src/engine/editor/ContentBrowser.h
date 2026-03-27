#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <string>

#include "core/assets/AssetType.h"

namespace Unnamed::EditorContentBrowser {
	using AssetTypeMask = uint32_t;
	inline constexpr AssetTypeMask kAssetTypeMaskAny = 0xFFFFFFFFu;
	inline constexpr const char*   kAssetDragDropPayloadId =
		"UNNAMED_ASSET_REF";

	struct AssetDragDropPayload {
		uint16_t assetType = 0;
		char     path[512] = {};
	};

	struct BrowserViewState {
		std::string rootPath    = "./content";
		std::string currentPath = "./content";
		std::string selectedPath;
		bool        iconView = false;
		float       iconSize = 96.0f;
	};

	[[nodiscard]] AssetTypeMask AssetTypeToMask(ASSET_TYPE type);
	[[nodiscard]] bool IsAssetTypeAccepted(
		ASSET_TYPE type,
		AssetTypeMask acceptedMask
	);

	bool DrawAssetPathPicker(
		const char*   label,
		std::string&  path,
		AssetTypeMask acceptedMask,
		const char*   helpText = nullptr
	);

	void DrawWindow(BrowserViewState& state, const char* windowName);
}

#endif
