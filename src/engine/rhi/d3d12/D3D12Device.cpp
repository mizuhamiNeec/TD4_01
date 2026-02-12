#include "D3D12Device.h"

#include <dxgidebug.h>

#include "D3D12SwapChain.h"
#include "D3D12Util.h"

#include "core/UnnamedMacro.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/DxcShaderCompiler.h"
#include "engine/unnamed/subsystem/console/Log.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace Unnamed::Rhi {
	static constexpr std::string_view kChannel = "D3D12";

	using namespace Microsoft::WRL;

	namespace {
		template <typename T, typename U>
		ComPtr<T> QueryInterface(const ComPtr<U>& source) {
			ComPtr<T>     result;
			const HRESULT hr = source.As(&result);
			UASSERT(SUCCEEDED(hr));
			return result;
		}
	}

	std::unique_ptr<IRhiDevice> CreateD3D12Device(
		void*                hwnd,
		const DeviceDesc&    deviceDesc,
		const SwapChainDesc& swapChainDesc
	) {
		// D3D12デバイスを作成して返す
		return std::make_unique<D3D12Device>(
			static_cast<HWND>(hwnd), deviceDesc, swapChainDesc
		);
	}

	D3D12Device::D3D12Device(
		HWND                 hwnd, const DeviceDesc& deviceDesc,
		const SwapChainDesc& swapChainDesc
	) : mHwnd(hwnd) {
		mFramesInFlight = swapChainDesc.bufferCount;
		mEnableVsync    = swapChainDesc.vSync;
		UASSERT(
			mFramesInFlight >= 2 && mFramesInFlight <= kMaxFramesInFlight &&
			"フレーム数が不正です"
		);

		DxcShaderCompiler compiler;
		if (!compiler.Initialize()) {
			Error(kChannel, "DxcShaderCompiler の初期化に失敗しました");
			UASSERT(false);
		}

		if (
			!compiler.CompileToFileDXIL(
				L"./content/core/shaders/src/CsWriteUav.hlsl",
				L"main",
				L"cs_6_6",
				{},
				{},
				L"./content/core/shaders/compiled/CsWriteUav.dxil"
			)
		) {
			Error(kChannel, "シェーダのコンパイルに失敗しました");
			UASSERT(false);
		}

		compiler.CompileToFileDXIL(
			L"./content/core/shaders/src/Fullscreen.hlsl",
			L"PsMain",
			L"ps_6_6",
			{},
			{},
			L"./content/core/shaders/compiled/FullscreenPS.dxil"
		);

		compiler.CompileToFileDXIL(
			L"./content/core/shaders/src/Fullscreen.hlsl",
			L"VsMain",
			L"vs_6_6",
			{},
			{},
			L"./content/core/shaders/compiled/FullscreenVS.dxil"
		);

		if (deviceDesc.enableDebugLayer) {
			EnableDebugLayer(deviceDesc.enableGpuBasedValidation);
		}

		CreateDevice();

		if (deviceDesc.enableDebugLayer) { EnableValidationLayer(); }

		CreateQueue();
		CreateSrvUavHeap();
		CreateOrResizeIntermediate(swapChainDesc.width, swapChainDesc.height);
		CreatePipelines();
		CreateCommandObjects();
		CreateFenceObjects();

		mSwapChain = std::make_unique<D3D12SwapChain>(
			mFactory,
			mDevice,
			mGraphicsQueue,
			hwnd,
			swapChainDesc
		);
	}

	D3D12Device::~D3D12Device() {
		// 待つ
		WaitForGpuIdle();

		// リソースの解放
		mSwapChain.reset();
		mCommandList.Reset();
		for (auto& frame : mFrames) { frame.commandAllocator.Reset(); }

		mSrvUavHeap.Reset();
		mIntermediate.Reset();

		mFsPipelineStateObject.Reset();
		mFsRootSignature.Reset();
		mCsPipelineStateObject.Reset();
		mCsRootSignature.Reset();

		mGraphicsQueue.Reset();
		mFence.Reset();
		mDevice.Reset();
		mFactory.Reset();

		if (mFenceEvent) {
			CloseHandle(mFenceEvent);
			mFenceEvent = nullptr;
		}

		// アダプタ変更イベントの解放
		if (mAdapterChangeEventCookie != 0) {
			if (mFactory7) {
				mFactory7->UnregisterAdaptersChangedEvent(
					mAdapterChangeEventCookie
				);
			}
			mAdapterChangeEventCookie = 0;
		}
		if (mAdapterChangeEvent) {
			CloseHandle(mAdapterChangeEvent);
			mAdapterChangeEvent = nullptr;
		}
		mFactory7.Reset();

#ifdef _DEBUG
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
#endif
	}

	void D3D12Device::BeginFrame() {
		const uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();

		WaitForFrame(frameIndex);

		Throw(mFrames[frameIndex].commandAllocator->Reset());
		Throw(
			mCommandList->Reset(
				mFrames[frameIndex].commandAllocator.Get(), nullptr
			)
		);
	}

	void D3D12Device::EndFrame() {
		const uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();

		// コマンドリストを閉じる
		Throw(mCommandList->Close());

		// コマンドリスト実行
		ID3D12CommandList* lists[] = {mCommandList.Get()};
		mGraphicsQueue->ExecuteCommandLists(1, lists);

		mSwapChain->Present(mEnableVsync);

		SignalFrame(frameIndex);
	}

	void D3D12Device::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) { return; }

		// 待っとく
		WaitForGpuIdle();

		mSwapChain->Resize(width, height);

		CreateOrResizeIntermediate(width, height);
	}

	BACKEND_TYPE D3D12Device::GetBackendType() const {
		return BACKEND_TYPE::D3D12;
	}

	IRhiSwapChain& D3D12Device::GetSwapChain() { return *mSwapChain; }

	ID3D12GraphicsCommandList* D3D12Device::GetCommandList() const {
		return mCommandList.Get();
	}

	D3D12SwapChain* D3D12Device::GetD3D12SwapChain() const {
		return mSwapChain.get();
	}

	void D3D12Device::EnableDebugLayer(const bool gpuValidation) {
		ComPtr<ID3D12Debug> debugController;
		if (
			SUCCEEDED(
				D3D12GetDebugInterface(
					IID_PPV_ARGS(debugController. GetAddressOf())
				)
			)
		) {
			debugController->EnableDebugLayer();

			if (gpuValidation) {
				ComPtr<ID3D12Debug1> debugController1;
				if (SUCCEEDED(debugController.As(&debugController1))) {
					debugController1->SetEnableGPUBasedValidation(TRUE);
				}
			}
		}
	}

	void D3D12Device::CreateDevice() {
		// --- ファクトリーの生成 ---
		UINT factoryFlags = 0;
#ifdef _DEBUG
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		Throw(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)));

		// --- アダプタの確認 ---
		constexpr std::array gpuPreferences = {
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			DXGI_GPU_PREFERENCE_MINIMUM_POWER,
			DXGI_GPU_PREFERENCE_UNSPECIFIED,
		};

		ComPtr<IDXGIAdapter4> adapter;
		if (const auto factory6 = QueryInterface<IDXGIFactory6>(mFactory)) {
			bool found = false;
			for (const auto preference : gpuPreferences) {
				UINT i = 0; // アダプタ列挙用インデックス
				while (
					factory6->EnumAdapterByGpuPreference(
						i, preference, IID_PPV_ARGS(&adapter)
					) != DXGI_ERROR_NOT_FOUND
				) {
					// アダプタの情報を取得
					DXGI_ADAPTER_DESC3 adapterDesc3;
					Throw(adapter->GetDesc3(&adapterDesc3));
					Msg(
						kChannel, "Found Adapter: {}",
						StrUtil::ToString(adapterDesc3.Description)
					);
					if (!(adapterDesc3.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	Use Adapter: {}",
							StrUtil::ToString(adapterDesc3.Description)
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Details: Vendor ID: 0x{:X}, Device ID: 0x{:X}",
							adapterDesc3.VendorId, adapterDesc3.DeviceId
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Memory: Dedicated Video: {:.2f} MB, Dedicated System: {:.2f} MB, Shared System: {:.2f} MB",
							static_cast<float>(
								adapterDesc3.DedicatedVideoMemory
							) / (1024.0f * 1024.0f),
							static_cast<float>(
								adapterDesc3. DedicatedSystemMemory
							) / (1024.0f * 1024.0f),
							static_cast<float>(
								adapterDesc3.SharedSystemMemory
							) / (1024.0f * 1024.0f)
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Revision: {}, SubSys ID: 0x{:X}",
							adapterDesc3.Revision,
							adapterDesc3.SubSysId
						);
						found = true;
						break;
					}
					adapter.Reset();
					++i;
				}
				if (found) { break; }
			}
			UASSERT(adapter != nullptr && "適切なアダプタが見つかりませんでした");
		} else {
			Fatal(kChannel, "それはちょっとねぇ、世間は許してくrえゃすぇんよ");
			UASSERT(false && "IDXGIFactory6の取得に失敗しました");
		}

		// --- アダプタ変更イベントの登録（できれば） ---
		if (const auto factory7 = QueryInterface<IDXGIFactory7>(mFactory)) {
			mAdapterChangeEvent = CreateEventW(
				nullptr, FALSE, FALSE, nullptr
			);
			UASSERT(mAdapterChangeEvent != nullptr && "アダプタ変更イベントの作成に失敗しました");

			Throw(
				factory7->RegisterAdaptersChangedEvent(
					mAdapterChangeEvent, &mAdapterChangeEventCookie
				)
			);

			DevMsg(kChannel, "アダプタ変更イベントが登録されました");
		}

		// 最新の機能レベルから順に対応しているか確認
		constexpr std::array featureLevels = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		// --- デバイスの生成 ---
		for (size_t i = 0; i < featureLevels.size(); ++i) {
			const HRESULT hr = D3D12CreateDevice(
				adapter.Get(),
				featureLevels[i],
				IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf())
			);

			if (SUCCEEDED(hr)) {
				constexpr std::array featureLevelStr = {
					"12.2", "12.1", "12.0", "11.1", "11.0",
				};
				Msg(
					kChannel,
					"Created D3D12 device with feature level {}",
					featureLevelStr[i]
				);
				break;
			}

			// 失敗したら次へ
			mDevice.Reset();
		}

		UASSERT(mDevice != nullptr && "D3D12デバイスの作成に失敗しました");
	}

	void D3D12Device::EnableValidationLayer() const {
		ComPtr<ID3D12InfoQueue> queue;
		if (SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&queue)))) {
			// やばいエラー時に止まる
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			// エラー時に止まる
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			// 警告時に止まる
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			std::array denyIds = {
				// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
				// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
				// これはオーバーレイ系を使うと出るので抑制しておく
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
			};

			// 抑制するレベル
			std::array              severities = {D3D12_MESSAGE_SEVERITY_INFO};
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = static_cast<UINT>(denyIds.size());
			filter.DenyList.pIDList = denyIds.data();
			filter.DenyList.NumSeverities =
				static_cast<UINT>(severities.size());
			filter.DenyList.pSeverityList = severities.data();
			// 指定したメッセージの表示を抑制
			Throw(queue->PushStorageFilter(&filter));
		}
	}

	void D3D12Device::CreateQueue() {
		// コマンドキューの生成
		D3D12_COMMAND_QUEUE_DESC desc;
		desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;

		Throw(
			mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mGraphicsQueue))
		);

		Throw(mGraphicsQueue->SetName(L"GraphicsQueue"));

		Msg(kChannel, "Created Graphics Command Queue for direct commands");
	}

	void D3D12Device::CreateSrvUavHeap() {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2; // SRVとUAV
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		Throw(
			mDevice->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mSrvUavHeap.ReleaseAndGetAddressOf())
			)
		);
		mSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		auto cpu = mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		auto gpu = mSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

		mIntermediateUavCPU = cpu;
		mIntermediateUavGPU = gpu;

		cpu.ptr += mSrvUavDescriptorSize;
		gpu.ptr += mSrvUavDescriptorSize;

		mIntermediateSrvCPU = cpu;
		mIntermediateSrvGPU = gpu;
	}

	void D3D12Device::CreatePipelines() {
		// コンピュート ルートシグネチャ UAV(u0)
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = 0; // u0
			range.RegisterSpace = 0;
			range.OffsetInDescriptorsFromTableStart =
				D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges = 1;
			param.DescriptorTable.pDescriptorRanges = &range;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters             = 1;
			desc.pParameters               = &param;
			desc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			ComPtr<ID3DBlob> blob, error;
			Throw(
				D3D12SerializeRootSignature(
					&desc,
					D3D_ROOT_SIGNATURE_VERSION_1,
					blob.ReleaseAndGetAddressOf(),
					error.ReleaseAndGetAddressOf()
				)
			);
			Throw(
				mDevice->CreateRootSignature(
					0,
					blob->GetBufferPointer(),
					blob->GetBufferSize(),
					IID_PPV_ARGS(mCsRootSignature.ReleaseAndGetAddressOf())
				)
			);
		}

		// フルスクリーン ルートシグネチャ SRV(t0) + static sampler(s0)
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = 0; // t0
			range.RegisterSpace = 0;
			range.OffsetInDescriptorsFromTableStart =
				D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges = 1;
			param.DescriptorTable.pDescriptorRanges = &range;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.ShaderRegister = 0; // s0
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters             = 1;
			desc.pParameters               = &param;
			desc.NumStaticSamplers         = 1;
			desc.pStaticSamplers           = &sampler;

			// フルスクリーン用なので IA を許可
			desc.Flags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> blob, error;
			Throw(
				D3D12SerializeRootSignature(
					&desc, D3D_ROOT_SIGNATURE_VERSION_1,
					blob.ReleaseAndGetAddressOf(),
					error.ReleaseAndGetAddressOf()
				)
			);
			Throw(
				mDevice->CreateRootSignature(
					0,
					blob->GetBufferPointer(),
					blob->GetBufferSize(),
					IID_PPV_ARGS(mFsRootSignature.ReleaseAndGetAddressOf())
				)
			);
		}

		// コンピュートシェーダ PSO
		{
			auto csBytes = LoadFileBytes(
				L"./content/core/shaders/compiled/CsWriteUav.dxil"
			);
			D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
			desc.pRootSignature                    = mCsRootSignature.Get();
			desc.CS.pShaderBytecode                = csBytes.data();
			desc.CS.BytecodeLength                 = csBytes.size();
			Throw(
				mDevice->CreateComputePipelineState(
					&desc, IID_PPV_ARGS(
						mCsPipelineStateObject.ReleaseAndGetAddressOf()
					)
				)
			);
		}

		{
			auto vsBytes = LoadFileBytes(
				L"./content/core/shaders/compiled/FullscreenVS.dxil"
			);
			auto psBytes = LoadFileBytes(
				L"./content/core/shaders/compiled/FullscreenPS.dxil"
			);

			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
			desc.pRootSignature = mFsRootSignature.Get();
			desc.VS = {vsBytes.data(), vsBytes.size()};
			desc.PS = {psBytes.data(), psBytes.size()};

			D3D12_BLEND_DESC blendDesc       = {};
			blendDesc.AlphaToCoverageEnable  = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;
			constexpr D3D12_RENDER_TARGET_BLEND_DESC
				defaultRenderTargetBlendDesc =
				{
					FALSE, FALSE,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_LOGIC_OP_NOOP,
					D3D12_COLOR_WRITE_ENABLE_ALL,
				};
			for (auto& rtBlendDesc : blendDesc.RenderTarget) {
				rtBlendDesc = defaultRenderTargetBlendDesc;
			}

			D3D12_RASTERIZER_DESC defaultRasterizerDesc = {};
			defaultRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
			defaultRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
			defaultRasterizerDesc.FrontCounterClockwise = FALSE;
			defaultRasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
			defaultRasterizerDesc.DepthBiasClamp =
				D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			defaultRasterizerDesc.SlopeScaledDepthBias =
				D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			defaultRasterizerDesc.DepthClipEnable       = TRUE;
			defaultRasterizerDesc.MultisampleEnable     = FALSE;
			defaultRasterizerDesc.AntialiasedLineEnable = FALSE;
			defaultRasterizerDesc.ForcedSampleCount     = 0;
			defaultRasterizerDesc.ConservativeRaster    =
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

			D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
			depthStencilDesc.DepthEnable = FALSE;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			depthStencilDesc.StencilEnable = FALSE;
			depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			depthStencilDesc.StencilWriteMask =
				D3D12_DEFAULT_STENCIL_WRITE_MASK;
			depthStencilDesc.FrontFace = {};
			depthStencilDesc.BackFace  = {};

			desc.BlendState                      = blendDesc;
			desc.RasterizerState                 = defaultRasterizerDesc;
			desc.DepthStencilState               = depthStencilDesc;
			desc.DepthStencilState.DepthEnable   = FALSE;
			desc.DepthStencilState.StencilEnable = FALSE;

			desc.SampleMask            = UINT_MAX;
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			desc.NumRenderTargets = 1;
			desc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;

			Throw(
				mDevice->CreateGraphicsPipelineState(
					&desc, IID_PPV_ARGS(
						mFsPipelineStateObject.ReleaseAndGetAddressOf()
					)
				)
			);
		}
	}

	void D3D12Device::CreateCommandObjects() {
		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			Throw(
				mDevice->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(
						mFrames[i].commandAllocator.ReleaseAndGetAddressOf()
					)
				)
			);
			mFrames[i].fenceValue = 0;
		}

		Throw(
			mDevice->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				mFrames[0].commandAllocator.Get(),
				nullptr,
				IID_PPV_ARGS(
					mCommandList.ReleaseAndGetAddressOf()
				)
			)
		);

		// 作成時は開いているので閉じとく
		Throw(mCommandList->Close());
	}

	void D3D12Device::CreateFenceObjects() {
		Throw(
			mDevice->CreateFence(
				0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(
					mFence.ReleaseAndGetAddressOf()
				)
			)
		);
		mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent) { throw std::runtime_error("Create Event failed."); }
		mNextFenceValue = 1;
	}

	void D3D12Device::WaitForFrame(const uint32_t frameIndex) const {
		const uint64_t fenceValue = mFrames[frameIndex].fenceValue;
		if (fenceValue == 0) { return; }

		if (mFence->GetCompletedValue() < fenceValue) {
			Throw(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}

	void D3D12Device::SignalFrame(const uint32_t frameIndex) {
		const uint64_t fenceValue      = mNextFenceValue++;
		mFrames[frameIndex].fenceValue = fenceValue;
		Throw(mGraphicsQueue->Signal(mFence.Get(), fenceValue));
	}

	void D3D12Device::WaitForGpuIdle() {
		// すべてのフレームが完了するまで待つ
		for (uint32_t i = 0; i < mFramesInFlight; ++i) { SignalFrame(i); }
		for (uint32_t i = 0; i < mFramesInFlight; ++i) { WaitForFrame(i); }
	}

	void D3D12Device::CreateOrResizeIntermediate(
		uint32_t width, uint32_t height
	) {
		mIntermediate.Reset();

		D3D12_RESOURCE_DESC tex = {};
		tex.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		tex.Alignment           = 0;
		tex.Width               = width;
		tex.Height              = height;
		tex.DepthOrArraySize    = 1;
		tex.MipLevels           = 1;
		tex.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex.SampleDesc.Count    = 1;
		tex.SampleDesc.Quality  = 0;
		tex.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		tex.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		Throw(
			mDevice->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&tex,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(mIntermediate.ReleaseAndGetAddressOf())
			)
		);

		mIntermediate->SetName(L"Intermediate");

		mIntermediateState = D3D12_RESOURCE_STATE_COMMON;

		// UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
		uav.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav.Format                           = DXGI_FORMAT_R8G8B8A8_UNORM;
		uav.Texture2D.MipSlice               = 0;
		uav.Texture2D.PlaneSlice             = 0;
		mDevice->CreateUnorderedAccessView(
			mIntermediate.Get(), nullptr, &uav, mIntermediateUavCPU
		);

		// SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = 1;
		srv.Texture2D.MostDetailedMip = 0;
		srv.Texture2D.ResourceMinLODClamp = 0.0f;
		mDevice->CreateShaderResourceView(
			mIntermediate.Get(), &srv, mIntermediateSrvCPU
		);
	}
}
