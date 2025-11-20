#pragma once

#include <memory>
#include "engine/renderer/RootSignatureManager.h"
#include "engine/renderer/PipelineState.h"

class D3D12;

/// @brief スプライト共通クラス
class SpriteCommon {
public:
	void Init(D3D12* d3d12);
	void Shutdown() const;
	void CreateRootSignature();

	void CreateGraphicsPipeline();

	void Render() const;

	D3D12* GetD3D12() const {
		return mD3d12;
	}

private:
	D3D12*                                mD3d12                = nullptr;
	std::unique_ptr<RootSignatureManager> mRootSignatureManager = nullptr;
	PipelineState                         mPipelineState;
};
