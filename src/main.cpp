#include <pch.h>

#include <engine/Engine.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // リークチェック
	[[maybe_unused]] HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// エンジンの作成・実行
	Unnamed::Engine engine;
	engine.Run();

	CoUninitialize();
	return EXIT_SUCCESS;
}
