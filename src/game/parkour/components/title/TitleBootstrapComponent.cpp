#include "TitleBootstrapComponent.h"

#include "core/ComponentRegistry.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/world/World.h"

namespace Unnamed {
	void TitleBootstrapComponent::OnTick(const float deltaTime) {
		(void)deltaTime;
		if (mTriggered) {
			return;
		}

		ConsoleSystem* console = GetConsoleSystem();
		World*         world   = GetWorld();
		if (!console || !world) {
			return;
		}

		// タイトルモードを有効化してからタイトル専用ゲームシーンへ遷移します。
		console->ExecuteCommand("cl_title_mode 1");
		world->RequestSceneTransition("./content/parkour/scenes/title_game.json");
		mTriggered = true;
	}

	std::string_view TitleBootstrapComponent::GetStableName() const {
		return "parkour.TitleBootstrap";
	}

	std::string_view TitleBootstrapComponent::GetComponentName() const {
		return "TitleBootstrap";
	}

	uint32_t TitleBootstrapComponent::GetIcon() const {
		return kIconDesktopLandscape;
	}

	void TitleBootstrapComponent::Deserialize(const JsonReader& reader) {
		(void)reader;
	}

	void TitleBootstrapComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	REGISTER_COMPONENT(TitleBootstrapComponent);
}

