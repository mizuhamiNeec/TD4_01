#include "RgResourceRegistry.h"

#include "d3dx12.h"

#include "core/UnnamedMacro.h"
#include "core/assets/types/TextureAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12SwapChain.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/Log.h"

using Microsoft::WRL::ComPtr;

namespace Unnamed::Render {
	static constexpr std::string_view kChannel = "RDG";

	RgResourceRegistry::RgResourceRegistry(Rhi::D3D12Device& dx) : mDx(dx) {
		mEntries.resize(1);

		mSrvUavHeapCapacity = dx.GetSrvUavHeapCapacity();
		mRtvCapacity        = dx.GetRtvHeapCapacity();
		mDsvCapacity        = dx.GetDsvHeapCapacity();

		SetFramesInFlight(2);
		RebuildDescriptorLayout();

		if (const auto* sc = dx.GetD3D12SwapChain()) {
			mBackBufferWidth  = sc->GetWidth();
			mBackBufferHeight = sc->GetHeight();
		}

		auto* device     = mDx.GetDevice();
		mCpuHeapCapacity = 8192; // とりあえず適当な値

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = mCpuHeapCapacity;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		Rhi::Throw(
			device->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mCpuSrvUavHeap.ReleaseAndGetAddressOf())
			)
		);

		mCpuSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}

	void RgResourceRegistry::SetFramesInFlight(const uint32_t framesInFlight) {
		// 最佁Eにする
		mFramesInFlight = std::max(2u, framesInFlight);
		mSrvUavFrameBase.resize(mFramesInFlight);
		mRtvFrameBase.resize(mFramesInFlight);
		mDsvFrameBase.resize(mFramesInFlight);
		RebuildDescriptorLayout();
	}

	void RgResourceRegistry::ResetFrame(const uint32_t frameIndex) {
		mCurrentFrameIndex = frameIndex % mFramesInFlight;
		mNextDsvLocal      = 0;
	}

	uint32_t RgResourceRegistry::CreateTexture(const RgTextureDesc& desc) {
		const uint32_t id = AllocateId();

		if (mEntries.size() <= id) { mEntries.resize(id + 1); }

		auto resolved = desc;
		if (resolved.extentMode == RG_EXTENT_MODE::MATCH_BACK_BUFFER) {
			resolved.width  = mBackBufferWidth;
			resolved.height = mBackBufferHeight;
			Msg(
				kChannel, "テクスチャ {} はバックバッファに合わせてリサイズされます。 {}x{}",
				desc.debugName, resolved.width, resolved.height
			);
		}

		auto& e = mEntries[id];
		e.desc  = resolved;

		CreateD3D12Texture(e);
		CreateDescriptors(e);

		return id;
	}

	uint32_t RgResourceRegistry::CreateTexture2DFromAsset(
		const TextureAssetData& texture, const std::string& debugName
	) {
		if (texture.width == 0 || texture.height == 0 || texture.mips.empty()) {
			return 0;
		}

		const uint32_t id = AllocateId();
		if (mEntries.size() <= id) { mEntries.resize(id + 1); }

		auto& e = mEntries[id];
		e.desc  = RgTextureDesc{
			.width          = texture.width,
			.height         = texture.height,
			.resourceFormat = texture.isSRGB ?
				                  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
				                  DXGI_FORMAT_R8G8B8A8_UNORM,
			.allowUav   = false,
			.allowRtv   = false,
			.allowDsv   = false,
			.debugName  = debugName,
			.extentMode = RG_EXTENT_MODE::FIXED,
		};

		auto* device = mDx.GetDevice();

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width               = e.desc.width;
		texDesc.Height              = e.desc.height;
		texDesc.DepthOrArraySize    = 1;
		texDesc.MipLevels           = 1;
		texDesc.Format              = e.desc.resourceFormat;
		texDesc.SampleDesc.Count    = 1;
		texDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		Rhi::Throw(
			device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(e.resource.ReleaseAndGetAddressOf())
			)
		);

		if (!debugName.empty()) {
			e.resource->SetName(StrUtil::ToWString(debugName).c_str());
		}

		const TextureMip& mip0 = texture.mips[0];
		if (!mip0.bytes.empty()) {
			auto& up = mDx.GetUploadContext();
			up.Begin();
			auto* cmd = up.GetCommandList();

			D3D12_RESOURCE_DESC rd = e.resource->GetDesc();

			uint64_t                           requiredBytes  = 0;
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint      = {};
			UINT                               numRows        = 0;
			uint64_t                           rowSizeInBytes = 0;
			device->GetCopyableFootprints(
				&rd,
				0,
				1,
				0,
				&footprint,
				&numRows,
				&rowSizeInBytes,
				&requiredBytes
			);

			ComPtr<ID3D12Resource> upload;
			D3D12_HEAP_PROPERTIES  upHeap = {};
			upHeap.Type                   = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC upDesc    = {};
			upDesc.Dimension              = D3D12_RESOURCE_DIMENSION_BUFFER;
			upDesc.Width                  = requiredBytes;
			upDesc.Height                 = 1;
			upDesc.DepthOrArraySize       = 1;
			upDesc.MipLevels              = 1;
			upDesc.SampleDesc.Count       = 1;
			upDesc.Layout                 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			upDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;

			Rhi::Throw(
				device->CreateCommittedResource(
					&upHeap,
					D3D12_HEAP_FLAG_NONE,
					&upDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(upload.ReleaseAndGetAddressOf())
				)
			);

			void*       map       = nullptr;
			D3D12_RANGE readRange = {0, 0};
			Rhi::Throw(upload->Map(0, &readRange, &map));
			for (UINT y = 0; y < numRows; ++y) {
				const uint8_t* src = mip0.bytes.data() +
				                     static_cast<size_t>(y) * mip0.rowPitch;
				uint8_t* dst = static_cast<uint8_t*>(map) + footprint.Offset +
				               static_cast<size_t>(y) *
				               footprint.Footprint.RowPitch;
				memcpy(dst, src, rowSizeInBytes);
			}
			upload->Unmap(0, nullptr);

			auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
				e.resource.Get(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_COPY_DEST
			);
			cmd->ResourceBarrier(1, &toCopy);

			D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
			srcLoc.pResource = upload.Get();
			srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLoc.PlacedFootprint = footprint;

			D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
			dstLoc.pResource = e.resource.Get();
			dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLoc.SubresourceIndex = 0;

			cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

			auto toCommon = CD3DX12_RESOURCE_BARRIER::Transition(
				e.resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_COMMON
			);
			cmd->ResourceBarrier(1, &toCommon);

			up.EndAndSubmitAndWait();
		}

		CreateDescriptors(e);
		return id;
	}

	ID3D12Resource* RgResourceRegistry::GetResource(uint32_t textureId) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return nullptr; }
		return mEntries[textureId].resource.Get();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrv(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return {}; }
		const auto& e = mEntries[textureId];
		if (e.srvLocal == UINT32_MAX) { return {}; }
		return GetSrvUavGpuAt(mCurrentFrameIndex, e.srvLocal);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return {}; }
		const auto& e = mEntries[textureId];
		if (e.srvCpu.ptr == 0) { return {}; }
		return e.srvCpu;
	}

	uint64_t RgResourceRegistry::GetSrvRevision(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return 0; }
		return mEntries[textureId].srvRevision;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetUav(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return {}; }
		const auto& e = mEntries[textureId];
		if (e.uavLocal == UINT32_MAX) { return {}; }
		return GetSrvUavGpuAt(mCurrentFrameIndex, e.uavLocal);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetRtvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return {}; }
		const auto& e = mEntries[textureId];
		if (e.rtvLocal == UINT32_MAX) { return {}; }

		auto*      rtvHeap = mDx.GetRtvHeap();
		const auto cpuBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		const UINT inc     = mDx.GetRtvDescriptorSize();

		const uint32_t slot = mRtvFrameBase[mCurrentFrameIndex] + e.rtvLocal;

		D3D12_CPU_DESCRIPTOR_HANDLE h = cpuBase;
		h.ptr                         += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetDsvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) { return {}; }
		return mEntries[textureId].dsvCpu;
	}

	void RgResourceRegistry::OnResize(
		const uint32_t width, const uint32_t height, const uint32_t frameIndex
	) {
		mBackBufferWidth  = width;
		mBackBufferHeight = height;

		// フレームリセット
		ResetFrame(frameIndex);

		for (auto& e : mEntries) {
			if (!e.resource) { continue; }

			if (e.desc.extentMode != RG_EXTENT_MODE::MATCH_BACK_BUFFER) {
				continue;
			}

			if (e.desc.width == width && e.desc.height == height) { continue; }

			e.desc.width  = width;
			e.desc.height = height;

			CreateD3D12Texture(e);
			CreateDescriptors(e);
		}
	}

	void RgResourceRegistry::CollectGarbage(
		const uint64_t completedFenceValue
	) {
		std::erase_if(
			mRetiredResources,
			[completedFenceValue](const RetiredTextureResource& retired) {
				return retired.retireFenceValue == 0 ||
				       retired.retireFenceValue <= completedFenceValue;
			}
		);
	}

	uint32_t RgResourceRegistry::AllocSrvUavSlot() {
		const uint32_t local = mNextSrvUavLocalGlobal++;
		if (local >= mSrvUavPerFrameSlots) {
			Fatal(
				kChannel, "SRV/UAV heap slots exhausted: local={}, perFrame={}",
				local, mSrvUavPerFrameSlots
			);
			UASSERT(false);
		}
		return local;
	}

	uint32_t RgResourceRegistry::AllocRtvSlot() {
		const uint32_t local = mNextRtvLocalGlobal++;
		if (local >= mRtvPerFrameSlots) {
			Fatal(
				kChannel, "RTV heap slots exhausted: local={}, perFrame={}",
				local, mRtvPerFrameSlots
			);
			UASSERT(false);
		}
		return local;
	}

	uint32_t RgResourceRegistry::AllocCpuSlot() {
		const uint32_t slot = mCpuNextSlot++;
		if (slot >= mCpuHeapCapacity) {
			Fatal(
				kChannel,
				"CPU SRV/UAV heap slots exhausted: slot={}, capacity={}",
				slot, mCpuHeapCapacity
			);
			UASSERT(false && "CPU SRV/UAV heap slots exhausted.");
		}
		return slot;
	}

	uint32_t RgResourceRegistry::AllocDsvSlot() {
		const uint32_t base = mDsvFrameBase[mCurrentFrameIndex];
		const uint32_t slot = base + mNextDsvLocal++;

		const uint32_t end = base + mDsvPerFrameSlots;
		if (slot >= end) {
			Fatal(
				kChannel,
				"DSV heap slots exhausted: slot={}, perFrame={}, frameIndex={}",
				slot, mDsvPerFrameSlots, mCurrentFrameIndex
			);
		}
		return slot;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvUavCpuAt(
		const uint32_t frameIndex, const uint32_t local
	) const {
		auto*      heap = mDx.GetSrvUavHeap();
		const auto base = heap->GetCPUDescriptorHandleForHeapStart();
		const UINT inc  = mDx.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		const uint32_t              slot = mSrvUavFrameBase[frameIndex] + local;
		D3D12_CPU_DESCRIPTOR_HANDLE h    = base;
		h.ptr                            += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetCpuSrvUavAt(
		const uint32_t local
	) const {
		const auto base = mCpuSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CPU_DESCRIPTOR_HANDLE h = base;
		h.ptr += static_cast<SIZE_T>(local) * mCpuSrvUavDescriptorSize;
		return h;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvUavGpuAt(
		uint32_t frameIndex, uint32_t local
	) const {
		auto*      heap = mDx.GetSrvUavHeap();
		const auto base = heap->GetGPUDescriptorHandleForHeapStart();
		const UINT inc  = mDx.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		const uint32_t              slot = mSrvUavFrameBase[frameIndex] + local;
		D3D12_GPU_DESCRIPTOR_HANDLE h    = base;
		h.ptr                            += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	void RgResourceRegistry::RebuildDescriptorLayout() {
		const uint32_t srvUavCap = mDx.GetSrvUavHeapCapacity();
		const uint32_t rtvCap    = mDx.GetRtvHeapCapacity();
		const uint32_t dsvCap    = mDx.GetDsvHeapCapacity();

		// Reserve at least one slot per frame.
		mSrvUavPerFrameSlots = std::max(1u, srvUavCap / mFramesInFlight);
		mRtvPerFrameSlots    = std::max(1u, rtvCap / mFramesInFlight);
		mDsvPerFrameSlots    = std::max(1u, dsvCap / mFramesInFlight);

		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			mSrvUavFrameBase[i] = i * mSrvUavPerFrameSlots;
			mRtvFrameBase[i]    = i * mRtvPerFrameSlots;
			mDsvFrameBase[i]    = i * mDsvPerFrameSlots;
		}

		DevMsg(
			kChannel,
			"DescriptorLayout: frames={}, srvUavCap={}, perFrame={}, rtvCap={}, perFrame={}",
			mFramesInFlight, srvUavCap, mSrvUavPerFrameSlots, rtvCap,
			mRtvPerFrameSlots
		);
	}

	uint32_t RgResourceRegistry::AllocateId() { return mNextId++; }

	uint64_t RgResourceRegistry::GetSafeRetireFenceValue() const {
		uint64_t       retireFenceValue = 0;
		const uint32_t framesInFlight   = mDx.GetFramesInFlight();
		for (uint32_t frameIndex = 0; frameIndex < framesInFlight; ++
		     frameIndex) {
			retireFenceValue = std::max(
				retireFenceValue,
				mDx.GetLastSubmittedFenceValue(frameIndex)
			);
		}
		return retireFenceValue;
	}

	void RgResourceRegistry::CreateD3D12Texture(TexEntry& e) const {
		auto* device = mDx.GetDevice();

		const DXGI_FORMAT resourceFormat = e.desc.resourceFormat;

		// ---- validation ----
		if (e.desc.allowDsv) {
			if (!e.desc.dsvFormat.has_value()) {
				Fatal(
					kChannel,
					"Depth texture {} requires dsvFormat.",
					e.desc.debugName
				);
				UASSERT(false);
			}

			if (
				resourceFormat != DXGI_FORMAT_R32_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R32G8X24_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R24G8_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R16_TYPELESS
			) {
				Warning(
					kChannel,
					"Depth texture {} is not typeless. SRV reads may fail.",
					e.desc.debugName
				);
			}
		}

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width               = e.desc.width;
		desc.Height              = e.desc.height;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.Format              = resourceFormat;
		desc.SampleDesc.Count    = 1;
		desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// UAV、RTV、DSVの使用フラグを設定
		if (e.desc.allowUav) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		if (e.desc.allowRtv) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if (e.desc.allowDsv) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		// 最適化されたクリアカラー/深度がある場合は、リソース作成時にクリア値を指定する。
		const D3D12_CLEAR_VALUE* clearPtr = nullptr;
		D3D12_CLEAR_VALUE        clear    = {};

		if (e.desc.allowRtv && e.desc.optimizedClearColor.has_value()) {
			clear.Format = e.desc.rtvFormat.value_or(e.desc.resourceFormat);
			for (int i = 0; i < 4; ++i) {
				clear.Color[i] = (*e.desc.optimizedClearColor)[i];
			}
			clearPtr = &clear;
		}

		if (e.desc.allowDsv) {
			clear.Format             = e.desc.dsvFormat.value();
			clear.DepthStencil.Depth = e.desc.optimizedClearDepth.
			                             value_or(1.0f);
			clear.DepthStencil.Stencil = e.desc.optimizedClearStencil.value_or(
				0
			);
			clearPtr = &clear;
		}

		auto* self = const_cast<RgResourceRegistry*>(this);
		if (e.resource) {
			RetiredTextureResource retired = {};
			retired.retireFenceValue       = std::max(
				self->GetSafeRetireFenceValue(), mDx.GetNextSignalFenceValue()
			);
			retired.resource = e.resource;
			self->mRetiredResources.emplace_back(std::move(retired));
			e.resource.Reset();
		}

		// Create resource
		Rhi::Throw(
			device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COMMON,
				clearPtr,
				IID_PPV_ARGS(e.resource.ReleaseAndGetAddressOf())
			)
		);

		// Set debug name
		if (!e.desc.debugName.empty()) {
			e.resource->SetName(
				StrUtil::ToWString(e.desc.debugName).c_str()
			);
		}

		++e.resourceRevision;
	}

	void RgResourceRegistry::CreateDescriptors(TexEntry& e) {
		auto* device = mDx.GetDevice();

		if (e.srvLocal == UINT32_MAX) { e.srvLocal = AllocSrvUavSlot(); }
		if (e.srvCpuLocal == UINT32_MAX) { e.srvCpuLocal = AllocCpuSlot(); }
		if (e.desc.allowUav && e.uavLocal == UINT32_MAX) {
			e.uavLocal = AllocSrvUavSlot();
		}
		if (e.desc.allowRtv && e.rtvLocal == UINT32_MAX) {
			e.rtvLocal = AllocRtvSlot();
		}
		if (e.desc.allowDsv && e.dsvLocal == UINT32_MAX) {
			e.dsvLocal = AllocDsvSlot();
		}

		const bool isDepth = e.desc.allowDsv;

		DXGI_FORMAT srvFormat = e.desc.srvFormat.value_or(
			e.desc.resourceFormat
		);
		if (isDepth) {
			if (!e.desc.srvFormat.has_value()) {
				Fatal(
					kChannel,
					"Depth texture {} requires srvFormat.",
					e.desc.debugName
				);
				UASSERT(false);
			}
			srvFormat = e.desc.srvFormat.value();
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format                          = srvFormat;
		srv.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MipLevels             = 1;
		srv.Shader4ComponentMapping         =
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
		uav.Format                           = e.desc.resourceFormat;
		uav.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;

		e.srvCpu = GetCpuSrvUavAt(e.srvCpuLocal);
		device->CreateShaderResourceView(
			e.resource.Get(), &srv, e.srvCpu
		);

		for (uint32_t fi = 0; fi < mFramesInFlight; ++fi) {
			const auto cpuSrv = GetSrvUavCpuAt(fi, e.srvLocal);
			device->CreateShaderResourceView(
				e.resource.Get(), &srv, cpuSrv
			);

			if (e.desc.allowUav) {
				const auto cpuUav = GetSrvUavCpuAt(fi, e.uavLocal);
				device->CreateUnorderedAccessView(
					e.resource.Get(), nullptr, &uav, cpuUav
				);
			}
		}

		if (e.desc.allowRtv) {
			auto*      rtvHeap = mDx.GetRtvHeap();
			const auto cpuBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			const UINT inc     = mDx.GetRtvDescriptorSize();

			for (uint32_t fi = 0; fi < mFramesInFlight; ++fi) {
				const uint32_t slot = mRtvFrameBase[fi] + e.rtvLocal;
				D3D12_CPU_DESCRIPTOR_HANDLE cpuRtv = cpuBase;
				cpuRtv.ptr += static_cast<SIZE_T>(slot) * inc;
				device->CreateRenderTargetView(
					e.resource.Get(), nullptr, cpuRtv
				);

				if (fi == mCurrentFrameIndex) {
					e.rtvCpu = cpuRtv; // ビューポート用に保存
				}
			}
		}

		if (e.desc.allowDsv) {
			if (e.dsvLocal == UINT32_MAX) { e.dsvLocal = AllocDsvSlot(); }

			auto*      dsvHeap = mDx.GetDsvHeap();
			auto       cpuBase = dsvHeap->GetCPUDescriptorHandleForHeapStart();
			const UINT inc     = mDx.GetDsvDescriptorSize();

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDsv = cpuBase;
			cpuDsv.ptr += static_cast<SIZE_T>(e.dsvLocal) * inc;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
			dsv.Format                        = e.desc.dsvFormat.value();
			dsv.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Flags                         = D3D12_DSV_FLAG_NONE;

			device->CreateDepthStencilView(e.resource.Get(), &dsv, cpuDsv);

			e.dsvCpu = cpuDsv;
		}

		++e.srvRevision;
	}
}
