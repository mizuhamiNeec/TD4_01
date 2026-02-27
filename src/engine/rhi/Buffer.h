#pragma once
#include <cstdint>
#include <d3d12.h>

#include <wrl/client.h>

#include "engine/unnamed/uprimitive/UPrimitives.h"

namespace Unnamed::Render {
	struct MeshBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb;
		Microsoft::WRL::ComPtr<ID3D12Resource> ib;

		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		D3D12_INDEX_BUFFER_VIEW  ibv = {};

		uint32_t indexCount = 0;

		AABB localAABB;
	};
}
