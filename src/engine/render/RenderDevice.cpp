#include "RenderDevice.h"

#include "core/assets/AssetManager.h"

#include "engine/rhi/d3d12/D3D12Device.h"

namespace Unnamed::Render {
	RenderDevice::RenderDevice(
		Rhi::IRhiDevice& rhiDevice, AssetManager& assetManager
	) : mRhiDevice(rhiDevice),
	    mAssetManager(assetManager),
	    mShaderLibrary(
		    assetManager,
		    dynamic_cast<Rhi::D3D12Device&>(rhiDevice).GetDxcCompiler()
	    ),
	    mPipelineCache(
		    dynamic_cast<Rhi::D3D12Device&>(rhiDevice).GetDevice(),
		    mShaderLibrary
	    ),
	    mRegistry(dynamic_cast<Rhi::D3D12Device&>(rhiDevice)) {
		HookHotReload();
	}

	RenderDevice::~RenderDevice() {
		mShaderLibrary.InvalidateAll();
	}

	AssetManager& RenderDevice::GetAssetManager() const {
		return mAssetManager;
	}

	Rhi::IRhiDevice& RenderDevice::GetRhiDevice() const {
		return mRhiDevice;
	}

	ShaderLibrary& RenderDevice::GetShaderLibrary() {
		return mShaderLibrary;
	}

	PipelineCache& RenderDevice::GetPipelineCache() {
		return mPipelineCache;
	}

	void RenderDevice::InvalidateAssetDerivedState(const AssetID id) {
		const auto& meta = mAssetManager.Meta(id);
		switch (meta.type) {
			case ASSET_TYPE::SHADER_SOURCE: {
				mShaderLibrary.InvalidateByShaderSource(id);
				mPipelineCache.InvalidateAll();
				break;
			}
			case ASSET_TYPE::SHADER_PROGRAM: {
				mPipelineCache.InvalidateAll();
				break;
			}
			case ASSET_TYPE::MESH: {
				mDirtyMeshAssets.emplace(id);
				break;
			}
			case ASSET_TYPE::TEXTURE:
			case ASSET_TYPE::MATERIAL:
			case ASSET_TYPE::MATERIAL_INSTANCE: {
				mMaterialsDirty = true;
				break;
			}
			case ASSET_TYPE::POST_FX_CHAIN: {
				mPostFxDirty = true;
				break;
			}
			default: break;
		}
	}

	void RenderDevice::ConsumeDirtyAssets(
		std::unordered_set<AssetID>& outMeshAssets,
		bool&                        outMaterialsDirty,
		bool&                        outPostFxDirty
	) {
		outMeshAssets = std::move(mDirtyMeshAssets);
		mDirtyMeshAssets.clear();
		outMaterialsDirty = mMaterialsDirty;
		outPostFxDirty    = mPostFxDirty;
		mMaterialsDirty   = false;
		mPostFxDirty      = false;
	}

	void RenderDevice::OnResize(const uint32_t width, const uint32_t height) {
		mRhiDevice.OnResize(width, height);

		mPipelineCache.InvalidateAll();
	}

	RgResourceRegistry& RenderDevice::GetRegistry() {
		return mRegistry;
	}

	void RenderDevice::HookHotReload() {
		mAssetManager.RegisterReload(
			[this](AssetID id) {
				InvalidateAssetDerivedState(id);
			}
		);
	}
}
