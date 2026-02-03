#pragma once

#include <d3d12.h>
#include <wrl/client.h>

class SrvManager;
class D3D12;
class SceneComponent;
class ShaderResourceViewManager;
struct Vec3;

/// @brief ImGuiの管理クラス
class ImGuiManager {
public:
	ImGuiManager(HWND hwnd, D3D12* renderer, SrvManager* srvManager);
	static void NewFrame();
	void        EndFrame();
	void        Shutdown();

	SrvManager* GetSrvManager() const;

private:
	D3D12*                                       mRenderer   = nullptr;
	SrvManager*                                  mSrvManager = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
};
