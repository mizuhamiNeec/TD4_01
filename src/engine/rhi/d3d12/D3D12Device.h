#pragma once
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <wrl/client.h>

#include "engine/Properties.h"
#include "engine/rhi/RhiTypes.h"
#include "engine/rhi/interface/IRhiDevice.h"

namespace Unnamed::Rhi {
	static constexpr uint32_t kMaxFramesInFlight = 3; // 最大同時フレーム数

	class D3D12Device final : public IRhiDevice {
	public:
		D3D12Device(
			HWND                 hwnd, const DeviceDesc& deviceDesc,
			const SwapChainDesc& swapChainDesc
		);
		~D3D12Device() override;

		void BeginFrame() override;
		void EndFrame() override;
		void OnResize(uint32_t width, uint32_t height) override;

		[[nodiscard]] BACKEND_TYPE GetBackendType() const override;
		IRhiSwapChain&             GetSwapChain() override;

		[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const;
		[[nodiscard]] class D3D12SwapChain*      GetD3D12SwapChain() const;

		ID3D12DescriptorHeap* GetSrvUavHeap() const {
			return mSrvUavHeap.Get();
		}


		ID3D12Resource* GetIntermediate() const { return mIntermediate.Get(); }

		D3D12_GPU_DESCRIPTOR_HANDLE GetIntermediateUavGPU() const {
			return mIntermediateUavGPU;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetIntermediateSrvGPU() const {
			return mIntermediateSrvGPU;
		}

		D3D12_RESOURCE_STATES GetIntermediateState() const {
			return mIntermediateState;
		}

		void SetIntermediateState(const D3D12_RESOURCE_STATES state) {
			mIntermediateState = state;
		}

		ID3D12RootSignature* GetCsRootSignature() const {
			return mCsRootSignature.Get();
		}

		ID3D12PipelineState* GetCsPipelineStateObject() const {
			return mCsPipelineStateObject.Get();
		}

		ID3D12RootSignature* GetFsRootSignature() const {
			return mFsRootSignature.Get();
		}

		ID3D12PipelineState* GetFsPipelineStateObject() const {
			return mFsPipelineStateObject.Get();
		}

	private:
		static void EnableDebugLayer(bool gpuValidation);
		void        CreateDevice();
		void        EnableValidationLayer() const;
		void        CreateQueue();
		void        CreateSrvUavHeap();
		void        CreatePipelines();
		void        CreateCommandObjects();
		void        CreateFenceObjects();
		void        WaitForFrame(uint32_t frameIndex) const;
		void        SignalFrame(uint32_t frameIndex);
		void        WaitForGpuIdle();

		void CreateOrResizeIntermediate(uint32_t width, uint32_t height);

		// D3D12
		Microsoft::WRL::ComPtr<IDXGIFactory6>      mFactory;
		Microsoft::WRL::ComPtr<ID3D12Device>       mDevice;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mGraphicsQueue;

		// スワップチェーン
		std::unique_ptr<D3D12SwapChain> mSwapChain;

		// コマンドリスト
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

		// フレームコンテキスト
		struct FrameContext {
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
			uint64_t                                       fenceValue = 0;
		};

		std::array<FrameContext, kMaxFramesInFlight> mFrames         = {};
		uint32_t                                     mFramesInFlight = 2;

		// フェンス
		uint64_t                            mNextFenceValue = 1;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		HANDLE                              mFenceEvent = nullptr;

		HWND mHwnd = nullptr;

		// アダプタ変更イベント
		HANDLE                                mAdapterChangeEvent;
		Microsoft::WRL::ComPtr<IDXGIFactory7> mFactory7;
		DWORD                                 mAdapterChangeEventCookie;

		bool mEnableVsync = false;


		// CBV/SRV/UAV のヒープ
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvUavHeap;
		uint32_t                                     mSrvUavDescriptorSize = 0;

		// 中間テクスチャ UAV --> SRV 用
		Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediate;
		D3D12_RESOURCE_STATES mIntermediateState = D3D12_RESOURCE_STATE_COMMON;

		D3D12_CPU_DESCRIPTOR_HANDLE mIntermediateUavCPU = {};
		D3D12_CPU_DESCRIPTOR_HANDLE mIntermediateSrvCPU = {};
		D3D12_GPU_DESCRIPTOR_HANDLE mIntermediateUavGPU = {};
		D3D12_GPU_DESCRIPTOR_HANDLE mIntermediateSrvGPU = {};

		// パイプライン
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mCsRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mCsPipelineStateObject;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mFsRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mFsPipelineStateObject;
	};
}
