#include "GameCountDownComponent.h"

void MyGame::GameCountDownComponent::OnAttached(){
	
}

void MyGame::GameCountDownComponent::OnTick(float deltaTime){

}

void MyGame::GameCountDownComponent::OnRenderTick(float renderDeltaTime, float interpolationAlpha){

}

void MyGame::GameCountDownComponent::OnDetached(){

}

std::string_view MyGame::GameCountDownComponent::GetStableName() const {
	return "mygame.GameCountDownComponent";
}
std::string_view MyGame::GameCountDownComponent::GetComponentName() const {
	return "Game Count Down Component";
}

void MyGame::GameCountDownComponent::DrawInspectorImGui(){

}

void MyGame::GameCountDownComponent::Deserialize(const Unnamed::JsonReader & reader){

}

void MyGame::GameCountDownComponent::Serialize(Unnamed::JsonWriter & writer) const{

}
