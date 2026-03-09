#include "RotateComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Quaternion.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	void RotateComponent::PrePhysicsTick(const float deltaTime) {
		if (!mRotationEnabled) {
			return;
		}
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		const Quaternion deltaRotation = Quaternion::EulerDegrees(
			mRotationRate * deltaTime
		);
		transform->SetRotation(transform->Rotation() * deltaRotation);
	}

	void RotateComponent::Deserialize(const JsonReader& reader) {
		const JsonReader rotationRate = reader["rotationRate"];
		if (rotationRate.Valid() && rotationRate.Size() >= 3) {
			mRotationRate = Vec3(
				rotationRate[0].GetFloat(),
				rotationRate[1].GetFloat(),
				rotationRate[2].GetFloat()
			);
		}
		const JsonReader enabled = reader["enabled"];
		if (enabled.Valid()) {
			mRotationEnabled = enabled.GetBool();
		}
	}

	void RotateComponent::Serialize(JsonWriter& writer) const {
		writer.Key("rotationRate");
		writer.BeginArray();
		writer.Write(mRotationRate.x);
		writer.Write(mRotationRate.y);
		writer.Write(mRotationRate.z);
		writer.EndArray();
		writer.Key("enabled");
		writer.Write(mRotationEnabled);
	}

#ifdef _DEBUG
	void RotateComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enabled", &mRotationEnabled);
		ImGui::DragFloat3("RotationRate", &mRotationRate.x, 0.1f);
	}
#endif

	TransformComponent* RotateComponent::GetTransform() const {
		UEntity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(RotateComponent);
}
