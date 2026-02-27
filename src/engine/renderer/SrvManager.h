#pragma once
#include <cstdint>
#include <d3d12.h>
#include <queue>
#include <vector>
#include <engine/Properties.h>
#include <wrl/client.h>

class D3D12;

/// @brief SRVマネージャークラス
class SrvManager {
public:
	void Init(D3D12* d3d12);
	void PreDraw() const;

	uint32_t Allocate();                    // 従来のAllocate（互換性のため残す）
	uint32_t AllocateForTexture2D();        // 2Dテクスチャ用SRVの割り当て
	uint32_t AllocateForTextureCube();      // キューブマップテクスチャ用SRVの割り当て
	uint32_t AllocateForTextureArray();     // テクスチャ配列用SRVの割り当て（将来用）
	uint32_t AllocateForStructuredBuffer(); // ストラクチャードバッファ用SRVの割り当て

	// 連続したテクスチャスロットを確保
	uint32_t AllocateConsecutiveTexture2DSlots(uint32_t count);
	uint32_t AllocateConsecutiveTextureCubeSlots(uint32_t count);

	// インデックス返却（再利用可能にする）
	void DeallocateTexture2D(uint32_t index);
	void DeallocateTextureCube(uint32_t index);
	void DeallocateTextureArray(uint32_t index);
	void DeallocateStructuredBuffer(uint32_t index);

	ID3D12DescriptorHeap* GetDescriptorHeap() const;
	void                  CreateSRVForTexture2D(
		uint32_t    srvIndex, ID3D12Resource* pResource,
		DXGI_FORMAT format, UINT              mipLevels
	) const;
	void CreateSRVForStructuredBuffer(
		uint32_t        srvIndex,
		ID3D12Resource* pResource,
		UINT            numElements,
		UINT            structureByteStride
	) const;
	void CreateSRVForTextureCube(
		uint32_t index,
		Microsoft::WRL::ComPtr<ID3D12Resource>
		resource,
		DXGI_FORMAT format, UINT mipLevels
	);
	void SetGraphicsRootDescriptorTable(
		UINT     rootParameterIndex,
		uint32_t srvIndex
	) const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		std::uint32_t index
	) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

	// 連続したスロットの先頭インデックスからGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetConsecutiveGPUHandle(
		uint32_t startIndex
	) const;

	uint32_t GetIndexFromCPUHandle(
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle
	) const;
	uint32_t GetIndexFromGPUHandle(
		const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle
	) const;


	bool CanAllocate() const;

private:
	D3D12* mD3d12 = nullptr;

	// 次に使用するSRVインデックス（従来の共通使用）
	uint32_t mUseIndex = kSrvIndexTop;

	// 2Dテクスチャ用SRVの次のインデックス
	uint32_t mTexture2DIndex = kTexture2DStartIndex;

	// キューブマップテクスチャ用SRVの次のインデックス
	uint32_t mTextureCubeIndex = kTextureCubeStartIndex;

	// テクスチャ配列用SRVの次のインデックス（将来用）
	uint32_t mTextureArrayIndex = kTextureArrayStartIndex;

	// ストラクチャードバッファ用SRVの次のインデックス
	uint32_t mStructuredBufferIndex = kStructuredBufferStartIndex;

	// 互換性のため残す
	uint32_t mTextureIndex = kTexture2DStartIndex;

	// SRV用のディスクリプタサイズ
	uint32_t mDescriptorSize = 0;
	// SRV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;

	// フリーリスト（返却されたインデックスの再利用用）
	std::queue<uint32_t> mFreeTexture2DIndices;
	std::queue<uint32_t> mFreeTextureCubeIndices;
	std::queue<uint32_t> mFreeTextureArrayIndices;
	std::queue<uint32_t> mFreeStructuredBufferIndices;

	// 使用中のインデックス管理（デバッグ用）
	std::vector<bool> mUsedTexture2DIndices;
	std::vector<bool> mUsedTextureCubeIndices;
	std::vector<bool> mUsedTextureArrayIndices;
	std::vector<bool> mUsedStructuredBufferIndices;
};
