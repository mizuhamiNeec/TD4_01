#pragma once
#include "ShaderKey.h"

#include <d3d12.h>

#include "PipelineCache.h"

namespace Unnamed::Render {
	struct PipelineKey {
		ShaderKey            cs;
		ID3D12RootSignature* rootSignature = nullptr;

		bool operator==(const ComputePipelineKey& rhs) const;
	};
}
