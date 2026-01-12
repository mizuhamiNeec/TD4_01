#pragma once
#include <d3d12.h>
#include <memory>
#include <unordered_map>

#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/urenderer/GraphicsDevice.h>

#include <runtime/assets/core/UAssetID.h>
#include <runtime/core/math/Math.h>

#include "runtime/render/utils/Frustum.h"

namespace Unnamed {
	class UAssetManager;
	class RenderResourceManager;
	class ShaderLibrary;
	class RootSignatureCache;
	class UPipelineCache;
	class UWorld;
	struct UMaterialRuntime;

	/// @brief フレームコンテキスト構造体
	struct LastSubmit {
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		uint64_t                            value = 0;
		bool                                valid = false;
	};

	/// @brief レンダーサブシステムクラス
	class URenderSubsystem : public ISubsystem {
	public:
		URenderSubsystem(
			GraphicsDevice*        graphicsDevice,
			RenderResourceManager* renderResourceManager,
			ShaderLibrary*         shaderLibrary,
			RootSignatureCache*    rootSignatureCache,
			UPipelineCache*        pipelineCache,
			UAssetManager*         assetManager
		);

		void BeginFrame();
		void RenderWorld(const UWorld& world);
		void EndFrame();

		void SetView(const RenderView& view) {
			mView = view;
		}

		[[nodiscard]] FrameContext GetContext() const;

		// ISubsystem
		bool Init() override;

		[[nodiscard]] const std::string_view GetName() const override;

	private:
		void Collect(const UWorld& world, const Mat4& parent);
		void DrawItems();

		UMaterialRuntime* EnsureMaterialGPU(
			AssetID materialAsset, ID3D12GraphicsCommandList* cmd
		);
		void OnAssetReloaded(AssetID id);

	private:
		struct FrameCBData {
			Mat4  view;
			Mat4  proj;
			Mat4  viewProj;
			Vec3  cameraPos;
			float time;
		};

		struct ObjectCBData {
			Mat4 world;
			Mat4 worldInverseTranspose;
		};

		struct TempCB {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			D3D12_GPU_VIRTUAL_ADDRESS              gpu = 0;
		};

		struct TransientBin {
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> keep;
			Microsoft::WRL::ComPtr<ID3D12Fence>                 fence;
			uint64_t                                            fenceValue = 0;
		};

		static constexpr int kFrameInFlight = kFrameBufferCount;
		std::array<TransientBin, kFrameInFlight> mTransient;

		D3D12_GPU_VIRTUAL_ADDRESS UploadCB(
			const void* src, size_t bytes
		) const;

		GraphicsDevice*        mGraphicsDevice        = nullptr;
		RenderResourceManager* mRenderResourceManager = nullptr;
		ShaderLibrary*         mShaderLibrary         = nullptr;
		RootSignatureCache*    mRootSignatureCache    = nullptr;
		UPipelineCache*        mPipelineCache         = nullptr;
		UAssetManager*         mAssetManager          = nullptr;

		FrameContext mContext;
		RenderView   mView = {};

		/// @brief レンダリングするアイテムの情報
		struct RenderItem {
			Mat4                matWorld;
			TransformComponent* tr = nullptr;
			MeshHandle          meshHandle; // 共有メッシュのハンドル

			AssetID           materialAsset = kInvalidAssetID;
			UMaterialRuntime* material      = nullptr;

			float                depthVS     = 0.0f;
			AssetID              psoId       = 0;
			AssetID              materialKey = 0;
			ID3D12RootSignature* rsPtr       = nullptr;
		};

		struct MaterialCacheEntry {
			std::unique_ptr<UMaterialRuntime> runtime;
			uint64_t                          lastBuiltVersion = 0;
		};

		struct BatchKey {
			uint32_t psoId       = 0;
			AssetID  materialKey = 0;
			uint32_t meshId      = 0;
			uint32_t meshGen     = 0;

			bool operator==(const BatchKey& r) const = default;
		};

		struct BatchKeyHash {
			size_t operator==(const BatchKey& k) const noexcept {
				size_t h   = 14695981039346656037ull;
				auto   Mix = [&](const uint64_t v) {
					h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
				};
				Mix(k.psoId);
				Mix(k.materialKey);
				Mix((static_cast<uint64_t>(k.meshId) << 32) | k.meshGen);
				return h;
			}
		};

		struct InstanceData {
			float worldCol0[4];
			float worldCol1[4];
			float worldCol2[4];

			float normalCol0[4];
			float normalCol1[4];
			float normalCol2[4];
		};

		std::vector<RenderItem> mItems;

		Frustum mFrustum;

		std::unordered_map<AssetID, MaterialCacheEntry> mMaterialCache;

		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBVA = 0;

		D3D12_GPU_VIRTUAL_ADDRESS mDummyObjectCBVA = 0;

		LastSubmit mLastSubmit;
	};
}
