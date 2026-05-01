#include <pch.h>

#include <engine/Engine.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"
#include "game/team/runtime/TeamGameModule.h"

namespace {
	[[nodiscard]] bool RegisterTeamGameRuntimeModule() {
		// App がリンク済みの TeamGame モジュールを Factory へ注入する。
		if (!Unnamed::RegisterGameModule(
			"TeamGame",
			&Unnamed::CreateTeamGameModule
		)) {
			return false;
		}
		(void)Unnamed::RegisterGameModuleAlias("Team", "TeamGame");
		return true;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("TeamGameApp.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("TeamGameApp", launchOptions);

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}

	if (!RegisterTeamGameRuntimeModule()) {
		Error("TeamGameApp", "Failed to register TeamGame game module profile.");
		return EXIT_FAILURE;
	}
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule("TeamGame");
	if (!gameModule) {
		return EXIT_FAILURE;
	}
	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::STANDALONE);
	return engine.Run();
}
