#include "RenderModule.h"

#include <utility>

#include "RenderDevice.h"
#include "URenderer.h"

namespace Unnamed::Render {
	RenderModule::RenderModule(
		AssetManager& assetManager, Rhi::IRhiDevice& rhiDevice
	) : mAssetManager(assetManager),
	    mRhiDevice(rhiDevice) {}

	void RenderModule::Init() {
		mRenderDevice = std::make_unique<RenderDevice>(
			mRhiDevice, mAssetManager
		);
		mRenderer = std::make_unique<URenderer>();
		mRenderer->Init(*mRenderDevice);
	}

	void RenderModule::Tick(RenderFrameInputs& inputs) const {
		mRenderer->RenderFrame(*mRenderDevice, inputs);
	}

	void RenderModule::OnResize(
		const uint32_t width, const uint32_t height
	) const {
		if (mRenderDevice) {
			mRenderDevice->OnResize(width, height);
			mRenderer->OnResize(width, height);
		}
	}

	void RenderModule::SetUiCallbacks(
		URenderer::UiMainRenderCallback     mainRenderCallback,
		URenderer::UiPlatformRenderCallback platformRenderCallback
	) const {
		if (!mRenderer) { return; }
		mRenderer->SetUiCallbacks(
			std::move(mainRenderCallback), std::move(platformRenderCallback)
		);
	}

	uint32_t RenderModule::GetSceneOutputTextureId() const {
		return mRenderer ? mRenderer->GetSceneOutputTextureId() : 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RenderModule::GetSceneOutputSrvCpu() const {
		if (!mRenderer || !mRenderDevice) { return {}; }
		const uint32_t textureId = mRenderer->GetSceneOutputTextureId();
		if (textureId == 0) { return {}; }
		return mRenderDevice->GetRegistry().GetSrvCpu(textureId);
	}

	Vec2 RenderModule::GetSceneOutputSize() const {
		return mRenderer ? mRenderer->GetSceneOutputSize() : Vec2(0.0f, 0.0f);
	}

	void RenderModule::SetSceneRenderRequest(
		const SceneRenderRequest& request
	) const {
		if (!mRenderer) { return; }
		mRenderer->SetSceneRenderRequest(request);
	}
}
