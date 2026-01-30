#include <engine/Window/WindowManager.h>

/// @brief コンストラクタ
OldWindowManager::OldWindowManager() = default;

/// @brief デストラクタ
OldWindowManager::~OldWindowManager() {
	ClearWindows();
}

/// @brief ウィンドウを追加する
/// @param window ウィンドウポインタ
void OldWindowManager::AddWindow(std::unique_ptr<BaseWindow> window) {
	mWindows.emplace_back(std::move(window));
}

/// @brief メッセージを処理する
/// @return 終了するならtrueを返す
bool OldWindowManager::ProcessMessage() {
	MSG  msg  = {};
	bool quit = false;

	// 全体のメッセージループ
	while (PeekMessage(&msg, mWindows[0]->GetWindowHandle(), 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			quit = true;
			break;
		}
	}
	// 各ウィンドウごとの処理
	for (const auto& win : mWindows) {
		if (win->ProcessMessage()) {
			quit = true;
			break;
		}
	}

	return quit;
}

/// @brief メインウィンドウを取得する
/// @return メインウィンドウポインタ
BaseWindow* OldWindowManager::GetMainWindow() {
	return mWindows.empty() ? nullptr : mWindows[0].get();
}

/// @brief ウィンドウをすべてクリアする
void OldWindowManager::ClearWindows() {
	mWindows.clear();
}

/// @brief ウィンドウリストを取得する
/// @return ウィンドウリスト
const std::vector<std::unique_ptr<BaseWindow>>& OldWindowManager::GetWindows() {
	return mWindows;
}

std::vector<std::unique_ptr<BaseWindow>> OldWindowManager::mWindows = {};