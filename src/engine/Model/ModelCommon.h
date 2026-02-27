#pragma once

class D3D12;

/// @brief モデル共通クラス
class ModelCommon {
public:
	~ModelCommon();

	void Init(D3D12* d3d12);

	[[nodiscard]] D3D12* GetD3D12() const;

private:
	D3D12* mD3d12 = nullptr;
};
