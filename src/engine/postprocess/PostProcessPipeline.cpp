#include <pch.h>

#include <engine/postprocess/PostProcessPipeline.h>

namespace Unnamed {
	void PostProcessPipeline::Init(
		D3D12*            renderer,
		SrvManager*       srvManager,
		const uint32_t    width,
		const uint32_t    height,
		const float*      clearColor,
		const DXGI_FORMAT rtvFormat,
		const DXGI_FORMAT dsvFormat
	) {
		mRenderer   = renderer;
		mSrvManager = srvManager;
		mWidth      = width;
		mHeight     = height;
		mRtvFormat  = rtvFormat;
		mDsvFormat  = dsvFormat;
		mPingIndex  = 0;

		mClearColorLocal[0] = clearColor[0];
		mClearColorLocal[1] = clearColor[1];
		mClearColorLocal[2] = clearColor[2];
		mClearColorLocal[3] = clearColor[3];

		CreateTargets(width, height);
	}

	void PostProcessPipeline::Shutdown() {
		mPasses.clear();
		for (auto& rt : mPingRtv) { rt.rtv.Reset(); }
		mRenderer   = nullptr;
		mSrvManager = nullptr;
		mWidth      = 0;
		mHeight     = 0;
		mPingIndex  = 0;
	}

	void PostProcessPipeline::AddPass(std::unique_ptr<IPostProcess> pass) {
		mPasses.emplace_back(std::move(pass));
	}

	void PostProcessPipeline::OnResize(
		const uint32_t width, const uint32_t height
	) {
		if (!mRenderer || width == 0 || height == 0) { return; }

		mWidth     = width;
		mHeight    = height;
		mPingIndex = 0;

		for (auto& rt : mPingRtv) { rt.rtv.Reset(); }

		CreateTargets(width, height);
	}

	void PostProcessPipeline::CreateTargets(
		const uint32_t width, const uint32_t height
	) {
		if (!mRenderer) { return; }
		for (auto& rt : mPingRtv) {
			rt = mRenderer->CreateRenderTargetTexture(
				width,
				height,
				Vec4{
					mClearColorLocal[0], mClearColorLocal[1],
					mClearColorLocal[2], mClearColorLocal[3]
				},
				mRtvFormat
			);
		}
	}

	void PostProcessPipeline::Execute(
		ID3D12GraphicsCommandList*        commandList,
		ID3D12Resource*                   inputTexture,
		const D3D12_CPU_DESCRIPTOR_HANDLE finalOutRtv,
		const uint32_t                    finalWidth,
		const uint32_t                    finalHeight
	) {
		if (!commandList || !inputTexture || !mRenderer) { return; }
		if (mPasses.empty()) {
			// 何も無い場合は入力をそのまま出したいが、既存のCopyImagePassを
			// 使うなどのポリシーが必要なため、ここでは何もしない。
			return;
		}

		ID3D12Resource* postProcessTarget = inputTexture;

		for (auto& pass : mPasses) {
			const uint32_t next = mPingIndex ^ 1;
			auto&          dest = mPingRtv[next];

			const bool     isLastPass = &pass == &mPasses.back();
			const auto     outRtv = isLastPass ? finalOutRtv : dest.rtvHandle;
			const uint32_t outW = isLastPass ? finalWidth : mWidth;
			const uint32_t outH = isLastPass ? finalHeight : mHeight;

			PostProcessContext context = {};
			context.commandList        = commandList;
			context.inputTexture       = postProcessTarget;
			context.outRtv             = outRtv;
			context.width              = outW;
			context.height             = outH;

			pass->Execute(context);

			if (!isLastPass) {
				postProcessTarget = dest.rtv.Get();
				mPingIndex        = next;
			}
		}
	}
}
