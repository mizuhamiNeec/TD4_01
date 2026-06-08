#include "GameCountDownComponent.h"

void MyGame::GameCountDownComponent::OnAttached() {

}

void MyGame::GameCountDownComponent::OnTick(float deltaTime) {

}

void MyGame::GameCountDownComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha) {

}

void MyGame::GameCountDownComponent::OnDetached() {

}

std::string_view MyGame::GameCountDownComponent::GetStableName() const {
	return "mygame.GameCountDownComponent";
}
std::string_view MyGame::GameCountDownComponent::GetComponentName() const {
	return "Game Count Down Component";
}

void MyGame::GameCountDownComponent::DrawInspectorImGui() {

}

void MyGame::GameCountDownComponent::Deserialize(const Unnamed::JsonReader& reader) {

}

void MyGame::GameCountDownComponent::Serialize(Unnamed::JsonWriter& writer) const {

}

void MyGame::GameCountDownComponent::Start(float seconds) {}

void MyGame::GameCountDownComponent::Stop() {}

void MyGame::GameCountDownComponent::Reset() {}

bool MyGame::GameCountDownComponent::IsActive() const {
	return false;
}

bool MyGame::GameCountDownComponent::IsFinished() const {
	return false;
}

float MyGame::GameCountDownComponent::GetRemainingTime() const {
	return 0.0f;
}

float MyGame::GameCountDownComponent::GetProgress01() const {
	return 0.0f;
}
