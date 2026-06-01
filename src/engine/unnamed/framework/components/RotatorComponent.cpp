#include "RotatorComponent.h"

#include <engine/ImGui/ImGuiWidgets.h>

#include "TransformComponent.h"

#include "../entity/Entity.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include "core/ComponentRegistry.h"

namespace Unnamed {
	RotatorComponent::~RotatorComponent() = default;

	void RotatorComponent::OnAttached() {
		mTransform = GetOwner()->GetComponent<TransformComponent>();
	}

	void RotatorComponent::OnTick(const float deltaTime) {
		if (!mTransform) {
			Error(GetComponentName(), "TransformComponent is null.");
			return;
		}

		Quaternion current = mTransform->GetRotation();

		current = current * Quaternion::EulerDegrees(mRotationSpeed * deltaTime);
		mTransform->SetRotation(current);
	}
	
	BaseComponent::TICK_GROUP RotatorComponent::GetTickGroup() const {
		return BaseComponent::GetTickGroup();
	}

	std::string_view RotatorComponent::GetStableName() const {
		return "engine.Rotator";
	}

	std::string_view RotatorComponent::GetComponentName() const {
		return "Rotator";
	}

	void RotatorComponent::Deserialize(const JsonReader& reader) {		
		mRotationSpeed   = reader["rotationSpeed"].GetVec3(mRotationSpeed);
	}

	void RotatorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("rotationSpeed");
		writer.BeginArray();
		writer.Write(mRotationSpeed.x);
		writer.Write(mRotationSpeed.y);
		writer.Write(mRotationSpeed.z);
		writer.EndArray();
	}

	uint32_t RotatorComponent::GetIcon() const {
		return BaseComponent::GetIcon();
	}

	void RotatorComponent::DrawInspectorImGui() {
		ImGuiWidgets::DragVec3("RotationSpeed", mRotationSpeed,Vec3::zero,0.25f,"%.2f [deg/s]");
	}
	
	REGISTER_COMPONENT(RotatorComponent);
}
