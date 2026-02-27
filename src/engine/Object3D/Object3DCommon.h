#pragma once

#include <memory>

class PipelineState;
class RootSignatureManager;
class CameraComponent;
class D3D12;

/// @brief 3Dオブジェクト共通クラス
class Object3DCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;

	void CreateRootSignature();
	void CreateGraphicsPipeline();

	void Render() const;

	D3D12* GetD3D12() const;

	// Getter
	static CameraComponent* GetDefaultCamera();

private:
	D3D12*                                mD3d12                = nullptr;
	std::unique_ptr<RootSignatureManager> mRootSignatureManager = nullptr;
	std::unique_ptr<PipelineState>        mPipelineState        = nullptr;
};
