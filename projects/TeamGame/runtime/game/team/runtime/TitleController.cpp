#include "TitleController.h"

#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>

std::string_view TitleController::GetStableName() const {
	return "mygame.TitleController";
}

std::string_view TitleController::GetComponentName() const {
	return "TitleController";
}

void TitleController::Deserialize(const Unnamed::JsonReader& reader) {
	mCommand = reader.Read<std::string>("command").value_or(mCommand);
}

void TitleController::Serialize(Unnamed::JsonWriter& writer) const {
	writer.Key("command");
	writer.Write(mCommand);
}

void TitleController::OnAttached() {
	BaseComponent::OnAttached();
}

void TitleController::OnFrameInputTick(const float deltaTime) {
	if (GetInputSystem()->IsPressed("gamestart")) {
		GetConsoleSystem()->ExecuteCommand(mCommand);
	}
}

Unnamed::BaseComponent::TICK_GROUP TitleController::GetTickGroup() const {
	return BaseComponent::GetTickGroup();
}
