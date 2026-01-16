#include <pch.h>

#include <engine/Engine.h>
#include <engine/platform/Win32App.h>
#include <engine/unnamed/uengine/UEngine.h>

/// @brief エントリーポイント
/// @param lpCmdLine コマンドライン引数
/// @return 終了コード
int WINAPI wWinMain(HINSTANCE, HINSTANCE, const PWSTR lpCmdLine, int) {
	// リークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	[[maybe_unused]] HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// デフォルトは旧エンジン。引数で指定されたら新エンジンを起動する
	const bool bNewEngine =
		lpCmdLine != nullptr &&
		std::wcsstr(lpCmdLine, L"-new") != nullptr;

	if (bNewEngine) {
		Unnamed::UEngine engine;
		engine.Run();
	} else {
		Unnamed::Engine engine;
		if (!engine.Init()) {
			UASSERT(false && "Failed to initialize Engine");
		}
		while (!Win32App::PollEvents()) {
			if (OldWindowManager::ProcessMessage()) {
				break;
			}
			engine.Update();
		}
		engine.Shutdown();
	}

	CoUninitialize();
	return EXIT_SUCCESS;
}
