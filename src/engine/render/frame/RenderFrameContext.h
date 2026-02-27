#pragma once
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"

namespace Unnamed::Render {
	struct RenderFrameContext {
		// フレーム内で使うスキニングパレットのキャッシュ
		std::unordered_map<AssetID, uint32_t> skinningPaletteIndexByMesh;
		std::vector<Mat4>                     skinningMatrices;

		void Reset() {
			skinningPaletteIndexByMesh.clear();
			skinningMatrices.clear();
		}
	};
}
