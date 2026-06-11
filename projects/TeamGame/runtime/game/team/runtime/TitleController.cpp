#include "TitleController.h"

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>

std::string_view TitleController::GetStableName() const {
	return "mygame.TitleController";
}

std::string_view TitleController::GetComponentName() const {
	return "TitleController";
}

void TitleController::Deserialize(const Unnamed::JsonReader& reader) {
	(void)reader;
}

void TitleController::Serialize(Unnamed::JsonWriter& writer) const {
	(void)writer;
}

void TitleController::OnAttached() {
	BaseComponent::OnAttached();
}

void TitleController::OnTick(const float deltaTime) {
	if (GetInputSystem()->IsPressed("gamestart")) {
		GetConsoleSystem()->ExecuteCommand("map ./projects/TeamGame/content/scenes/game.json");
	}
}

Unnamed::BaseComponent::TICK_GROUP TitleController::GetTickGroup() const {
	return BaseComponent::GetTickGroup();
}
