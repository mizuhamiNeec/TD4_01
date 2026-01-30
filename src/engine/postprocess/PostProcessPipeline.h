#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include <d3d12.h>

#include <engine/postprocess/IPostProcess.h>
#include <engine/renderer/D3D12.h>

#include <engine/renderer/SrvManager.h>

namespace Unnamed {
	/// @brief ポストプロセスチェーン管理とPingPongターゲット管理
	class PostProcessPipeline {
	public:
		PostProcessPipeline() = default;
		~PostProcessPipeline() = default;

		PostProcessPipeline(const PostProcessPipeline&) = delete;
		PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

		void Init(
			D3D12* renderer,
			SrvManager* srvManager,
			uint32_t width,
			uint32_t height,
			const float* clearColor,
			DXGI_FORMAT rtvFormat,
			DXGI_FORMAT dsvFormat
		);

		void Shutdown();

		void OnResize(uint32_t width, uint32_t height);

		void Execute(
			ID3D12GraphicsCommandList* commandList,
			ID3D12Resource* inputTexture,
			D3D12_CPU_DESCRIPTOR_HANDLE finalOutRtv,
			uint32_t finalWidth,
			uint32_t finalHeight
		);

		void AddPass(std::unique_ptr<IPostProcess> pass);

		size_t GetPassCount() const { return mPasses.size(); }
		IPostProcess* GetPassAt(size_t index) const {
			return (index < mPasses.size()) ? mPasses[index].get() : nullptr;
		}

		template <class T>
		T* FindPass() const {
			static_assert(std::is_base_of_v<IPostProcess, T>);
			for (const auto& p : mPasses) {
				if (auto* casted = dynamic_cast<T*>(p.get())) { return casted; }
			}
			return nullptr;
		}

		uint64_t GetActivePingSrvGpuPtr() const {
			return mPingRtv[mPingIndex].srvHandleGPU.ptr;
		}

		D3D12_RESOURCE_DESC GetActivePingRtvDesc() const {
			return mPingRtv[mPingIndex].rtv->GetDesc();
		}

	private:
		void CreateTargets(uint32_t width, uint32_t height);

		D3D12* mRenderer = nullptr;
		SrvManager* mSrvManager = nullptr;

		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		DXGI_FORMAT mRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDsvFormat = DXGI_FORMAT_D32_FLOAT;
		float mClearColorLocal[4]{};

		std::vector<std::unique_ptr<IPostProcess>> mPasses;
		std::array<RenderTargetTexture, 2> mPingRtv{};
		uint32_t mPingIndex = 0;
	};
}
