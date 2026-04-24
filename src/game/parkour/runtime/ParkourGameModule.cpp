#include "ParkourGameModule.h"

#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "game/core/replay/DemoManager.h"
#include "game/parkour/runtime/ParkourGameWorld.h"

namespace Unnamed {
	void ParkourGameModule::Initialize(EngineServices& services) {
		// 現時点の Parkour モジュールでは初期化フックは予約のみです。
		(void)services;
	}

	std::unique_ptr<World> ParkourGameModule::CreateRuntimeWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<ParkourGameWorld>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<World> ParkourGameModule::CreatePlayWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<ParkourGameWorld>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<IDemoService> ParkourGameModule::CreateDemoService() {
		return std::make_unique<DemoManager>();
	}

	void ParkourGameModule::RegisterGameComponents(
		ComponentRegistry& componentRegistry
	) {
		// コンポーネントは static 登録マクロで初期化されます。
		(void)componentRegistry;
	}

	std::string ParkourGameModule::GetDefaultStartupScenePath() const {
		return "./content/parkour/scenes/title.json";
	}

	std::string ParkourGameModule::GetDefaultUiDocumentPath() const {
		return "./content/parkour/ui/MainMenu.ui.json";
	}

	std::unique_ptr<IGameModule> CreateParkourGameModule() {
		return std::make_unique<ParkourGameModule>();
	}
}
