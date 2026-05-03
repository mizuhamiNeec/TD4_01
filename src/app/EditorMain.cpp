#include <pch.h>

#include <engine/Engine.h>
#include <engine/unnamed/subsystem/console/Log.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"
#include "game/team/runtime/TeamGameModule.h"

namespace {
	[[nodiscard]] bool RegisterEditorRuntimeModules() {
		if (!Unnamed::RegisterGameModule(
			"TeamGame",
			&Unnamed::CreateTeamGameModule
		)) {
			Error(
				"EditorApp",
				"ゲームモジュール 'TeamGame' の登録に失敗しました。"
			);
			return false;
		}

		(void)Unnamed::RegisterGameModuleAlias("Team", "TeamGame");
		return true;
	}

	[[nodiscard]] std::string ResolveEditorGameName(
		const Unnamed::AppLaunchOptions& launchOptions
	) {
		if (
			!launchOptions.gameName.has_value() ||
			launchOptions.gameName->empty()
		) { return "TeamGame"; }
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
	if (!RegisterEditorRuntimeModules()) { return EXIT_FAILURE; }

	const std::string gameName = ResolveEditorGameName(launchOptions);
	const std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);

	if (!gameModule) {
		Fatal("EditorApp", "ゲームモジュール '{}' の生成に失敗しました。", gameName);
		return EXIT_FAILURE;
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
			.emitDetailedLogs            = true,
		}
	);
	if (!validated) { return EXIT_FAILURE; }
	if (launchOptions.validateStartupOnly) { return EXIT_SUCCESS; }

	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::EDITOR);
	return engine.Run();
}
