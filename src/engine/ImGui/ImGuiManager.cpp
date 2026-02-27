#ifdef _DEBUG
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <engine/Engine.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/platform/WindowsUtils.h>
#include <engine/renderer/SrvManager.h>

#include "ImGuiUtil.h"

#include "engine/renderer/D3D12.h"

/// @brief ImGuiマネージャーのコンストラクタ
/// @param renderer D3D12レンダラーへのポインタ
/// @param srvManager SRVマネージャーへのポインタ
ImGuiManager::ImGuiManager(
	const HWND  hwnd,
	D3D12*      renderer,
	SrvManager* srvManager
) :
	mRenderer(renderer),
	mSrvManager(srvManager) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io    = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

	// 少し角丸に
	ImGuiStyle* style     = &ImGui::GetStyle();
	style->WindowRounding = 8;
	style->FrameRounding  = 4;

	ImFontConfig imFontConfig;
	imFontConfig.OversampleH      = 4;
	imFontConfig.OversampleV      = 4;
	imFontConfig.PixelSnapH       = true;
	imFontConfig.SizePixels       = 128.0f;
	imFontConfig.GlyphMinAdvanceX = 2.0f;

	// Ascii
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\JetBrainsMono.ttf)", 14.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesDefault()
	);
	imFontConfig.MergeMode = true;

	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\NotoSansJP.ttf)", 14.0f, &imFontConfig,
		ImGui::GetIO().Fonts->GetGlyphRangesJapanese()
	);

	// ??? 何故かアイコンフォントのベースラインがずれるので補正
	imFontConfig.GlyphOffset = ImVec2(0.0f, 2.0f);

	static constexpr ImWchar iconRanges[] = {0xe003, 0xf8ff, 0};
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\MaterialSymbolsRounded_Filled_28pt-Regular.ttf)",
		14.0f, &imFontConfig, iconRanges
	);

	// テーマの設定
	if (WindowsUtils::IsAppDarkTheme()) { ImGuiUtil::StyleColorsDark(); } else {
		ImGuiUtil::StyleColorsLight();
	}

	ImGui_ImplWin32_Init(hwnd);

	// ImGuiの初期化
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device                  = mRenderer->GetDevice();
	init_info.NumFramesInFlight       = kFrameBufferCount;
	init_info.RTVFormat               = kBufferFormat;
	init_info.CommandQueue            = mRenderer->GetCommandQueue();
	init_info.SrvDescriptorHeap       = mSrvManager->GetDescriptorHeap();
	init_info.UserData                = this;

	init_info.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo*     info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
	) {
			const auto mgr = static_cast<ImGuiManager*>(info->UserData);
			const auto index = mgr->GetSrvManager()->AllocateForTexture2D();
			*cpuHandle = mgr->GetSrvManager()->GetCPUDescriptorHandle(index);
			*gpuHandle = mgr->GetSrvManager()->GetGPUDescriptorHandle(index);
		};
	init_info.SrvDescriptorFreeFn = [](
		ImGui_ImplDX12_InitInfo*    info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE
	) {
			const auto mgr      = static_cast<ImGuiManager*>(info->UserData);
			const auto cpuIndex = mgr->GetSrvManager()->GetIndexFromCPUHandle(
				cpuHandle
			);
			mgr->GetSrvManager()->DeallocateTexture2D(cpuIndex);
		};

	ImGui_ImplDX12_Init(&init_info);
}

/// @brief ImGuiの新しいフレームを開始します。
void ImGuiManager::NewFrame() {
	// ImGuiの新しいフレームを開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

/// @brief ImGuiのフレームを終了し、描画コマンドを発行します。
void ImGuiManager::EndFrame() {
	// ImGuiのフレームを終了しレンダリング準備
	ImGui::Render();

	// レンダーターゲットとして使用されたテクスチャリソースをすべてSRV状態に遷移
	// これによりImGuiのマルチビューポートでも安全にテクスチャとして使用できる
	ID3D12DescriptorHeap* imGuiHeap = mSrvManager->GetDescriptorHeap();
	mRenderer->GetCommandList()->SetDescriptorHeaps(1, &imGuiHeap);

	// メインウィンドウのImGuiコンテンツを描画
	ImGui_ImplDX12_RenderDrawData(
		ImGui::GetDrawData(),
		mRenderer->GetCommandList()
	);

	// マルチビューポートの更新前に、すべてのコマンドを確実にフラッシュする
	mRenderer->GetCommandList()->Close();
	ID3D12CommandList* cmdLists[] = {mRenderer->GetCommandList()};
	mRenderer->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

	mRenderer->WaitPreviousFrame();

	// マルチビューポート用のレンダリング（新しいコマンドリストが内部で作成される）
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	// 新しいコマンドリストを作成して作業を続行
	mRenderer->ResetCommandList();

	ImGui::EndFrame();
}

/// @brief ImGuiをシャットダウンし、リソースを解放します。
void ImGuiManager::Shutdown() {
	ImGui::DestroyPlatformWindows();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	mSrvHeap.Reset();
}

SrvManager* ImGuiManager::GetSrvManager() const { return mSrvManager; }

#endif
