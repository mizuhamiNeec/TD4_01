#include "GameRuleSystemComponent.h"

void MyGame::GameRuleSystemComponent::OnAttached()
{}

void MyGame::GameRuleSystemComponent::OnTick(float deltaTime)
{}

void MyGame::GameRuleSystemComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha)
{}

void MyGame::GameRuleSystemComponent::OnDetached()
{}

std::string_view MyGame::GameRuleSystemComponent::GetStableName() const {
	return "mygame.GameRuleSystemComponent";
}

std::string_view MyGame::GameRuleSystemComponent::GetComponentName() const {
	return "Game Rule System Component";
}

void MyGame::GameRuleSystemComponent::DrawInspectorImGui()
{}

void MyGame::GameRuleSystemComponent::Deserialize(const Unnamed::JsonReader & reader)
{}

void MyGame::GameRuleSystemComponent::Serialize(Unnamed::JsonWriter & writer) const
{}
