#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <wrl/client.h>

struct ImGui_ImplDX12_InitInfo;

namespace Unnamed {
	namespace Rhi {
		class D3D12Device;
	}

	namespace Render {
		class RenderPassContext;
	}

	class UImGuiLayer {
	public:
		UImGuiLayer(
			HWND              hwnd,
			Rhi::D3D12Device& device,
			uint32_t          framesInFlight,
			DXGI_FORMAT       rtvFormat
		);
		~UImGuiLayer();

		UImGuiLayer(const UImGuiLayer&)            = delete;
		UImGuiLayer& operator=(const UImGuiLayer&) = delete;

		void BeginFrame() const;
		void EndFrame() const;

		void RenderMainDrawData(
			const Render::RenderPassContext& passContext
		) const;
		void RenderPlatformWindows() const;

		ImTextureID GetOrCreateTextureId(
			uint64_t                    key,
			D3D12_CPU_DESCRIPTOR_HANDLE sourceSrv
		);

		[[nodiscard]] ID3D12DescriptorHeap* GetDescriptorHeap() const {
			return mSrvHeap.Get();
		}

	private:
		[[nodiscard]] uint32_t                    AllocateSlot();
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(
			uint32_t slot
		) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(
			uint32_t slot
		) const;

		Rhi::D3D12Device& mDevice;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
		uint32_t                                     mDescriptorSize = 0;
		uint32_t                                     mCapacity       = 0;
		uint32_t                                     mNextSlot       = 0;
		uint32_t                                     mFramesInFlight = 1;
		uint32_t                                     mFrameIndex     = 0;

		struct TextureSlots {
			std::vector<uint32_t> frameSlots;
		};

		std::unordered_map<uint64_t, TextureSlots> mTextureSlotsByKey;
	};
}

#endif
