#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "RendererDraw.h"
#include "frame/RenderFrameInputs.h"

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"

#include "engine/rhi/Buffer.h"
#include "engine/rhi/Constants.h"
#include "engine/rhi/UploadBuffer.h"

#include "foundation/AdvancedRenderFoundation.h"

#include "rendergraph/RenderGraph.h"

#include "shaders/PipelineCache.h"

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderPassContext;
	class RenderDevice;

	class URenderer {
	public:
		URenderer() = default;

		/// @brief レンダラの初期化処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice
		void Init(RenderDevice& renderDevice);

		/// @brief 毎フレームの描画処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice
		/// @param inputs 描画に必要な入力データ
		void RenderFrame(
			RenderDevice& renderDevice, const RenderFrameInputs& inputs
		);

		/// @brief クライアントのリサイズ時の処理に呼び出されます。
		void OnResize(uint32_t width, uint32_t height);

		using UiMainRenderCallback = std::function<void(RenderPassContext&)>;
		using UiPlatformRenderCallback = std::function<void()>;

		void SetUiCallbacks(
			UiMainRenderCallback     mainRenderCallback,
			UiPlatformRenderCallback platformRenderCallback
		);

		void SetSceneRenderRequest(const SceneRenderRequest& request);

		[[nodiscard]] Vec2 GetSceneOutputSize() const {
			return Vec2(
				static_cast<float>(mSceneRenderWidth),
				static_cast<float>(mSceneRenderHeight)
			);
		}

		[[nodiscard]] uint32_t GetSceneOutputTextureId() const {
			return mSceneOutputTextureId;
		}

	private:
		/// @brief レンダリンググラフの構築
		/// @param renderDevice 描画に使用するRenderDevice
		void BuildGraph(RenderDevice& renderDevice);
		static std::pair<uint32_t, uint32_t> ResolveSceneRenderExtent(
			uint32_t                  backBufferWidth,
			uint32_t                  backBufferHeight,
			const SceneRenderRequest& request
		);

		void BuildDrawBatches();

		void CreateTriangleTestResources(Rhi::D3D12Device& dx);
		bool EnsureMeshResourceLoaded(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx,
			AssetID       meshAssetId
		);
		void LoadSceneMeshResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void LoadMaterialResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void LoadPostFxChain(RenderDevice& renderDevice);

		bool ResolveShaderProgramStageKey(
			RenderDevice& renderDevice,
			AssetID       shaderProgramId,
			const char*   stage,
			ShaderKey&    outKey
		) const;

		struct FullscreenPassRes {
			ID3D12RootSignature* rootSig = nullptr; // TODO: DX12寄りだが今は最小
			GraphicsPsoKey       psoKey  = {};
		};

		struct ComputePassRes {
			ID3D12RootSignature* rootSig = nullptr;
			ComputePipelineKey   psoKey  = {};
		};

		struct GeometryPassRes {
			ID3D12RootSignature* rootSig = nullptr;
			GraphicsPsoKey       psoKey  = {};

			ID3D12PipelineState* pso = nullptr;

			Microsoft::WRL::ComPtr<ID3D12Resource> vb;
			Microsoft::WRL::ComPtr<ID3D12Resource> ib;
			D3D12_VERTEX_BUFFER_VIEW               vbv        = {};
			D3D12_INDEX_BUFFER_VIEW                ibv        = {};
			uint32_t                               indexCount = 0;
			AABB                                   localAABB  = {};

			DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
		};

		struct MaterialBinding {
			Rhi::MaterialConstants constants          = {};
			AssetID                materialInstanceId = kInvalidAssetID;
			uint32_t               albedoTextureId    = 0;
			uint32_t               padding0           = 0;
		};

		struct PostFxRuntimePass {
			std::string       name;
			bool              enabled = true;
			FullscreenPassRes pass    = {};
		};

		struct PortalPassRes {
			FullscreenPassRes    maskPass     = {};
			GraphicsPsoKey       scenePsoKey  = {};
			ID3D12RootSignature* sceneRootSig = nullptr;
			ID3D12PipelineState* scenePso     = nullptr;
			uint32_t             stencilRef   = 1;
		};

		static constexpr uint32_t kMaxDrawObjects = 1024; // とりあえず

		RenderGraph mGraph;

		FullscreenPassRes        mFullscreenPass     = {};
		FullscreenPassRes        mDepthVisPass       = {};
		ComputePassRes           mComputePass        = {};
		GeometryPassRes          mGeometryPass       = {};
		PortalPassRes            mPortalPass         = {};
		AdvancedRenderFoundation mAdvancedFoundation = {};

		Rhi::UploadBuffer<Rhi::FrameConstants> mFrameCb;
		Rhi::UploadBuffer<Rhi::ObjectConstants> mObjectCb;
		Rhi::UploadBuffer<Rhi::MaterialConstants> mMaterialCb;
		Rhi::UploadBuffer<Rhi::SkinningPaletteConstants> mSkinningCb;
		std::vector<MeshBuffer> mSceneMeshes;
		std::unordered_map<AssetID, MeshBuffer> mSceneMeshesByAsset;
		AssetID mLoadedMeshAsset = kInvalidAssetID;
		AssetID mDefaultMaterialInstance =
			kInvalidAssetID;
		AssetID mPostFxChainAsset = kInvalidAssetID;
		std::unordered_map<AssetID, MaterialBinding> mMaterialBindings;
		std::vector<PostFxRuntimePass> mPostFxPasses;

		std::vector<MeshDrawItem> mDrawList;
		std::vector<DrawBatch>    mBatches;
		bool                      mPortalEnabled        = false;
		uint32_t                  mSceneOutputTextureId = 0;
		SceneRenderRequest        mSceneRenderRequest   = {};
		uint32_t                  mSceneRenderWidth     = 0;
		uint32_t                  mSceneRenderHeight    = 0;
		UiMainRenderCallback      mUiMainRenderCallback;
		UiPlatformRenderCallback  mUiPlatformRenderCallback;

		bool mGraphBuilt = false;
	};
}
