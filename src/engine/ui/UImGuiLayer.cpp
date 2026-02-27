#ifdef _DEBUG
#include "UImGuiLayer.h"

#include <algorithm>

#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "engine/ImGui/ImGuiUtil.h"
#include "engine/platform/WindowsUtils.h"
#include "engine/render/rendergraph/RenderPassContext.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"

namespace Unnamed {
	UImGuiLayer::UImGuiLayer(
		const HWND        hwnd,
		Rhi::D3D12Device& device,
		const uint32_t    framesInFlight,
		const DXGI_FORMAT rtvFormat
	) : mDevice(device) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io    = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

		ImGuiStyle* style     = &ImGui::GetStyle();
		style->WindowRounding = 8.0f;
		style->FrameRounding  = 4.0f;

		ImFontConfig fontCfg     = {};
		fontCfg.OversampleH      = 4;
		fontCfg.OversampleV      = 4;
		fontCfg.PixelSnapH       = true;
		fontCfg.SizePixels       = 128.0f;
		fontCfg.GlyphMinAdvanceX = 2.0f;

		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\JetBrainsMono.ttf)",
			14.0f,
			&fontCfg,
			io.Fonts->GetGlyphRangesDefault()
		);
		fontCfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\NotoSansJP.ttf)",
			14.0f,
			&fontCfg,
			io.Fonts->GetGlyphRangesJapanese()
		);

		fontCfg.GlyphOffset                    = ImVec2(0.0f, 2.0f);
		static constexpr ImWchar kIconRanges[] = {0xe003, 0xf8ff, 0};
		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\MaterialSymbolsRounded_Filled_28pt-Regular.ttf)",
			14.0f,
			&fontCfg,
			kIconRanges
		);

		if (WindowsUtils::IsAppDarkTheme()) {
			ImGuiUtil::StyleColorsDark();
		} else { ImGuiUtil::StyleColorsLight(); }

		mCapacity       = 4096;
		mFramesInFlight = std::max(1u, framesInFlight);
		mFrameIndex     = 0;
		mDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = mCapacity;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		Rhi::Throw(
			device.GetDevice()->CreateDescriptorHeap(
				&heapDesc, IID_PPV_ARGS(mSrvHeap.ReleaseAndGetAddressOf())
			)
		);

		ImGui_ImplWin32_Init(hwnd);

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device                  = device.GetDevice();
		initInfo.CommandQueue            = device.GetGraphicsQueue();
		initInfo.NumFramesInFlight       = static_cast<int>(framesInFlight);
		initInfo.RTVFormat               = rtvFormat;
		initInfo.DSVFormat               = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap       = mSrvHeap.Get();
		initInfo.UserData                = this;

		initInfo.SrvDescriptorAllocFn = [](
			ImGui_ImplDX12_InitInfo*     info,
			D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
			D3D12_GPU_DESCRIPTOR_HANDLE* outGpu
		) {
				auto* layer = static_cast<UImGuiLayer*>(info->UserData);
				const uint32_t slot = layer->AllocateSlot();
				*outCpu = layer->CpuHandleAt(slot);
				*outGpu = layer->GpuHandleAt(slot);
			};
		initInfo.SrvDescriptorFreeFn = [](
			ImGui_ImplDX12_InitInfo*,
			D3D12_CPU_DESCRIPTOR_HANDLE,
			D3D12_GPU_DESCRIPTOR_HANDLE
		) {};

		ImGui_ImplDX12_Init(&initInfo);
	}

	UImGuiLayer::~UImGuiLayer() {
		// TODO: ↓ あるとクラッシュする Why?
		//if (ImGui::GetCurrentContext()) { ImGui::DestroyPlatformWindows(); }
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void UImGuiLayer::BeginFrame() const {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void UImGuiLayer::EndFrame() const {
		ImGui::Render();
		auto& self       = const_cast<UImGuiLayer&>(*this);
		self.mFrameIndex = (self.mFrameIndex + 1u) % self.mFramesInFlight;
	}

	void UImGuiLayer::RenderMainDrawData(
		const Render::RenderPassContext& passContext
	) const {
		if (!ImGui::GetCurrentContext()) { return; }
		ImDrawData* drawData = ImGui::GetDrawData();
		if (!drawData || drawData->CmdListsCount <= 0) { return; }

		ID3D12DescriptorHeap* heaps[] = {mSrvHeap.Get()};
		passContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(drawData, passContext.GetCommandList());
	}

	void UImGuiLayer::RenderPlatformWindows() const {
		if (
			(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ==
			0
		) { return; }

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	ImTextureID UImGuiLayer::GetOrCreateTextureId(
		const uint64_t key, const D3D12_CPU_DESCRIPTOR_HANDLE sourceSrv
	) {
		if (sourceSrv.ptr == 0) { return 0; }

		auto& [frameSlots] = mTextureSlotsByKey[key];
		if (frameSlots.empty()) {
			frameSlots.resize(mFramesInFlight, UINT32_MAX);
		}

		const uint32_t frameSlotIndex = mFrameIndex % mFramesInFlight;
		uint32_t&      slot           = frameSlots[frameSlotIndex];
		if (slot == UINT32_MAX) { slot = AllocateSlot(); }

		const D3D12_CPU_DESCRIPTOR_HANDLE dst = CpuHandleAt(slot);
		mDevice.GetDevice()->CopyDescriptorsSimple(
			1, dst, sourceSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		const D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(slot);
		return gpu.ptr;
	}

	uint32_t UImGuiLayer::AllocateSlot() {
		if (mNextSlot >= mCapacity) { return mCapacity - 1; }
		return mNextSlot++;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE UImGuiLayer::CpuHandleAt(
		const uint32_t slot
	) const {
		D3D12_CPU_DESCRIPTOR_HANDLE handle = mSrvHeap->
			GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(slot) * mDescriptorSize;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE UImGuiLayer::GpuHandleAt(
		const uint32_t slot
	) const {
		D3D12_GPU_DESCRIPTOR_HANDLE handle = mSrvHeap->
			GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<UINT64>(slot) * mDescriptorSize;
		return handle;
	}
}

#endif
