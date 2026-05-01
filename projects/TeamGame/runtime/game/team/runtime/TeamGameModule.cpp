#include "TeamGameModule.h"
#include "TeamGameComponentRegistration.h"

#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/world/World.h"

namespace Unnamed {
	void TeamGameModule::Initialize(EngineServices& services) {
		(void)services;
	}

	std::unique_ptr<World> TeamGameModule::CreateRuntimeWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<World> TeamGameModule::CreatePlayWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<IDemoService> TeamGameModule::CreateDemoService() {
		return nullptr;
	}

	void TeamGameModule::RegisterGameComponents(
		ComponentRegistry& componentRegistry
	) {
		RegisterTeamGameComponents(componentRegistry);
	}

	GameModulePaths TeamGameModule::GetGameModulePaths() const {
		return {
			.gameName            = "TeamGame",
			.gameRoot            = "./projects/TeamGame",
			.contentRoot         = "./projects/TeamGame/content",
			.configRoot          = "./projects/TeamGame/config",
			.defaultStartupScene = "scenes/bootstrap.json",
		};
	}

	std::string TeamGameModule::GetDefaultStartupScenePath() const {
		return GetGameModulePaths().defaultStartupScene;
	}

	std::string TeamGameModule::GetDefaultUiDocumentPath() const {
		return {};
	}

	std::unique_ptr<IGameModule> CreateTeamGameModule() {
		return std::make_unique<TeamGameModule>();
	}
}
