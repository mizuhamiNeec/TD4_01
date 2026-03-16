#pragma once
#include <cstdint>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

#include "engine/unnamed/primitive/Primitives.h"

namespace Unnamed::Render {
	enum class PROJECTION_DEPTH_MODE : uint8_t {
		ForwardZ,
		ReverseZ,
	};

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
		bool              presentToSwapChain = true;
		bool              clearSwapChainWhenNotPresenting = false;
	};

	struct RenderCameraInput {
		Mat4                  view      = Mat4::identity;
		Mat4                  proj      = Mat4::identity;
		Mat4                  viewProj  = Mat4::identity;
		Vec3                  cameraPos = Vec3::zero;
		float                 nearZ     = 0.001f;
		float                 farZ      = 10000.0f;
		PROJECTION_DEPTH_MODE depthMode = PROJECTION_DEPTH_MODE::ReverseZ;
		bool                  valid     = false;
	};

	struct VisibleRenderObject {
		AssetID  meshAssetId        = kInvalidAssetID;
		AssetID  materialInstanceId = kInvalidAssetID;
		uint64_t ownerEntityGuid    = 0;
		bool     isPortalSurface    = false;

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
		bool     enabled               = false;
		uint64_t fromPortalGuid        = 0;
		uint64_t toPortalGuid          = 0;
		Mat4     fromPortalWorld       = Mat4::identity;
		Mat4     toPortalWorld         = Mat4::identity;
		Vec2     fromPortalHalfExtents = Vec2(0.5f, 1.0f);
		Vec2     toPortalHalfExtents   = Vec2(0.5f, 1.0f);
	};

	struct ScreenSpriteInput {
		AssetID textureAssetId = kInvalidAssetID;
		Vec2    positionPx     = Vec2::zero;
		Vec2    sizePx         = Vec2::one;
		Vec2    anchor         = Vec2(0.5f, 0.5f);
		float   rotationRad    = 0.0f;
		Vec4    color          = Vec4::one;
		int32_t sortKey        = 0;
	};

	struct WorldBillboardInput {
		AssetID textureAssetId = kInvalidAssetID;
		Vec3    worldPosition  = Vec3::zero;
		Vec2    sizeWorld      = Vec2::one;
		Vec4    color          = Vec4::one;
		float   rotationRad    = 0.0f;
		int32_t sortKey        = 0;
	};

	struct RenderFrameInputs {
		uint32_t frameIndex = 0;

		RenderCameraInput                 camera = {};
		std::vector<VisibleRenderObject>  visibleObjects;
		std::vector<SkinningPaletteInput> skinningPalettes;
		std::vector<PortalPairInput>      portalPairs;
		std::vector<WorldBillboardInput>  worldBillboards;
		std::vector<ScreenSpriteInput>    screenSprites;
		SceneRenderRequest                sceneRenderRequest = {};

		float time = 0.0f;
	};
}
