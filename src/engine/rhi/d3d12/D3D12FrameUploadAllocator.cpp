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

		constexpr D3D12_RANGE range = {0, 0}; // CPUのみ書き込み
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

	void D3D12FrameUploadAllocator::BeginFrame() {
		mOffset = 0;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12FrameUploadAllocator::AllocateConstantBuffer(
		const void* srcData, uint32_t bytes
	) {
		return AllocateBuffer(srcData, bytes, 256u);
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12FrameUploadAllocator::AllocateBuffer(
		const void* srcData, const uint32_t bytes, const uint32_t alignment
	) {
		if (!mResource || !mMapped || srcData == nullptr || bytes == 0) {
			Error(
				"FUA",
				"無効なアロケータ状態でAllocateConstantBufferが呼ばれました。resource={}, mapped={}, srcData={}, bytes={}",
				static_cast<const void*>(mResource.Get()),
				static_cast<const void*>(mMapped),
				srcData,
				bytes
			);
			return 0;
		}

		const uint32_t safeAlignment = alignment == 0 ? 1u : alignment;
		const uint32_t alignedOffset = AlignTo(mOffset, safeAlignment);
		const uint32_t endOffset     = alignedOffset + bytes;

		// アロケータの容量を超える場合はエラー
		if (endOffset > mCapacity) {
			Error(
				"FUA", "フレームアップロードアロケータの容量を超えました。必要な容量: {} bytes",
				endOffset
			);
			return 0;
		}

		// データをコピー
		std::memcpy(mMapped + alignedOffset, srcData, bytes);

		// GPU仮想アドレスを取得
		D3D12_GPU_VIRTUAL_ADDRESS gpuVa = mResource->GetGPUVirtualAddress();

		// オフセットを加算
		gpuVa += alignedOffset;

		// オフセットを更新
		mOffset = endOffset;

		return gpuVa;
	}

	uint32_t D3D12FrameUploadAllocator::Align256(const uint32_t v) {
		return AlignTo(v, 256u);
	}

	uint32_t D3D12FrameUploadAllocator::AlignTo(
		const uint32_t v, const uint32_t alignment
	) {
		const uint32_t safeAlignment = alignment == 0 ? 1u : alignment;
		return (v + safeAlignment - 1u) & ~(safeAlignment - 1u);
	}
}
