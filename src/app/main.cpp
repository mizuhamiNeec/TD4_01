#include <pch.h>
#include <engine/Engine.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"

namespace {
	[[nodiscard]] std::string ResolveStandaloneGameName(
		const Unnamed::AppLaunchOptions& launchOptions
	) {
		if (!launchOptions.gameName.has_value() || launchOptions.gameName->empty()) {
			return "Parkour";
		}
		return *launchOptions.gameName;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
	(void)commandLine;

	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("UnnamedLauncher.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedLauncher", launchOptions);

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}

	// NOTE(Phase 4): 現在は Premake のビルド対象外です。共通ランチャ候補として維持します。
	// TODO(Phase 5): EditorMain/GameMain/TeamGameMain をこの共通ランチャへ統合します。
	Unnamed::RegisterDefaultGameModuleProfiles();

	const std::string gameName = ResolveStandaloneGameName(launchOptions);
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);
	if (!gameModule) {
		gameModule = Unnamed::CreateGameModule("Parkour");
		if (!gameModule) {
			return EXIT_FAILURE;
		}
	}
	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::STANDALONE);
	return engine.Run();
}
