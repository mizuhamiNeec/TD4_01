#pragma once
#include <d3d12.h>

#include <engine/gameframework/component/base/BaseComponent.h>

#include "engine/urootsignaturecache/RootSignatureCache.h"

#include "runtime/assets/core/UAssetID.h"
#include "runtime/render/types/RenderTypes.h"

namespace Unnamed {
	class RenderResourceManager;
	class ShaderLibrary;
	class RRootSignatureCache;
	class UPipelineCache;
	class GraphicsDevice;

	/// @brief メッシュレンダラーコンポーネント
	class MeshRendererComponent : public BaseComponent {
		struct WorldBoundsSphereCache {
			Vec3  center = Vec3::zero;
			float radius = 0.0f;
		};

	public:
		AssetID meshAsset     = kInvalidAssetID;
		AssetID materialAsset = kInvalidAssetID;

		MeshHandle meshHandle = {}; // 共有メッシュのハンドル

	public:
		// MeshRendererComponent
		[[nodiscard]] std::string_view GetComponentName() const override;

		bool EnsureGPU(
			GraphicsDevice*            graphicsDevice,
			RenderResourceManager*     renderResourceManager,
			ShaderLibrary*             shaderLibrary,
			RootSignatureCache*        rootSignatureCache,
			UPipelineCache*            pipelineCache,
			ID3D12GraphicsCommandList* cmd
		);
		void InvalidateGPU(RenderResourceManager* renderResourceManager);

		[[nodiscard]] const WorldBoundsSphereCache&
		WorldBoundsSphere() const noexcept;

		void UpdateWorldBoundsCache(
			const Mat4&         worldMat,
			uint64_t            worldRevision,
			const BoundsSphere& bounds
		);

		// BaseComponent interface
		void OnAttached() override;
		void OnDetached() override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

	private:
		bool mGPUReady = false;

		WorldBoundsSphereCache mWorldBoundsSphere   = {};
		uint64_t               mWorldBoundsRevision = 0;
	};
}
