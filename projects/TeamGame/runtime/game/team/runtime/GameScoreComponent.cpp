#include "GameScoreComponent.h"

void MyGame::GameScoreComponent::OnAttached() {}

void MyGame::GameScoreComponent::OnTick(float deltaTime) {}

void MyGame::GameScoreComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha) {}

void MyGame::GameScoreComponent::OnDetached() {}

std::string_view MyGame::GameScoreComponent::GetStableName() const {
	return "mygame.GameScoreComponent";
}

std::string_view MyGame::GameScoreComponent::GetComponentName() const {
	return "Game Score Component";
}

void MyGame::GameScoreComponent::DrawInspectorImGui() {}

void MyGame::GameScoreComponent::Deserialize(const Unnamed::JsonReader& reader) {}

void MyGame::GameScoreComponent::Serialize(Unnamed::JsonWriter& writer) const {

}
