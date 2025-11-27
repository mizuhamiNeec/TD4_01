#pragma once

#ifdef _DEBUG
#include <imgui.h>
#else
#endif
#include <wrl/client.h>
#include <d3d12.h>

class SrvManager;
class D3D12;
class SceneComponent;
class ShaderResourceViewManager;
struct Vec3;

/// @brief ImGuiの管理クラス
class ImGuiManager {
public:
	ImGuiManager(D3D12* renderer, SrvManager* srvManager);
	static void NewFrame();
	void        EndFrame();
	void        Shutdown();
	void        Recreate() const;

	SrvManager* GetSrvManager() const;

private:
#ifdef _DEBUG
	const ImWchar* GetGlyphRangesJapanese();
#endif

	D3D12*                                       mRenderer   = nullptr;
	SrvManager*                                  mSrvManager = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
};
