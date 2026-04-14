#include <pch.h>
#include <engine/Engine.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	Unnamed::Engine engine; // エンジンの作成・実行
	return engine.Run();
}
