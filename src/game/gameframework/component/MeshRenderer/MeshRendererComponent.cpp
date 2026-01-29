//-----------------------------------------------------------------------------

#include <core/unnamed/json/JsonReader.h>

#include <engine/unnamed/urenderer/GraphicsDevice.h>

#include <game/gameframework/component/MeshRenderer/MeshRendererComponent.h>

#include <runtime/render/resources/RenderResourceManager.h>

#include "engine/unnamed/urootsignaturecache/RootSignatureCache.h"

#include "runtime/render/utils/RenderUtils.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "MeshRendererComponent";

	/// @brief メッシュレンダラーコンポーネントがアタッチされたときの処理
	void MeshRendererComponent::OnAttached() { BaseComponent::OnAttached(); }

	/// @brief メッシュレンダラーコンポーネントがデタッチされたときの処理
	void MeshRendererComponent::OnDetached() { BaseComponent::OnDetached(); }

	/// @brief シリアライズ
	/// @param writer JSONライター
	void MeshRendererComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	/// @brief デシリアライズ
	/// @param reader JSONリーダー
	void MeshRendererComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("mesh")) {
			// TODO: AssetManagerからロード
		}
		if (reader.Has("material")) {
			// 文字列をAssetに解決して materialAsset
		}
	}

	std::string_view MeshRendererComponent::GetComponentName() const {
		return "MeshRenderer";
	}

	///	 @brief GPUリソースの確保
	/// @param renderResourceManager レンダーリソースマネージャー
	/// @return GPUリソースが確保できたらtrueを返す
	bool MeshRendererComponent::EnsureGPU(
		GraphicsDevice*            graphicsDevice,
		RenderResourceManager*     renderResourceManager,
		ShaderLibrary*             shaderLibrary,
		RootSignatureCache*        rootSignatureCache,
		UPipelineCache*            pipelineCache,
		ID3D12GraphicsCommandList* cmd
	) {
		(void)graphicsDevice;
		(void)shaderLibrary;
		(void)rootSignatureCache;
		(void)pipelineCache;
		(void)cmd;

		if (mGPUReady) { return true; }

		if (!meshHandle.IsValid() && meshAsset != kInvalidAssetID) {
			meshHandle = renderResourceManager->AcquireMesh(meshAsset);
		}

		mGPUReady = meshHandle.IsValid();
		return mGPUReady;
	}

	///	@brief GPUリソースの解放
	/// @param renderResourceManager レンダーリソースマネージャー
	void MeshRendererComponent::InvalidateGPU(
		RenderResourceManager* renderResourceManager
	) {
		// メッシュの解放
		if (meshHandle.IsValid()) {
			renderResourceManager->ReleaseMesh(meshHandle, nullptr, 0);
			meshHandle = {};
		}

		mGPUReady = false;
	}

	const MeshRendererComponent::WorldBoundsSphereCache& MeshRendererComponent::
	WorldBoundsSphere() const noexcept { return mWorldBoundsSphere; }

	void MeshRendererComponent::UpdateWorldBoundsCache(
		const Mat4& worldMat,
		uint64_t    worldRevision, const BoundsSphere& bounds
	) {
		if (mWorldBoundsRevision == worldRevision) { return; }

		const Vec3 centerWS = TransformPointRowVec(worldMat, bounds.center);
		float      radiusWS = bounds.radius * MaxScaleRowVec(worldMat);

		mWorldBoundsSphere.center = centerWS;
		mWorldBoundsSphere.radius = radiusWS;
		mWorldBoundsRevision      = worldRevision;
	}
}
