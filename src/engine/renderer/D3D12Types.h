#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

#include "core/math/Vec4.h"


/// @brief バッファフォーマット
struct RenderTargetTexture {
	Microsoft::WRL::ComPtr<ID3D12Resource> rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE            rtvHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE            srvHandleGPU;
	uint32_t                               srvIndex;
};

/// @brief 深度ステンシルテクスチャ構造体
struct DepthStencilTexture {
	Microsoft::WRL::ComPtr<ID3D12Resource> dsv;
	D3D12_CPU_DESCRIPTOR_HANDLE            dsvHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE            srvHandleGPU;
	uint32_t                               srvIndex;
};

/// @brief レンダーパスターゲット構造体
struct RenderPassTargets {
	D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs; // 複数可
	UINT                         numRTVs;
	D3D12_CPU_DESCRIPTOR_HANDLE* pDSV; // nullptr でもOK
	Vec4                         clearColor;
	float                        clearDepth;
	uint8_t                      clearStencil;
	bool                         bClearColor; // クリアカラーするか?
	bool                         bClearDepth; // 深度クリアするか?
};
