#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace Unnamed::Render {
	struct DrawKey {
		ID3D12PipelineState*     pso       = nullptr;
		ID3D12RootSignature*     rootSig   = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vbv       = {};
		D3D12_INDEX_BUFFER_VIEW  ibv       = {};
		DXGI_FORMAT              rtvFormat = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT              dsvFormat = DXGI_FORMAT_UNKNOWN;

		bool operator==(const DrawKey& other) const;
	};

	struct DrawKeyHash {
		size_t operator()(const DrawKey& key) const noexcept;
	};

	struct MeshDrawItem {
		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		D3D12_INDEX_BUFFER_VIEW  ibv = {};

		uint32_t indexCount      = 0;
		uint32_t objectCbIndex   = 0; // オブジェクト定数バッファのインデックス
		uint32_t materialCbIndex = 0;
		uint32_t albedoTextureId = 0;
		uint32_t skinningCbIndex = 0;
		uint32_t useSkinning     = 0;

		// TODO: マテリアルIDとかPSOキーとかルートシグネチャとかテクスチャとか
	};

	struct DrawBatch {
		DrawKey                   key;
		std::vector<MeshDrawItem> items;
	};

	std::vector<DrawBatch> BuildDrawBatchesFromItems(
		const std::vector<MeshDrawItem>& drawList, const DrawKey& baseKey
	);
}
