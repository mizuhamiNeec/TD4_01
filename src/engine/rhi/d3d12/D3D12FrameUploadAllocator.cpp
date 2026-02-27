#include "D3D12FrameUploadAllocator.h"

#include "core/UnnamedMacro.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Rhi {
	void D3D12FrameUploadAllocator::Initialize(
		ID3D12Device* device, uint32_t bytes
	) {
		mCapacity = Align256(bytes);

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width               = mCapacity;
		desc.Height              = 1;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.SampleDesc.Count    = 1;
		desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		if (
			FAILED(
				device->CreateCommittedResource(
					&heap,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(
						mResource.ReleaseAndGetAddressOf()
					)
				)
			)
		) {
			UASSERT(false && "作成に失敗");
			return;
		}

		const D3D12_RANGE range = {0, 0}; // CPUのみ書き込み
		if (FAILED(
			mResource->Map(0, &range, reinterpret_cast<void**>(&mMapped))
		)) {
			UASSERT(false && "マップに失敗");
			return;
		}
		mOffset = 0;
	}

	void D3D12FrameUploadAllocator::Shutdown() {
		if (mResource && mMapped) {
			mResource->Unmap(0, nullptr);
			mMapped = nullptr;
		}

		mResource.Reset();
		mCapacity = 0;
		mOffset   = 0;
	}

	void D3D12FrameUploadAllocator::BeginFrame() { mOffset = 0; }

	D3D12_GPU_VIRTUAL_ADDRESS D3D12FrameUploadAllocator::AllocateConstantBuffer(
		const void* srcData, uint32_t bytes
	) {
		// 256バイト境界にアラインメントする
		const uint32_t aligned = Align256(bytes);

		// アロケータの容量を超える場合はエラー
		if (mOffset + aligned > mCapacity) {
			Fatal(
				"FUA", "フレームアップロードアロケータの容量を超えました。必要な容量: {} bytes",
				mOffset + aligned
			);
		}

		// データをコピー
		std::memcpy(mMapped + mOffset, srcData, bytes);

		// GPU仮想アドレスを取得
		D3D12_GPU_VIRTUAL_ADDRESS gpuVa = mResource->GetGPUVirtualAddress();

		// オフセットを加算
		gpuVa += mOffset;

		// オフセットを更新
		mOffset += aligned;

		return gpuVa;
	}

	uint32_t D3D12FrameUploadAllocator::Align256(const uint32_t v) {
		return v + 255u & ~255u;
	}
}
