#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	enum class MATERIAL_DOMAIN : uint8_t {
		UNLIT           = 0,
		PBR_METAL_ROUGH = 1,
	};

	struct MaterialRenderStateData {
		bool depthEnable  = true;
		bool depthWrite   = true;
		bool cullBackFace = true;
		bool blendEnable  = false;

		bool    stencilEnable    = false;
		uint8_t stencilReadMask  = 0xFF;
		uint8_t stencilWriteMask = 0xFF;
	};

	struct MaterialAssetData {
		std::string name;

		AssetID         shaderProgramId = kInvalidAssetID;
		std::string     shaderProgramPath;
		MATERIAL_DOMAIN domain = MATERIAL_DOMAIN::PBR_METAL_ROUGH;

		MaterialRenderStateData renderState = {};

		std::unordered_map<std::string, float> scalarParams;
		std::unordered_map<std::string, Vec4>  vectorParams;
	};
}
