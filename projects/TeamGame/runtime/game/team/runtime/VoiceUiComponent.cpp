#include "VoiceUiComponent.h"

void MyGame::VoiceUiComponent::OnAttached(){
}

void MyGame::VoiceUiComponent::OnTick(float deltaTime){
}

void MyGame::VoiceUiComponent::OnDetached(){
}

std::string_view MyGame::VoiceUiComponent::GetStableName() const {
	return "mygame.VoiceUiComponent";
}

std::string_view MyGame::VoiceUiComponent::GetComponentName() const {
	return "Voice Ui Component";
}

void MyGame::VoiceUiComponent::DrawInspectorImGui(){
}

void MyGame::VoiceUiComponent::Deserialize(const Unnamed::JsonReader & reader){

}

void MyGame::VoiceUiComponent::Serialize(Unnamed::JsonWriter & writer) const{

}
