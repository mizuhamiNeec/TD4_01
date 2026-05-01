#include <pch.h>

#include <engine/Engine.h>
#include <engine/unnamed/subsystem/console/Log.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"
#include "game/parkour/runtime/ParkourGameModule.h"
#include "game/team/runtime/TeamGameModule.h"

namespace {
	[[nodiscard]] bool RegisterEditorRuntimeModules() {
		if (!Unnamed::RegisterGameModule(
			"Parkour",
			&Unnamed::CreateParkourGameModule
		)) {
			Error(
				"EditorApp",
				"Failed to register Parkour runtime module."
			);
			return false;
		}
		if (!Unnamed::RegisterGameModule(
			"TeamGame",
			&Unnamed::CreateTeamGameModule
		)) {
			Error(
				"EditorApp",
				"Failed to register TeamGame runtime module."
			);
			return false;
		}

		(void)Unnamed::RegisterGameModuleAlias("ParkourGame", "Parkour");
		(void)Unnamed::RegisterGameModuleAlias("Team", "TeamGame");
		return true;
	}

	[[nodiscard]] std::string ResolveEditorGameName(
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
		Unnamed::PrintLaunchHelp("UnnamedEditorApp.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedEditorApp", launchOptions);

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}

	// Editor は複数ゲーム Runtime をリンクし、選択ゲームの実体モジュールを生成する。
	Unnamed::RegisterDefaultGameModuleProfiles();
	if (!RegisterEditorRuntimeModules()) {
		return EXIT_FAILURE;
	}

	const std::string gameName = ResolveEditorGameName(launchOptions);
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);
	if (!gameModule) {
		Error("EditorApp", "Unknown game profile '{}'. Fallback to Parkour", gameName);
		gameModule = Unnamed::CreateGameModule("Parkour");
		if (!gameModule) {
			Fatal("EditorApp", "Failed to create fallback Parkour game module.");
			return EXIT_FAILURE;
		}
	}

#ifdef _DEBUG
	constexpr bool failOnUnknown = true;
#else
	constexpr bool failOnUnknown = false;
#endif
	const bool validated = Unnamed::ValidateGameModuleStartupProfile(
		*gameModule,
		{
			.failOnUnknownComponentTypes = failOnUnknown,
			.emitDetailedLogs = true,
		}
	);
	if (!validated) {
		return EXIT_FAILURE;
	}
	if (launchOptions.validateStartupOnly) {
		return EXIT_SUCCESS;
	}

	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::EDITOR);
	return engine.Run();
}
