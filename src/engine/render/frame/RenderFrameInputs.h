#pragma once
#include <cstdint>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/uprimitive/UPrimitives.h"

namespace Unnamed::Render {
	enum class SCENE_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,
		FIXED_ASPECT_16X9,
		HD_720P,
		FHD_1080P,
	};

	struct SceneRenderRequest {
		SCENE_RENDER_MODE mode = SCENE_RENDER_MODE::FIT_VIEWPORT;
		uint32_t          viewportPanelWidth = 0;
		uint32_t          viewportPanelHeight = 0;
		bool              preferRealtimeResize = true;
	};

	struct RenderCameraInput {
		Mat4 viewProj  = Mat4::identity;
		Vec3 cameraPos = Vec3::zero;
		bool valid     = false;
	};

	struct VisibleRenderObject {
		AssetID meshAssetId        = kInvalidAssetID;
		AssetID materialInstanceId = kInvalidAssetID;

		Mat4 world       = Mat4::identity;
		AABB worldBounds = {};

		bool     isSkinned         = false;
		uint32_t skeletonPaletteId = 0;
	};

	struct SkinningPaletteInput {
		AssetID           meshAssetId = kInvalidAssetID;
		std::vector<Mat4> boneMatrices;
	};

	struct PortalPairInput {
		bool enabled         = false;
		Mat4 fromPortalWorld = Mat4::identity;
		Mat4 toPortalWorld   = Mat4::identity;
	};

	struct RenderFrameInputs {
		uint32_t frameIndex = 0;

		RenderCameraInput                 camera = {};
		std::vector<VisibleRenderObject>  visibleObjects;
		std::vector<SkinningPaletteInput> skinningPalettes;
		std::vector<PortalPairInput>      portalPairs;
		SceneRenderRequest                sceneRenderRequest = {};

		float time = 0.0f;
	};
}
