#include "BaseCharacter.h"

#include "core/ComponentRegistry.h"

namespace Unnamed {
	BaseCharacter::BaseCharacter() {}
	BaseCharacter::~BaseCharacter() {}

	void BaseCharacter::OnAttached() {
		UBaseComponent::OnAttached();
	}

	void BaseCharacter::OnDetached() {
		UBaseComponent::OnDetached();
	}

	void BaseCharacter::PrePhysicsTick(float deltaTime) {
		UBaseComponent::PrePhysicsTick(deltaTime);
	}

	void BaseCharacter::OnTick(float deltaTime) {
		UBaseComponent::OnTick(deltaTime);
	}

	void BaseCharacter::PostPhysicsTick(float deltaTime) {
		UBaseComponent::PostPhysicsTick(deltaTime);
	}

	void BaseCharacter::DrawInspectorImGui() {
		UBaseComponent::DrawInspectorImGui();
	}

	std::string_view BaseCharacter::GetStableName() const {
		return "game.BaseCharacter";
	}

	std::string_view BaseCharacter::GetComponentName() const {
		return "BaseCharacter";
	}

	void BaseCharacter::Deserialize(const JsonReader& reader) {}
	void BaseCharacter::Serialize(JsonWriter& writer) const {}

	REGISTER_COMPONENT(BaseCharacter);
}
