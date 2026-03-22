#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "RendererDraw.h"

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"

#include "engine/rhi/Buffer.h"
#include "engine/rhi/Constants.h"
#include "engine/rhi/UploadBuffer.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "foundation/AdvancedRenderFoundation.h"

#include "frame/RenderFrameInputs.h"

#include "rendergraph/RenderGraph.h"

#include "shaders/PipelineCache.h"

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderPassContext;
	class RenderDevice;

	struct SceneOutputView {
		uint32_t                    textureId   = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE srvCpu      = {};
		uint64_t                    srvRevision = 0;
	};

	class Renderer {
	public:
		Renderer(ConsoleSystem* console);

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

		[[nodiscard]] SceneOutputView GetViewOutputView(
			const RenderDevice& renderDevice,
			std::string_view    viewKey
		) const;
		[[nodiscard]] Vec2 GetViewOutputSize(std::string_view viewKey) const;

	private:
		// シーンの描画にはHDRを使う!
		static constexpr DXGI_FORMAT kSceneHdrColorFormat =
			DXGI_FORMAT_R16G16B16A16_FLOAT;

		// UIなどはLDRを使う
		static constexpr DXGI_FORMAT kSceneLdrColorFormat =
			DXGI_FORMAT_R8G8B8A8_UNORM;

		/// @brief レンダリンググラフの構築
		/// @param renderDevice 描画に使用するRenderDevice
		void BuildGraph(
			RenderDevice&                       renderDevice,
			const std::vector<RenderViewInput>& frameViews
		);
		static std::pair<uint32_t, uint32_t> ResolveSceneRenderExtent(
			uint32_t                   backBufferWidth,
			uint32_t                   backBufferHeight,
			const SceneViewRenderMode& request
		);

		void CreateTriangleTestResources(Rhi::D3D12Device& dx);
		void CreatePortalQuadResources(Rhi::D3D12Device& dx);
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
			ID3D12RootSignature* rootSig = nullptr; // TODO: DX12から独立
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
			std::string                            name;
			bool                                   enabled = true;
			std::unordered_map<std::string, float> scalarDefaults;
			std::unordered_map<std::string, Vec4>  colorDefaults;
			FullscreenPassRes                      pass = {};
		};

		struct PortalPassRes {
			GeometryPassRes      maskPassGeom   = {};
			GeometryPassRes      compositeGeom  = {};
			GraphicsPsoKey       scenePsoKey    = {};
			ID3D12RootSignature* sceneRootSig   = nullptr;
			ID3D12PipelineState* scenePso       = nullptr;
			uint32_t             stencilRefBase = 1;
		};

		struct ActivePortalView {
			PortalPairInput pair              = {};
			uint32_t        stencilRef        = 0;
			uint32_t        maskObjectCbIndex = 0;
		};

		struct RecursivePortalView {
			PortalPairInput pair                   = {};
			Mat4            cameraWorld            = Mat4::identity;
			uint32_t        frameCbIndex           = 0;
			uint32_t        compositeObjectCbIndex = 0;
			uint32_t        stencilRef             = 0;
			bool            visibleFromPrevious    = false;
		};

		struct SpritePassRes {
			GeometryPassRes geom = {};
		};

		struct BillboardPassRes {
			GeometryPassRes geom = {};
		};

		struct DebugLineVertex {
			float px = 0.0f;
			float py = 0.0f;
			float pz = 0.0f;
			float r  = 1.0f;
			float g  = 1.0f;
			float b  = 1.0f;
			float a  = 1.0f;
		};

		struct LinePassRes {
			ID3D12RootSignature* rootSig = nullptr;
			GraphicsPsoKey       psoKey  = {};
			ID3D12PipelineState* pso     = nullptr;

			Microsoft::WRL::ComPtr<ID3D12Resource> dynamicVb;
			DebugLineVertex*                       mappedVertices   = nullptr;
			D3D12_VERTEX_BUFFER_VIEW               frameVbv         = {};
			uint32_t                               vertexCapacity   = 0;
			uint32_t                               frameVertexCount = 0;
		};

		struct ViewRuntimeState {
			RENDER_VIEW_TYPE      type = RENDER_VIEW_TYPE::SCENE;
			RenderViewOutputDesc  output = {};
			uint32_t              width = 1;
			uint32_t              height = 1;
			uint32_t              requestedWidth = 1;
			uint32_t              requestedHeight = 1;
			uint32_t              pendingShrinkWidth = 0;
			uint32_t              pendingShrinkHeight = 0;
			uint32_t              pendingShrinkStableFrames = 0;
			uint32_t              colorTextureId = 0;
			uint32_t              depthTextureId = 0;
			uint32_t              postFxTextureAId = 0;
			uint32_t              postFxTextureBId = 0;
			std::vector<uint32_t> bloomMipTextureIds = {};
			uint32_t              outputTextureId = 0;
		};

		static constexpr uint32_t kMaxDrawObjects       = 1024; // TODO: とりあえず
		static constexpr uint32_t kMaxPortalViews       = 16;
		static constexpr uint32_t kPortalRecursionDepth = 6;
		static constexpr uint32_t kPortalDirections     = 2;
		static constexpr uint32_t kMaxDebugLines        = 65536; // TODO: とりあえず

		ConsoleSystem* mConsole       = nullptr;
		ConVar<int>*   mBloomMipCount = nullptr;

		RenderGraph mGraph;

		FullscreenPassRes        mFullscreenPass      = {};
		FullscreenPassRes        mHdrCopyPass         = {};
		FullscreenPassRes        mToneMapPass         = {};
		FullscreenPassRes        mBloomDownsamplePass = {};
		FullscreenPassRes        mBloomUpsamplePass   = {};
		FullscreenPassRes        mBloomCombinePass    = {};
		FullscreenPassRes        mDepthVisPass        = {};
		ComputePassRes           mComputePass         = {};
		GeometryPassRes          mGeometryPass        = {};
		PortalPassRes            mPortalPass          = {};
		SpritePassRes            mSpritePass          = {};
		BillboardPassRes         mBillboardPass       = {};
		LinePassRes              mLinePass            = {};
		AdvancedRenderFoundation mAdvancedFoundation  = {};

		Rhi::UploadBuffer<Rhi::FrameConstants> mFrameCb;
		Rhi::UploadBuffer<Rhi::FrameConstants> mPortalFrameCb;
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
		std::unordered_map<AssetID, uint32_t> mSpriteTextureIds;
		uint32_t mSpriteFallbackTextureId = 0;

		std::vector<MeshDrawItem>     mMainDrawList;
		std::vector<MeshDrawItem>     mPortalDrawList;
		std::vector<DrawBatch>        mMainBatches;
		std::vector<DrawBatch>        mPortalBatches;
		bool                          mPortalEnabled = false;
		std::vector<ActivePortalView> mActivePortalViews;
		std::array<std::array<RecursivePortalView, kPortalRecursionDepth>,
		           kPortalDirections>
		mPortalRecursionViews = {};
		std::array<std::array<uint32_t, kPortalRecursionDepth>,
		           kPortalDirections>
		mPortalRecursionColorIds = {};
		std::array<std::array<uint32_t, kPortalRecursionDepth>,
		           kPortalDirections>
		mPortalRecursionDepthIds = {};
		std::unordered_map<std::string, ViewRuntimeState> mViewStates;
		std::vector<std::string> mViewExecutionOrder;
		std::vector<RenderViewInput> mFrameViews;
		std::vector<DebugLineInput> mFrameDebugLines;
		std::string mPresentViewKey;
		UiMainRenderCallback mUiMainRenderCallback;
		UiPlatformRenderCallback mUiPlatformRenderCallback;

		bool mGraphBuilt           = false;
		Mat4 mBillboardCameraWorld = Mat4::identity;

		uint32_t EnsureSpriteTextureLoaded(
			RenderDevice& renderDevice, AssetID textureAssetId
		);
		uint32_t ResolveSpriteTexture(
			RenderDevice& renderDevice, const SpriteTextureRef& textureRef
		);
		void        EnsureSpriteFallbackTexture(RenderDevice& renderDevice);
		void        InitializeDebugLineResources(Rhi::D3D12Device& dx);
		void        UploadDebugLinesForFrame();
		static void ReleaseViewRuntimeTextures(
			RenderDevice& renderDevice, ViewRuntimeState& state
		);
	};
}
