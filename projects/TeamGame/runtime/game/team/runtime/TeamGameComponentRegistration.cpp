#include "TeamGameComponentRegistration.h"

#include "core/ComponentRegistry.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "MicComponent.h"
#include "PlayerMoveComponent.h"
#include "PlayerControlComponent.h"

namespace Unnamed {
	namespace {
		template <typename T>
		void RegisterComponentIfMissing(
			ComponentRegistry &componentRegistry,
			const std::string_view stableName,
			const std::string_view displayName) {
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				displayName);
			if (!registered) {
				Warning(
					"TeamGameRuntime",
					"Failed to register game component '{}'.",
					stableName);
			}
		}
	}

	void RegisterTeamGameComponents(ComponentRegistry &componentRegistry) {
		RegisterComponentIfMissing<MyGame::MicComponent>(
			componentRegistry, "mygame.MicComponent", "Mic Component");
		RegisterComponentIfMissing<MyGame::PlayerMoveComponent>(
			componentRegistry, "mygame.PlayerMoveComponent", "Player Move Component");
		RegisterComponentIfMissing<MyGame::PlayerControlComponent>(
			componentRegistry, "mygame.PlayerControlComponent", "Player Control Component");
	}
}
