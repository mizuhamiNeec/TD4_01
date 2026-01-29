#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

#include <wrl/client.h>

namespace Unnamed {
	/// @brief ディスクリプタアロケータクラス
	class DescriptorAllocator {
	public:
		~DescriptorAllocator() = default;

		/// @brief 初期化
		/// @param device デバイス
		/// @param type デスクリプタヒープタイプ
		/// @param numDescriptors デスクリプタ数
		/// @param shaderVisible シェーダー可視フラグ
		void Init(
			ID3D12Device*              device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			uint32_t                   numDescriptors,
			bool                       shaderVisible = false
		);

		/// @brief デスクリプタを割り当てる
		/// @return 割り当てられたデスクリプタのインデックス
		uint32_t Allocate();

		/// @brief デスクリプタを解放する
		/// @param index 解放するデスクリプタのインデックス
		void Free(uint32_t index);

		/// @brief CPUデスクリプタハンドルを取得する
		/// @param index デスクリプタのインデックス
		/// @return CPUデスクリプタハンドル
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(
			uint32_t index
		) const;

		/// @brief GPUデスクリプタハンドルを取得する
		/// @param index デスクリプタのインデックス
		/// @return GPUデスクリプタハンドル
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(
			uint32_t index
		) const;

		/// @brief デスクリプタヒープを取得する
		/// @return デスクリプタヒープ
		[[nodiscard]] ID3D12DescriptorHeap* GetHeap() const;

		/// @brief デスクリプタアロケータの容量を取得する
		/// @return デスクリプタアロケータの容量
		[[nodiscard]] uint32_t Capacity() const;

		/// @brief 空きデスクリプタ数を取得する
		/// @return 空きデスクリプタ数
		[[nodiscard]] uint32_t NumFree() const;

		/// @brief シェーダー可視かどうかを取得する
		/// @return シェーダー可視ならtrueを返す
		[[nodiscard]] bool IsShaderVisible() const;

		/// @brief デスクリプタヒープタイプを取得する
		/// @return デスクリプタヒープタイプ
		[[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE Type() const;

		/// @brief アロケータをリセットする
		void Reset();

		/// @brief CPUデスクリプタハンドルからインデックスを取得する
		/// @param cpuHandle CPUデスクリプタハンドル
		/// @return インデックス
		uint32_t GetIndexFromCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
		ID3D12Device*                                mDevice = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE                   mType   =
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		bool mShaderVisible = false;

		uint32_t mCapacity = 0;
		uint32_t mDescSize = 0;

		std::vector<uint32_t> mFreeList;
	};
}
