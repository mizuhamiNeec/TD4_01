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

	RenderDevice::~RenderDevice() { mShaderLibrary.InvalidateAll(); }

	AssetManager& RenderDevice::GetAssetManager() const {
		return mAssetManager;
	}

	Rhi::IRhiDevice& RenderDevice::GetRhiDevice() const { return mRhiDevice; }

	ShaderLibrary& RenderDevice::GetShaderLibrary() { return mShaderLibrary; }

	PipelineCache& RenderDevice::GetPipelineCache() { return mPipelineCache; }

	void RenderDevice::OnResize(const uint32_t width, const uint32_t height) {
		mRhiDevice.OnResize(width, height);

		// リサイズに伴うパイプラインの再作成が必要な場合はここで行う
		mPipelineCache.InvalidateAll(); // TODO: 最初は雑で
		// Scene 描画解像度は URenderer 側の SceneRenderRequest で管理する。
		// ここで Registry をスワップチェーンサイズに追従させると
		// Editor Viewport がメインウィンドウサイズへ引きずられるため抑止する。
	}

	RgResourceRegistry& RenderDevice::GetRegistry() { return mRegistry; }

	void RenderDevice::HookHotReload() {
		mAssetManager.RegisterReload(
			[this](AssetID id) {
				const auto& meta = mAssetManager.Meta(id);
				if (meta.type == ASSET_TYPE::SHADER_SOURCE) {
					mShaderLibrary.InvalidateByShaderSource(id);
					mPipelineCache.InvalidateAll(); // TODO: 最初は雑で
				}
			}
		);
	}
}
