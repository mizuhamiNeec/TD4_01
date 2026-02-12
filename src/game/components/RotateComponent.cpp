#include "RotateComponent.h"

#include <core/math/Quaternion.h>

#include <engine/Components/Transform/SceneComponent.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiWidgets.h>

void RotateComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mTransform = mOwner->GetTransform();
}

void RotateComponent::PrePhysics(const float deltaTime) {
	if (!mRotationEnabled || !mTransform) { return; }

	Vec3 rotationDelta = Vec3::zero;

	if (mPitchEnabled) { rotationDelta.x = mRotationRate.x * deltaTime; }
	if (mYawEnabled) { rotationDelta.y = mRotationRate.y * deltaTime; }
	if (mRollEnabled) { rotationDelta.z = mRotationRate.z * deltaTime; }

	if (rotationDelta.x != 0.0f || rotationDelta.y != 0.0f || rotationDelta.z !=
	    0.0f) {
		const Vec3       rotationDeltaRad = rotationDelta * Math::deg2Rad;
		const Quaternion deltaRotation    = Quaternion::Euler(rotationDeltaRad);

		const Quaternion currentRotation = mTransform->GetLocalRot();
		Quaternion       newRotation     = currentRotation * deltaRotation;
		newRotation.Normalize();

		mTransform->SetLocalRot(newRotation);
	}
}

void RotateComponent::Update(const float deltaTime) { deltaTime; }

void RotateComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (
		ImGui::CollapsingHeader(
			"RotateComponent", ImGuiTreeNodeFlags_DefaultOpen
		)
	) {
		ImGui::Checkbox("Enable Rotation", &mRotationEnabled);
		ImGui::Checkbox("Enable Pitch", &mPitchEnabled);
		ImGui::Checkbox("Enable Yaw", &mYawEnabled);
		ImGui::Checkbox("Enable Roll", &mRollEnabled);

		ImGuiWidgets::DragVec3(
			"Rotation Rate (deg/s)", mRotationRate,
			Vec3::zero, 0.1f, "%.2f"
		);
	}

#endif
}
