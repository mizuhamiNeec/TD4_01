#pragma once
#include <memory>


class CameraComponent;
class D3D12;
class PipelineState;
class RootSignatureManager;

/// @brief ライン描画の共通クラス
class LineCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Render() const;

	[[nodiscard]] D3D12* GetRenderer() const;

private:
	CameraComponent*                      mDefaultCamera        = nullptr;
	D3D12*                                mRenderer             = nullptr;
	std::unique_ptr<RootSignatureManager> mRootSignatureManager = nullptr;
	std::unique_ptr<PipelineState>        mPipelineState;
};
