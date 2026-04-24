#include <pch.h>

#include <engine/Engine.h>

#include <game/parkour/runtime/ParkourGameModule.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateParkourGameModule();
	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::EDITOR);
	return engine.Run();
}
